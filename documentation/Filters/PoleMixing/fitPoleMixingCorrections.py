#!/usr/bin/env python3

# Fits two corrections from raw (uncorrected) pole-mixing filter measurements, following the
# same measure -> curve_fit -> emit-C++ pattern as
# documentation/Filters/BandpassImpulses/fitBandPassCompensation.py. Reads
# pm_raw_correction_data.txt (raw_cutoff_hz measured_peak_hz critical_resonance, one row per
# swept raw cutoff, all against setCutoffFrequencyClean() - no correction already applied)
# and fits:
#   1. target peak frequency -> required raw cutoff (replaces adaptResonanceFrequency())
#   2. raw cutoff -> critical (self-oscillation-threshold) resonance, so setResonance() can be
#      normalized to a fraction of critical rather than a raw, frequency-dependent value
# Both are log-space polynomials; see documentation/Filters/PoleMixing/README.md for why.

import argparse
import numpy as np


def load_data(path):
    raw_cutoff, measured_peak, critical_resonance = np.loadtxt(path, comments="#", unpack=True)
    return raw_cutoff, measured_peak, critical_resonance


def trim_to_musical_resonance_range(raw_cutoff, measured_peak, critical_resonance, max_resonance):
    # A plain least-squares polyfit doesn't need monotonic or noise-free y data - occasional
    # duplicate/out-of-order measured_peak values (the peak search's 1.02x grid is coarser
    # than the sweep's own ~1.0194x step, so adjacent points sometimes tie or jitter) are not
    # a problem for it. What *is* a problem is relevance: above some critical resonance no
    # musical use case sets resonance that high, and the measurement itself gets less precise
    # there too (very broad, shallow peaks). Keep only points at or below max_resonance.
    keep = critical_resonance <= max_resonance
    return raw_cutoff[keep], measured_peak[keep], critical_resonance[keep]


def fit_linear_cubic(x, y):
    coeffs = np.polyfit(x, y, 3)
    predicted = np.polyval(coeffs, x)
    return coeffs, predicted


def fit_log_polynomial(x, y, degree):
    log_x = np.log(x)
    log_y = np.log(y)
    coeffs = np.polyfit(log_x, log_y, degree)
    predicted = np.exp(np.polyval(coeffs, log_x))
    return coeffs, predicted


def relative_error_stats(measured, predicted):
    rel_err = np.abs(predicted - measured) / measured
    return float(np.max(rel_err)), float(np.mean(rel_err))


def format_horner_natural(coeffs, var_name):
    terms = [f"{c:.10e}f" for c in coeffs]
    expr = terms[0]
    for c in terms[1:]:
        expr = f"({expr} * {var_name} + {c})"
    return expr


def choose_log_polynomial_fit(label, x, y, degree, degree_range=range(3, 8)):
    print(f"\n{label}: log-space polynomial fit, by degree:")
    chosen = None
    for candidate_degree in degree_range:
        coeffs, predicted = fit_log_polynomial(x, y, candidate_degree)
        max_err, mean_err = relative_error_stats(y, predicted)
        print(f"  degree {candidate_degree}: max err {max_err * 100:.3f}%, mean err {mean_err * 100:.3f}%")
        if candidate_degree == degree:
            chosen = (candidate_degree, coeffs, max_err, mean_err)
    if chosen is None:
        raise SystemExit(f"--{label}-degree {degree} was not evaluated (must be in {list(degree_range)})")
    return chosen


def emit_cutoff_correction(degree, coeffs, max_err, mean_err, lo_hz, hi_hz):
    horner = format_horner_natural(list(coeffs), "logTarget")
    doc = f"""/**
 * correctedRawCutoffForTargetFrequency(targetHz) returns the raw cutoff to feed
 * setCutoffFrequencyClean() so the filter's actual resonant peak lands at targetHz, at 90%
 * of critical resonance. Fit domain: [{lo_hz:.1f}, {hi_hz:.1f}] Hz. Degree {degree}
 * polynomial in log(targetHz) -> log(rawCutoff). Max relative error over the fit domain:
 * {max_err * 100.0:.3f}%, mean {mean_err * 100.0:.3f}%.
 */
"""
    return (
        doc
        + "[[nodiscard]] inline float correctedRawCutoffForTargetFrequency(const float targetHz) noexcept\n{\n"
        + f"    // fit domain: [{lo_hz:.1f}, {hi_hz:.1f}] Hz - clamp the input so an out-of-domain request\n"
        + "    // extrapolates the polynomial from its own edge rather than from an arbitrary far-off point.\n"
        + f"    const float clampedHz = std::clamp(targetHz, {lo_hz:.1f}f, {hi_hz:.1f}f);\n"
        + "    const float logTarget = std::log(clampedHz);\n"
        + f"    return std::clamp(std::exp({horner}), 10.f, 22000.f);\n"
        + "}\n"
    )


