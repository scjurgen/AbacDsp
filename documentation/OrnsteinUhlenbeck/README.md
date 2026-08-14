# Ornstein-Uhlenbeck timeline plot

Visualises `AbacDsp::OrnsteinUhlenbeckProcess` (`src/includes/Generators/OrnsteinUhlenbeckProcess.h`)
at work: several instances at different `sigma` "speeds" each trace a timeline, stacked as
wide subplots - one per speed - on one shared y-scale so their relative jitter and reversion
rate are directly comparable at a glance - the mean-reverting noise the class exists to
produce, made visible instead of just heard.

Because a timeline alone makes it hard to tell "faster" from "just noisier" by eye, a second
plot shows each speed's empirical autocorrelation against the process's theoretical decay
`exp(-theta*tau)` - the actual quantitative check that the reversion rate is correct, not
just a visual impression.

`ou_timeline.png` and `ou_autocorr.png` in this folder are checked-in samples of both plots,
produced by the pipeline described here.

## Generating the data

The C++ side only writes plain text; all plotting is done by the shared
`documentation/Plot/PyConPlot.py` tool, avoiding a C++ plotting dependency.

Build (enabled behind the `EXPLORE_STUFF` CMake option; no `dev-scripts/` wrapper covers
this, so configure/build directly) and run it, from the repo root (optional arguments are
the timeline and autocorrelation output files, default `ou_timelines.txt` /
`ou_autocorr.txt`):

```bash
mkdir -p build && cd build
cmake -DEXPLORE_STUFF=ON -DCMAKE_BUILD_TYPE=Release ..
cmake --build . --target OrnsteinUhlenbeckTimeline
./documentation/OrnsteinUhlenbeck/OrnsteinUhlenbeckTimeline ou_timelines.txt ou_autocorr.txt
```

Both outputs use `PyConPlot.py`'s `@New plot:` / `#group` text format, one `@New plot:`
section per sigma so each renders as its own subplot:

- `ou_timelines.txt`: one `#sigma=<value>` group of `t x` pairs (time in seconds) per
  section.
- `ou_autocorr.txt`: two groups per section, `#empirical` (measured from the same simulated
  run, post burn-in) and `#theoretical exp(-theta*tau)` (the closed-form decay), so they
  overlay directly in one subplot.

## Rendering the plots

From `documentation/OrnsteinUhlenbeck/` (the venv and `requirements.txt` below live here,
not in `build/`):

```bash
python3 -m venv .venv && source .venv/bin/activate
pip install -r requirements.txt

python3 ../Plot/PyConPlot.py -f ../../build/ou_timelines.txt -o ou_timeline.png \
    --labelx "time (s)" --labely "x" --width 2400 --height 200 --cols 1 --miny -0.2 --maxy 0.2

python3 ../Plot/PyConPlot.py -f ../../build/ou_autocorr.txt -o ou_autocorr.png \
    --labelx "lag tau (s)" --labely "ACF" --width 1200 --height 250 --cols 1 --miny -0.1 --maxy 1.05
```

`--cols 1` (added to `PyConPlot.py` for this) stacks the four subplots in a single column
top to bottom, instead of the tool's default roughly-square grid. `--width`/`--height` give
the timeline subplots a 12:1 aspect ratio - readable as one long timeline rather than a
squarer plot with heavy horizontal compression. `--miny`/`--maxy` fix one shared y-axis
range across all four subplots in each plot, rather than letting each auto-scale to its own
data - see below for why a shared range matters for the timelines specifically; the
autocorrelation is already naturally bounded to roughly `[-1, 1]` so a shared range there is
just for tidy, consistent axes rather than correctness.

## Why the timelines look the way they do

`OrnsteinUhlenbeckProcess::setSigma()` derives both the reversion rate `theta` and the mean
`mu` from `sigma` (`theta = sigma * 20 + 1`, `mu = sigma`) - one control moves speed and
amplitude together, by that class's own design (see its doc comment). Three consequences
worth knowing before reading the plots:

- Every timeline point is written relative to its own `mu` (`process.step() - sigma`), so
  all four are centred on zero for comparison. Left as-is, larger `sigma` would just shift
  the whole timeline up instead of visibly changing how much it moves.
- Higher `sigma` reverts *faster* but also injects *more* noise per step. The process's
  stationary variance is `sigma^2 / (2 * theta)`, which still grows with `sigma` here despite
  the faster pull-back - so the higher-speed timelines are both quicker to jitter and wider,
  not smaller. That is the real, if slightly counter-intuitive, behaviour of this exact
  class, not an artefact of the plot - and exactly why the timelines share one y-axis range:
  on auto-scaled axes, "wider" and "narrower" would look the same size and the actual
  amplitude difference would be invisible.
- Because speed and amplitude are entangled, the timeline plot alone can't really confirm
  *how much* faster a higher sigma reverts - only that it looks jitterier. The
  autocorrelation plot isolates that: its empirical curve's decay rate is a direct estimate
  of `theta`, independent of amplitude, and it can be compared against the exact theoretical
  curve. The first ~1s or so of each subplot should track its theoretical curve reasonably
  closely; beyond that the estimate gets progressively noisier since fewer independent
  sample pairs are available at longer lags in one finite run.
