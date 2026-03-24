# Sinc Filter Generator

Generates windowed-sinc FIR filter coefficients and writes them into a C++ header
using a template file. Translated from the original Octave implementation.

## Requirements

`Python 3.10+`

## Setup

Create and activate a virtual environment:

    python3 -m venv .venv

    # Linux / macOS
    source .venv/bin/activate

    # Windows
    .venv\Scripts\activate

Install dependencies:
    
    pip install --upgrade pip
    pip install -r requirements.txt

To deactivate the environment when done:

    deactivate

## Usage

Place SincFilter.class.template in the working directory, then call make_filter
from your script:

    from make_filter import make_filter
    half_coeffs = make_filter(cycles=8, increment=4, atten=100.0)

This produces a Sinc<cycles>.h file in the working directory.

## Parameters

    cycles      float   Number of sinc lobes (controls filter length)
    increment   int     Interpolation/decimation factor
    atten       float   Target stop-band attenuation in dB

## How it works

The generator produces a one-sided (half) set of FIR coefficients for a
windowed-sinc low-pass filter, ready to embed directly in a C++ header.

Step 1 - Filter generation (_generate_filter)
A symmetric sinc kernel of length N is computed, where N is derived from
the number of lobe cycles, the interpolation factor (increment), and an
internal fudge factor that controls the transition band width. A Kaiser
window is applied to the kernel to achieve the target stop-band attenuation,
and the result is normalised to unity DC gain.

Step 2 - Filter measurement (_measure_filter)
The filter magnitude response is evaluated via a large FFT (400 000 points).
Three values are extracted: stop-band attenuation, the normalised frequency
at which the stop band begins, and the normalised -3 dB frequency.

Step 3 - Bisection optimisation (make_filter)
The fudge factor is tuned by bisection between 1.0 and 1.25 until the
stop-band start frequency converges to 0.5 / increment (the Nyquist of the
decimated rate) within 1e-10. This maximises the usable pass-band width
for the given attenuation and filter length.

Step 4 - Coefficient extraction
Only the second half of the symmetric filter is retained. The array is
padded with trailing zeros to align its length to a multiple of 4,
suitable for SIMD processing.

Step 5 - Header generation
The half-coefficients are written as C++ float literals into an
AbacDsp::SincFilter::InitParam initialiser, with a comment block
summarising the filter characteristics.
