# Pole-mixing filter verification plots

Visualizes and verifies `AbacDsp`'s pole-mixing / multimode VCF family
(`src/includes/Filters/PoleMixingFilter.h`) directly, with no JUCE involved: magnitude/phase
curves for a representative set of `poleMixingList` presets, resonance behavior up to
self-oscillation, the `atan`/`x/sqrt(1+x^2)` saturators' overdrive behavior, cutoff-frequency
correctness measured just below self-oscillation, real-time behavior when cutoff/resonance
change while the filter is running, and the difference between the two resonance topologies
still in the file: classic last-stage feedback (`FixedFourStageFilter`) and bandpass-tap
feedback (`Filter1Pole4StageSmooth`).

This folder is also where `src/includes/Filters/PoleMixingCorrections_generated.h` -
`Filter1Pole4StageSmooth`'s cutoff correction - is regenerated from: measure the raw,
uncorrected filter directly, fit a correction from that measurement, emit it as C++. See
"Regenerating the cutoff correction" below.

`pm_response.png`, `pm_resonance.png`, `pm_overdrive.png`, `pm_cutoff_accuracy.png`,
`pm_realtime.png` and `pm_topology.png` in this folder are checked-in samples of all six
plots, produced by the pipeline described here.

## Quick start

`./generate.sh` runs the whole pipeline in one step: configures/builds `PoleMixingExplore`
(behind `EXPLORE_STUFF`, into a gitignored `build/` at the repo root), runs it, sets up a
local `.venv` from `requirements.txt`, and renders all six PNGs in this folder. The sections
below explain what it's doing and how to run each step by hand.

## Generating the data

The C++ side only writes plain text; all plotting is done by the shared
`documentation/Plot/PyConPlot.py` tool, avoiding a C++ plotting dependency.

Build (enabled behind the `EXPLORE_STUFF` CMake option; no `dev-scripts/` wrapper covers
this, so configure/build directly) and run it, from the repo root (optional arguments are
the seven output files, default `pm_response.txt` / `pm_resonance.txt` / `pm_overdrive.txt` /
`pm_cutoff_accuracy.txt` / `pm_realtime.txt` / `pm_topology.txt` /
`pm_raw_correction_data.txt`):

```bash
mkdir -p build && cd build
cmake -DEXPLORE_STUFF=ON -DCMAKE_BUILD_TYPE=Release ..
cmake --build . --target PoleMixingExplore
./documentation/Filters/PoleMixing/PoleMixingExplore
```

All measurements are made on the running filter, not on private state: a shared bisection
helper (`findCriticalResonance`) finds the largest "user" resonance value whose impulse
response still decays, by actually running the filter and checking whether the tail of a
16384-sample impulse response has decayed below half its own peak; "just below
self-oscillation" throughout this folder means 90% of that measured value, and "past it"
means 105%. A second shared helper (`measureSteadyStateAmplitude`) drives the filter with a
sine probe and reads back the steady-state output amplitude, used both to build magnitude
sweeps for the topology that has no closed form (see below) and to locate resonant peaks.

- `pm_response.txt`: two subplots, magnitude and phase, for a representative subset of
  `poleMixingList` (`LP1`/`LP2`/`LP4`, `HP1`/`HP2`/`HP4`, `BP2`/`BP4`, `AP2`/`AP4`, `Notch`)
  at a fixed 1 kHz cutoff, resonance 0 - not all ~48 presets, to keep the plot legible. Both
  curves come from `FourStageFilterTheoretical`: magnitude via `magnitudeBP()` (the discrete
  cascade, accurate across the whole sweep), phase via `phase()` (the analog approximation,
  which the class itself documents as accurate only well below Nyquist - matched here by the
  unit tests' own practice of only checking phase/magnitude agreement up to `12x` the
  cutoff).
- `pm_resonance.txt`: three subplots on the bandpass-tap topology (`Filter1Pole4StageSmooth`).
  The first two sweep `LP4` and `BP4`'s magnitude response at four resonance values
  (`0`, `50%`, `80%`, `95%` of the measured critical resonance) to show the peak sharpening
  as resonance approaches the threshold. The third drives `LP4` at `105%` of critical with a
  single impulse and nothing after, showing the envelope ramp up and then hold, bounded by
  the `x/sqrt(1+x^2)` saturator rather than decay back to silence - self-oscillation made
  directly visible rather than only inferred from a peaky curve.
