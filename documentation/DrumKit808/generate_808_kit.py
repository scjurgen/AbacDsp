#!/usr/bin/env python3
"""Synthesizes a full TR-808-style drum kit, purely algorithmically.

Every piece is generated from oscillators and filtered noise - no recorded or
sampled audio goes in, so unlike samples/drums/reggae/ (a real, user-supplied
pack), this kit has nothing to license. See samples/README.md for the naming
convention this follows and for how to add a real pack alongside it.

Output uses the same <code>_<n>.wav round-robin scheme and code vocabulary as
the shipped reggae kit (see GrooveKit::classifyReggaeKit() in
src/includes/Sampler/GrooveKit.h, and GrooveNoteMap.h for the tag each code
resolves to), so it drops into ABACDSP_DRUM_SAMPLES_DIR with no C++ changes.

The hihat/cymbal/ride "metallic" voices sum six square-ish partials at the
approximate fixed frequencies of the real TR-808's hihat oscillator bank
(205.3, 304.4, 369.6, 522.7, 540.0, 800.0 Hz), then highpass and decay them -
the same technique widely documented for 808 clones/emulations.

Output is plain 48 kHz stereo IEEE-float WAV, written by hand with struct (no
numpy/soundfile dependency), matching the reggae kit's own file format.
"""

from __future__ import annotations

import math
import os
import random
import struct
from typing import Callable

SAMPLE_RATE = 48000
CHANNELS = 2
ROUND_ROBIN_COUNT = 4
FADE_OUT_SECONDS = 0.003

# decay_seconds throughout means "time to fall to -60 dB", not a raw time constant.
LN1000 = math.log(1000.0)

HIHAT_PARTIALS_HZ = (205.3, 304.4, 369.6, 522.7, 540.0, 800.0)
CYMBAL_PARTIALS_HZ = HIHAT_PARTIALS_HZ + (1050.0, 1400.0)
RIDE_PARTIALS_HZ = (369.6, 522.7, 540.0)

RenderFn = Callable[[random.Random], list[float]]


# --- low-level DSP helpers --------------------------------------------------


def cents_to_ratio(cents: float) -> float:
    return 2.0 ** (cents / 1200.0)


def decay_envelope(n: int, decay_seconds: float) -> list[float]:
    tau = max(decay_seconds, 0.001) / LN1000
    return [math.exp(-i / (tau * SAMPLE_RATE)) for i in range(n)]


def one_pole_lowpass(samples: list[float], cutoff_hz: float) -> list[float]:
    a = math.exp(-2.0 * math.pi * cutoff_hz / SAMPLE_RATE)
    y = 0.0
    out = [0.0] * len(samples)
    for i, x in enumerate(samples):
        y = (1.0 - a) * x + a * y
        out[i] = y
    return out


def one_pole_highpass(samples: list[float], cutoff_hz: float) -> list[float]:
    low = one_pole_lowpass(samples, cutoff_hz)
    return [x - low_x for x, low_x in zip(samples, low)]


def noise_click(n: int, hp_hz: float, rng: random.Random) -> list[float]:
    return one_pole_highpass([rng.uniform(-1.0, 1.0) for _ in range(n)], hp_hz)


def pitched_sine(n: int, start_freq: float, end_freq: float, pitch_decay_s: float) -> list[float]:
    tau = max(pitch_decay_s, 0.001) / LN1000
    phase = 0.0
    out = [0.0] * n
    for i in range(n):
        freq = end_freq + (start_freq - end_freq) * math.exp(-i / (tau * SAMPLE_RATE))
        phase += 2.0 * math.pi * freq / SAMPLE_RATE
        out[i] = math.sin(phase)
    return out


# --- voice factories ---------------------------------------------------------
# Each factory returns a render(rng) closure; the round-robin loop in main()
# seeds rng per (code, take), so every take is deterministic but distinct.


