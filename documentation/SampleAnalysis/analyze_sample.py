#!/usr/bin/env python3
"""Estimates harmonic content, envelope shape, and modulation from a rendered synth-patch
WAV file, and writes a JSON report plus an illustrative PNG plot.

Everything here is heuristic (peak-picking, slope thresholds, periodicity detection on a
detrended signal) rather than a source-separation model, so the report speaks in terms of
"estimated"/"likely" and the numbers should be read as a starting point for ear-matching a
patch, not a certified measurement.

Usage:
    analyze_sample.py <wav_file> [-o/--output-dir DIR]

Writes <output-dir>/<name>.analysis.json and <output-dir>/<name>.analysis.png. DIR defaults
to a "generated" folder next to this script.
"""
from __future__ import annotations

import argparse
import json
from dataclasses import dataclass
from pathlib import Path

import librosa
import matplotlib
import numpy as np
import soundfile as sf
from scipy.signal import butter, sosfiltfilt
from scipy.signal.windows import hann

matplotlib.use("Agg")
import matplotlib.pyplot as plt

HOP_LENGTH = 512
FRAME_LENGTH = 2048
EPS_LIN = 1e-6
DB_FLOOR = -180.0
STEREO_BANDS_HZ = [(20, 200), (200, 800), (800, 3000), (3000, 8000), (8000, None)]
PLATEAU_SLOPE_DB_PER_SEC = 6.0
PLATEAU_MIN_DUR_SEC = 0.15
SLOPE_SMOOTH_SEC = 0.1
SLOPE_RUN_PERCENTILE = 90
AM_FREQ_RANGE_HZ = (0.5, 20.0)
VIBRATO_FREQ_RANGE_HZ = (2.0, 12.0)
FILTER_SWEEP_FREQ_RANGE_HZ = (0.2, 10.0)
PERIODICITY_PROMINENCE_RATIO = 4.0


# ---------------------------------------------------------------------------
# small numeric helpers
# ---------------------------------------------------------------------------

def to_db(linear, floor=DB_FLOOR):
    return np.maximum(20.0 * np.log10(np.maximum(linear, 10 ** (floor / 20.0))), floor)


def rms_dbfs(x):
    if x is None or len(x) == 0:
        return None
    rms = float(np.sqrt(np.mean(np.asarray(x, dtype=np.float64) ** 2)))
    return float(to_db(rms))


def peak_dbfs(x):
    if x is None or len(x) == 0:
        return None
    peak = float(np.max(np.abs(x)))
    return float(to_db(peak))


def safe_float(x):
    return float(x) if x is not None and np.isfinite(x) else None


def longest_true_run(mask):
    best_start, best_len, cur_start, cur_len = None, 0, None, 0
    for i, v in enumerate(mask):
        if v:
            if cur_start is None:
                cur_start = i
            cur_len += 1
            if cur_len > best_len:
                best_start, best_len = cur_start, cur_len
        else:
            cur_start, cur_len = None, 0
    if best_start is None:
        return None
    return best_start, best_start + best_len


# ---------------------------------------------------------------------------
# I/O
# ---------------------------------------------------------------------------

def load_audio(path):
    data, sr = sf.read(path, always_2d=True)
    data = data.astype(np.float64)
    stereo = data if data.shape[1] >= 2 else None
    mono = data.mean(axis=1)
    return mono, stereo, sr


# ---------------------------------------------------------------------------
# envelope + segmentation
# ---------------------------------------------------------------------------

@dataclass
class Segments:
    peak_idx: int
    attack_start_idx: int
    decay_end_idx: int
    release_start_idx: int | None
    release_end_idx: int


def compute_envelope(mono, sr):
    rms = librosa.feature.rms(y=mono, frame_length=FRAME_LENGTH, hop_length=HOP_LENGTH)[0]
    times = librosa.times_like(rms, sr=sr, hop_length=HOP_LENGTH)
    return times, to_db(rms), rms


def moving_average(x, window):
    if window <= 1:
        return x
    pad_before = window // 2
    padded = np.pad(x, (pad_before, window - 1 - pad_before), mode="edge")
    return np.convolve(padded, np.ones(window) / window, mode="valid")


