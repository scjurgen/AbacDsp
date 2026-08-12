# DroneSequencer

A Lua-scripted drone sequencer: an N-string Karplus-Strong ensemble (the same plucked-string
voice engine and reverb chain as `tanpura`), driven by a user-editable Lua script instead of a
fixed pattern. The script decides what to play; BPM/division just set the clock the script is
asked to keep up with.

## Controls

| Control | Range | Description |
|---|---|---|
| Level | -80 - 0 dB | Output level |
| Tuning | 400 - 800 Hz | Reference pitch for note 60 |
| Transpose | -24 - +24 st | Added to every note height the script returns |
| Detune | 0 - 100 ct | Per-string detune spread (scales a fixed per-voice pattern) |
| Voices | 1 - 8 | How many strings are triggerable; `channel` values beyond this clamp to the last one |
| Script | (button) | Opens the popup editor for the current patch's script. The editor's own Reset button replaces the text with a full skeleton (every available hook, stubbed out) - Cancel discards it, Apply commits it. |
| BPM | 40 - 250 | Tempo |
| Host Sync | on/off | Follow the host transport's BPM/play state instead of BPM/Play |
| Division | 1/1 - 1/16T | Clock division the script is asked for notes at |
| Play | on/off | Starts/stops the clock (manual mode only) |
| Humanize Timing | 0 - 100% | Random jitter around the nominal beat, applied on top of the script's `delay` |
| Humanize Level | 0 - 100% | Random gain jitter, applied on top of the script's `velocity` |
| Attack/Decay/Decay Octave/Sustain/Sustain Humanize | - | Karplus-Strong voice envelope, same as tanpura |
| Filter LFO Depth/Speed/Variation | - | Per-voice filter LFO, same as tanpura |
| Filter Attack/Decay/Sustain/Cutoff/Resonance/Contour | - | Voice lowpass filter, same as tanpura |
| Reverb Dry/Wet/Size/Decay/Shelf Low/Shelf High | - | FDN reverb, same as tanpura |

**Settings > Scripts** manages a named pool of saved scripts (Load / Save As / Delete / Rename),
separate from the script embedded in the current patch. **Settings > Patches** saves/loads full
patches, including whichever script is currently applied.

A script can also pull in a shared library script with `import "name"` (see `../../LUA.md`) -
useful for boilerplate reused across several patches. Built-in libraries live in this repo's
`base-scripts/` folder and are synced to disk on every launch; your own go alongside them in
`Library/User/`, under the same per-app data directory as the Scripts pool above. There is no
in-app browser for these in this version - add, edit, or remove them as plain `.lua` files, and
type the `import` line yourself.

## Scripting

This section covers what's specific to DroneSequencer: the `OnTiming`/`NextNotes` contract and
the note-table shape. For everything shared with any other Lua-scripted example - MIDI
handlers, `OnStart`/`OnStop`, dynamic UI parameters, the `Music`/`Vel`/`Rr`/`Rhythm` helper
library, `Timer`, `Transport`, the available stdlib functions, and sandbox/error-handling
notes - see `../../LUA.md`.

The script controls two things: what happens when the clock's BPM/division changes, and what to
play on each beat.

```lua
function OnTiming(bpm, division)
    -- Called whenever BPM or Division changes (not every block). Store what you need
    -- as globals; nothing here is required, but a script has no other way to see the
    -- current tempo.
    BPM = bpm
    DIVISION = division
end

function NextNotes()
    -- Called a fixed lookahead before each beat. Return an array of 0..8 notes to
    -- play this beat; an empty table means "play nothing".
    return {
        { note = 60, velocity = 0.8, channel = 0, length = 0, delay = 0 },
    }
end
```

Each note is a table with:

| Field | Meaning |
|---|---|
| `note` | MIDI-style note number (60 = middle C at the Tuning reference pitch). Transpose is added on top of this. |
| `velocity` | 0..1 pluck strength. Humanize Level jitters this further. |
| `channel` | Which string to pluck (0-based). Clamped to `[0, Voices)`. |
| `length` | ms until the string is muted; 0 lets it ring out via Attack/Decay/Sustain instead of being cut off. |
| `delay` | ms offset from the nominal beat; may be negative to fire early. The sequencer always asks a short lookahead before the beat, so a small negative `delay` is normal, not a bug. |

`NextNotes()` returning more than 8 notes in one call has the extras dropped. Not yet
implemented: pitch bends - the note struct is deliberately built so a `bend = {interval, time,
curve}` field can be added later without reshaping what's already there.

`OnStart`/`OnStop` (see `../../LUA.md`) fire on a start/stop transition of the clock here
specifically: the manual Play switch toggling, or the host transport's play state when Host
Sync is on - whichever one is actually driving playback. Useful for resetting your own
counters/state (e.g. the `step` variable in the example below) so a script restarts from a
known point every time. There's no separate "paused" callback: the host doesn't reliably
report pause as distinct from stop, and the sequencer's own clock always resets on any
stop-to-start transition rather than resuming, so there's nothing paused to report either.

### Example: a 4-step arpeggio across three strings

Demonstrates stepping through a pattern, mixing short plucks with one longer sustained note,
and firing a note slightly ahead of the beat with a negative `delay`:

```lua
local pattern = {
    { note = 60, channel = 0, length = 0,   delay = 0 },
    { note = 64, channel = 1, length = 300, delay = 0 },
    { note = 67, channel = 2, length = 0,   delay = 0 },
    { note = 64, channel = 1, length = 300, delay = -10 },
}
local step = 1

function OnTiming(bpm, division)
    BPM = bpm
    DIVISION = division
end

function OnStart()
    step = 1 -- always start the pattern from the beginning, not wherever it last stopped
end

function NextNotes()
    local n = pattern[step]
    step = step + 1
    if step > #pattern then
        step = 1
    end
    return {
        { note = n.note, velocity = 0.8, channel = n.channel, length = n.length, delay = n.delay },
    }
end
```