def pitched_voice(start_freq: float, end_freq: float, pitch_decay_s: float, amp_decay_s: float,
                   click_mix: float = 0.0, saturate: float = 0.0) -> RenderFn:
    def render(rng: random.Random) -> list[float]:
        ratio = cents_to_ratio(rng.uniform(-6.0, 6.0))
        amp_decay = amp_decay_s * rng.uniform(0.90, 1.15)
        duration = max(pitch_decay_s, amp_decay) * 1.4 + 0.02
        n = int(SAMPLE_RATE * duration)
        tone = pitched_sine(n, start_freq * ratio, end_freq * ratio, pitch_decay_s)
        env = decay_envelope(n, amp_decay)
        out = [tone[i] * env[i] for i in range(n)]
        if click_mix > 0.0:
            click_n = min(n, int(SAMPLE_RATE * 0.006))
            click = noise_click(click_n, 1500.0, rng)
            click_env = decay_envelope(click_n, 0.006)
            for i in range(click_n):
                out[i] += click_mix * click[i] * click_env[i]
        if saturate > 0.0:
            out = [math.tanh(s * (1.0 + saturate)) for s in out]
        return out

    return render


def metallic_voice(partials_hz: tuple[float, ...], hp_hz: float, decay_s: float, shimmer: float = 0.0,
                    ping_freq: float = 0.0, detune_spread: float = 0.0) -> RenderFn:
    def render(rng: random.Random) -> list[float]:
        decay = decay_s * rng.uniform(0.90, 1.15)
        n = int(SAMPLE_RATE * (decay * 1.3 + 0.02))
        ratio = cents_to_ratio(rng.uniform(-8.0, 8.0))
        amp = 1.0 / len(partials_hz)
        out = [0.0] * n
        for base_freq in partials_hz:
            spread = 1.0 + rng.uniform(-detune_spread, detune_spread) if detune_spread > 0.0 else 1.0
            omega = 2.0 * math.pi * base_freq * ratio * spread / SAMPLE_RATE
            phase = rng.uniform(0.0, 2.0 * math.pi)
            for i in range(n):
                out[i] += amp * math.sin(omega * i + phase)
        out = one_pole_highpass(out, hp_hz)
        env = decay_envelope(n, decay)
        out = [out[i] * env[i] for i in range(n)]
        if shimmer > 0.0:
            noise = noise_click(n, hp_hz * 0.8, rng)
            out = [out[i] + shimmer * noise[i] * env[i] for i in range(n)]
        if ping_freq > 0.0:
            ping_n = min(n, int(SAMPLE_RATE * 0.05))
            ping_env = decay_envelope(ping_n, 0.02)
            for i in range(ping_n):
                out[i] += 0.55 * math.sin(2.0 * math.pi * ping_freq * i / SAMPLE_RATE) * ping_env[i]
        return out

    return render


def snare_voice(f1: float, f2: float, tone_decay_s: float, noise_decay_s: float, noise_mix: float,
                 hp_hz: float = 900.0, lp_hz: float = 6500.0) -> RenderFn:
    def render(rng: random.Random) -> list[float]:
        ratio = cents_to_ratio(rng.uniform(-4.0, 4.0))
        tone_decay = tone_decay_s * rng.uniform(0.9, 1.1)
        noise_decay = noise_decay_s * rng.uniform(0.9, 1.15)
        n = int(SAMPLE_RATE * (max(tone_decay, noise_decay) * 1.4 + 0.02))
        tone_env = decay_envelope(n, tone_decay)
        w1 = 2.0 * math.pi * f1 * ratio / SAMPLE_RATE
        w2 = 2.0 * math.pi * f2 * ratio / SAMPLE_RATE
        tone = [(0.6 * math.sin(w1 * i) + 0.4 * math.sin(w2 * i)) * tone_env[i] for i in range(n)]
        noise = one_pole_lowpass(noise_click(n, hp_hz, rng), lp_hz)
        noise_env = decay_envelope(n, noise_decay)
        return [(1.0 - noise_mix) * tone[i] + noise_mix * noise[i] * noise_env[i] for i in range(n)]

    return render


def rim_voice(tone_freq: float, tone_decay_s: float, hp_hz: float, noise_decay_s: float) -> RenderFn:
    def render(rng: random.Random) -> list[float]:
        ratio = cents_to_ratio(rng.uniform(-5.0, 5.0))
        tone_decay = tone_decay_s * rng.uniform(0.9, 1.15)
        noise_decay = noise_decay_s * rng.uniform(0.9, 1.15)
        n = int(SAMPLE_RATE * (max(tone_decay, noise_decay) * 1.6 + 0.01))
        omega = 2.0 * math.pi * tone_freq * ratio / SAMPLE_RATE
        tone_env = decay_envelope(n, tone_decay)
        tone = [math.sin(omega * i) * tone_env[i] for i in range(n)]
        noise = noise_click(n, hp_hz, rng)
        noise_env = decay_envelope(n, noise_decay)
        out = [0.5 * tone[i] + 0.7 * noise[i] * noise_env[i] for i in range(n)]
        return [math.tanh(s * 1.3) for s in out]

    return render


