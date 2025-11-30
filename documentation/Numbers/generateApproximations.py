#!/usr/bin/env python3

import subprocess
import tempfile
import os
from typing import List, Dict

def generate_sollya_script(function: str, degree: int, domain: List[float], is_full_wave: bool = False) -> str:
    """Generate Sollya script for polynomial approximation."""
    domain_str = f"[{domain[0]}, {domain[1]}]"

    # Determine if we need domain mapping
    is_normalized_domain = abs(domain[0] + 1.0) < 1e-10 and abs(domain[1] - 1.0) < 1e-10

    if is_normalized_domain:
        # For [-1, 1] domain, map x to the function's natural domain
        if is_full_wave:
            # x in [-1, 1] maps to x * pi for full-wave (cos(x*pi), sin(x*pi))
            function_call = f"{function}(x * pi)"
        else:
            # x in [-1, 1] maps to x * pi/2 for half-wave (cos(x*pi/2), sin(x*pi/2))
            function_call = f"{function}(x * pi / 2)"
    else:
        # For [-pi/2, pi/2] or [-pi, pi] domain, use x directly
        function_call = f"{function}(x)"

    # For sine, only use odd coefficients; for cos, only even
    if function == "sin":
        # Extract odd coefficients
        coeff_extracts = "\n".join([f"coeff{i} = coeff(p, {i});" for i in range(0, degree + 1)])
        odd_terms = " + ".join([f"coeff{i} * x^{i}" for i in range(1, degree + 1, 2)])
        simplified_poly = f"p_simplified = {odd_terms};"
    else:  # cos
        # Extract even coefficients
        coeff_extracts = "\n".join([f"coeff{i} = coeff(p, {i});" for i in range(0, degree + 1)])
        even_terms = " + ".join([f"coeff{i} * x^{i}" if i > 0 else "coeff0" for i in range(0, degree + 1, 2)])
        simplified_poly = f"p_simplified = {even_terms};"

    program = f"""
prec = 53;
f = {function_call};
p = remez(f, {degree}, {domain_str});

{coeff_extracts}

// Create simplified polynomial
{simplified_poly}

write("Function: {function}\\n");
write("Degree: {degree}\\n");
write("Domain: {domain_str}\\n");
write("Target function: {function_call}\\n");
write("Full polynomial: ", p, "\\n");
write("Simplified polynomial: ", p_simplified, "\\n\\n");

simplified_error = dirtyinfnorm(f - p_simplified, {domain_str});
write("Max error: ", simplified_error, "\\n\\n");

// Output coefficients for C++
"""

    if function == "sin":
        for i in range(1, degree + 1, 2):
            program += f'write("coeff{i}: ", coeff{i}, "\\n");\n'
    else:  # cos
        for i in range(0, degree + 1, 2):
            program += f'write("coeff{i}: ", coeff{i}, "\\n");\n'

    program += """
quit;
"""
    return program

def run_sollya(script: str) -> str:
    """Run Sollya script and return output."""
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
    """Parse Sollya output to extract coefficients and error."""
    lines = output.split('\n')
    result = {'coefficients': {}, 'error': 0.0}

    for line in lines:
        if line.startswith('Max error:'):
            try:
                error_str = line.split(':')[1].strip()
                result['error'] = float(error_str)
            except:
                pass
        elif line.startswith('coeff'):
            try:
                parts = line.split(':')
                coeff_num = int(parts[0].replace('coeff', ''))
                coeff_val = float(parts[1].strip())
                result['coefficients'][coeff_num] = coeff_val
            except:
                pass

    return result

def format_cpp_function(func_name: str, domain_name: str, coeffs: Dict, error: float) -> str:
    """Format C++ template specialization."""
    error_comment = f"// Max error: {error:.2e}"
    func_header = f"template<>\ninline float {func_name}<{domain_name}>(float x) noexcept {{"

    if 'sin' in func_name.lower():
        # Sine - odd coefficients only, use Horner form with x2
        x2_decl = " const auto x2 = x * x;"

        # Build Horner form from highest to lowest odd degree
        odd_keys = sorted([k for k in coeffs.keys() if k % 2 == 1], reverse=True)

        if odd_keys:
            horner_expr = f"{coeffs[odd_keys[0]]:.9f}f"
            for i in range(1, len(odd_keys)):
                horner_expr = f"({horner_expr} * x2 + {coeffs[odd_keys[i]]:.9f}f)"
            func_body = f"{x2_decl}\n return x * {horner_expr};"
        else:
            func_body = " return 0.0f;"

    else:  # Cosine - even coefficients, use Horner form with x2
        x2_decl = " const auto x2 = x * x;"

        # Build Horner form from highest to lowest even degree
        even_keys = sorted([k for k in coeffs.keys() if k % 2 == 0], reverse=True)

        if even_keys:
            horner_expr = f"{coeffs[even_keys[0]]:.9f}f"
            for i in range(1, len(even_keys)):
                horner_expr = f"({horner_expr} * x2 + {coeffs[even_keys[i]]:.9f}f)"
            func_body = f"{x2_decl}\n return {horner_expr};"
        else:
            func_body = " return 0.0f;"

    return f"{error_comment}\n{func_header}\n{func_body}\n}}"

