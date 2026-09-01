#!/usr/bin/env python3
"""Renders one or more FFT-magnitude grids (dB) as stacked spectrogram heatmaps.

Sibling to Plot/PyConPlot.py for the one shape of data that script doesn't handle: a 2D
time/frequency grid rather than named x/y line series. Input format, one file per panel:
a "rows cols sampleRate fftLength hop" header line, then `rows` lines of `cols`
space-separated dB values (row = time frame, column = frequency bin).
"""
import argparse

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np


def load_grid(path):
    with open(path) as f:
        rows, cols, sample_rate, fft_length, hop = f.readline().split()
        rows, cols, fft_length, hop = int(rows), int(cols), int(fft_length), int(hop)
        sample_rate = float(sample_rate)
        data = np.loadtxt(f, max_rows=rows) if rows > 0 else np.zeros((0, cols))
    return data.reshape(rows, cols), sample_rate, fft_length, hop


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--panel", action="append", nargs=2, metavar=("FILE", "TITLE"), required=True,
                         help="one grid file and its subplot title; repeat for multiple stacked panels")
    parser.add_argument("-o", "--outfile", required=True)
    parser.add_argument("--width", type=float, default=12.0, help="figure width in inches")
    parser.add_argument("--height", type=float, default=3.5, help="height per panel in inches")
    parser.add_argument("--mindb", type=float, default=-140.0)
    parser.add_argument("--maxdb", type=float, default=-10.0)
    parser.add_argument("--maxfreq", type=float, default=None,
                         help="crop the frequency axis to [0, maxfreq] Hz; default shows up to Nyquist")
    args = parser.parse_args()

    fig, axes = plt.subplots(len(args.panel), 1, figsize=(args.width, args.height * len(args.panel)), squeeze=False)
    for (path, title), ax in zip(args.panel, axes[:, 0]):
        grid, sample_rate, _fft_length, hop = load_grid(path)
        extent = [0.0, grid.shape[0] * hop / sample_rate, 0.0, sample_rate / 2.0]
        image = ax.imshow(grid.T, origin="lower", aspect="auto", extent=extent, cmap="magma",
                           vmin=args.mindb, vmax=args.maxdb)
        ax.set_title(title)
        ax.set_xlabel("time (s)")
        ax.set_ylabel("frequency (Hz)")
        if args.maxfreq is not None:
            ax.set_ylim(0.0, args.maxfreq)
        fig.colorbar(image, ax=ax, label="magnitude (dB)")

    fig.tight_layout()
    fig.savefig(args.outfile, dpi=150)
    print(f"Plot saved to: {args.outfile}")


if __name__ == "__main__":
    main()