def detect_segments(times, db, noise_floor_db) -> Segments:
    peak_idx = int(np.argmax(db))
    peak_db = db[peak_idx]

    attack_floor_db = peak_db - 40.0
    below = np.where(db[: peak_idx + 1] <= attack_floor_db)[0]
    attack_start_idx = int(below[-1] + 1) if len(below) else 0

    frame_dur = times[1] - times[0] if len(times) > 1 else HOP_LENGTH / 44100.0
    plateau_frames = max(3, int(round(PLATEAU_MIN_DUR_SEC / frame_dur)))
    # Frame-to-frame dB slope is far noisier than "dB/sec" suggests at this frame rate (a
    # single frame's worth of jitter reads as a huge rate), so smooth before differentiating
    # rather than requiring every raw frame to individually pass the flatness threshold.
    smooth_frames = max(1, int(round(SLOPE_SMOOTH_SEC / frame_dur)))
    db_smooth = moving_average(db, smooth_frames)
    deriv = np.diff(db_smooth) / np.diff(times) if len(times) > 1 else np.zeros(0)

    decay_end_idx = _find_flat_run(deriv, peak_idx, len(db), plateau_frames, db, noise_floor_db)
    if decay_end_idx is None:
        # No plateau above the noise floor at all: nothing principled separates "decay"
        # from "release" here, so the whole post-peak tail is treated as one release stage.
        release_end_idx = _trim_release_tail(db, peak_idx, noise_floor_db)
        return Segments(peak_idx, attack_start_idx, peak_idx, peak_idx, release_end_idx)

    release_start_idx = _find_decline_run(deriv, decay_end_idx, len(db), plateau_frames)
    if release_start_idx is None:
        return Segments(peak_idx, attack_start_idx, decay_end_idx, None, len(db) - 1)

    release_end_idx = _trim_release_tail(db, release_start_idx, noise_floor_db)
    return Segments(peak_idx, attack_start_idx, decay_end_idx, release_start_idx, release_end_idx)


def _trim_release_tail(db, release_start_idx, noise_floor_db, margin_db=3.0):
    tail = np.where(db[release_start_idx:] > noise_floor_db + margin_db)[0]
    return release_start_idx + int(tail[-1]) if len(tail) else len(db) - 1


def _find_flat_run(deriv, start, n, run_len, db, noise_floor_db, margin_db=6.0):
    for i in range(start, n - run_len):
        window = np.abs(deriv[i : i + run_len])
        if np.percentile(window, SLOPE_RUN_PERCENTILE) < PLATEAU_SLOPE_DB_PER_SEC and \
                np.mean(db[i : i + run_len]) > noise_floor_db + margin_db:
            return i
    return None


def _find_decline_run(deriv, start, n, run_len):
    for i in range(start, n - run_len):
        window = deriv[i : i + run_len]
        if np.percentile(window, SLOPE_RUN_PERCENTILE) <= -PLATEAU_SLOPE_DB_PER_SEC:
            return i
    return None


def classify_shape(t, amp_linear):
    if len(t) < 3:
        return None
    t0 = t - t[0]
    amp_linear = np.maximum(amp_linear, EPS_LIN)

    lin_coeffs = np.polyfit(t0, amp_linear, 1)
    lin_err = float(np.sqrt(np.mean((np.polyval(lin_coeffs, t0) - amp_linear) ** 2)))

    db_coeffs = np.polyfit(t0, to_db(amp_linear), 1)
    exp_pred_linear = 10 ** (np.polyval(db_coeffs, t0) / 20.0)
    exp_err = float(np.sqrt(np.mean((exp_pred_linear - amp_linear) ** 2)))

    shape = "exponential" if exp_err <= lin_err else "linear"
    return {
        "shape": shape,
        "rate_db_per_sec": float(db_coeffs[0]),
        "linear_fit_rmse": lin_err,
        "exponential_fit_rmse": exp_err,
    }


# ---------------------------------------------------------------------------
# level stats
# ---------------------------------------------------------------------------

