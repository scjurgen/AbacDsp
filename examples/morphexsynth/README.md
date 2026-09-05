# Morphexsynth

An MPE subtractive synth: three wavetable oscillators through a resonant pole-mixing filter,
amp/filter/pitch envelopes, an LFO, a distortion stage, and a 10-slot MPE routing matrix that
lets any of X (pitch bend)/Y (timbre)/Z (pressure)/velocity/note/envelope amount feed any of a
dozen-plus destinations, shaped through a curve and scaled by depth. Ported from modabacad's
`NubuNiif`.

The C++ voice is a complete instrument on its own - the dials below already play a useful
sound. Lua scripting (see below) customizes it further: oscillator waveform/tuning, envelope
times, filter type, the LFO, distortion, the whole MPE routing matrix, and all three
master-bus effects (phaser, chorus, reverb) are all Lua-only, not blueprint dials, per this
project's minimal-dial-Lua-first-UI convention.

## Controls

| Control | Range | Description |
|---|---|---|
| Vol | -100 - 12 dB | Output level |
| Cutoff | 0 - 127 | Filter cutoff, MIDI-note-ish scale (matches `SetFilter`'s own `cutoff` field) |
| Reso | 0 - 120 % | Filter resonance (0 none, 100 near self-oscillation) |
| Script | (button) | Opens the popup editor for the current patch's script. The editor's own Reset button replaces the text with a full skeleton (every available hook, stubbed out) - Cancel discards it, Apply commits it. A dropdown in the editor also lets you view any installed library script, always read-only. |

**Settings > Scripts** manages a named pool of saved scripts (Load / Save As / Delete / Rename),
separate from the script embedded in the current patch. **Settings > Patches** saves/loads full
patches, including whichever script is currently applied.

A script can also pull in a shared library script with `import "name"` (see `../../LUA.md`) -
useful for boilerplate reused across several patches. Built-in libraries live in this repo's
`base-scripts/` folder and are synced to disk on every launch; your own go alongside them in
`Library/User/`, under the same per-app data directory as the Scripts pool above. The script
editor's dropdown (see the Script control above) lets you view any of them read-only.

## MPE

Morphexsynth is MPE-aware at the voice-pool level, not just per-note-pitch-bend: a 12-voice pool
is gated to a configurable zone (a master channel plus a `lower..upper` range of member
channels - the default is the conventional MPE Lower Zone, master channel 1, members 2-16), and
any note-on outside that zone is ignored. Once a note lands, its channel's pitch bend, CC74
(timbre)/mod wheel, and channel pressure route through whichever of the 10 MPE routing-matrix
slots a script has pointed at that dimension - `SetCtrlSlot` below. There is no fixed
X-to-pitch-bend/Y-to-cutoff/Z-to-amplitude wiring baked into the C++ engine the way many
synths hardcode it; every dimension is patchable to every destination, or to nothing at all.

Voice stealing is oldest-by-age: a free voice is reused first, otherwise the voice assigned
longest ago is retriggered. There is no voice-freeze and no overflow queue - a note arriving
with every voice busy always steals immediately, relying on the amplitude envelope's own
ramp-from-current-gain behavior (not a hard jump) as its declick.

## Scripting

This section covers what's specific to Morphexsynth. For everything shared with any other
Lua-scripted example - MIDI handlers, `OnStart`/`OnStop`, dynamic UI parameters, the
`Music`/`Vel`/`Rr`/`Rhythm` helper library, `Timer`, `Transport`, the available stdlib
functions, and sandbox/error-handling notes - see `../../LUA.md`.

Morphexsynth has no Play switch or Host Sync concept (unlike most `../../LUA.md`-documented
`OnStart`/`OnStop` users): `OnStart()` fires exactly once, right after the plugin's engine is
constructed - the usual place to set a patch's static configuration. `OnStop()` is never fired.

**Every `SetXxx()` call below must happen from inside a hook** (`OnStart`, `OnNoteOn`, a
`UICreateParameterSet` callback, a `Timer` callback, ...) - never at the script's own top
level. A fresh script's custom bindings aren't wired up until after its own top-level code has
already run once, so a top-level `SetXxx()` call in the very script that would define it always
fails; every C++ engine here works this way (see `base-scripts/*.lua` for the pattern).

`OnNoteOn` runs, and anything it sets is applied, *before* the note it fires for actually
triggers - a per-note customization (see `mpe-expressive-lead.lua`'s velocity-scaled filter
envelope) reaches the note that caused it, not the next one.

### Oscillators

```lua
SetOscillator(index, { waveform = 0, detune = 0, level = 0, pwm = 0, pitchFactor = 1 })
```

`index` is `0`, `1`, or `2` (the three oscillators). `waveform`: `0` Triangle, `1` SharkFin,
`2` Saw, `3` Square, `4` White noise - or `import "constants"` and use `OscWaveform.Triangle`
etc. (see `base-scripts/constants.lua`). `detune` is semitones; `level` is `-1..1`; `pwm` is
`-1..1` (a phase-distortion pulse-width effect, not a literal duty-cycle change); `pitchFactor`
is a frequency multiplier relative to the note played (`1` = unison, `2` = an octave up, `0.5`
an octave down).

### Envelopes

```lua
SetAmpEnvelope({ attackMs = 10, decayMs = 200, sustainLevel = 0.5, releaseMs = 100 })
SetFilterEnvelope({ attackMs = 10, decayMs = 200, sustainLevel = 0.5, releaseMs = 100, contour = 0 })
SetPitchEnvelope({ attackMs = 0, decayMs = 0, depthSemitones = 0, glideMsPerOctave = 0 })
```

`SetAmpEnvelope`/`SetFilterEnvelope` are both plain ADSR shapes. `SetFilterEnvelope`'s
`contour` (`-4..4`, same scale as `SetLfo`'s `filterDepth`) is how much this envelope
additionally sweeps the filter cutoff as it runs - positive opens the filter as the
envelope rises, negative closes it; `0` (the default) leaves cutoff untouched, so
`SetFilterEnvelope` without `contour` only shapes what the MPE routing matrix's own
`EnvelopeFilter` source reads (see `SetCtrlSlot` below), not the cutoff directly.
`SetPitchEnvelope` is a two-stage attack/decay pitch envelope (`depthSemitones` is how far
it swings, positive or negative) plus `glideMsPerOctave` - portamento time between
consecutive notes, scaled by how far apart they are (0 disables glide).

### LFO

```lua
SetLfo({ waveform = 0, speedHz = 1, filterDepth = 0, oscDepth = 0, keyFollow = 0 })
```

`waveform`: `0` Sine, `1` Triangle, `2` Saw, `3` Square, `4` Noise, `5` SampleHoldNoise,
`6` SampleHoldFlipFlop, `7` BrownNoise - or `import "constants"` and use `LfoWaveform.Sine`
etc. `filterDepth` scales how many semitones of filter
cutoff modulation the LFO contributes. `oscDepth` scales how many semitones of pitch
modulation the LFO contributes, applied uniformly to all 3 oscillators (the same bus MIDI
pitch bend and `SetPitchEnvelope` use, so it stacks with both rather than targeting any one
oscillator). `keyFollow` scales how much the LFO's own rate rises with the played note (0 =
fixed rate regardless of note).

### Filter and distortion

```lua
SetFilter({ cutoff = 72, resonance = 0, type = "LP4" })
SetDistortion(presetIndex)
```

`cutoff` matches the Cutoff dial's own scale; `resonance` is `0..~1.2` (1 is near
self-oscillation); `type` is a preset name from `PoleMixingFilter.h`'s `poleMixingList` - e.g.
`"LP4"` (24 dB lowpass, the default), `"HP2"`, `"BP4"`, `"Notch"` - see that file for the full
~48-entry set. `SetDistortion(0)` bypasses the distortion stage; `1` and up select a 1-indexed
preset from `WaveShaperTables.h`'s `kDistortionWaveTableSet` (30 presets - "classic silicon"
through "broken amp 5").

