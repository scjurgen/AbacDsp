#!/usr/bin/env python3

import subprocess
import tempfile
import os
from typing import List, Dict, Tuple

def generate_sollya_script(function: str, degree: int, domain: List[float],
                           is_full_wave: bool = False, use_abs_fold: bool = False) -> str:
    is_normalized_domain = abs(domain[0] + 1.0) < 1e-10 and abs(domain[1] - 1.0) < 1e-10
    prec = 128 if is_full_wave else 64

    if use_abs_fold:
        # Fit on [0, upper] — symmetry handled by abs(x) at runtime
        is_normalized_domain = abs(domain[0] + 1.0) < 1e-10 and abs(domain[1] - 1.0) < 1e-10
        fold_upper = 1.0 if is_normalized_domain else domain[1]
        fold_domain = [0.0, fold_upper]
        domain_str = f"[{fold_domain[0]}, {fold_domain[1]}]"
        function_call = "cos(x * pi)" if is_normalized_domain else "cos(x)"
        coeff_extracts = "\n".join([f"coeff{i} = coeff(p, {i});" for i in range(degree + 1)])
        all_terms = " + ".join(
            [f"coeff{i} * x^{i}" if i > 0 else "coeff0" for i in range(degree + 1)]
        )
        simplified_poly = f"p_simplified = {all_terms};"
        coeff_range = range(degree + 1)
    else:
        domain_str = f"[{domain[0]}, {domain[1]}]"
        if is_normalized_domain:
            function_call = f"{function}(x * pi)" if is_full_wave else f"{function}(x * pi / 2)"
        else:
            function_call = f"{function}(x)"

        if function == "sin":
            coeff_extracts = "\n".join([f"coeff{i} = coeff(p, {i});" for i in range(degree + 1)])
            odd_terms = " + ".join([f"coeff{i} * x^{i}" for i in range(1, degree + 1, 2)])
            simplified_poly = f"p_simplified = {odd_terms};"
            coeff_range = range(1, degree + 1, 2)
        else:
            coeff_extracts = "\n".join([f"coeff{i} = coeff(p, {i});" for i in range(degree + 1)])
            even_terms = " + ".join(
                [f"coeff{i} * x^{i}" if i > 0 else "coeff0" for i in range(0, degree + 1, 2)]
            )
            simplified_poly = f"p_simplified = {even_terms};"
            coeff_range = range(0, degree + 1, 2)

    program = f"""
prec = {prec};
f = {function_call};
p = remez(f, {degree}, {domain_str});

{coeff_extracts}

{simplified_poly}

write("Function: {function}\\n");
write("Degree: {degree}\\n");
write("Domain: {domain_str}\\n");
write("Target function: {function_call}\\n");
write("Simplified polynomial: ", p_simplified, "\\n\\n");

simplified_error = dirtyinfnorm(f - p_simplified, {domain_str});
write("Max error: ", simplified_error, "\\n\\n");

"""
    for i in coeff_range:
        program += f'write("coeff{i}: ", coeff{i}, "\\n");\n'

    program += "\nquit;\n"
    return program


def run_sollya(script: str) -> str:
    with tempfile.NamedTemporaryFile(mode='w', suffix='.sollya', delete=False) as f:
        f.write(script)
        f.flush()
        try:
            result = subprocess.run(['sollya', f.name],
                                    capture_output=True, text=True, timeout=30)
            return result.stdout
        except subprocess.CalledProcessError as e:
            return f"Error running Sollya: {e}\nStderr: {e.stderr}"
        except FileNotFoundError:
            return "Error: Sollya not found. Install Sollya first."
        finally:
            os.unlink(f.name)


def parse_sollya_output(output: str) -> Dict:
    lines = output.split('\n')
    result = {'coefficients': {}, 'error': 0.0}
    for line in lines:
        if line.startswith('Max error:'):
            try:
                result['error'] = float(line.split(':')[1].strip())
            except Exception:
                pass
        elif line.startswith('coeff'):
            try:
                parts = line.split(':')
                coeff_num = int(parts[0].replace('coeff', ''))
                result['coefficients'][coeff_num] = float(parts[1].strip())
            except Exception:
                pass
    return result


