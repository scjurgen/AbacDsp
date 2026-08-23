# Example plugins

JUCE-based standalone/VST3/AU plugins under `examples/`, built with
`-DBUILD_FULL_PROJECT=ON` (see root `README.md`). Each plugin's JUCE plumbing
(processor, editor, parameter layout) is generated from a blueprint in
`JuceStandaloneGenerator/blueprints/`. The actual DSP algorithm is a separate,
hand-written class in that example's `src/impl/` (for example
`src/impl/TanpuraImpl.h`); the generator wires parameters to it but never
generates or overwrites it.

## Plaingain

Passthrough gain stage with low/high shelving, IO level metering, spectrogram
and a latency compensation control.

## Guisandbox

UI/widget experimentation sandbox, not a real effect: audio passes through
mostly unchanged while the controls exist to try out dial/switch/gauge
layouts and behavior before they get used elsewhere.

## Minireverb

Order-32 FDN reverb (`FdnTankBlockDelayWalshSIMD`/`FdnTankSpiced`) with
Walsh-Hadamard mixing, two in-tank pitch shifters, and low/high-pass shaping
per delay line.

## Maxdiffuser

Diffuser delay chain: up to 100 modulated allpass delays in series, with
pre-delay, pitching, bulge size distribution and damping.

## Delay

Feedback delay with host-synced or free time, low/high/all-pass filtering in
the feedback path, and delay-time modulation.

## Resonik

A bank of up to 100 parallel resonator chains (`BiquadResoBP` feeding
`SvfResoBP`), spread across a frequency range with distributed decay, gain
and delay, turning audio into ringing, decaying tones.

## Metronome

Settable-BPM click generator with a damped-sine tick, swing, bar-drop
patterns, host sync, and circular beat/spectrogram displays.

## Looper

Records audio, quantizes the loop to whole bars, slices it, and plays the
slices back locked to a metronome click, with a concentric bar/loop clock
display. 

## Sampleplayer

Multi-voice sample player: per-voice level and pitch, solo, and a shared
reverb send with decay/shimmer controls.

## Sampleplayertimestretched

Sample player with independent pitch/time-stretching: position and playback
advance are controlled separately from pitch.

## Dronesequencer

Lua-scripted drone sequencer: a Karplus-Strong string ensemble driven by a
user script (note timing, pattern, per-voice detune), with its own script
editor and named-script/patch storage.

## Tanpura

Plucked-string simulation of the Indian drone instrument: a Karplus-Strong
string ensemble with a fixed pattern sequencer, per-string filter envelope,
and send reverb.