def emit_resonance_correction(degree, coeffs, max_err, mean_err, lo_hz, hi_hz):
    horner = format_horner_natural(list(coeffs), "logCutoff")
    doc = f"""/**
 * criticalResonanceForRawCutoff(rawCutoffHz) returns the measured self-oscillation-threshold
 * resonance at the given raw cutoff. Filter1Pole4StageSmooth::setResonance() multiplies its
 * normalized (0=none, 1=at threshold) input by this, so a given resonance value means
 * roughly the same thing at any cutoff. Fit domain: [{lo_hz:.1f}, {hi_hz:.1f}] Hz. Degree
 * {degree} polynomial in log(rawCutoffHz) -> log(criticalResonance). Max relative error over
 * the fit domain: {max_err * 100.0:.3f}%, mean {mean_err * 100.0:.3f}%.
 */
"""
    return (
        doc
        + "[[nodiscard]] inline float criticalResonanceForRawCutoff(const float rawCutoffHz) noexcept\n{\n"
        + f"    const float clampedHz = std::clamp(rawCutoffHz, {lo_hz:.1f}f, {hi_hz:.1f}f);\n"
        + "    const float logCutoff = std::log(clampedHz);\n"
        + f"    return std::exp({horner});\n"
        + "}\n"
    )


def emit_header(cutoff_body, resonance_body):
    banner = """/**
 * @file PoleMixingCorrections_generated.h
 * @brief Cutoff and resonance corrections for Filter1Pole4StageSmooth, regenerated from
 * direct measurement of the raw (uncorrected) filter.
 *
 * Generated by documentation/Filters/PoleMixing/fitPoleMixingCorrections.py from
 * documentation/Filters/PoleMixing/pm_raw_correction_data.txt - see that folder's
 * README.md. Do not hand-edit: regenerate via fitPoleMixingCorrections.py instead.
 */
#pragma once

#include <algorithm>
#include <cmath>

namespace AbacDsp
{

"""
    return banner + cutoff_body + "\n" + resonance_body + "\n}\n"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("-f", "--infile", default="generated/pm_raw_correction_data.txt")
    parser.add_argument("-o", "--outfile", default="PoleMixingCorrections_generated.h")
    parser.add_argument("--cutoff-degree", type=int, default=7, help="log-space degree for the cutoff fit")
    parser.add_argument("--resonance-degree", type=int, default=7, help="log-space degree for the resonance fit")
    parser.add_argument("--max-resonance", type=float, default=20.0,
                        help="drop points whose measured critical resonance exceeds this for the "
                             "*cutoff* fit only (not musically useful, and peak search is imprecise there)")
    args = parser.parse_args()

    raw_cutoff_full, measured_peak_full, critical_resonance_full = load_data(args.infile)
    print(f"Loaded {len(raw_cutoff_full)} points from {args.infile}")

    raw_cutoff, measured_peak, _ = trim_to_musical_resonance_range(
        raw_cutoff_full, measured_peak_full, critical_resonance_full, args.max_resonance)
    print(f"Cutoff fit: kept {len(raw_cutoff)} of {len(raw_cutoff_full)} points "
          f"(critical_resonance <= {args.max_resonance})")
    print(f"  raw_cutoff range: {raw_cutoff.min():.1f} - {raw_cutoff.max():.1f} Hz")
    print(f"  measured_peak range: {measured_peak.min():.1f} - {measured_peak.max():.1f} Hz")

    # Old model shape: a plain cubic in linear Hz (matches the pre-regeneration
    # adaptResonanceFrequency()'s own model shape), fit here to the *raw* data for a fair,
    # apples-to-apples error comparison against the log-space fit chosen below.
    _, predicted_linear = fit_linear_cubic(measured_peak, raw_cutoff)
    max_err_linear, mean_err_linear = relative_error_stats(raw_cutoff, predicted_linear)
    print(f"  linear-Hz cubic (old model shape): max err {max_err_linear * 100:.2f}%, "
          f"mean err {mean_err_linear * 100:.2f}%")

    cutoff_degree, cutoff_coeffs, cutoff_max_err, cutoff_mean_err = choose_log_polynomial_fit(
        "Cutoff correction (target frequency -> raw cutoff)", measured_peak, raw_cutoff, args.cutoff_degree)

    # The resonance fit uses the *full*, untrimmed sweep: the max-resonance trim above exists
    # because the peak-frequency column gets search-grid-quantized at extreme resonance, not
    # because the bisection-measured critical-resonance column itself degrades there.
    resonance_degree, resonance_coeffs, resonance_max_err, resonance_mean_err = choose_log_polynomial_fit(
        "Resonance correction (raw cutoff -> critical resonance)", raw_cutoff_full, critical_resonance_full,
        args.resonance_degree)

    cutoff_body = emit_cutoff_correction(cutoff_degree, cutoff_coeffs, cutoff_max_err, cutoff_mean_err,
                                         float(measured_peak.min()), float(measured_peak.max()))
    resonance_body = emit_resonance_correction(resonance_degree, resonance_coeffs, resonance_max_err,
                                               resonance_mean_err, float(raw_cutoff_full.min()),
                                               float(raw_cutoff_full.max()))
    header = emit_header(cutoff_body, resonance_body)
    with open(args.outfile, "w") as f:
        f.write(header)
    print(f"\nWrote {args.outfile}")
    print(f"  cutoff correction: degree {cutoff_degree}, max err {cutoff_max_err * 100:.3f}%, "
          f"mean err {cutoff_mean_err * 100:.3f}%")
    print(f"  resonance correction: degree {resonance_degree}, max err {resonance_max_err * 100:.3f}%, "
          f"mean err {resonance_mean_err * 100:.3f}%")


if __name__ == "__main__":
    main()