def generate_approximations():
    """Generate all polynomial approximations."""
    # Configuration for all approximations to generate
    approximations = [
        # Half-wave approximations (original domain: [-pi/2, pi/2])
        ('sin', 3, [-3.14159265359/2, 3.14159265359/2], 'remezSinP3', 'DomainMinusPiHalfToPiHalf', False),
        ('sin', 5, [-3.14159265359/2, 3.14159265359/2], 'remezSinP5', 'DomainMinusPiHalfToPiHalf', False),
        ('cos', 4, [-3.14159265359/2, 3.14159265359/2], 'remezCosP4', 'DomainMinusPiHalfToPiHalf', False),
        ('cos', 6, [-3.14159265359/2, 3.14159265359/2], 'remezCosP6', 'DomainMinusPiHalfToPiHalf', False),

        # Half-wave approximations (normalized domain: [-1, 1] mapped to sin(x*pi/2), cos(x*pi/2))
        ('sin', 3, [-1.0, 1.0], 'remezSinP3', 'DomainMinusOneToOne', False),
        ('sin', 5, [-1.0, 1.0], 'remezSinP5', 'DomainMinusOneToOne', False),
        ('cos', 4, [-1.0, 1.0], 'remezCosP4', 'DomainMinusOneToOne', False),
        ('cos', 6, [-1.0, 1.0], 'remezCosP6', 'DomainMinusOneToOne', False),

        # Full-wave approximations (original domain: [-pi, pi])
        ('sin', 5, [-3.14159265359, 3.14159265359], 'remezFullSinP5', 'DomainMinusPiHalfToPiHalf', True),
        ('sin', 7, [-3.14159265359, 3.14159265359], 'remezFullSinP7', 'DomainMinusPiHalfToPiHalf', True),
        ('sin', 9, [-3.14159265359, 3.14159265359], 'remezFullSinP9', 'DomainMinusPiHalfToPiHalf', True),
        ('cos', 6, [-3.14159265359, 3.14159265359], 'remezFullCosP6', 'DomainMinusPiHalfToPiHalf', True),
        ('cos', 8, [-3.14159265359, 3.14159265359], 'remezFullCosP8', 'DomainMinusPiHalfToPiHalf', True),
        ('cos', 10, [-3.14159265359, 3.14159265359], 'remezFullCosP10', 'DomainMinusPiHalfToPiHalf', True),

        # Full-wave approximations (normalized domain: [-1, 1] mapped to sin(x*pi), cos(x*pi))
        ('sin', 5, [-1.0, 1.0], 'remezFullSinP5', 'DomainMinusOneToOne', True),
        ('sin', 7, [-1.0, 1.0], 'remezFullSinP7', 'DomainMinusOneToOne', True),
        ('sin', 9, [-1.0, 1.0], 'remezFullSinP9', 'DomainMinusOneToOne', True),
        ('cos', 6, [-1.0, 1.0], 'remezFullCosP6', 'DomainMinusOneToOne', True),
        ('cos', 8, [-1.0, 1.0], 'remezFullCosP8', 'DomainMinusOneToOne', True),
        ('cos', 10, [-1.0, 1.0], 'remezFullCosP10', 'DomainMinusOneToOne', True),
    ]

    # Generate header with domain tags
    header = """#pragma once

#include <cmath>

namespace Approximation {

// Domain tags
struct DomainMinusOneToOne {};
struct DomainMinusPiHalfToPiHalf {};

// Template declarations
"""

    # Generate declarations from approximations list
    seen = set()
    for function, degree, domain, func_name, domain_name, is_full_wave in approximations:
        decl = f"template <typename Domain>\ninline float {func_name}(float x) noexcept;"
        if decl not in seen:
            header += decl + "\n"
            seen.add(decl)

    header += "\n"

    cpp_implementations = []

    for function, degree, domain, func_name, domain_name, is_full_wave in approximations:
        print(f"Generating {func_name}<{domain_name}>...")

        # Generate and run Sollya script
        sollya_script = generate_sollya_script(function, degree, domain, is_full_wave)
        sollya_output = run_sollya(sollya_script)

        # Parse results
        result = parse_sollya_output(sollya_output)

        if result['coefficients']:
            cpp_impl = format_cpp_function(func_name, domain_name,
                                           result['coefficients'], result['error'])
            cpp_implementations.append(cpp_impl)
            print(f" Max error: {result['error']:.2e}")
        else:
            print(f" Failed to generate coefficients")

    # Combine all parts
    full_header = header + "\n\n".join(cpp_implementations) + """

} // namespace Approximation
"""

    # Write to file
    with open('Approximations_generated.h', 'w') as f:
        f.write(full_header)

    print(f"\nGenerated header saved as 'Approximations_generated.h'")
    return full_header

if __name__ == "__main__":
    generate_approximations()