def format_cpp_function(func_name: str, domain_name: str, coeffs: Dict,
                        error: float, use_abs_fold: bool = False) -> str:
    error_comment = f"// Max error: {error:.2e}"
    func_header = f"template<>\ninline float {func_name}<{domain_name}>(float x) noexcept {{"

    if use_abs_fold:
        all_keys = sorted(coeffs.keys(), reverse=True)
        horner = f"{coeffs[all_keys[0]]:.9f}f"
        for i in range(1, len(all_keys)):
            horner = f"({horner} * ax + {coeffs[all_keys[i]]:.9f}f)"
        func_body = "    const auto ax = std::abs(x);\n" \
                    f"    return {horner};"
    elif 'sin' in func_name.lower():
        odd_keys = sorted([k for k in coeffs if k % 2 == 1], reverse=True)
        if odd_keys:
            horner = f"{coeffs[odd_keys[0]]:.9f}f"
            for i in range(1, len(odd_keys)):
                horner = f"({horner} * x2 + {coeffs[odd_keys[i]]:.9f}f)"
            func_body = "    const auto x2 = x * x;\n" \
                        f"    return x * {horner};"
        else:
            func_body = "    return 0.0f;"
    else:
        even_keys = sorted([k for k in coeffs if k % 2 == 0], reverse=True)
        if even_keys:
            horner = f"{coeffs[even_keys[0]]:.9f}f"
            for i in range(1, len(even_keys)):
                horner = f"({horner} * x2 + {coeffs[even_keys[i]]:.9f}f)"
            func_body = "    const auto x2 = x * x;\n" \
                        f"    return {horner};"
        else:
            func_body = "    return 0.0f;"

    return f"{error_comment}\n{func_header}\n{func_body}\n}}"


DOXYGEN_HEADER = """\
/**
 * @file Approximations_generated.h
 * @brief Minimax (Remez) polynomial approximations of sin and cos.
 *
 * All functions are single-precision, branch-free, and use Horner evaluation.
 * Coefficients are computed by Sollya at prec=64 (half-wave) or prec=128 (full-wave).
 *
 * ## Domain tags
 * | Tag                      | Input range        | Notes                              |
 * |--------------------------|--------------------|------------------------------------|
 * | DomainMinusPiHalfToPiHalf| [-π/2,  π/2]       | Half-wave, natural radian input    |
 * | DomainMinusOneToOne      | [-1,    1]         | Normalised; mapped to ±π/2 or ±π  |
 * | DomainMinusPiToPi        | [-π,    π]         | Full-wave, natural radian input    |
 *
 * ## Which variant to use
 *
 * **Half-wave (`remezSin*`, `remezCos*`)**
 * Use when the caller already range-reduces to [-π/2, π/2], e.g. inside a
 * CORDIC loop or a wavetable oscillator that folds quadrants externally.
 * Lowest degree (P3/P4) suffices for control-rate modulation (~1e-3 error).
 * P5/P6 is adequate for audio-rate oscillators (~1e-5 error).
 *
 * **Full-wave symmetric (`remezFullCos*` with DomainMinusPiToPi / DomainMinusOneToOne)**
 * Use when the input covers the full cycle and no external range reduction
 * is available. Fits even-only coefficients on the full domain; same cost
 * as half-wave at equal degree but somewhat larger error due to wider domain.
 *
 * **Full-wave abs-folded (`remezFullCosAbsFold*`)**
 * Folds the domain to [0, π] via std::abs(x) before polynomial evaluation.
 * All polynomial degrees are active (not just even), giving a tighter minimax
 * fit for the same degree compared to the symmetric variant.
 * Preferred for full-wave cosine when a single extra abs is acceptable.
 * No equivalent for sine (sine is odd, not even; abs-folding breaks it).
 *
 * **Normalised domain (`DomainMinusOneToOne`)**
 * Use when the phase is already in a unit range, e.g. a phasor oscillator
 * producing values in [-1, 1]. Avoids an explicit multiply by π at the call site.
 *
 * ## Error budget summary (approximate, post-rounding)
 * | Function                              | Max error  |
 * |---------------------------------------|------------|
 * | remezSinP3 / remezCosP4               | ~4e-3      |
 * | remezSinP5 / remezCosP6               | ~7e-5      |
 * | remezFullSinP5 / remezFullCosP6       | ~2e-3      |
 * | remezFullSinP7 / remezFullCosP8       | ~2e-5      |
 * | remezFullSinP9 / remezFullCosP10      | ~2e-7      |
 * | remezFullCosAbsFoldP4                 | ~2e-4      |
 * | remezFullCosAbsFoldP6                 | ~2e-7      |
 */
"""