- `pm_overdrive.txt`: a transfer-curve subplot (`atan(x)` vs. `x/sqrt(1+x^2)`, `x` in
  `-6..6`) plus one FFT-spectrum subplot per topology (`classic`/`bandpass-tap`, both `LP4`,
  resonance 0), each at three input levels (`0.05`, `0.5`, `2.0`) through a 300 Hz probe
  tone, to show harmonic growth as each saturator engages.
- `pm_cutoff_accuracy.txt`: requested cutoff vs. the actually-measured resonant-peak
  frequency (a global search over 50 Hz-20 kHz, not a narrow window around the request - the
  correction under test is exactly what might move the peak far from the request), at 90% of
  each topology's own critical resonance, for both `FixedFourStageFilter`
  (`warpCutoffForSampleRate` correction) and `Filter1Pole4StageSmooth`
  (`adaptResonanceFrequency` correction). Two subplots: a 100 Hz-12 kHz overview (log-log)
  and a 1000-12 kHz zoom (linear axes, so the divergence isn't visually compressed by the
  log scale), both swept on a musical grid - a third of a semitone per step
  (`2^(1/36)`) - rather than an arbitrary round-number step size. This is by far the most
  expensive plot to generate (roughly 250 requested-cutoff points per topology, each one its
  own critical-resonance bisection plus a full peak search): `generate.sh` takes on the order
  of a minute or two because of it, not seconds.
- `pm_realtime.txt`: two subplots, a 1500 Hz probe tone through an `LP4` filter with a
  cutoff jump (800 -> 4000 Hz) partway through, and a probe tone at cutoff through an `LP4`
  filter with a resonance jump (0 -> 90% of critical) partway through - each showing
  `FixedFourStageFilter` (classic) against `Filter1Pole4StageSmooth` (bandpass-tap). Both
  jumps happen 100 ms into a 300 ms render, envelope window matched to one full probe period
  so the window itself doesn't beat against the tone.
- `pm_topology.txt`: three subplots. The first overlays `LP4`'s resonance-peak shape for
  both topologies (each at 90% of its own critical resonance, both curves normalized to
  0 dB at their own peak - the two topologies' weight conventions aren't at the same
  absolute scale, so only shape is comparable). The second overlays both topologies'
  self-oscillation envelope (105% of critical, single impulse, `LP4`). The third is the
  comparison requested directly: `LP4`, `HP4` and `BP4`, each drawn twice (classic feedback
  vs. bandpass-tap feedback), six curves in one plot, all normalized to their own peak.
- `pm_raw_correction_data.txt`: not `PyConPlot` format - plain columns
  (`raw_cutoff_hz measured_peak_hz critical_resonance`), one row per raw cutoff swept with
  `setCutoffFrequencyClean()` (i.e. no correction applied at all) on the same
  third-of-a-semitone grid, 20 Hz-20 kHz. This is `fitCutoffCorrection.py`'s input; see
  "Regenerating the cutoff correction" below.

## Rendering the plots

From `documentation/Filters/PoleMixing/` (the venv and `requirements.txt` below live here,
not in `build/`):

```bash
python3 -m venv .venv && source .venv/bin/activate
pip install -r requirements.txt

python3 ../../Plot/PyConPlot.py -f ../../../build/pm_response.txt -o pm_response.png \
    --labelx "frequency (Hz)" --labely "magnitude (dB) / phase (deg)" --width 1200 --height 450 --cols 1

python3 ../../Plot/PyConPlot.py -f ../../../build/pm_resonance.txt -o pm_resonance.png \
    --labelx "frequency (Hz) / time (s)" --labely "magnitude (dB) / amplitude" --width 1200 --height 350 --cols 1

python3 ../../Plot/PyConPlot.py -f ../../../build/pm_overdrive.txt -o pm_overdrive.png \
    --labelx "input / frequency (Hz)" --labely "output / magnitude (dB)" --width 1200 --height 350 --cols 1

python3 ../../Plot/PyConPlot.py -f ../../../build/pm_cutoff_accuracy.txt -o pm_cutoff_accuracy.png \
    --labelx "requested cutoff (Hz)" --labely "measured resonant-peak frequency (Hz)" --width 1200 --height 500 --cols 1

python3 ../../Plot/PyConPlot.py -f ../../../build/pm_realtime.txt -o pm_realtime.png \
    --labelx "time (s)" --labely "output amplitude" --width 1200 --height 350 --cols 1

python3 ../../Plot/PyConPlot.py -f ../../../build/pm_topology.txt -o pm_topology.png \
    --labelx "frequency (Hz) / time (s)" --labely "magnitude (dB) / amplitude" --width 1200 --height 350 --cols 1
```

