# Architecture

## Class design

- float based unless we really need extra precision for iterative algorithms or reference
- mostly templates based for adaptive code and better optimisations.
- classes should ctor with the samplerate
- blockoperations of BlockSize=8 or 16 Samples for better compiler optimisations
- operations on simple buffer design with interleaved or mono array
- raw float operations allowed but with BlockSize only
- testability

## Fixed internal sample rate in example plugins

Every generated example plugin (`examples/*/src/*Processor.h`) wraps its DSP implementation
in `AbacDsp::InternalRateNormalizingProcessor`
(`src/includes/SamplerateConverter/InternalRateNormalizingProcessor.h`), which sinc-resamples
host audio to/from a fixed `kInternalSampleRate = 48000.f` and is bypassed at zero cost when
the host already runs at 48 kHz. The DSP implementation classes themselves (e.g. `LooperImpl`)
therefore always see `sampleRate == 48000.f`, never the host's actual rate. Any bundled audio
asset authored at 48 kHz (samples, impulse responses, etc.) can be loaded and played back
verbatim by these implementations with no extra resampling.

## Submodules used

- googletest
- Audiofile
- juce v8
- pffft
- lua
- sol2

## What's in the library

- Analysis (FFT, Yin pitch detection, spectrogram, envelope follower, onset/transient
  slicing, zero-crossings, octave-band analysis)
- Audio buffers, fader and fixed-size block processor building blocks
- Delays, Diffuser and Reverbs (FDN with Hadamard mixing)
- Filters (biquad, ladder, SVF bandpass, one-pole)
- Generators (naive and band-limited), plus a beat sequencer and metronome click generator
- Modulation (wow/flutter)
- Non-linear (hysteresis / saturation)
- Parameter smoothing and ramping
- Sampler: loop recorder, beat-locked slice player, sample playback,
  pitch/time-stretching and sample-rate conversion
- Spectral processing and Wavetables
- Numbers: math/conversion helpers (interpolation, easing, dB/frequency)
- WAV/OGG file I/O

## Usage

For usage check always the unit-tests or examples, these contain implementations that should
cover and which should be self-explanatory.
