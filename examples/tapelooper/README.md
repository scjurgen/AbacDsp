# Tapelooper

A varispeed 3-track tape recorder (A, B, C) plus a parallel groove track, all riding one
shared transport speed. Each tape track records and plays back independently - the read head
trails the write head by a fixed loop length (bars at the current BPM), and every block reads
before it writes, so recording overdubs onto whatever is already looping. The groove track
plays a loaded MIDI groove (or a Lua-scripted click, see Scripting below) through the same
varispeed transport, so it pitches and stretches along with the tape tracks instead of playing
at a fixed rate.

## Controls

| Control | Range | Description |
|---|---|---|
| Tape Speed | 0.25 - 4.0x | Shared transport speed for every track (tape + groove) |
| Bars | 1 - 32 | Loop length, at the current BPM |
| Input Level | -60 - 12 dB | Live input gain, before it reaches the tape tracks |
| Groove Level | -60 - 12 dB | Groove track output level |
| Rec/Play/Clear A, B, C | (switches) | Per-track record, play, and clear (momentary) |
| Track Gain A, B, C | -60 - 12 dB | Per-track playback level - a fader, not a mute; Play stays the hard on/off |
| Groove | (switch) | Play/stop the groove track |
| BPM | 50 - 250 | Groove/click tempo |
| Groove Var | 0 - 31 | Selects among the loaded style's variations |
| Script | (button) | Opens the popup editor for the current patch's script. The editor's own Reset button replaces the text with a full skeleton (every available hook, stubbed out) - Cancel discards it, Apply commits it. |

Everything else - filter, reverb send/size/decay, wow/flutter, drive/distortion, chorus,
echo, compressor, ring mod, and tremolo - is Lua-only: there's no dial for it, only the
functions documented below. This keeps the UI to what's essential (transport and the
record/play basics) while every sound-shaping parameter stays reachable and automatable
from a script.

**Settings > Scripts** manages a named pool of saved scripts, separate from the script embedded
in the current patch. **Settings > Patches** saves/loads full patches, including whichever
script is currently applied. **Settings > Groove** picks the loaded MIDI groove style/variation.

A script can also pull in a shared library script with `import "name"` (see `../../LUA.md`) -
useful for boilerplate reused across several patches. Built-in libraries live in this repo's
`base-scripts/` folder and are synced to disk on every launch; your own go alongside them in
`Library/User/`, under the same per-app data directory as the Scripts pool above. The script
editor's dropdown lets you view any of them read-only.

## Scripting

This section covers what's specific to Tapelooper: transport/track control and the groove/click
source switch. For everything shared with any other Lua-scripted example - MIDI handlers,
`OnStart`/`OnStop`, dynamic UI parameters, `Timer`, `Transport`, and sandbox/error-handling
notes - see `../../LUA.md`.

### Transport and track control

Take effect on the next audio block:

```lua
SetTapeSpeed(ratio)                 -- 1.0 is nominal, matches the Tape Speed dial's range
SetBpm(bpm)                         -- groove/click tempo
SetGrooveVariation(index)           -- 0-based, picks among the loaded style's variations
SetTrackRecord(track, isRecording)  -- track is 0-based: A=0, B=1, C=2
SetTrackPlay(track, isPlaying)
SetTrackGain(track, gain)           -- linear multiplier: 0 silent, 1 unity, ~4 is the
                                     -- Track Gain dial's own +12 dB ceiling
```

Each of these writes into the same underlying state the plugin's own dials/switches do, so
whichever - host automation or script - sets a value last wins; there's no separate "script
override" layer to fight with the UI. `SetTrackGain` ramps linearly to its new value over one
audio block, so repeated calls (e.g. from `Timer.Every`) fade smoothly instead of zippering.

### Track filter

```lua
SetTrackFilter(track, cutoffHz, resonance)             -- mode defaults to "LP4"
SetTrackFilter(track, cutoffHz, resonance, modeName)
```

