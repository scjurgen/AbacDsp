# Web references

External sources cited from `@see` tags in `src/includes/`, grouped by topic.
Every URL here resolves (checked HTTP 200). Verify with:

```bash
dev-scripts/dev-check-urls.sh              # everything in src/includes and this file
dev-scripts/dev-check-urls.sh URL [URL...] # a candidate, before citing it
```

Check a link **before** citing it, not after. One dead link was already caught
that way: `ccrma.stanford.edu/~jos/pasp/Vibrato.html` is a 404 and was replaced
with `Vibrato_Simulation.html`. Two more in existing source comments were found
dead and repointed (see the notes at the end).

## Standing sources

Stable, broad, and worth knowing about independently of any one class.

| Source | Scope |
| --- | --- |
| [Julius O. Smith, online books](https://ccrma.stanford.edu/~jos/) | Four volumes on digital filters, physical audio signal processing, spectral audio signal processing and the mathematics of the DFT. The default citation target here. |
| [Audio EQ Cookbook](https://webaudio.github.io/Audio-EQ-Cookbook/audio-eq-cookbook.html) | Robert Bristow-Johnson's biquad design formulas, in the W3C-hosted copy that has outlived several earlier URLs. |
| [DAFx paper archive](https://www.dafx.de/paper-archive/) | Proceedings of the International Conference on Digital Audio Effects, free PDFs back to 1998. |
| [AES E-Library](https://aes2.org/publications/elibrary/) | Paywalled, so citations give author, title and year in the text and the URL only as a convenience. |
| [musicdsp.org](https://www.musicdsp.org/) | Long-running archive of DSP snippets and derivations. |

## Filters

| Reference | Cited from |
| --- | --- |
| [One-pole filters (JOS)](https://ccrma.stanford.edu/~jos/filters/One_Pole.html) | `OnePoleFilter.h`, and by delegation `BlockProcLowpass.h`, `BlockProcHighpass.h` |
| [Transposed direct forms (JOS)](https://ccrma.stanford.edu/~jos/filters/Transposed_Direct_Forms.html) | `Biquad.h` |
| [Audio EQ Cookbook](https://webaudio.github.io/Audio-EQ-Cookbook/audio-eq-cookbook.html) | `Biquad.h`, `PeakBiquad` |
| [Chebyshev filter](https://en.wikipedia.org/wiki/Chebyshev_filter) | `ChebyshevBiquad` |
| [Cytomic SVF, linear trapezoidal optimised](https://cytomic.com/files/dsp/SvfLinearTrapOptimised2.pdf) | `SvfResoBP.h`. Andrew Simper's derivation of the zero-delay-feedback state variable filter. |
| [Electronotes EN85, voltage controlled filters](http://electronotes.netfirms.com/EN85VCF.pdf) | `PoleMixingFilter.h`, the original pole-mixing writeup |
| [Expedition Electronics, pole mixing](https://expeditionelectronics.com/Diy/Polemixing) | `PoleMixingFilter.h` |
| [Pink noise filter (musicdsp)](https://www.musicdsp.org/en/latest/Filters/76-pink-noise-filter.html) | `PinkFilter.h`, Paul Kellett's pole sets |
| [Pink noise](https://en.wikipedia.org/wiki/Pink_noise) | `PinkFilter.h` |
| [Memoryless nonlinearities (JOS)](https://ccrma.stanford.edu/~jos/pasp/Memoryless_Nonlinearities.html) | `Distortion.h` |
| [Digital audio resampling (JOS)](https://ccrma.stanford.edu/~jos/resample/) | `Sinc/SincFilter.h` |
| [Modal representation (JOS)](https://ccrma.stanford.edu/~jos/pasp/Modal_Representation.html) | `BiquadResoBandPassParallel.h`, `ResoGenerator.h` |
| [AoS and SoA](https://en.wikipedia.org/wiki/AoS_and_SoA) | `BiquadResoBPParallelSIMD.h`, `ResoParallelSIMD.h` |

## Delays and modulation

| Reference | Cited from |
| --- | --- |
| [Time-varying delay effects (JOS)](https://ccrma.stanford.edu/~jos/pasp/Time_Varying_Delay_Effects.html) | `FracReadHead.h`, `ModulationDelay.h`. Why a moving read head shifts pitch. |
| [Vibrato simulation (JOS)](https://ccrma.stanford.edu/~jos/pasp/Vibrato_Simulation.html) | `BlockProcVibrato.h` |
| [Dispersion (JOS)](https://ccrma.stanford.edu/~jos/pasp/Dispersion.html) | `DispersionDelay.h`, allpass-induced frequency-dependent delay |
| [Wow and flutter](https://en.wikipedia.org/wiki/Wow_and_flutter) | `VariSpeedTapeDelay.h`, `Flutter.h`, `Wow.h` |

## Diffusion and reverb

| Reference | Cited from |
| --- | --- |
| [Schroeder allpass sections (JOS)](https://ccrma.stanford.edu/~jos/pasp/Schroeder_Allpass_Sections.html) | `SchroederAllpass.h`, `SchroederDiffuser.h` |
| [FDN reverberation (JOS)](https://ccrma.stanford.edu/~jos/pasp/FDN_Reverberation.html) | `FdnTankRef.h`, `FdnReverb.h`, `FdnTankGlide.h` |
| [Hadamard matrix](https://en.wikipedia.org/wiki/Hadamard_matrix) | `HadamardFeed.h`, on why an orthogonal mix makes decay a single scalar |
| [Interaural time difference](https://en.wikipedia.org/wiki/Interaural_time_difference) | `FdnTankSpicedBase.h`, on the 0.33 ms ceiling for stereo taps |

## Spectral and analysis

| Reference | Cited from |
| --- | --- |
| [Phase vocoder (JOS)](https://ccrma.stanford.edu/~jos/sasp/Phase_Vocoder.html) | `PhaseVocoderPitcher.h`, `StretchedSampleProducer.h` |
| [Short-time Fourier transform (JOS)](https://ccrma.stanford.edu/~jos/sasp/Short_Time_Fourier_Transform.html) | `SpectrogramBase` |
| [Mathematics of the DFT (JOS)](https://ccrma.stanford.edu/~jos/mdft/) | `BasicFFT` |
| [Welch's method](https://en.wikipedia.org/wiki/Welch%27s_method) | `FFTResponse` |
| [Window function](https://en.wikipedia.org/wiki/Window_function) | `Excitation.h` |
| [Mel scale](https://en.wikipedia.org/wiki/Mel_scale) | `MelSpectroGram` |
| [Constant-Q transform](https://en.wikipedia.org/wiki/Constant-Q_transform) | `OctaveBandAnalyzer.h` |
| [Hidden-line removal](https://en.wikipedia.org/wiki/Hidden-line_removal) | `FloatingHorizonFFTImage` |
| [kissfft](https://github.com/mborgerding/kissfft) | `KissFft` |

Cited by author and year rather than URL, being paywalled or journal-hosted:

- de Cheveigne and Kawahara, "YIN, a fundamental frequency estimator for speech
  and music", JASA 111(4), 2002 - `YinPitchDetector.h`
- Bello et al., "A Tutorial on Onset Detection in Music Signals", IEEE Trans.
  Speech and Audio Processing 13(5), 2005 - `Slicer.h`
- Valimaki, Holm-Rasmussen, Alary, Lehtonen, "Late Reverberation Synthesis Using
  Filtered Velvet Noise", Applied Sciences 7(5), 2017 - `VelvetCrackle.h`

## Generators, wavetables and numerics

| Reference | Cited from |
| --- | --- |
| [A wavetable oscillator (earlevel)](https://www.earlevel.com/main/2012/05/04/a-wavetable-oscillator-part-1/) | `WaveTableStorage.h`, `WaveTableOscillator.h` |
| [Aliasing](https://en.wikipedia.org/wiki/Aliasing) | `NaiveGenerators/Generator.h` |
| [Additive synthesis](https://en.wikipedia.org/wiki/Additive_synthesis) | `HarmonicGenerator.h` |
| [Ornstein-Uhlenbeck process](https://en.wikipedia.org/wiki/Ornstein%E2%80%93Uhlenbeck_process) | `OrnsteinUhlenbeckProcess.h` |
| [Julia set](https://en.wikipedia.org/wiki/Julia_set) | `JuliaWalk.h` |
| [Elephant, polynomial interpolators (Niemitalo)](https://yehar.com/blog/wp-content/uploads/2009/08/deip.pdf) | `Interpolation.h`, `MultichannelInterpolation.h`. Source of the optimal least-squares kernels. |
| [Decibel](https://en.wikipedia.org/wiki/Decibel) | `Convert.h` |
| [Hysteresis](https://en.wikipedia.org/wiki/Hysteresis) | `SimpleHysteresis.h` |
| [vorbisfile API](https://xiph.org/vorbis/doc/vorbisfile/) | `LoadOgg.h` |

## Notes on link rot

- `ccrma.stanford.edu/~jos/pasp/Vibrato.html` - 404. Use `Vibrato_Simulation.html`.
- `ccrma.stanford.edu/~jos/pasp/Feedback_Delay_Networks.html` - 404. Use
  `FDN_Reverberation.html`.
- `expeditionelectronics.com/Diy/Polemixing/math` - 404; was cited in
  `PoleMixingFilter.h` before this pass. The parent page is live and now cited
  instead. The derivation itself survives in the Wayback Machine: prefix the
  dead URL above with `web.archive.org/web/2023/`.
- `firstpr.com.au/dsp/pink-noise/` - the canonical Kellett writeup, but the host
  did not respond when checked. The musicdsp mirror is cited instead.
