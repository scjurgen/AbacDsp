#!/usr/bin/env python3
"""Splits a WAV file into separate WAV files at points of detected silence.

Preprocessing for analyze_sample.py: a render that turns out to contain more than one
note/take back to back (see its onset_count field) needs cutting into individual files
before per-note analysis makes sense.

Usage:
    split_on_silence.py <wav_file> [-o/--output-dir DIR] [--silence-db -50]
                         [--min-silence-sec 0.3] [--min-segment-sec 0.15] [--padding-sec 0.05]

DIR defaults to the repo's SynthSamples/ folder, written flat as <stem>_NNN.wav.
"""
from __future__ import annotations

import argparse
from pathlib import Path

import numpy as np
import soundfile as sf

FRAME_SEC = 0.02
HOP_SEC = 0.01
REPO_ROOT = Path(__file__).resolve().parents[2]


def to_db(linear, floor=-180.0):
    return np.maximum(20.0 * np.log10(np.maximum(linear, 10 ** (floor / 20.0))), floor)


def load_audio(path):
    data, sr = sf.read(path, always_2d=True)
    info = sf.info(str(path))
    return data.astype(np.float64), sr, info.subtype


def detection_envelope_db(data, sr):
    peak_per_sample = np.max(np.abs(data), axis=1)
    frame_len = max(1, int(FRAME_SEC * sr))
    hop_len = max(1, int(HOP_SEC * sr))
    n_frames = max(0, (len(peak_per_sample) - frame_len) // hop_len + 1)
    db = np.empty(n_frames)
    for i in range(n_frames):
        start = i * hop_len
        frame = peak_per_sample[start : start + frame_len]
        db[i] = to_db(float(np.sqrt(np.mean(frame ** 2))))
    return db, hop_len


def find_silent_runs(db, silence_db, min_silence_frames):
    silent = db < silence_db
    runs = []
    start = None
    for i, is_silent in enumerate(silent):
        if is_silent and start is None:
            start = i
        elif not is_silent and start is not None:
            if i - start >= min_silence_frames:
                runs.append((start, i))
            start = None
    if start is not None and len(silent) - start >= min_silence_frames:
        runs.append((start, len(silent)))
    return runs


def segments_between_gaps(n_frames, gaps):
    bounds = [0]
    for gap_start, gap_end in gaps:
        bounds.append((gap_start + gap_end) // 2)
    bounds.append(n_frames)
    return [(bounds[i], bounds[i + 1]) for i in range(len(bounds) - 1)]


def trim_segment(db, start_frame, end_frame, silence_db, padding_frames):
    above = np.where(db[start_frame:end_frame] >= silence_db)[0]
    if len(above) == 0:
        return None
    trimmed_start = start_frame + max(0, int(above[0]) - padding_frames)
    trimmed_end = start_frame + min(end_frame - start_frame, int(above[-1]) + 1 + padding_frames)
    return trimmed_start, trimmed_end


def split_file(path, output_dir, silence_db, min_silence_sec, min_segment_sec, padding_sec):
    data, sr, subtype = load_audio(path)
    db, hop_len = detection_envelope_db(data, sr)

    min_silence_frames = max(1, int(round(min_silence_sec / HOP_SEC)))
    min_segment_frames = max(1, int(round(min_segment_sec / HOP_SEC)))
    padding_frames = max(0, int(round(padding_sec / HOP_SEC)))

    gaps = find_silent_runs(db, silence_db, min_silence_frames)
    raw_segments = segments_between_gaps(len(db), gaps)

    output_dir.mkdir(parents=True, exist_ok=True)
    stem = path.stem
    written = []
    for start_frame, end_frame in raw_segments:
        if end_frame - start_frame < min_segment_frames:
            continue
        trimmed = trim_segment(db, start_frame, end_frame, silence_db, padding_frames)
        if trimmed is None:
            continue
        trimmed_start, trimmed_end = trimmed
        if trimmed_end - trimmed_start < min_segment_frames:
            continue

        sample_start = trimmed_start * hop_len
        sample_end = min(len(data), trimmed_end * hop_len)
        out_path = output_dir / f"{stem}_{len(written) + 1:03d}.wav"
        sf.write(out_path, data[sample_start:sample_end], sr, subtype=subtype)
        written.append((out_path, sample_start / sr, sample_end / sr))

    return written


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("wav_file", type=Path)
    parser.add_argument("-o", "--output-dir", type=Path, default=None)
    parser.add_argument("--silence-db", type=float, default=-50.0)
    parser.add_argument("--min-silence-sec", type=float, default=0.3)
    parser.add_argument("--min-segment-sec", type=float, default=0.15)
    parser.add_argument("--padding-sec", type=float, default=0.05)
    args = parser.parse_args()

    output_dir = args.output_dir or (REPO_ROOT / "SynthSamples")
    written = split_file(args.wav_file, output_dir, args.silence_db, args.min_silence_sec,
                          args.min_segment_sec, args.padding_sec)

    if not written:
        print("No segments found above the silence threshold.")
        return
    for out_path, start_sec, end_sec in written:
        print(f"{out_path}  [{start_sec:.3f}s - {end_sec:.3f}s, {end_sec - start_sec:.3f}s]")


if __name__ == "__main__":
    main()