def clap_voice() -> RenderFn:
    def render(rng: random.Random) -> list[float]:
        n = int(SAMPLE_RATE * 0.22)
        out = [0.0] * n
        for offset in (0.0, 0.010, 0.020):
            start = max(0, int(SAMPLE_RATE * (offset + rng.uniform(-0.002, 0.002))))
            burst_n = min(n - start, int(SAMPLE_RATE * 0.02))
            if burst_n <= 0:
                continue
            noise = one_pole_lowpass(noise_click(burst_n, 1000.0, rng), 7000.0)
            burst_env = decay_envelope(burst_n, 0.015)
            for i in range(burst_n):
                out[start + i] += 0.8 * noise[i] * burst_env[i]
        tail_start = int(SAMPLE_RATE * 0.028)
        tail_n = n - tail_start
        noise = one_pole_lowpass(noise_click(tail_n, 1200.0, rng), 6500.0)
        tail_env = decay_envelope(tail_n, 0.11 * rng.uniform(0.9, 1.15))
        for i in range(tail_n):
            out[tail_start + i] += 0.6 * noise[i] * tail_env[i]
        return out

    return render


def shaker_voice(hp_hz: float, decay_s: float, shimmer: float = 0.0) -> RenderFn:
    def render(rng: random.Random) -> list[float]:
        decay = decay_s * rng.uniform(0.9, 1.15)
        n = int(SAMPLE_RATE * (decay * 1.4 + 0.01))
        env = decay_envelope(n, decay)
        out = [noise_click(n, hp_hz, rng)[i] * env[i] for i in range(n)]
        if shimmer > 0.0:
            metallic = metallic_voice(HIHAT_PARTIALS_HZ, hp_hz, decay)(rng)
            for i in range(min(n, len(metallic))):
                out[i] += shimmer * metallic[i]
        return out

    return render


# --- the kit: (code, render, peak_target) -----------------------------------
# peak_target is each piece's own final peak level (0..1), authored relative
# to the others (ghost hits quiet, kick/snare hot) rather than a single
# constant - GrooveDrumPlayer trusts recorded/rendered levels as-is.