`resonance` is normalized: 1.0 is the measured self-oscillation threshold at the current
cutoff. `modeName` isn't limited to a curated `"LP4"`/`"HP4"`/`"BP4"`/`"Notch"` subset - any
name from `AbacDsp::poleMixingList` (`Filters/PoleMixingFilter.h`) works, e.g. `"AP2"` or
`"BP Notch"`. An unrecognized name is ignored and the track's filter mode stays whatever it
was - cutoff and resonance still apply.
Both cutoff and resonance ramp smoothly on their own (the filter class smooths them
internally), so `Timer.Every`-driven sweeps don't need any extra smoothing in the script.

### Reverb send

```lua
SetTrackReverbSend(track, amount)  -- 0 dry, 1 fully sent to that track's reverb
SetReverbSize(meters)              -- shared by every track's reverb
SetReverbDecay(ms)                 -- shared by every track's reverb
```

Each track has its own `AbacDsp::FdnTankGlide` reverb instance - independent tails, no
cross-track bleed through a shared bus - but Size and Decay tune all three together, tapped
post-filter at each track's own send level (0 is dry, regardless of size/decay). Each track's
reverb keeps ringing on its own momentum after that track's send drops to 0 or Play stops,
the way a real room does.

### Wow and flutter

```lua
SetTrackWow(track, depth, rate, drift)  -- slow speed-drift character
SetTrackFlutter(track, depth, rate)     -- fast speed-irregularity character
```

Drives `VariSpeedTapeDelay`'s own wow/flutter model directly. `depth` is 0-1, `rate` is in
Hz, `drift` (wow only) is 0-1. All default to a small nonzero amount (a plain patch already
sounds like tape, not digitally locked); `SetTrackWow(track, 0, 0, 0)` /
`SetTrackFlutter(track, 0, 0)` removes it entirely.

### Drive

```lua
SetTrackDrive(track, amount)  -- 0 clean (exact bypass), 1 fully hysteresis-distorted
```

Runs the track through `AbacDsp::SimpleHysteresis`: asymmetric attack/decay rates that
depend on signal direction, the classic tape-saturation non-linearity. 0 is an exact
per-sample identity, not just "low amount".

### Chorus

```lua
SetTrackChorus(track, depth, rateHz)  -- depth (0-1) also sets the wet/dry mix
```

A short modulated delay (`AbacDsp::ModulationDelayNoFeedback`) blended in by `depth` - 0 is
fully dry. `rateHz` is the modulation LFO speed.

### Echo

```lua
SetTrackEcho(track, divisionIndex, feedback)  -- feedback (0-1) also sets the send level
```

A single BPM-synced tap (`AbacDsp::MultiTapDelay`) with manual feedback: each repeat is the
previous one scaled by `feedback`, and `feedback` doubles as the send level, so 0 means no
echo at all rather than "one repeat at full volume". `divisionIndex` is 0-based into:

| Index | 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 | 10 | 11 | 12 |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| Division | 1/1 | 1/2 | 1/2. | 1/2T | 1/4 | 1/4. | 1/4T | 1/8 | 1/8. | 1/8T | 1/16 | 1/16. | 1/16T |

### Compressor

```lua
SetTrackCompressor(track, thresholdDb, ratio, attackMs, releaseMs)  -- ratio 1 = off
```

A feedforward peak compressor (`AbacDsp::Compressor`) with a fixed 3 dB soft knee and no
makeup gain. `ratio` of 1 is a mathematically exact no-op regardless of threshold.

### Ring mod

```lua
SetTrackRingMod(track, freqHz, mix)  -- mix 0 dry, 1 fully ring-modulated
```

Multiplies the track by an audio-rate sine carrier (`AbacDsp::RingModulator`), producing
sum/difference sidebands instead of the original pitch - classic bell/metallic tones.

### Tremolo

```lua
SetTrackTremolo(track, rateHz, depth, drive)  -- drive squares the LFO toward a hard gate
```