## Regenerating the cutoff correction

`Filter1Pole4StageSmooth::adaptResonanceFrequency()` used to be a hand-fit piecewise cubic.
`fitCutoffCorrection.py` replaces it with one regenerated from direct measurement, following
the same measure -> `curve_fit` -> emit-C++ pattern as
`documentation/Filters/BandpassImpulses/fitBandPassCompensation.py`:

```bash
python3 fitCutoffCorrection.py -f pm_raw_correction_data.txt \
    -o ../../../src/includes/Filters/PoleMixingCorrections_generated.h --degree 7
```

This is a separate, deliberate step - not part of `generate.sh` - since it overwrites a file
under `src/includes/`, real library source, not a documentation artifact.

The old correction was fit against `pm_cutoff_accuracy.txt`-style data: target vs. the
*already-corrected* cutoff. That's circular by construction, and `adaptResonanceFrequency
(12000)` evaluates to about 25.3 kHz - past Nyquist at 48 kHz - before that value is even
used to place the pole, which is almost certainly why the old correction's error blew up the
way `pm_cutoff_accuracy.png` showed. `fitCutoffCorrection.py` instead fits directly against
`pm_raw_correction_data.txt`'s raw (`setCutoffFrequencyClean()`) sweep, so the fit can never
itself reason about a past-Nyquist value.

It fits `log(raw_cutoff) = polynomial(log(target_frequency))` - log-space rather than the old
model's plain cubic in linear Hz, since a single low-degree polynomial does not fit a
multiplicative (octave-spanning) relationship well in linear space. The script prints both
for comparison: refit to the same data, the old model shape's error is `26.9%` max / `3.8%`
mean; the log-space degree-7 fit is `1.55%` max / `0.52%` mean, over its measured domain
(`20 Hz-8.6 kHz`, i.e. `critical_resonance <= 20` - see below for why that domain limit).
Points are excluded above `critical_resonance = 20`: past there the resonance peak is so
broad that the peak-search grid can no longer resolve it precisely, and no real patch would
run resonance that high anyway. The generated function clamps its input to that domain
before evaluating, so an out-of-domain request extrapolates from the domain's own edge -
visible in `pm_cutoff_accuracy.png` as the bandpass-tap curve going flat above roughly an
8.5 kHz request, rather than the old correction's unbounded overshoot-then-collapse.

**Resonance behavior, not yet corrected:** the same raw sweep also records the measured
critical resonance at every point. It is not flat: `3.93` at 20 Hz rising to `19.97` at the
edge of the fit domain (`8.6 kHz` target / `11.1 kHz` raw cutoff) - a `291.6%` relative
spread within just the *musically useful* part of the range, let alone above it. A single
1 kHz spot-check earlier in this README suggested critical resonance was roughly flat
(`~4.56` for both `LP4` and `BP4`); the fuller sweep shows that was only true near that one
point; a fixed `setResonance()` value means something very different at 200 Hz than at
5 kHz. This is exactly what the existing but currently-unwired `ResonanceFrequencyModifier`
class was designed to compensate for (its own, simpler `fs/8`-threshold heuristic), now with
real measured numbers behind it. Not corrected in this pass - what a predictable
resonance-vs-frequency behavior should actually look like (a normalized "fraction of
critical" parameter? a generated correction table alongside the cutoff one?) is an open
design decision, not yet made.

## What the plots show

**Response curves** (`pm_response.png`) match every represented preset's own name: the `LPn`
family rolls off above 1 kHz at increasingly steep slopes (`LP4` reaches roughly -80 dB by
12 kHz), `HPn` mirrors that below 1 kHz, `BP2`/`BP4` peak at the cutoff and roll off both
sides, `AP2`/`AP4` stay close to 0 dB everywhere (an allpass moves phase, not magnitude) while
their phase subplot sweeps through the expected wide range, and `Notch` shows a clear dip
right at 1 kHz.

**Resonance behavior** (`pm_resonance.png`): both `LP4` and `BP4` sharpen smoothly as
resonance rises from 0 toward the measured critical value (`~4.56` in this topology's "user"
units, for both presets, at a 1 kHz cutoff), with the peak growing to a few dB above 0 dB
just before the threshold. Past it, the third subplot's `LP4` impulse response rings up in
about 60 ms and then holds flat near amplitude `0.089` indefinitely - self-oscillation, not
inferred from a peaky curve but actually rendered as a sustained, saturator-bounded
oscillation.

**Overdrive** (`pm_overdrive.png`): the two saturators' transfer curves are close but not
identical - `atan(x)` reaches roughly `+/-1.41` at `x=+/-6` where `x/sqrt(1+x^2)` has already
flattened out near `+/-1.0`, i.e. `atan` keeps admitting more level at large input. At the
signal levels tested here (`0.05` to `2.0`) the two topologies' output spectra are visually
almost indistinguishable - both saturators sit close to identity near their shared operating
point, and the harmonic ladder (300 Hz probe, harmonics stepping up through the spectrum)
grows the same way with level in both. The two saturators' curves only pull apart
noticeably at much larger `x`, outside what's driven here.

**Cutoff-frequency correctness** (`pm_cutoff_accuracy.png`): after regenerating
`Filter1Pole4StageSmooth`'s correction from direct raw-filter measurement (see
"Regenerating the cutoff correction" above), the bandpass-tap curve now tracks `y=x` closely
across essentially the whole range that was previously broken - visually indistinguishable
from the reference line from 100 Hz up to about 8.5 kHz, where the old correction's error had
peaked near `+47%`. Above ~8.5 kHz the curve goes flat at its fit domain's own edge (by
design - see above - rather than the old correction's unbounded overshoot). Note this domain
limit is a property of the *correction*, regenerated to stay honest about how far its
measurement covered, not evidence the underlying filter itself cannot resonate higher.
`FixedFourStageFilter`'s `warpCutoffForSampleRate` is untouched by this work and shows the
same behavior as before: tracks within about 4% up to roughly 8-9 kHz, then increasingly
overshoots, reaching `+30%` (`~15.6 kHz` measured) by a 12 kHz request.

**Real-time behavior** (`pm_realtime.png`): the cutoff jump shows the documented difference
directly - `FixedFourStageFilter`'s linear ramp (driven through `processBlock()`, since
`step()` alone never advances it - see the "no smoothing" note in the class doc) rises and
then locks to its new value with a visible hard corner once the ramp's fixed step count is
used up, while `Filter1Pole4StageSmooth`'s exponential smoother keeps easing in for longer
afterward with no corner, consistent with the class comment that "the smoother never quite
arrives". The resonance jump is the sharper contrast: `FixedFourStageFilter::setResonance()`
has no smoothing at all, so the step produces a visible transient overshoot/ring before
settling, while `Filter1Pole4StageSmooth` smooths resonance the same way it smooths cutoff,
producing a clean monotonic glide with no overshoot at all.

**Topology comparison** (`pm_topology.png`): the first subplot's `LP4` peaks land at almost
the same frequency for both topologies, with the bandpass-tap curve slightly broader/less
sharp near the peak. The second subplot's self-oscillation plateaus differ slightly
(`~0.094` classic vs. `~0.089` bandpass-tap) - both sustain indefinitely, neither decays.
The third subplot is the direct answer to "how do the two topologies differ at the low end
and in resonance behavior": across all three presets (`LP4`/`HP4`/`BP4`), the bandpass-tap
curve sits consistently 10-20 dB above (i.e. less attenuated than) the classic curve toward
the low end, most dramatically for `LP4` (`~-5 dB` bandpass-tap vs. `~-20 dB` classic,
relative to each curve's own peak). This follows directly from the two feedback signals'
own definitions: `bandpassTap()` is `-v3 + 2*v2 - v1`, a discrete second difference that is
exactly zero at DC by construction (every one-pole stage has unity DC gain, so `v1 = v2 = v3`
at DC and the difference cancels) - resonance feedback contributes nothing at DC regardless
of how high it's turned up. The classic topology feeds back `v3` itself, which is *not* zero
at DC, so raising resonance measurably pulls the DC/low-end gain down through negative
feedback, on top of whatever the preset's own low-end shape already does. This matches the
class's own doc comment that the bandpass tap "peaks the response without dragging the
cutoff" - it also, as a direct consequence, does not drag the low end either.
