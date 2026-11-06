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
| `Delays/` | documentation | One subfolder per class. `VariSpeedTapeDelay/`: wow/flutter baked into recorded pitch, octave-based transport-speed glide, write-side aliasing vs. ratio; `WobbleDelay/`: single-rate delay with Wow and Flutter on the read head, crossing clamp, retune glide, swept-tone spectrograms; `MultiTapDelay/`: whole-sample-only tap spacing |
| `ClassicBeats/` | generation | Original, copyright-free full-drumset MIDI grooves (Rock, Blues, Jazz Swing, Latin, Funk, Fusion, Hip-Hop, Progressive, Reggae, Pop, Soul) informed by aggregate statistics over reference packs, staged locally for later review before joining `MidiDrums/` |
| `Diffuser/` | documentation | `DiffuserDelayChain`: Schroeder vs. Direct allpass magnitude flatness, echo-density growth vs. element count |
| `DrumKit808/` | generation | Synthesizes a full TR-808-style drum kit into `samples/drums/808/` - a copyright-free alternative to a real recorded pack, see `samples/README.md` |
| `Filters/BandpassImpulses/` | documentation, exploration | Resonant-bandpass family: `SvfResoBP`, `BiquadResoBP`, `BiquadResoBandPassParallel`, `BiquadResoBPParallelSIMD` - magnitude response, topology agreement, decay accuracy, resonance compensation |
| `Filters/Biquads/` | documentation | `Biquad<type>` family magnitude/phase response, `ChebyshevBiquad` Type1/Type2, empirical stability under frequency/Q stress, `PeakBiquad` boost/cut symmetry |
| `Filters/OnePoleFilter/` | documentation | `OnePoleFilter`: magnitude response across all four characteristics, `setDecayTime()` exact timing, `AllPass` flat magnitude/90-degree-at-cutoff phase |
| `Filters/LinkwitzRileySplit/` | documentation, exploration | `LinkwitzRileySplit`: measured band magnitudes at four split frequencies, deviation from the bilinear closed form, and the sum against an `AllPass` biquad |
| `Filters/PinkFilter/` | documentation | `PinkFilter`: measured PSD vs. ideal -3dB/octave slope, 3-pole vs. 7-pole accuracy (Welch-averaged white-noise PSD) |
| `Filters/PoleMixing/` | generation, documentation, exploration | `PoleMixingFilter`: `fitPoleMixingCorrections.py` generates `src/includes/Filters/PoleMixingCorrections_generated.h`; `PoleMixingExplore.cpp` plots magnitude/phase, resonance, saturator overdrive behavior; `PeakTunedExplore.cpp` and the section "Two pole-mixing variants, and why both exist" in its README compare `PoleMixingFilter.h` with `PeakTunedPoleMixing.h` and show `CutoffTrajectory` |
| `Filters/SincFilterDesign/` | generation | Regenerates the windowed-sinc FIR tables in `src/includes/Filters/Sinc/` |
| `Generators/` | documentation | `Envelope` segment shapes: the polynomial `CurvedValue` against the exponential `ExponentialApproach` (best polynomial fit and its overshoot per number of time constants), and a pitch envelope in MIDI notes against the closed form |
| `KarplusStrong/` | documentation | `KarplusStrongString`/`KarplusStrongVoice`: spectral brightness loss, decay-time accuracy, pitch bend behavior |
| `MelColumns/` | documentation | `MelColumns`: the level of a tone against its frequency between band centres, the dB scale against the amplitude, the level of white noise per band against the triangle area |
| `Metronome/` | generation | Click samples and MIDI pattern library for tapelooper's built-in groove, not a library-algorithm write-up |
| `Modulation/` | documentation | `Wow`/`Flutter` LFO character (timeline, spectrum, Poincare orbit plot), `Tremolo` drive morph, `RingModulator` sidebands |
| `NoteSegmenter/` | documentation | `Analysis/NoteSegmenter`: onset, pitch and level of plucked notes against what was played, accuracy against noise, and the limit for a repeated note that rings on; `Analysis/NoteVerifier`: a real guitar recording checked against its score, with the onset, pitch and unexpected-sound results and how to add a recording |
| `Numbers/` | generation | Sollya setup for the minimax sin/cos polynomials in `src/includes/Numbers/Approximation.h` |
| `OrnsteinUhlenbeck/` | documentation | `OrnsteinUhlenbeckProcess`: mean-reverting noise timelines across `sigma` speeds; `ExactOrnsteinUhlenbeck`: autocorrelation and stationary distribution from 7 Hz to 2 kHz step rate, stability against an Euler-Maruyama step |
| `OverSampling/` | documentation | `UpDownSampler` wrapping `WobbleDelay`: swept-tone spectrograms at oversampling and undersampling ratios, delay time and latency vs. ratio, abrupt vs. slow ratio changes |
| `Parameters/` | documentation | `OnePoleSmoother`: step response against `1 - exp(-t / tau)`, and the distance to the target against `OnePoleFilter<LowPass>`, which stalls short of it |
| `Plot/` | shared tooling | `PyConPlot.py` - the matplotlib wrapper most other folders' plots are rendered with |
| `Reverbs/FDN/` | generation | Hadamard-matrix coefficients via SageMath, used to regenerate `src/includes/Reverbs/HadamardWalsh{4,8,16,32}.h` |
| `Reverbs/Tanks/` | documentation | `FdnTankGlide`/`FdnTankSpicedBase`/`FdnTankBlockDelayWalshSIMD`: late-tail spectral flatness, RT60 vs. frequency band, Schroeder energy decay curves |
| `Slicer/` | exploration | Offline harness for tuning `Analysis::Slicer` against hand-labelled audio |
| `VelvetNoise/` | exploration | `Generators/RandomStyle/VelvetCrackle.h` explore program; no README yet - see `TODO.md` |
| `WaveTableRingModulator/` | documentation | `WaveTableRingModulator`: carrier level against morph and pitch before and after the compensation, the sideband lines, carrier harmonics against theory, the harmonics against the morph, aliasing with the input lowpass open and closed, the response of that lowpass |
| `Wavetables/` | documentation | `WaveTableOscillator`/`WaveTableStorage`: mipmap band-limiting across the pitch range, at a mip-table boundary, under PWM and morph; `changeFrequency()` vs. `setFrequency()` aliasing comparison |
| `YinPitch/` | documentation | `StampedYinPitchDetector` and `PitchNoteTracker` over 40 Hz to 2 kHz: pitch error, time to the first note, noise, a guitar scale against its score; why `YinPitchDetector` and `StampedYinPitchDetector` both exist; the quiet-noise defect of `YinPitchDetector` |
