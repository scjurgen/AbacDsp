#!/usr/bin/env python3
"""Generates the reggae kit's "clicklow"/"clickhigh" round-robin samples.

Each take is a damped sine rendered by hand-porting AbacDsp::SvfResoBP's
free-running step0() recurrence (src/includes/Filters/SvfResoBP.h) at the
same frequency/decay/gain AbacDsp::ClickGenerator uses for its downbeat (low,
400 Hz) and beat (high, 800 Hz) voices (src/includes/Generators/
ClickGenerator.h) - so the sample-based click matches the plugin's own
synthesized one. Round robins vary the pitch by a few cents and the decay
time by a few percent, the way a struck instrument's takes never ring
identically twice.

Output goes to samples/drums/reggae/, next to the kit's other <code>_<n>.wav
round-robin files, as plain 48 kHz stereo IEEE-float WAV (no numpy/soundfile
dependency - written by hand with struct, matching that folder's own format).
"""

from __future__ import annotations

import math
import os
import struct
from dataclasses import dataclass

SAMPLE_RATE = 48000
CHANNELS = 2

# AbacDsp::SvfResoBP::setByDecay()'s Q-from-decay-time constant.
DECAY_CONST = 0.1447648273
# AbacDsp::ClickGenerator: kBoostDb=24, base level -6 dB, both accents share this gain.
BASE_GAIN = 10.0 ** ((-6.0 + 24.0) / 20.0)
BASE_DECAY_SECONDS = 0.04
TAIL_SECONDS = 0.25
FADE_OUT_SECONDS = 0.002
PEAK_TARGET = 0.7

ROUND_ROBIN_CENTS = (-4.0, -1.0, 2.0, 5.0)
ROUND_ROBIN_DECAY_SCALE = (0.92, 0.98, 1.05, 1.12)


@dataclass
class ClickVoice:
    name: str
    frequency_hz: float


VOICES = (
    ClickVoice("clicklow", 400.0),
    ClickVoice("clickhigh", 800.0),
)


def render_click(frequency_hz: float, decay_seconds: float, gain: float) -> list[float]:
    g = math.tan(math.pi * frequency_hz / SAMPLE_RATE)
    q = math.pi * frequency_hz * decay_seconds * DECAY_CONST
    k = 1.0 / max(q, 0.01)
    denom = 1.0 / (1.0 + g * (g + k))
    a1, a2, a3 = denom, g * denom, g * (g * denom)

    z0, z1 = 0.0, gain
    num_samples = int(SAMPLE_RATE * TAIL_SECONDS)
    samples = [0.0] * num_samples
    for n in range(num_samples):
        v3 = -z1
        v1 = a1 * z0 + a2 * v3
        v2 = z1 + a2 * z0 + a3 * v3
        z0 = max(-1000.0, min(1000.0, 2.0 * v1 - z0))
        z1 = max(-1000.0, min(1000.0, 2.0 * v2 - z1))
        samples[n] = k * v1
    return samples


def normalize_and_fade(samples: list[float]) -> list[float]:
    peak = max(abs(s) for s in samples) or 1.0
    scale = PEAK_TARGET / peak
    fade_len = int(SAMPLE_RATE * FADE_OUT_SECONDS)
    out = [s * scale for s in samples]
    for i in range(fade_len):
        out[-(i + 1)] *= i / fade_len
    return out


def write_stereo_float_wav(path: str, samples: list[float]) -> None:
    frame_count = len(samples)
    interleaved = struct.pack(f"<{frame_count * CHANNELS}f", *[s for s in samples for _ in range(CHANNELS)])
    byte_rate = SAMPLE_RATE * CHANNELS * 4
    block_align = CHANNELS * 4
    fmt_chunk = struct.pack("<HHIIHH", 3, CHANNELS, SAMPLE_RATE, byte_rate, block_align, 32)
    data_size = len(interleaved)
    riff_size = 4 + (8 + len(fmt_chunk)) + (8 + data_size)
    with open(path, "wb") as wav_file:
        wav_file.write(b"RIFF")
        wav_file.write(struct.pack("<I", riff_size))
        wav_file.write(b"WAVE")
        wav_file.write(b"fmt ")
        wav_file.write(struct.pack("<I", len(fmt_chunk)))
        wav_file.write(fmt_chunk)
        wav_file.write(b"data")
        wav_file.write(struct.pack("<I", data_size))
        wav_file.write(interleaved)


def main() -> None:
    script_dir = os.path.dirname(os.path.abspath(__file__))
    root_dir = os.path.abspath(os.path.join(script_dir, "..", ".."))
    out_dir = os.path.join(root_dir, "samples", "drums", "reggae")
    os.makedirs(out_dir, exist_ok=True)

    for voice in VOICES:
        for take in range(4):
            cents = ROUND_ROBIN_CENTS[take]
            frequency_hz = voice.frequency_hz * (2.0 ** (cents / 1200.0))
            decay_seconds = BASE_DECAY_SECONDS * ROUND_ROBIN_DECAY_SCALE[take]
            samples = normalize_and_fade(render_click(frequency_hz, decay_seconds, BASE_GAIN))
            out_path = os.path.join(out_dir, f"{voice.name}_{take + 1}.wav")
            write_stereo_float_wav(out_path, samples)
            print(f"wrote {out_path}")


if __name__ == "__main__":
    main()
