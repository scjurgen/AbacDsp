# Documentation

Offline exploration, verification, and generator programs for `src/includes/`
algorithms - separate from `examples/`, which are JUCE plugins. Each
subfolder here does one or more of:

- **documentation** - verification plots that visualize and check a class's
  behavior against theory (spectral response, decay accuracy, pitch bend,
  distribution, ...), usually with a small C++ program that writes plain-text
  series data and a Python script (via `Plot/PyConPlot.py`) that renders it.
- **exploration** - an offline harness for tuning or testing an algorithm
  against real data (e.g. hand-labelled audio) outside of a plugin.
- **generation** - a tool that produces a `src/includes/` header from a
  design process (e.g. a Sollya polynomial fit, a windowed-sinc filter
  design), rather than consuming one.

See `DSP_MODULES_USED.md` for which `src/includes/` headers each program
here actually touches, including a few that no `examples/` plugin reaches at
all. See `TODO.md` for folders and algorithms still missing plots.

## Folders

| Folder | Kind | Covers |
|---|---|---|
| `Diffuser/` | documentation | `DiffuserDelayChain`: Schroeder vs. Direct allpass magnitude flatness, echo-density growth vs. element count |
| `Filters/BandpassImpulses/` | documentation, exploration | Resonant-bandpass family: `SvfResoBP`, `BiquadResoBP`, `BiquadResoBandPassParallel`, `BiquadResoBPParallelSIMD` - magnitude response, topology agreement, decay accuracy, resonance compensation |
| `Filters/PoleMixing/` | generation, documentation, exploration | `PoleMixingFilter`: `fitPoleMixingCorrections.py` generates `src/includes/Filters/PoleMixingCorrections_generated.h`; `PoleMixingExplore.cpp` plots magnitude/phase, resonance, saturator overdrive behavior |
| `Filters/SincFilterDesign/` | generation | Regenerates the windowed-sinc FIR tables in `src/includes/Filters/Sinc/` |
| `KarplusStrong/` | documentation | `KarplusStrongString`/`KarplusStrongVoice`: spectral brightness loss, decay-time accuracy, pitch bend behavior |
| `Metronome/` | generation | Click samples and MIDI pattern library for tapelooper's built-in groove, not a library-algorithm write-up |
| `Numbers/` | generation | Sollya setup for the minimax sin/cos polynomials in `src/includes/Numbers/Approximation.h` |
| `OrnsteinUhlenbeck/` | documentation | `OrnsteinUhlenbeckProcess`: mean-reverting noise timelines across `sigma` speeds |
| `Plot/` | shared tooling | `PyConPlot.py` - the matplotlib wrapper most other folders' plots are rendered with |
| `Reverbs/FDN/` | generation | Hadamard-matrix coefficients via SageMath, used to regenerate `src/includes/Reverbs/HadamardWalsh{4,8,16,32}.h` |
| `Slicer/` | exploration | Offline harness for tuning `Analysis::Slicer` against hand-labelled audio |
| `VelvetNoise/` | exploration | `Generators/RandomStyle/VelvetCrackle.h` explore program; no README yet - see `TODO.md` |
