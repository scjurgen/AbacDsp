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
| Groove | (switch) | Play/stop the groove track |
| BPM | 50 - 250 | Groove/click tempo |
| Groove Var | 0 - 31 | Selects among the loaded style's variations |
| Wow Depth/Rate/Drift (A, B, C) | 0-1, 0-3 Hz, 0-1 | Per-track slow speed-drift character (`VariSpeedTapeDelay`'s own wow model) |
| Flutter Depth/Rate (A, B, C) | 0-1, 0-10 Hz | Per-track fast speed-irregularity character |
| Script | (button) | Opens the popup editor for the current patch's script. The editor's own Reset button replaces the text with a full skeleton (every available hook, stubbed out) - Cancel discards it, Apply commits it. |

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
```

Each of these writes into the same underlying state the plugin's own dials/switches do, so
whichever - host automation or script - sets a value last wins; there's no separate "script
override" layer to fight with the UI.

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