### MPE routing matrix

```lua
SetCtrlSlot(slot, { source = 0, curve = 2, target = 0, valueType = 0, depth = 0 })
```

`slot` is `0..9` - one of 10 independent routing entries. Each slot reads one `source`, shapes
it through `curve`, scales by `depth`, and adds the result into `target`. Multiple slots can
target the same destination; their contributions sum. `import "constants"` gives named
alternatives for all four enum fields below (`CtrlSource`, `CtrlCurve`, `CtrlValueType`,
`CtrlTarget` - see `base-scripts/constants.lua`).

| Field | Values |
|---|---|
| `source` | `0` X (pitch bend), `1` Y (CC74/timbre), `2` Z (channel pressure), `3` Velocity, `4` Note, `5` EnvelopeFilter, `6` EnvelopeAmplitude |
| `curve` | `0` CubeRoot, `1` SquareRoot, `2` Linear, `3` Square, `4` Cube |
| `valueType` | `0` Abs (one-directional, e.g. Velocity), `1` BiPolar (signed deflection around center, e.g. pitch bend) |
| `target` | `0` None, `1` PitchBend, `2` PitchBendSecondary, `3` FilterCutoff, `4` FilterResonance, `5` Pan, `6` OscFreq2, `7` OscFreq3, `8` OscLevel1, `9` OscLevel2, `10` OscLevel3, `11` LfoLevel (unused), `12` SustainVol, `13` AttackTime, `14` DecayTime, `15` ReleaseTime, `16` Distortion |

