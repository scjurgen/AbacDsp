# Example plugins

JUCE-based standalone/VST3/AU plugins under `examples/`, built with
`-DBUILD_FULL_PROJECT=ON` (see root `README.md`). Each plugin's JUCE plumbing
(processor, editor, parameter layout) is generated from a blueprint in
`JuceStandaloneGenerator/blueprints/`. The actual DSP algorithm is a separate,
hand-written class in that example's `src/impl/` (for example
`src/impl/TanpuraImpl.h`); the generator wires parameters to it but never
generates or overwrites it.

| Name                      | Type       | Lua | Samples | Custom UI                  | DSP                                                                                                                            |
|---------------------------|------------|-----|---------|-----------------------------|---------------------------------------------------------------------------------------------------------------------------------|
| Delay                     | Effect     |     |         |                             | modulated feedback delay, one-pole low/high/all-pass filters                                                                    |
| Dronesequencer            | Instrument | x   |         |                             | Karplus-Strong string ensemble, Ornstein-Uhlenbeck excitation drift, ADS envelope, biquad, FDN reverb                           |
| Groover                   | Effect     |     | x       |                             | sample-based drum kit playback (GrooveKit/GrooveDrumPlayer)                                                                     |
| Guisandbox                | Effect     |     |         | widget sandbox              | none (passthrough, UI-only)                                                                                                     |
| Looper                    | Effect     |     | x       | slice/loop clock displays  | transient slicing (FFT-based), beat/click sequencer, loop recorder, MIDI/pattern sequencer, sample-based drum kit for the click |
| Maxdiffuser               | Effect     |     |         | processing-size displays   | series allpass diffuser chain, FDN reverb, pitch shifting, biquad, waveshaping distortion                                       |
| Metronome                 | Effect     |     |         | beat/spectrogram displays  | beat sequencer, click generator, onset/timing analysis against the beat grid                                                    |
| Minireverb                | Effect     |     |         |                             | order-32 FDN reverb (Walsh-Hadamard mix), two in-tank pitch shifters, per-line hi/lo-pass filtering, vibrato                     |
| Pingsynth                 | Instrument | x   |         |                             | modal resonator (ringing bandpass) bank, FDN reverb, pitch detection for Lua-side tuning                                        |
| Plaingain                 | Effect     |     |         |                             | gain staging, biquad shelving filters, latency-compensation delay                                                               |
| Resonik                   | Effect     | x   |         |                             | resonator bank (biquad + SVF bandpass), multi-tap delay, pitch detection                                                        |
| Sampleplayer              | Instrument |     | x       |                             | sample playback (Ogg), samplerate conversion, pitch/hi/lo-pass block processors, FDN send reverb                                |
| Sampleplayertimestretched | Instrument |     | x       |                             | independent time-stretch/pitch sample playback, resampling pitch shifter                                                        |
| Spectraltap               | Effect     | x   |         |                             | comb resonator, multi-mode SVF and SVF bandpass filters, multi-tap delay, FDN reverb, pitch detection                           |
| Tanpura                   | Instrument |     |         |                             | Karplus-Strong string ensemble, Ornstein-Uhlenbeck excitation drift, ADS envelope, biquad, FDN send reverb                      |
| Tapelooper                | Effect     | x   | x       | tape display                | variable-speed sinc-interpolated tape delay, tape hysteresis, compressor, ring modulator, tremolo, pole-mixing filter, multi-tap delay, FDN reverb, sample-based drum kit, beat-locked loop timekeeping |

Here the list in alphabetical order.
## Delay
Feedback delay with host-synced or free time, low/high/all-pass filtering in
the feedback path, and delay-time modulation.

## Dronesequencer
Lua-scripted drone sequencer: a Karplus-Strong string ensemble driven by a
user script (note timing, pattern, per-voice detune), with its own script
editor and named-script/patch storage.

## Groover
Experimental midifile bases drum machine with humanize options.
Humanize are not random variations the shift timing and dynamics using some researched patterns
that humans do. You can force laid back rhythms.

## Guisandbox
UI/widget experimentation sandbox, not a real effect: audio passes through
mostly unchanged while the controls exist to try out dial/switch/gauge
layouts and behavior before they get used elsewhere.

## Looper
Records audio, quantizes the loop to whole bars, slices it, and plays the
slices back locked to a metronome click, with a concentric bar/loop clock
display. 

## Maxdiffuser
Diffuser delay chain: up to 100 modulated allpass delays in series, with
pre-delay, pitching, bulge size distribution and damping.

## Metronome
Settable-BPM click generator with a damped-sine tick, swing, bar-drop
patterns, host sync, and circular beat/spectrogram displays. An Analysis mode
listens to the input for onsets, measures their timing against the beat/subdivision
grid, and exports an HTML report (histograms per beat/subdivision, a hit timeline).

## Minireverb
Order-32 FDN reverb (`FdnTankBlockDelayWalshSIMD`/`FdnTankSpiced`) with
Walsh-Hadamard mixing, two in-tank pitch shifters, and low/high-pass shaping
per delay line.

## Pingsynth
A Lua-scripted modal resonator synth: each voice is a bank of ringing bandpass resonators,
excited by an impulse.


## Plaingain
Passthrough gain stage with low/high shelving, IO level metering, spectrogram
and a latency compensation control.

## Resonik
A bank of up to 100 parallel resonator chains (`BiquadResoBP` feeding
`SvfResoBP`), spread across a frequency range with distributed decay, gain
and delay, turning audio into ringing, decaying tones.

## Sampleplayer
Multi-voice sample player: per-voice level and pitch, solo, and a shared
reverb send with decay/shimmer controls.

## Sampleplayertimestretched
Sample player with independent pitch/time-stretching: position and playback
advance are controlled separately from pitch.

## Tanpura
Plucked-string simulation of the Indian drone instrument: a Karplus-Strong
string ensemble with a fixed pattern sequencer, per-string filter envelope,
and send reverb.

## Tapelooper
Another looper based on a variable speed tape loop with 3 Stereo Tracks and a Drumtrack.
Freely programmable effects chains for every track so you can e.g. do filter effects that morph in 
time.