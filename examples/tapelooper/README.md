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
**Settings > Loops** saves/loads tracks A/B/C's own recorded audio (not the groove track, which
plays a loaded MIDI groove rather than holding recorded audio of its own) by name, alongside the
current patch parameters and the loop's own bars/BPM - Save writes over the currently loaded
loop, Save As... prompts for a name, and Load replaces tracks A/B/C's audio and restores that
loop's own bars/BPM. Large loops save and load in the background without blocking playback.

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

### Effect chain order

```lua
SetTrackChain(track, {"filter", "distortion", "chorus", "echo", "compressor", "ringmod", "tremolo"})
```

Reorders a track's own effects - the default order is exactly the list above, so an untouched
track processes the same whether or not a script ever calls this. Any of the 7 names may be
left out entirely (that effect is simply skipped) but not repeated, and an unrecognized name
rejects the whole call, leaving the previously-installed chain in effect. Reverb send is not
in this list - it always taps whatever the chain's last node produces, regardless of order.

### Example: track C effects presets

Four directly-runnable smoke tests for the effects above, each self-contained -
`base-scripts/lofi-tape-fx-track-c.lua` (drive + chorus + echo, a warped-tape character),
`base-scripts/modulation-fx-track-c.lua` (compressor + ring mod + tremolo),
`base-scripts/digital-clean-track-c.lua` (wow/flutter to 0, removing tape's default
speed-drift), and `base-scripts/reordered-chain-track-c.lua` (distortion moved before the
filter via `SetTrackChain`, for a grittier result than the default order). Record something
onto track C and play it back to hear any of them.

### Groove source

```lua
SetGrooveSource(mode)  -- "groove" (the loaded MIDI groove, the default) or "click"
```

There is one conductor for the groove track: `GrooveDrumPlayer`, always. `"click"` is not a
separate playback mechanism - it loads a built-in "Metronome" MIDI groove (plain 4/4, ordinary
sample pieces `click_low`/`click_high`) through the exact same style-loading path a Groove-menu
pick uses, so it inherits sample-accurate loop-begin sync for free. `"groove"` switches back to
whichever style/variation was loaded before switching to click.

### Per-instrument groove control

```lua
SetInstrumentGain(name, gain)          -- linear multiplier, 0 mutes that instrument
MuteInstrument(name)                   -- sugar for SetInstrumentGain(name, 0)
SetInstrumentReverbSend(name, amount)  -- sent to the groove track's own reverb
```

`name` is a lowercase instrument tag - the full vocabulary a groove note can carry, matching
`AbacDsp::kGrooveTagNames` (`Sampler/GrooveNoteMap.h`):

```
kick, snare, snare_roll, snare_alt, rimshot, sidestick,
tom, tom_low, tom1, tom2, tom3, tom_left,
hihat, hihat_closed, hihat_open, hihat_open_tip, hihat_closed_pedal, hihat_closed_edge,
hihat_open_pedal, hihat_open1, hihat_open2, hihat_open3, hihat_step, hihat_half_open,
hihat_stopped, hihat_soft_step, hihat_ghost,
cymbal, crash, crash_stopped, crash_long, ride, ride_bell, china,
timbale, timbale1, timbale2, timbale3, timbale4, timbale_damped, woodblock,
click_low, click_high
```

Which of these actually resolve to audio depends on the currently loaded kit - an unknown name,
or a name the kit has no piece for, is a silent no-op rather than an error, so the same
instrument name works across kits with different pieces or naming, without a script needing to
know which kit is loaded.

The groove track has its own `AbacDsp::FdnTankGlide` reverb, separate from the three tape
tracks' own reverbs, fed by a true per-instrument mix (each instrument's own audio, weighted by
its own send) carried through the same varispeed tape processing as the main groove signal - so
it wow/flutters and speed-changes in sync rather than following raw tempo. `SetReverbSize`/
`SetReverbDecay` above tune this bus too, alongside the three tape tracks' own.

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

### Example: a groove instrument mix

```lua
-- Drop kick and snare, leave hats/cymbals - a classic "breakdown" filter - and swell
-- the closed hihat's own reverb send over ~4 seconds.
MuteInstrument("kick")
MuteInstrument("snare")

local i = 0
Timer.Every(200, function()
    i = i + 1
    SetInstrumentReverbSend("hihat_closed", math.min(0.6, i / 20))
end)
```

### Example: `base-scripts/groove-instrument-mix.lua`

The same mix, ready to run as-is - turn on the Groove switch to hear it.

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
