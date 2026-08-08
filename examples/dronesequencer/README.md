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

## Scripting

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

## Transport

```lua
function OnStart() end  -- fires on any effective start
function OnStop() end   -- fires on any effective stop
```

These fire on a start/stop transition of the clock: the manual Play switch toggling, or the
host transport's play state when Host Sync is on - whichever one is actually driving playback.
Useful for resetting your own counters/state (e.g. the `step` variable in the arpeggio example
below) so a script restarts from a known point every time, rather than wherever it happened to
be left. There's no separate "paused" callback: the host doesn't reliably report pause as
distinct from stop, and the sequencer's own clock always resets on any stop-to-start
transition rather than resuming, so there's nothing paused to report either.

## Incoming MIDI

If the host sends the plugin MIDI, each event calls an optional handler - define whichever
ones your script needs; an undefined handler is simply never called, same as `OnTiming`. None
of these are required to make sound (that's `NextNotes()`'s job) - they're for reacting to a
controller, DAW automation lane, or another track's MIDI, e.g. to change what `NextNotes()`
does next by setting a global.

```lua
function OnNoteOn(channel, note, velocity) end     -- velocity 1..127 (0 arrives as OnNoteOff instead)
function OnNoteOff(channel, note, velocity) end    -- velocity 0..127 (release velocity, often 0)
function OnCC(channel, ccNumber, value) end        -- ccNumber and value 0..127
function OnProgramChange(channel, program) end     -- program 0..127
function OnAftertouch(channel, value) end          -- channel pressure, value 0..127
function OnPolyPressure(channel, note, value) end  -- per-note pressure, value 0..127
function OnPitchBend(channel, bendValue) end       -- 14-bit, -8192..8191, center 0
```

`channel` is 0-based (0..15), matching `channel` in the note table above - it is the MIDI
channel the event arrived on, unrelated to which string gets plucked. A Note On with velocity
0 is normalized to a Note Off before your script ever sees it, per standard MIDI convention;
you don't need to check for that case yourself in `OnNoteOn`.

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

## Available functions

Only Lua's `base`, `math`, `table`, and `string` standard libraries are loaded - there is no
`io`, `os`, or `require`. Notably, **there is no bare `rand()`** (that's a C function, not
Lua) - use `math.random()`.

| Call | Returns |
|---|---|
| `math.random()` | float in `[0, 1)` |
| `math.random(m)` | integer in `[1, m]` |
| `math.random(m, n)` | integer in `[m, n]` |
| `math.randomseed(x)` | reseeds the generator (scripts don't need this; each load starts freshly seeded) |
| `math.floor(x)`, `math.ceil(x)` | round down/up to an integer (as a float) |
| `math.abs(x)`, `math.max(a, b, ...)`, `math.min(a, b, ...)` | |
| `math.sin(x)`, `math.cos(x)`, `math.tan(x)` | radians, e.g. for LFO-style modulation of `note`/`delay` over successive calls |
| `math.sqrt(x)`, `math.exp(x)`, `math.log(x)`, `math.log(x, base)` | |
| `math.fmod(x, y)` | floating-point remainder |
| `math.pi`, `math.huge` | constants |
| `x ^ y` | power (there is no `math.pow` in this Lua version - use the `^` operator) |
| `#t` | length of table/array `t` |
| `table.insert(t, v)`, `table.remove(t)`, `table.concat(t, sep)`, `table.sort(t)` | |
| `string.format(fmt, ...)`, `string.sub`, `string.len`, `#s` | mainly useful for building error messages, not note data |
| `tostring(x)`, `tonumber(x)`, `type(x)` | |

Example using `math.random` for a wandering pitch, and `math.sin` for a slow vibrato-like
drift applied via `delay`:

```lua
function NextNotes()
    local jitterSemitones = math.random(-2, 2)
    local wobbleMs = 8 * math.sin(os_time and os_time() or 0) -- os is not available; see below
    return {
        { note = 60 + jitterSemitones, velocity = 0.8, channel = 0, length = 0, delay = 0 },
    }
end
```

(That `os_time` reference is deliberately left broken above as a reminder: `os` is not
loaded, so keep any notion of "elapsed time" in your own counter - e.g. a `local step`
incremented once per `NextNotes()` call, as in the arpeggio example - rather than reaching
for a wall-clock function.)

## Notes on the sandbox

- A script that fails to *compile* (a syntax error, or an error in code that runs
  immediately when the script loads) is rejected at Apply time; the popup shows the error
  inline and stays open so you can fix it without losing your edit.
- A script that compiles fine but errors *when actually called* later (inside `NextNotes()`
  or `OnTiming()`, once real playback reaches that code path) can't be caught at Apply time -
  Lua doesn't know that in advance. That kind of error shows up in the status bar instead,
  the first time it happens, and playback silently produces no notes until it's fixed.
- Either way, whatever script was running before (the stub, on a fresh patch) keeps playing
  underneath a rejected Apply - a bad script never leaves you with nothing.
- `NextNotes()` returning more than 8 notes in one call has the extras dropped.
- Not yet implemented: pitch bends. The note struct is deliberately built so a `bend =
  {interval, time, curve}` field can be added later without reshaping what's already there.
