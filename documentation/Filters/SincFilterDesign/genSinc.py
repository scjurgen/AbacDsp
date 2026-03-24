#!/usr/bin/env python3

import numpy as np
from numpy import kaiser


def _generate_filter(cycles: float, fudge_factor: float, increment: int, atten: float) -> np.ndarray:
    N = round(4 * cycles * fudge_factor * increment)
    if N % 2 != 0:
        N -= 1
    m = np.arange(-(N - 1) / 2, (N - 1) / 2 + 1)
    f = np.sinc(m / fudge_factor / increment)
    w = kaiser(N, (atten + 0.5) / 10)
    f = f * w
    return f / f.sum()


def _measure_filter(f: np.ndarray, atten: float) -> tuple[float, float, float]:
    spec_len = 400_000
    padded = np.zeros(spec_len)
    padded[:len(f)] = f
    spec = 20 * np.log10(np.abs(np.fft.fft(padded))[:spec_len // 2])

    first_null = 0
    for k in range(1, len(spec) - 1):
        if spec[k] < -0.8 * atten and spec[k - 1] > spec[k] and spec[k] < spec[k + 1]:
            first_null = k
            break

    stop_atten = spec[first_null:].max()

    atten_start = 0
    for k in range(first_null):
        if spec[k] > stop_atten and spec[k + 1] < stop_atten:
            atten_start = k
            break

    stop_band_start = (atten_start + (stop_atten - spec[atten_start]) /
                       (spec[atten_start + 1] - spec[atten_start])) / spec_len

    minus_3db = 0
    for k in range(first_null):
        if spec[k] > -3.0 and spec[k + 1] < -3.0:
            minus_3db = k
            break

    minus_3db = (minus_3db + (stop_atten - spec[minus_3db]) /
                 (spec[minus_3db + 1] - spec[minus_3db])) / spec_len

    return stop_atten, stop_band_start, minus_3db


def _format_coeffs(values: np.ndarray) -> list[str]:
    lines, row = [], []
    count = len(values)
    for i, v in enumerate(values):
        row.append(f"{v:9.6e}f,")
        if len(row) == 8 or i == count - 1:
            lines.append("    " + " ".join(row))
            row = []
    return lines


def make_filter(cycles: float, increment: int, atten: float) -> np.ndarray:
    filename = f"Sinc{int(cycles)}.h"

    ff1, ff2 = 1.0, 1.25
    f1 = _generate_filter(cycles, ff1, increment, atten)
    _, sbs1, _ = _measure_filter(f1, atten)
    f2 = _generate_filter(cycles, ff2, increment, atten)
    _, sbs2, _ = _measure_filter(f2, atten)

    f, fudge_factor = f1, ff1

    while abs(sbs1 - sbs2) > 1e-10:
        if sbs1 < sbs2:
            print("stop_band_start1 < stop_band_start2")
            break

        fudge_factor = ff1 + (ff2 - ff1) / 2
        f = _generate_filter(cycles, fudge_factor, increment, atten)
        stop_atten, stop_band_start, minus_3db = _measure_filter(f, atten)

        if stop_band_start > 1.0:
            print(f"A {ff1:.8f} {fudge_factor:.8f} {ff2:.8f}")
            continue

        if stop_band_start < 0.5 / increment:
            f2, sbs2, ff2 = f, stop_band_start, fudge_factor
        else:
            f1, sbs1, ff1 = f, stop_band_start, fudge_factor

    print()

    N = len(f)
    stop_atten, stop_band_start, minus_3db = _measure_filter(f, atten)
    f = increment * f

    if N % 2 != 0:
        raise ValueError("Length of filter coefficients should be even.")

    peak_idx = np.where(f == f.max())[0]
    if len(peak_idx) == 2:
        new_f = f.copy()
        for i in range(N - 1):
            new_f[i] = f[i] + (f[i + 1] - f[i]) / 2
        f = new_f
        peak_idx = np.where(f == f.max())[0]

    idx = int(peak_idx.min())
    half_f = f[idx:]

    trailing_zeros = 4 - (len(half_f) % 4)

    print(f"# f = make_filter ({int(cycles)}, {increment}, {atten:4.1f}) ;")
    print(f"# Coeff. count     : {N}")
    print(f"# Fudge factor     : {fudge_factor:9.7f}")
    print(f"# Pass band width  : {stop_band_start:12.10f} (should be {0.5 / increment:12.10f})")
    print(f"# Stop band atten. : {abs(stop_atten):5.2f} dB")
    print(f"# -3dB band Width  : {0.5 / increment / minus_3db:5.3f}")
    print(f"# Half length      : {len(half_f) + trailing_zeros}")
    print(f"# Increment        : {increment}")

    lines = _format_coeffs(half_f)
    if trailing_zeros > 0:
        lines += _format_coeffs(np.zeros(trailing_zeros))
    fir_coeffs = "\n".join(lines) + "\n"

    comment_section = (
        f" *  f = make_filter ({int(cycles)}, {increment}, {atten:4.1f}, \"{filename}\") ;\n"
        f" *  N                 : {N}\n"
        f" *  Pass band width   : {stop_band_start:9.7f} (should be {0.5 / increment:9.7f})\n"
        f" *  Stop band atten.  : {abs(stop_atten):5.2f} dB\n"
        f" *  -3dB band width   : {0.5 / increment / minus_3db:5.3f}\n"
        f" *  Half length       : {len(half_f)}\n"
        f" *  Increment         : {increment}\n"
        f" *  FIR multiplies    : {round(N / increment) - 1}\n"
    )

    cycles_str = str(int(cycles))
    output = f"""\
#pragma once

#include "SincFilter.h"

/*
{comment_section}*/

const AbacDsp::SincFilter::InitParam sinc{cycles_str} = // clang-format off
{{
{increment},
{{
{fir_coeffs}}}
}};
"""

    with open(filename, "w") as out:
        out.write(output)

    print(f"File generated: {filename}\n")
    return half_f


if __name__ == "__main__":
    import argparse

    parser = argparse.ArgumentParser(description="Generate windowed-sinc FIR filter coefficients.")
    parser.add_argument("cycles",      type=float, help="Number of sinc lobes")
    parser.add_argument("increment",   type=int,   help="Interpolation/decimation factor")
    parser.add_argument("attenuation", type=float, help="Stop-band attenuation in dB")
    args = parser.parse_args()

    make_filter(args.cycles, args.increment, args.attenuation)