Sine-LFO amplitude modulation (`AbacDsp::Tremolo`). At `depth` 0 it's an exact unity gain
regardless of `drive`. Raising `drive` pushes the LFO through a tanh waveshaper, morphing it
from a sine toward a near-square wave - full `drive` and `depth` is a hard on/off "stutter".

### Example: track C effects presets

Three directly-runnable smoke tests for the effects above, each self-contained -
`base-scripts/lofi-tape-fx-track-c.lua` (drive + chorus + echo, a warped-tape character),
`base-scripts/modulation-fx-track-c.lua` (compressor + ring mod + tremolo), and
`base-scripts/digital-clean-track-c.lua` (wow/flutter to 0, removing tape's default
speed-drift). Record something onto track C and play it back to hear any of them.

### Groove source

```lua
SetGrooveSource(mode)  -- "groove" (the loaded MIDI groove, the default) or "click"
```

`"click"` substitutes a tempo-locked click (accented on beat 1 of the bar) for the groove
track's input, still passing through the same varispeed tape - so it wow/flutters and
speed-changes right along with everything else. Only takes effect while the Groove switch is
playing, same as the MIDI groove path it replaces.

### Recording notified

```lua
function OnRecordStateChanged(track, isRecording)
end
```

Fires whenever a track's *applied* record state changes (edge-triggered against the value the
audio thread has actually caught up to, not the raw switch, so a script never reacts to a state
the audio thread hasn't reached yet). track is 0-based, same as `SetTrackRecord`.

### Example: fading a track

```lua
-- Fade track A out over 4 seconds, useful for a performance-style drop.
local fadeId = nil

function StartFadeOut()
    local steps, stepMs = 40, 100
    local i = 0
    fadeId = Timer.Every(stepMs, function()
        i = i + 1
        SetTrackGain(0, math.max(0, 1 - i / steps))
        if i >= steps then Timer.Cancel(fadeId) end
    end)
end
```

### Example: `base-scripts/fade-track-a.lua`

A directly-runnable smoke test for the fade above: record onto track A, stop recording, and
it fades itself out over 4 seconds with no further steps - `OnRecordStateChanged` triggers the
fade automatically instead of waiting on a `StartFadeOut()` call from elsewhere.

### Example: a resonant filter sweep

```lua
-- Slow resonant filter sweep on track B, for a build-up feel.
local phase = 0
Timer.Every(50, function()
    phase = phase + 0.02
    local cutoff = 400 + (math.sin(phase) * 0.5 + 0.5) * 3000
    SetTrackFilter(1, cutoff, 0.6, "LP4")
end)
```

### Example: `base-scripts/filter-sweep-track-b.lua`

The same sweep, ready to run as-is - it starts as soon as the script loads, no further steps
needed to hear it (make sure track B has something recorded and is playing).

### Example: reverb swelling in

```lua
-- Reverb swells in on track A the moment it stops recording.
function OnRecordStateChanged(track, isRecording)
    if track == 0 and not isRecording then
        local i = 0
        Timer.Every(200, function()
            i = i + 1
            SetTrackReverbSend(0, math.min(0.6, i / 20))
        end)
    end
end
```

### Example: `base-scripts/reverb-swell-track-a.lua`

The same swell, ready to run as-is - record onto track A and stop recording to hear it build.

### Default (stub) script

Out of the box, a fresh patch uses the click as a recording aid: any track recording switches
the groove track to click, so there's something to record against, and switches back to the
loaded groove once nothing is recording.

```lua
local recording = {}

function OnRecordStateChanged(track, isRecording)
    recording[track] = isRecording
    local anyRecording = recording[0] or recording[1] or recording[2]
    SetGrooveSource(anyRecording and "click" or "groove")
end
```

### Example: `base-scripts/click-while-recording.lua`

The same script as the stub above, kept as a browsable library script too (see the script
editor's dropdown) so a patch that has since diverged can reload it as a starting point.