### MPE zone

```lua
SetMpeZone(masterChannel, lowerChannel, upperChannel)  -- 1-indexed MIDI channels
```

Reconfigures which channels the voice pool accepts note-on/CC/pitch-bend/pressure from (see
MPE above). Default: `SetMpeZone(1, 2, 16)`, the standard MPE Lower Zone.

### Master effects

```lua
SetPhaser({ rateHz = 0.3, depth = 0.5, feedback = 0, mix = 0.5 })
SetChorus({ rateHz = 0.6, depth = 0.5, mix = 0.5 })
SetReverb({ sizeMeters = 12, decayMs = 1500, dryDb = 0, mixDb = -100 })
```

All three run on the final stereo mix, after every voice - not per voice - in the order
phaser -> chorus -> reverb. Each defaults to off at startup (phaser/chorus `mix = 0`,
reverb `mixDb = -100`), so an existing patch's sound is unaffected until a script turns one
on.

`SetPhaser`: 8 allpass poles total (two 4-pole stages in series per channel, both channels
swept by one shared LFO - a deliberately mono sweep, not a stereo-width one). `rateHz` is
`0.01..10`; `depth` (`0..1`) is how far the sweep spans a fixed 200 Hz..2 kHz range; `feedback`
(`0..~1.1`) is resonance around the allpass chain, sharpening the notches as it approaches
self-oscillation; `mix` is `0` dry to `1` fully phased.

`SetChorus`: one modulated delay line per channel, the right channel phase-offset from the
left for stereo width. `rateHz` is `0.01..8`; `depth` is `0..1`; `mix` is `0` dry to `1` fully
wet.

`SetReverb`: an order-32 FDN. `sizeMeters` (`1..60`) sets the room size (internally spread
`sizeMeters / 2.3 .. sizeMeters * 2.3` across the 32 delay lines); `decayMs` is `0..20000`;
`dryDb`/`mixDb` are independent dry and wet levels in dB (`-100..12`), not a single crossfade
- so both can be turned up together for a wet+dry blend, unlike `SetPhaser`/`SetChorus`'s
single `mix`.

### Example scripts

`base-scripts/mpe-expressive-lead.lua` routes all three MPE dimensions somewhere audible and
shapes the filter envelope per note from velocity - a fuller demonstration of the routing
matrix and `OnNoteOn` customization. `base-scripts/wobble-bass.lua` is a static patch (all
setup in `OnStart`, no per-note scripting) with a square-wave LFO wobbling the filter and a
touch of distortion.