def estimate_noise_floor(mono, sr, attack_start_sample):
    # A pre-onset window isn't trustworthy on its own: some renders start hot (no leading
    # silence) or have a real silent gap elsewhere in the file that's quieter than the
    # pre-roll. Always scan for the quietest window too and take whichever is lower.
    candidates = []
    pre_roll = mono[:attack_start_sample]
    if len(pre_roll) >= int(0.05 * sr):
        candidates.append((rms_dbfs(pre_roll), "pre_onset_silence"))

    window = int(0.1 * sr)
    if window < len(mono):
        hop = max(1, window // 4)
        best = None
        for start in range(0, len(mono) - window, hop):
            val = rms_dbfs(mono[start : start + window])
            if best is None or val < best:
                best = val
        candidates.append((best, "quietest_window_scan"))
    else:
        candidates.append((rms_dbfs(mono), "whole_file_fallback"))

    return min(candidates, key=lambda c: c[0])


def compute_level_stats(mono, times, sr, segments: Segments, noise_floor_db, noise_floor_method):
    def seg_slice(a, b):
        return mono[int(times[a] * sr) : int(times[b] * sr)]

    release_start = segments.release_start_idx
    stats = {
        "peak_dbfs": peak_dbfs(mono),
        "rms_dbfs": rms_dbfs(mono),
        "noise_floor_dbfs": noise_floor_db,
        "noise_floor_method": noise_floor_method,
        "segment_rms_dbfs": {
            "attack": rms_dbfs(seg_slice(segments.attack_start_idx, segments.peak_idx)),
            "decay": rms_dbfs(seg_slice(segments.peak_idx, segments.decay_end_idx))
            if segments.decay_end_idx > segments.peak_idx
            else None,
            "sustain": rms_dbfs(seg_slice(segments.decay_end_idx, release_start))
            if release_start is not None and release_start > segments.decay_end_idx
            else None,
            "release": rms_dbfs(seg_slice(release_start, segments.release_end_idx))
            if release_start is not None
            else None,
        },
    }
    stats["crest_factor_db"] = safe_float(stats["peak_dbfs"] - stats["rms_dbfs"])
    return stats


# ---------------------------------------------------------------------------
# pitch / harmonics
# ---------------------------------------------------------------------------

def track_pitch(mono, sr):
    f0, voiced_flag, _voiced_prob = librosa.pyin(
        mono,
        fmin=librosa.note_to_hz("C1"),
        fmax=librosa.note_to_hz("C7"),
        sr=sr,
        frame_length=FRAME_LENGTH,
        hop_length=HOP_LENGTH,
    )
    f0_times = librosa.times_like(f0, sr=sr, hop_length=HOP_LENGTH)
    voiced_flag = voiced_flag & ~np.isnan(f0)
    return f0, f0_times, voiced_flag


def hz_to_note_info(f0_hz):
    if f0_hz is None or f0_hz <= 0 or not np.isfinite(f0_hz):
        return None
    midi = librosa.hz_to_midi(f0_hz)
    nearest_midi = int(round(midi))
    return {
        "note": librosa.midi_to_note(nearest_midi),
        "cents_offset": float((midi - nearest_midi) * 100.0),
    }


def pick_harmonic_window(times, segments: Segments, f0_times, voiced_flag):
    release_start = segments.release_start_idx if segments.release_start_idx is not None else len(times) - 1
    if release_start - segments.decay_end_idx >= 3:
        start_sec, end_sec = times[segments.decay_end_idx], times[release_start]
        if end_sec - start_sec >= 0.05:
            return start_sec, end_sec, "sustain_plateau"

    run = longest_true_run(voiced_flag)
    if run is not None and f0_times[run[1] - 1] - f0_times[run[0]] >= 0.05:
        return f0_times[run[0]], f0_times[run[1] - 1], "longest_voiced_run"

    duration = times[-1]
    return duration * 0.25, duration * 0.75, "middle_half_fallback"


def analyze_harmonics(mono, sr, start_sec, end_sec, f0_hz):
    start = int(start_sec * sr)
    end = min(int(end_sec * sr), len(mono))
    max_len = int(1.0 * sr)
    seg = mono[start : min(end, start + max_len)]
    if len(seg) < 256 or f0_hz is None or not np.any(seg):
        return None

    window = np.hanning(len(seg))
    spec = np.fft.rfft(seg * window)
    mag = np.abs(spec)
    freqs = np.fft.rfftfreq(len(seg), d=1.0 / sr)
    mag_db = to_db(mag)
    ref_db = float(np.max(mag_db))
    mag_db_rel = mag_db - ref_db
    bin_width = freqs[1] - freqs[0]
    nyquist = sr / 2.0

    partials, n, consecutive_below = [], 1, 0
    while True:
        target = n * f0_hz
        if target > nyquist - bin_width:
            break
        span = max(2 * bin_width, 0.03 * target)
        idx_lo = max(0, int((target - span) / bin_width))
        idx_hi = min(len(freqs) - 1, int((target + span) / bin_width) + 1)
        if idx_hi <= idx_lo:
            break
        local_idx = idx_lo + int(np.argmax(mag_db_rel[idx_lo : idx_hi + 1]))
        level_db = float(mag_db_rel[local_idx])
        partials.append({"n": n, "freq_hz": float(freqs[local_idx]), "ideal_freq_hz": float(target), "level_db": level_db})
        consecutive_below = consecutive_below + 1 if level_db < -70.0 else 0
        if consecutive_below >= 5 or n >= 60:
            break
        n += 1

    deviations = [abs(p["freq_hz"] - p["ideal_freq_hz"]) / p["ideal_freq_hz"] for p in partials if p["level_db"] > -60.0]
    inharmonicity_pct = float(np.mean(deviations) * 100.0) if deviations else None

    def db_to_power(db):
        return 10 ** (db / 10.0)

    odd_power = sum(db_to_power(p["level_db"]) for p in partials if p["n"] % 2 == 1)
    even_power = sum(db_to_power(p["level_db"]) for p in partials if p["n"] % 2 == 0)
    odd_even_ratio_db = float(10 * np.log10(odd_power / even_power)) if even_power > 0 else None

    centroid_hz = float(np.sum(freqs * mag) / np.sum(mag))
    cumulative_energy = np.cumsum(mag ** 2)
    rolloff_idx = int(np.searchsorted(cumulative_energy, 0.85 * cumulative_energy[-1]))
    rolloff_hz = float(freqs[min(rolloff_idx, len(freqs) - 1)])

    return {
        "fundamental_hz": float(f0_hz),
        "partials": partials,
        "inharmonicity_pct": inharmonicity_pct,
        "odd_even_energy_ratio_db": odd_even_ratio_db,
        "spectral_centroid_hz": centroid_hz,
        "spectral_rolloff_hz": rolloff_hz,
    }, freqs, mag_db_rel


# ---------------------------------------------------------------------------
# modulation
# ---------------------------------------------------------------------------

def detect_periodicity(signal, frame_rate_hz, freq_range):
    n = len(signal)
    if n < 8:
        return None
    idx = np.arange(n)
    detrended = signal - np.polyval(np.polyfit(idx, signal, 1), idx)
    spec = np.abs(np.fft.rfft(detrended * np.hanning(n)))
    freqs = np.fft.rfftfreq(n, d=1.0 / frame_rate_hz)
    mask = (freqs >= freq_range[0]) & (freqs <= freq_range[1])
    if not np.any(mask):
        return None
    band_freqs, band_spec = freqs[mask], spec[mask]
    peak_idx = int(np.argmax(band_spec))
    median_val = float(np.median(spec[mask])) + 1e-12
    if band_spec[peak_idx] < PERIODICITY_PROMINENCE_RATIO * median_val:
        return None
    duration_sec = n / frame_rate_hz
    if duration_sec * band_freqs[peak_idx] < 2.0:
        return None  # fewer than 2 full cycles observed: too little evidence to call it periodic
    return {
        "rate_hz": float(band_freqs[peak_idx]),
        "depth_peak_to_peak": float(np.max(detrended) - np.min(detrended)),
        "prominence_ratio": float(band_spec[peak_idx] / median_val),
    }


def analyze_amplitude_modulation(times, db, segments: Segments):
    release_start = segments.release_start_idx if segments.release_start_idx is not None else len(db)
    lo, hi = segments.decay_end_idx, release_start
    if hi - lo < 8:
        return {"detected": False, "reason": "sustain region too short to analyze"}
    frame_rate_hz = 1.0 / (times[1] - times[0])
    result = detect_periodicity(db[lo:hi], frame_rate_hz, AM_FREQ_RANGE_HZ)
    if result is None:
        return {"detected": False}
    return {"detected": True, "rate_hz": result["rate_hz"], "depth_db": result["depth_peak_to_peak"]}


def analyze_pitch_modulation(f0, f0_times, voiced_flag):
    run = longest_true_run(voiced_flag)
    if run is None or run[1] - run[0] < 8:
        return {"detected": False, "reason": "no sufficiently long voiced region"}
    lo, hi = run
    segment_f0 = f0[lo:hi]
    median_f0 = float(np.median(segment_f0))
    cents = 1200.0 * np.log2(segment_f0 / median_f0)
    frame_rate_hz = 1.0 / (f0_times[1] - f0_times[0])

    result = detect_periodicity(cents, frame_rate_hz, VIBRATO_FREQ_RANGE_HZ)
    if result is not None:
        return {"detected": True, "type": "vibrato", "rate_hz": result["rate_hz"], "depth_cents": result["depth_peak_to_peak"]}

    idx = np.arange(len(cents))
    drift_slope = np.polyfit(idx, cents, 1)[0] * (len(cents) - 1)
    if abs(drift_slope) > 5.0:
        return {"detected": True, "type": "drift", "drift_semitones": float(drift_slope / 100.0)}
    return {"detected": False}


def analyze_unison_beating(amplitude_mod, pitch_mod, fundamental_hz):
    if not amplitude_mod.get("detected") or fundamental_hz is None:
        return {"likely_cause": "none"}
    am_rate = amplitude_mod["rate_hz"]
    if pitch_mod.get("detected") and pitch_mod.get("type") == "vibrato":
        if abs(am_rate - pitch_mod["rate_hz"]) / max(am_rate, pitch_mod["rate_hz"]) < 0.25:
            return {"likely_cause": "lfo_linked_vibrato_and_tremolo"}
        return {"likely_cause": "tremolo_lfo"}
    estimated_detune_cents = float(1200.0 * np.log2((fundamental_hz + am_rate) / fundamental_hz))
    return {"likely_cause": "possible_unison_beating", "estimated_detune_cents": estimated_detune_cents}


def analyze_filter_sweep(mono, sr, times, segments: Segments):
    centroid = librosa.feature.spectral_centroid(y=mono, sr=sr, hop_length=HOP_LENGTH)[0]
    centroid_times = librosa.times_like(centroid, sr=sr, hop_length=HOP_LENGTH)
    end_idx = segments.release_end_idx if segments.release_end_idx else len(times) - 1
    start_sec, end_sec = times[segments.attack_start_idx], times[end_idx]
    mask = (centroid_times >= start_sec) & (centroid_times <= end_sec)
    c = centroid[mask]
    if len(c) < 5:
        return {"direction": "unknown", "reason": "not enough sounding frames to analyze"}

    edge = max(1, len(c) // 10)
    start_val, end_val = float(np.mean(c[:edge])), float(np.mean(c[-edge:]))
    frac_change = (end_val - start_val) / max(start_val, 1e-6)

    frame_rate_hz = 1.0 / (centroid_times[1] - centroid_times[0])
    periodicity = detect_periodicity(c, frame_rate_hz, FILTER_SWEEP_FREQ_RANGE_HZ)

    if periodicity is not None and periodicity["depth_peak_to_peak"] > 0.1 * np.mean(c):
        direction = "oscillating"
    elif frac_change <= -0.15:
        direction = "closing"
    elif frac_change >= 0.15:
        direction = "opening"
    else:
        direction = "static"

    return {
        "direction": direction,
        "centroid_start_hz": start_val,
        "centroid_end_hz": end_val,
        "centroid_range_hz": float(np.max(c) - np.min(c)),
        "oscillation_rate_hz": periodicity["rate_hz"] if periodicity else None,
    }


def bandpass(x, sr, low, high):
    nyquist = sr / 2.0
    high = nyquist * 0.999 if high is None or high >= nyquist else high
    low = max(low, 1.0)
    sos = butter(4, [low / nyquist, high / nyquist], btype="band", output="sos")
    return sosfiltfilt(sos, x)


def analyze_stereo_phase(stereo, sr):
    if stereo is None:
        return None
    left, right = stereo[:, 0], stereo[:, 1]
    overall_corr = safe_float(np.corrcoef(left, right)[0, 1]) if np.std(left) > 0 and np.std(right) > 0 else None

    bands = []
    for low, high in STEREO_BANDS_HZ:
        bl, br = bandpass(left, sr, low, high), bandpass(right, sr, low, high)
        corr = safe_float(np.corrcoef(bl, br)[0, 1]) if np.std(bl) > 1e-9 and np.std(br) > 1e-9 else None
        bands.append({"low_hz": low, "high_hz": high, "correlation": corr})

    scored = [b for b in bands if b["correlation"] is not None]
    worst = min(scored, key=lambda b: b["correlation"]) if scored else None
    flagged = worst is not None and worst["correlation"] < 0.3
    return {
        "overall_correlation": overall_corr,
        "bands": bands,
        "phase_cancellation_likely": flagged,
        "flagged_band": worst if flagged else None,
    }


# ---------------------------------------------------------------------------
# notes
# ---------------------------------------------------------------------------

def build_notes(onset_count, harmonics, amplitude_mod, pitch_mod, unison, filter_sweep, stereo_phase):
    notes = []
    if onset_count > 1:
        notes.append(f"{onset_count} onsets detected - this may not be a single sustained note; "
                      "envelope/harmonic analysis assumes one.")
    if harmonics and harmonics["inharmonicity_pct"] and harmonics["inharmonicity_pct"] > 2.0:
        notes.append(f"partials deviate ~{harmonics['inharmonicity_pct']:.1f}% from an ideal harmonic series - "
                      "bell/electric-piano-like timbre or a detuned oscillator stack likely.")
    if pitch_mod.get("detected") and pitch_mod.get("type") == "vibrato":
        notes.append(f"vibrato detected at {pitch_mod['rate_hz']:.2f} Hz, ~{pitch_mod['depth_cents']:.1f} cents depth.")
    elif pitch_mod.get("detected") and pitch_mod.get("type") == "drift":
        notes.append(f"pitch drifts {pitch_mod['drift_semitones']:+.2f} semitones over the note - "
                      "detune settling or a slow LFO likely.")
    if unison.get("likely_cause") == "possible_unison_beating":
        notes.append(f"amplitude beats at {amplitude_mod['rate_hz']:.2f} Hz with no matching vibrato - "
                      f"possible detuned-unison voices (~{unison['estimated_detune_cents']:.1f} cents apart).")
    elif unison.get("likely_cause") == "tremolo_lfo":
        notes.append(f"amplitude modulation at {amplitude_mod['rate_hz']:.2f} Hz, ~{amplitude_mod['depth_db']:.1f} dB "
                      "depth - tremolo LFO likely.")
    elif unison.get("likely_cause") == "lfo_linked_vibrato_and_tremolo":
        notes.append("amplitude and pitch modulation share a similar rate - one LFO likely drives both amp and pitch.")
    if filter_sweep.get("direction") in ("closing", "opening"):
        verb = "falls" if filter_sweep["direction"] == "closing" else "rises"
        notes.append(f"spectral centroid {verb} from {filter_sweep['centroid_start_hz']:.0f} Hz to "
                      f"{filter_sweep['centroid_end_hz']:.0f} Hz - {filter_sweep['direction']} filter envelope (VCF) likely.")
    elif filter_sweep.get("direction") == "oscillating":
        notes.append(f"spectral centroid oscillates at ~{filter_sweep['oscillation_rate_hz']:.2f} Hz - filter LFO likely.")
    if stereo_phase and stereo_phase["phase_cancellation_likely"]:
        band = stereo_phase["flagged_band"]
        high = band["high_hz"] if band["high_hz"] is not None else "Nyquist"
        notes.append(f"low inter-channel correlation ({band['correlation']:.2f}) between {band['low_hz']}-{high} Hz - "
                      "phase cancellation / chorus-style stereo widening likely.")
    return notes


# ---------------------------------------------------------------------------
# report assembly
# ---------------------------------------------------------------------------

def build_report(path, mono, stereo, sr):
    duration_sec = len(mono) / sr
    times, db, _rms = compute_envelope(mono, sr)

    onset_times = librosa.onset.onset_detect(y=mono, sr=sr, units="time")
    onset_count = len(onset_times)

    provisional_noise_floor, _ = estimate_noise_floor(mono, sr, int(0.05 * sr))
    segments = detect_segments(times, db, provisional_noise_floor)
    noise_floor_db, noise_floor_method = estimate_noise_floor(mono, sr, int(times[segments.attack_start_idx] * sr))

    level_stats = compute_level_stats(mono, times, sr, segments, noise_floor_db, noise_floor_method)

    attack_slice = slice(segments.attack_start_idx, segments.peak_idx + 1)
    attack_shape = classify_shape(times[attack_slice], _rms[attack_slice])
    decay_shape = (
        classify_shape(times[segments.peak_idx : segments.decay_end_idx + 1], _rms[segments.peak_idx : segments.decay_end_idx + 1])
        if segments.decay_end_idx > segments.peak_idx
        else None
    )
    release_shape = None
    release_time_to_60db_sec = None
    if segments.release_start_idx is not None:
        r0, r1 = segments.release_start_idx, segments.release_end_idx + 1
        release_shape = classify_shape(times[r0:r1], _rms[r0:r1])
        if release_shape:
            target_db = db[segments.release_start_idx] - 60.0
            below = np.where(db[r0:r1] <= target_db)[0]
            if len(below):
                release_time_to_60db_sec = float(times[r0 + below[0]] - times[segments.release_start_idx])

    sustain_level_db = None
    if segments.release_start_idx is not None and segments.release_start_idx > segments.decay_end_idx:
        sustain_level_db = float(np.median(db[segments.decay_end_idx : segments.release_start_idx]))

    envelope_report = {
        "peak_time_sec": float(times[segments.peak_idx]),
        "attack": {
            "start_sec": float(times[segments.attack_start_idx]),
            "end_sec": float(times[segments.peak_idx]),
            "duration_sec": float(times[segments.peak_idx] - times[segments.attack_start_idx]),
            "shape": attack_shape,
        },
        "decay": {
            "end_sec": float(times[segments.decay_end_idx]),
            "duration_sec": float(times[segments.decay_end_idx] - times[segments.peak_idx]),
            "shape": decay_shape,
        }
        if segments.decay_end_idx > segments.peak_idx
        else None,
        "sustain": {
            "start_sec": float(times[segments.decay_end_idx]),
            "end_sec": float(times[segments.release_start_idx]) if segments.release_start_idx is not None else None,
            "level_db_relative_to_peak": safe_float(sustain_level_db - db[segments.peak_idx]) if sustain_level_db is not None else None,
        }
        if segments.decay_end_idx != segments.peak_idx or segments.release_start_idx != segments.peak_idx
        else None,
        "release": {
            "start_sec": float(times[segments.release_start_idx]),
            "end_sec": float(times[segments.release_end_idx]),
            "duration_sec": float(times[segments.release_end_idx] - times[segments.release_start_idx]),
            "time_to_minus_60db_sec": release_time_to_60db_sec,
            "shape": release_shape,
        }
        if segments.release_start_idx is not None
        else None,
        "onset_count": onset_count,
    }

    f0, f0_times, voiced_flag = track_pitch(mono, sr)
    window_start, window_end, window_source = pick_harmonic_window(times, segments, f0_times, voiced_flag)
    window_mask = (f0_times >= window_start) & (f0_times <= window_end) & voiced_flag
    fundamental_hz = float(np.median(f0[window_mask])) if np.any(window_mask) else None
    if fundamental_hz is None and np.any(voiced_flag):
        fundamental_hz = float(np.median(f0[voiced_flag]))

    harmonics = None
    if fundamental_hz is not None:
        harmonics_result = analyze_harmonics(mono, sr, window_start, window_end, fundamental_hz)
        if harmonics_result is not None:
            harmonics, _freqs, _mag_db_rel = harmonics_result
            harmonics["analysis_window_sec"] = [float(window_start), float(window_end)]
            harmonics["analysis_window_source"] = window_source
            harmonics["nearest_note"] = hz_to_note_info(fundamental_hz)

    amplitude_mod = analyze_amplitude_modulation(times, db, segments)
    pitch_mod = analyze_pitch_modulation(f0, f0_times, voiced_flag)
    unison = analyze_unison_beating(amplitude_mod, pitch_mod, fundamental_hz)
    filter_sweep = analyze_filter_sweep(mono, sr, times, segments)
    stereo_phase = analyze_stereo_phase(stereo, sr)

    notes = build_notes(onset_count, harmonics, amplitude_mod, pitch_mod, unison, filter_sweep, stereo_phase)

    return {
        "file": Path(path).name,
        "sample_rate": sr,
        "channels": 2 if stereo is not None else 1,
        "duration_sec": duration_sec,
        "level": level_stats,
        "envelope": envelope_report,
        "harmonics": harmonics,
        "modulation": {
            "amplitude": amplitude_mod,
            "pitch": pitch_mod,
            "unison_beating": unison,
            "filter_sweep": filter_sweep,
            "stereo_phase": stereo_phase,
        },
        "notes": notes,
    }, (times, db, segments, f0, f0_times, fundamental_hz)


# ---------------------------------------------------------------------------
# plotting
# ---------------------------------------------------------------------------

MARKER_LABEL_Y_POSITIONS = [0.97, 0.80, 0.63, 0.46]


def _mark_segments(ax, times, segments: Segments):
    markers = [("attack start", segments.attack_start_idx), ("peak", segments.peak_idx), ("decay end", segments.decay_end_idx)]
    if segments.release_start_idx is not None:
        markers.append(("release start", segments.release_start_idx))
    for (label, idx), y in zip(markers, MARKER_LABEL_Y_POSITIONS):
        t = times[idx]
        ax.axvline(t, color="#888888", linestyle="--", linewidth=0.8)
        ax.annotate(label, (t, y), xycoords=("data", "axes fraction"), xytext=(3, 0), textcoords="offset points",
                    fontsize=7, ha="left", va="top", color="#666666")


def _plot_end_sec(duration_sec, times, segments: Segments):
    # Renders often carry a long silent/padded tail; cropping to the sounding region plus
    # a margin keeps the interesting part of the plot from being squeezed into a sliver.
    content_end = times[segments.release_end_idx]
    return min(duration_sec, max(content_end * 1.15, content_end + 0.5))


def _plot_waveform_envelope(ax, mono, sr, times, db, segments, plot_end_sec):
    t = np.arange(len(mono)) / sr
    ax.plot(t, mono, color="#a8c4e6", linewidth=0.4, label="waveform")
    ax.set_xlabel("time (s)")
    ax.set_ylabel("amplitude")
    ax.set_xlim(0, plot_end_sec)
    ax_db = ax.twinx()
    ax_db.plot(times, db, color="#d94f4f", linewidth=1.3, label="envelope (dB)")
    ax_db.set_ylabel("level (dB)")
    _mark_segments(ax, times, segments)
    ax.set_title("Waveform + envelope")


SPECTROGRAM_MIN_HZ = 20.0


def _plot_spectrogram(fig, ax, mono, sr, times, segments, f0, f0_times, plot_end_sec):
    stft = librosa.stft(mono, n_fft=FRAME_LENGTH, hop_length=HOP_LENGTH)
    # librosa.stft's default window is a periodic Hann; normalizing by its coherent gain
    # (sum(window)/2) makes 0dB correspond to a full-scale sinusoid, matching the dBFS
    # scale used everywhere else in this script - raw FFT magnitude does not.
    coherent_gain = float(np.sum(hann(FRAME_LENGTH, sym=False)) / 2.0)
    spec_db = to_db(np.abs(stft) / coherent_gain)
    freqs = librosa.fft_frequencies(sr=sr, n_fft=FRAME_LENGTH)
    frame_times = librosa.frames_to_time(np.arange(spec_db.shape[1]), sr=sr, hop_length=HOP_LENGTH)
    image = ax.pcolormesh(frame_times, freqs, spec_db, cmap="magma", vmin=-100.0, vmax=0.0, shading="auto")
    ax.set_yscale("log")
    ax.set_ylabel("frequency (Hz)")
    ax.set_xlabel("time (s)")
    ax.set_xlim(0, plot_end_sec)
    ax.set_ylim(SPECTROGRAM_MIN_HZ, sr / 2.0)
    valid = ~np.isnan(f0)
    if np.any(valid):
        ax.plot(f0_times[valid], f0[valid], color="#00ffcc", linewidth=1.0, label="f0")
    _mark_segments(ax, times, segments)
    ax.set_title("Spectrogram (STFT, dB)")
    fig.colorbar(image, ax=ax, label="magnitude (dB)")


def _plot_fft_slices(ax, mono, sr, times, segments, harmonics):
    def window_samples(a_sec, b_sec, max_len_sec=0.3):
        a, b = int(a_sec * sr), int(b_sec * sr)
        b = min(b, a + int(max_len_sec * sr), len(mono))
        return mono[a:b]

    release_end = times[segments.release_end_idx]
    slices = {
        "attack": window_samples(times[segments.attack_start_idx], times[segments.peak_idx]),
        "sustain": window_samples(
            times[segments.decay_end_idx],
            times[segments.release_start_idx] if segments.release_start_idx is not None else release_end,
        ),
        "release": window_samples(
            times[segments.release_start_idx] if segments.release_start_idx is not None else times[segments.decay_end_idx],
            release_end,
        ),
    }
    colors = {"attack": "#4f8fd9", "sustain": "#d9a94f", "release": "#d94f4f"}
    for label, seg in slices.items():
        if len(seg) < 64:
            continue
        mag_db = to_db(np.abs(np.fft.rfft(seg * np.hanning(len(seg)))))
        freqs = np.fft.rfftfreq(len(seg), d=1.0 / sr)
        ax.plot(freqs, mag_db, label=label, color=colors[label], linewidth=1.0)

    if harmonics:
        for p in harmonics["partials"][:8]:
            ax.axvline(p["ideal_freq_hz"], color="#999999", linestyle=":", linewidth=0.6)
            ax.text(p["ideal_freq_hz"], ax.get_ylim()[1], str(p["n"]), fontsize=6, ha="center", va="top", color="#666666")

    ax.set_xscale("log")
    ax.set_xlim(SPECTROGRAM_MIN_HZ, min(sr / 2.0, 8000.0 if not harmonics else max(8000.0, harmonics["partials"][-1]["ideal_freq_hz"] * 1.2)))
    ax.set_xlabel("frequency (Hz)")
    ax.set_ylabel("magnitude (dB, relative)")
    ax.set_title("FFT slices (attack / sustain / release)")
    ax.legend(loc="upper right", fontsize=8)


def render_plot(out_path, mono, sr, analysis_state, harmonics):
    times, db, segments, f0, f0_times, _fundamental_hz = analysis_state
    duration_sec = len(mono) / sr
    plot_end_sec = _plot_end_sec(duration_sec, times, segments)
    fig, axes = plt.subplots(3, 1, figsize=(12, 10.5))
    _plot_waveform_envelope(axes[0], mono, sr, times, db, segments, plot_end_sec)
    _plot_spectrogram(fig, axes[1], mono, sr, times, segments, f0, f0_times, plot_end_sec)
    _plot_fft_slices(axes[2], mono, sr, times, segments, harmonics)
    fig.tight_layout()
    fig.savefig(out_path, dpi=150)
    plt.close(fig)


# ---------------------------------------------------------------------------
# CLI
# ---------------------------------------------------------------------------

class NumpyJSONEncoder(json.JSONEncoder):
    def default(self, obj):
        if isinstance(obj, np.floating):
            return float(obj)
        if isinstance(obj, np.integer):
            return int(obj)
        if isinstance(obj, np.ndarray):
            return obj.tolist()
        if isinstance(obj, np.bool_):
            return bool(obj)
        return super().default(obj)


def analyze_file(path, output_dir):
    mono, stereo, sr = load_audio(path)
    report, analysis_state = build_report(path, mono, stereo, sr)

    output_dir.mkdir(parents=True, exist_ok=True)
    stem = Path(path).stem
    json_path = output_dir / f"{stem}.analysis.json"
    png_path = output_dir / f"{stem}.analysis.png"

    with open(json_path, "w") as f:
        json.dump(report, f, indent=2, cls=NumpyJSONEncoder, allow_nan=False)
    render_plot(png_path, mono, sr, analysis_state, report["harmonics"])

    print(f"Report saved to: {json_path}")
    print(f"Plot saved to: {png_path}")


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("wav_file", type=Path)
    parser.add_argument("-o", "--output-dir", type=Path, default=None)
    args = parser.parse_args()

    output_dir = args.output_dir or (Path(__file__).parent / "generated")
    analyze_file(args.wav_file, output_dir)


if __name__ == "__main__":
    main()