def generate_approximations():
    PI = 3.14159265358979323846

    # (function, degree, domain, func_name, domain_name, is_full_wave, use_abs_fold)
    approximations: List[Tuple] = [
        # Half-wave: [-pi/2, pi/2]
        ('sin', 3, [-PI/2, PI/2], 'remezSinP3',  'DomainMinusPiHalfToPiHalf', False, False),
        ('sin', 5, [-PI/2, PI/2], 'remezSinP5',  'DomainMinusPiHalfToPiHalf', False, False),
        ('cos', 4, [-PI/2, PI/2], 'remezCosP4',  'DomainMinusPiHalfToPiHalf', False, False),
        ('cos', 6, [-PI/2, PI/2], 'remezCosP6',  'DomainMinusPiHalfToPiHalf', False, False),

        # Half-wave: [-1, 1] -> [-pi/2, pi/2]
        ('sin', 3, [-1.0, 1.0],   'remezSinP3',  'DomainMinusOneToOne',       False, False),
        ('sin', 5, [-1.0, 1.0],   'remezSinP5',  'DomainMinusOneToOne',       False, False),
        ('cos', 4, [-1.0, 1.0],   'remezCosP4',  'DomainMinusOneToOne',       False, False),
        ('cos', 6, [-1.0, 1.0],   'remezCosP6',  'DomainMinusOneToOne',       False, False),

        # Full-wave: [-pi, pi]
        ('sin', 5, [-PI,   PI],   'remezFullSinP5',  'DomainMinusPiToPi',     True,  False),
        ('sin', 7, [-PI,   PI],   'remezFullSinP7',  'DomainMinusPiToPi',     True,  False),
        ('sin', 9, [-PI,   PI],   'remezFullSinP9',  'DomainMinusPiToPi',     True,  False),
        ('cos', 6, [-PI,   PI],   'remezFullCosP6',  'DomainMinusPiToPi',     True,  False),
        ('cos', 8, [-PI,   PI],   'remezFullCosP8',  'DomainMinusPiToPi',     True,  False),
        ('cos', 10,[-PI,   PI],   'remezFullCosP10', 'DomainMinusPiToPi',     True,  False),

        # Full-wave: [-1, 1] -> [-pi, pi]
        ('sin', 5, [-1.0, 1.0],   'remezFullSinP5',  'DomainMinusOneToOne',   True,  False),
        ('sin', 7, [-1.0, 1.0],   'remezFullSinP7',  'DomainMinusOneToOne',   True,  False),
        ('sin', 9, [-1.0, 1.0],   'remezFullSinP9',  'DomainMinusOneToOne',   True,  False),
        ('cos', 6, [-1.0, 1.0],   'remezFullCosP6',  'DomainMinusOneToOne',   True,  False),
        ('cos', 8, [-1.0, 1.0],   'remezFullCosP8',  'DomainMinusOneToOne',   True,  False),
        ('cos', 10,[-1.0, 1.0],   'remezFullCosP10', 'DomainMinusOneToOne',   True,  False),

        # Full-wave abs-folded cosine: fit on [0, pi], eval as p(|x|)
        ('cos', 4, [-PI,   PI],   'remezFullCosAbsFoldP4',  'DomainMinusPiToPi',   True, True),
        ('cos', 6, [-PI,   PI],   'remezFullCosAbsFoldP6',  'DomainMinusPiToPi',   True, True),
        ('cos', 4, [-1.0, 1.0],   'remezFullCosAbsFoldP4',  'DomainMinusOneToOne', True, True),
        ('cos', 6, [-1.0, 1.0],   'remezFullCosAbsFoldP6',  'DomainMinusOneToOne', True, True),
    ]

    header = DOXYGEN_HEADER + """\
#pragma once

#include <cmath>

namespace Approximation {

// Domain tags
struct DomainMinusOneToOne {};
struct DomainMinusPiHalfToPiHalf {};
struct DomainMinusPiToPi {};

// Template declarations
"""

    seen = set()
    for function, degree, domain, func_name, domain_name, is_full_wave, use_abs_fold in approximations:
        decl = f"template <typename Domain>\ninline float {func_name}(float x) noexcept;"
        if decl not in seen:
            header += decl + "\n"
            seen.add(decl)

    header += "\n"
    cpp_implementations = []

    for function, degree, domain, func_name, domain_name, is_full_wave, use_abs_fold in approximations:
        print(f"Generating {func_name}<{domain_name}>...")
        script = generate_sollya_script(function, degree, domain, is_full_wave, use_abs_fold)
        output = run_sollya(script)
        result = parse_sollya_output(output)

        if result['coefficients']:
            cpp_impl = format_cpp_function(func_name, domain_name,
                                           result['coefficients'], result['error'], use_abs_fold)
            cpp_implementations.append(cpp_impl)
            print(f"  Max error: {result['error']:.2e}")
        else:
            print(f"  Failed to generate coefficients")

    full_header = header + "\n\n".join(cpp_implementations) + "\n\n} // namespace Approximation\n"

    with open('Approximations_generated.h', 'w') as f:
        f.write(full_header)

    print("\nGenerated header saved as 'Approximations_generated.h'")
    return full_header


if __name__ == "__main__":
    generate_approximations()