VOICES: list[tuple[str, RenderFn, float]] = [
    ("bd", pitched_voice(170.0, 58.0, 0.05, 0.45, click_mix=0.25, saturate=0.4), 0.95),
    ("rs", rim_voice(900.0, 0.03, 2200.0, 0.02), 0.65),
    ("sstick", rim_voice(750.0, 0.025, 1800.0, 0.018), 0.55),
    ("sd", snare_voice(185.0, 330.0, 0.10, 0.16, 0.55), 0.85),
    ("sd2", snare_voice(205.0, 355.0, 0.09, 0.15, 0.58), 0.85),
    ("sdroll", snare_voice(185.0, 330.0, 0.12, 0.20, 0.50), 0.85),
    ("tomlo", pitched_voice(95.0, 80.0, 0.06, 0.32), 0.8),
    ("tom1", pitched_voice(118.0, 100.0, 0.05, 0.28), 0.8),
    ("tom2", pitched_voice(150.0, 128.0, 0.05, 0.26), 0.8),
    ("tom3", pitched_voice(190.0, 162.0, 0.045, 0.24), 0.8),
    ("tomlt", pitched_voice(230.0, 195.0, 0.04, 0.22), 0.8),
    ("timb1", pitched_voice(310.0, 296.0, 0.02, 0.30, click_mix=0.08), 0.7),
    ("timb2", pitched_voice(360.0, 345.0, 0.02, 0.28, click_mix=0.08), 0.7),
    ("timb3", pitched_voice(410.0, 394.0, 0.02, 0.26, click_mix=0.08), 0.7),
    ("timb4", pitched_voice(470.0, 452.0, 0.02, 0.24, click_mix=0.08), 0.7),
    ("timbdmp", pitched_voice(380.0, 368.0, 0.015, 0.10, click_mix=0.10), 0.65),
    ("hh", metallic_voice(HIHAT_PARTIALS_HZ, 7000.0, 0.09), 0.55),
    ("hhstep", metallic_voice(HIHAT_PARTIALS_HZ, 7500.0, 0.05), 0.6),
    ("hhstep2", metallic_voice(HIHAT_PARTIALS_HZ, 7300.0, 0.045), 0.6),
    ("hhstep3", metallic_voice(HIHAT_PARTIALS_HZ, 7600.0, 0.04), 0.62),
    ("hhhalf", metallic_voice(HIHAT_PARTIALS_HZ, 6500.0, 0.18), 0.55),
    ("hhstop", metallic_voice(HIHAT_PARTIALS_HZ, 8000.0, 0.02), 0.5),
    ("hhsoft", metallic_voice(HIHAT_PARTIALS_HZ, 7000.0, 0.06), 0.3),
    ("hhghost", metallic_voice(HIHAT_PARTIALS_HZ, 7000.0, 0.03), 0.18),
    ("hhopen", metallic_voice(HIHAT_PARTIALS_HZ, 6000.0, 0.45), 0.55),
    ("crash", metallic_voice(CYMBAL_PARTIALS_HZ, 5000.0, 0.9, shimmer=0.4), 0.8),
    ("crstop", metallic_voice(CYMBAL_PARTIALS_HZ, 6000.0, 0.15, shimmer=0.3), 0.75),
    ("crstop2", metallic_voice(CYMBAL_PARTIALS_HZ, 6200.0, 0.13, shimmer=0.35), 0.75),
    ("crlong", metallic_voice(CYMBAL_PARTIALS_HZ, 4500.0, 1.3, shimmer=0.45), 0.8),
    ("crlong2", metallic_voice(CYMBAL_PARTIALS_HZ, 4700.0, 1.4, shimmer=0.5), 0.8),
    ("china", metallic_voice(CYMBAL_PARTIALS_HZ, 5000.0, 0.8, shimmer=0.6, detune_spread=0.03), 0.75),
    ("chinasht", metallic_voice(CYMBAL_PARTIALS_HZ, 5500.0, 0.15, shimmer=0.55, detune_spread=0.03), 0.7),
    ("ride", metallic_voice(RIDE_PARTIALS_HZ, 4000.0, 0.55, shimmer=0.15, ping_freq=800.0), 0.6),
    ("ride2", metallic_voice(RIDE_PARTIALS_HZ, 4200.0, 0.5, shimmer=0.18, ping_freq=850.0), 0.6),
    ("ridebell", metallic_voice((1200.0,), 3000.0, 0.25, shimmer=0.05, ping_freq=1200.0), 0.55),
    ("wood", pitched_voice(1250.0, 1150.0, 0.008, 0.05, click_mix=0.25, saturate=0.5), 0.65),
    ("clap", clap_voice(), 0.75),
    ("shaker", shaker_voice(3200.0, 0.09), 0.5),
    ("tamb", shaker_voice(2600.0, 0.15, shimmer=0.25), 0.55),
    ("clicklow", pitched_voice(400.0, 400.0, 0.001, 0.05, click_mix=0.15), 0.6),
    ("clickhigh", pitched_voice(800.0, 800.0, 0.001, 0.035, click_mix=0.15), 0.6),
]


# --- rendering + file output -------------------------------------------------


def normalize_to_peak(samples: list[float], peak_target: float) -> list[float]:
    peak = max((abs(s) for s in samples), default=0.0) or 1.0
    scale = peak_target / peak
    return [s * scale for s in samples]


def apply_fadeout(samples: list[float]) -> list[float]:
    fade_len = min(len(samples), int(SAMPLE_RATE * FADE_OUT_SECONDS))
    out = list(samples)
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
    out_dir = os.path.join(root_dir, "samples", "drums", "808")
    os.makedirs(out_dir, exist_ok=True)

    for code, render, peak_target in VOICES:
        for take in range(ROUND_ROBIN_COUNT):
            rng = random.Random(f"{code}:{take}")
            samples = apply_fadeout(normalize_to_peak(render(rng), peak_target))
            out_path = os.path.join(out_dir, f"{code}_{take + 1}.wav")
            write_stereo_float_wav(out_path, samples)
            print(f"wrote {out_path}")


if __name__ == "__main__":
    main()
