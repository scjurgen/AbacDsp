# Performance suite

Single-thread capacity benchmark for `src/includes/`: how many instances of each DSP
module family this machine can run in one thread, in real time. Answers questions like
"how many reverbs" or "how many Biquads" fit in one audio thread before missing the
callback deadline.

## What it measures

One category per module family, each with one or more concrete variants:

| Category              | Variants                                                         |
|------------------------|--------------------------------------------------------------------|
| Reverb                 | `FdnTankGlide`, `FdnTankSpicedBase`, `FdnTankBlockDelayWalshSIMD`, each at order 8, 16, and 32 |
| Biquad                 | LowPass, BandPass, Peak                                            |
| PoleMixingFilter        | `Lp24Smooth` (4-pole resonant lowpass)                              |
| Wavetables              | `WaveTableOscillator`                                               |
| Generators              | `KarplusStrongVoice` (plucked string + resonant VCF)                |
| Synthesizer             | `MorphexsynthVoice` (the full ported synth voice)                   |
| Diffuser                | `DiffuserDelayChain`, a 4-stage Schroeder chain                     |
| PitchShift              | `BlockProc::Pitch`, both engines: crossfade delay and phase vocoder |
| Analysis                | `YinPitchDetector`                                                  |
| SamplePlayer            | `SamplePlayerBasic`                                                 |
| SampleRateConversion    | `SrPushConverter` upsample 44.1kHz->48kHz and downsample 48kHz->44.1kHz |

Every category is fed real audio: a fixed-seed white-noise block (or, for the sample
player, a longer noise vector it loops internally), never silence. Every category with a
tunable parameter (filter cutoff, oscillator pitch, playback rate) sweeps it once per
block via a slow control-rate LFO, so steady-state cost is not the whole story - parameter
recompute is measured too. The reverb tanks keep their own always-on delay-line
modulation, so no extra sweep is needed there; their delay lines are sized for roughly
0.4-1.0 seconds (up from a first pass that used a few tens of milliseconds), so the
`max instances` numbers reflect genuinely large reverb tails, not a token configuration.
The two voice-like categories (`KarplusStrongVoice`, `MorphexsynthVoice`) re-trigger a
note periodically rather than sweeping a parameter, since neither has a natural
per-block-modulated control and both keep running their full cost regardless of envelope
state (confirmed by reading their code - neither has an idle/silence shortcut).

## Methodology

- One shared sample rate (48 kHz) and block size (128 samples, ~2.7 ms) across every
  category, so results are directly comparable.
- Single-instance timing: after a calibration pass, three timed probes (`--seconds` each,
  default 0.3 s), median taken, to get a stable ns/block, samples/sec, and realtime
  multiple.
- Max-instances-in-one-thread: doubles the instance count K while the *worst* single pass
  over all K instances still finishes inside one block period (`128 / 48000` s), then
  binary-searches the exact boundary. Using the worst observed pass, not the average, is
  what makes this a real-time-safety number rather than a throughput average. The search
  stops at a safety cap (100000 by default); a `+` after the number in the report means
  the cap was hit before real time broke, not that no more would have fit.
- `bytes/instance` is `sizeof()` of the benchmarked object; it does not include internal
  heap buffers a class may hold (relevant for the sample-rate converters, whose shared
  sinc kernel is amortized across all instances and not counted per instance at all).

## Building and running

Must run as a Release build - a Debug build's timings are meaningless (the report warns
if it detects one, but a Release run is the point).

Build-only compile check via the project's own tests-only build:

```
./dev-scripts/dev-test-full.sh PerformanceSuite -- "^$"
```

For a real, timed run, build a Release configuration directly (no dev-script targets a
Release full-project build dir):

```
cmake --build cmake-build-release --target PerformanceSuite
cd documentation/Performance && ../../cmake-build-release/documentation/Performance/PerformanceSuite --comment "..."
```

Or point `--out` at any path; the default is `generated/performance_report.html` relative
to the current directory, so run it from `documentation/Performance/` (which already has
a `generated/` folder, gitignored) or pass `--out` explicitly. Run `--help` for the full
flag list.
