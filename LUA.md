# Lua scripting

This covers the Lua scripting API shared by every Lua-scripted JUCE example generated via
`JuceStandaloneGenerator` - the machinery lives in `LuaScriptEngineBase.h` and
`LuaMusicMathLib.h`, both fixed, always-copied components (`CPP_SOURCE_FILES_FIXED` in
`generate-juce-standalone.py`) that any generated example can pull in. Five examples use it
today - each one's own additional hooks are documented in its own README.md's "Scripting"
section, not repeated here:

| Example | README | Its own hooks |
|---|---|---|
| `dronesequencer` | `examples/dronesequencer/README.md` | `NextNotes`/`OnTiming`, note-table format, `Excite()` playing techniques |
| `resonik` | `examples/resonik/README.md` | `SetFreqRange`/`SetDecayRange`/`SetGainRange`/`SetDelayRange`/`SetQ`/`SetResonanceBody` |
| `pingsynth` | `examples/pingsynth/README.md` | `SetHarmonics`/`SetPitchBendRange`, `OnMpeModeChanged` |
| `spectraltap` | `examples/spectraltap/README.md` | `SetMaxTaps`/`SetTap`, `SetFrequency`/`SetResonance`/`SetFormant`/`SetPan`/`SetGain` |
| `tapelooper` | `examples/tapelooper/README.md` | `SetTapeSpeed`/`SetBpm`/`SetGrooveVariation`/`SetTrackRecord`/`SetTrackPlay`/`SetTrackGain`/`SetTrackFilter`/`SetTrackReverbSend`/`SetReverbSize`/`SetReverbDecay`/`SetGrooveSource`/`SetTrackWow`/`SetTrackFlutter`/`SetTrackDrive`/`SetTrackChorus`/`SetTrackEcho`/`SetTrackCompressor`/`SetTrackRingMod`/`SetTrackTremolo`, `OnRecordStateChanged` |

## Sandbox

Only Lua's `base`, `math`, `table`, and `string` standard libraries are loaded - there is no
`io`, `os`, or `require`. 
 lu
| Call | Returns |
|---|---|
| `math.random()` | float in `[0, 1)` |
| `math.random(m)` | integer in `[1, m]` |
| `math.random(m, n)` | integer in `[m, n]` |
| `math.randomseed(x)` | reseeds the generator (scripts don't need this; each load starts freshly seeded) |
| `math.floor(x)`, `math.ceil(x)` | round down/up to an integer (as a float) |
| `math.abs(x)`, `math.max(a, b, ...)`, `math.min(a, b, ...)` | |
| `math.sin(x)`, `math.cos(x)`, `math.tan(x)` | radians |
| `math.sqrt(x)`, `math.exp(x)`, `math.log(x)`, `math.log(x, base)` | |
| `math.fmod(x, y)` | floating-point remainder |
| `math.pi`, `math.huge` | constants |
| `x ^ y` | power (there is no `math.pow` in this Lua version - use the `^` operator) |
| `#t` | length of table/array `t` |
| `table.insert(t, v)`, `table.remove(t)`, `table.concat(t, sep)`, `table.sort(t)` | |
| `string.format(fmt, ...)`, `string.sub`, `string.len`, `#s` | mainly useful for building error messages |
| `tostring(x)`, `tonumber(x)`, `type(x)` | |

Since `os` is not loaded, there is no wall clock: keep any notion of "elapsed time" in your
own counter (a `local step` incremented once per call), or use `Timer.After`/`Timer.Every`
below for a millisecond-scale notion of elapsed time instead.

## Importing shared library scripts

Since there is no `require` (see Sandbox above), a script pulls in a shared library script
with its own `import` line instead:

```lua
import "sequencer"
import "scales.lua"  -- a trailing .lua is accepted and stripped, same library as "scales"
```

Import lines are only recognized in the script's leading header: starting from the top,
each line must be blank, a `--` comment, or an `import "name"` line for the header to keep
growing; the first line matching none of those ends it - from there on, `import` is just
inert script text (it isn't a real Lua function, so a later one fails to parse rather than
doing anything). Within the header, though, any line shaped like `import "..."` is treated
as a real import attempt: `name` may end in `.lua` (stripped before lookup), but what's left
must be letters, digits, `_`, or `-` - anything else rejects the script with a clear "invalid
library name" error rather than silently falling through to a confusing Lua error.

Each resolved library's source is spliced in, in the order written, ahead of the rest of
the script, then the whole thing is compiled as one chunk - so a library's top-level code
(typically function/table definitions) becomes directly available to the rest of the
script. A library is not itself scanned for further `import` lines - nested imports are not
supported; import each one directly from the patch script instead.

Library scripts are plain `.lua` files on disk, in one of two places, checked in this
order:

| Location | Purpose |
|---|---|
| `Library/User/<name>.lua` | Your own scripts. Never touched automatically - add, edit, or remove them yourself. |
| `Library/Base/<name>.lua` | Synced from this repo's `base-scripts/` folder each time the app starts. Not meant to be hand-edited - a `User` script of the same name overrides it instead. |

Both live under the same per-app data directory as the existing named Scripts pool (see
`Settings > Scripts` in an example's own README), e.g.
`~/Library/Application Support/AbacDsp/<Example>/Library/` on macOS. For dronesequencer,
a `Library/User/` script can also be pushed via the `llm-genscripts/generated/libraries/`
watched-folder workflow instead of editing the file by hand - see `llm-genscripts/CLAUDE.md`.

An `import` naming a library neither directory has fails `loadScript()` immediately, before
anything is compiled - the same "rejected at Apply time, previous script keeps playing
underneath" behavior as any other compile failure. The error names both full paths that
were checked, and is also printed to the console, so a missing/misspelled library is easy
to track down even without the in-app error display visible.

## Incoming MIDI

If the host sends the plugin MIDI, each event calls an optional handler - define whichever
ones your script needs; an undefined handler is simply never called.

```lua
function OnNoteOn(channel, note, velocity) end     -- velocity 1..127 (0 arrives as OnNoteOff instead)
function OnNoteOff(channel, note, velocity) end    -- velocity 0..127 (release velocity, often 0)
function OnCC(channel, ccNumber, value) end        -- ccNumber and value 0..127
function OnProgramChange(channel, program) end     -- program 0..127
function OnAftertouch(channel, value) end          -- channel pressure, value 0..127
function OnPolyPressure(channel, note, value) end  -- per-note pressure, value 0..127
function OnPitchBend(channel, bendValue) end       -- 14-bit, -8192..8191, center 0
```

`channel` is 0-based (0..15): the MIDI channel the event arrived on. A Note On with velocity
0 is normalized to a Note Off before your script ever sees it, per standard MIDI convention;
you don't need to check for that case yourself in `OnNoteOn`.

## Start/stop

```lua
function OnStart() end
function OnStop() end
```

Fires on whatever this particular example considers an effective start/stop transition -
check that example's own README for exactly what drives it (e.g. dronesequencer ties this to
its manual Play switch or host-synced transport, whichever is actually driving playback).
Useful for resetting your own counters/state so a script restarts from a known point every
time, rather than wherever it happened to be left.

## Dynamic UI parameters

A script can declare its own knobs/dropdowns/switches, shown in the plugin's Lua Controls
area, and get called back when the user (or host automation/CC) changes one. Call this once,
typically at the top level of the script (not inside a per-tick function):

```lua
UICreateParameterSet({
    { id = "depth", name = "Depth", type = "knob", range = { min = 0, max = 1, step = 0, skew = 1 }, default = 0.5 },
    { id = "mode", name = "Mode", type = "drop", items = { "A", "B", "C" }, default = 0 },
    { id = "enabled", name = "Enabled", type = "switch", default = 0 },
})

function OnDepthChanged(value) end  -- value is already mapped through the declared range
function OnModeChanged(value) end   -- value is the selected item's index, 0-based
function OnEnabledChanged(value) end -- value is 0 or 1
```

| Field | Meaning |
|---|---|
| `id` | Must start with a letter, contain only letters/digits/underscores. Builds the callback name: `id`'s first letter capitalized, wrapped as `On<Id>Changed`. |
| `name` | Display label; defaults to `id` if omitted. |
| `type` | `"knob"`, `"drop"`, or `"switch"`. |
| `range` | Required for `"knob"` only: `{ min, max, step, skew }`. `step = 0` means continuous; `skew` follows the usual JUCE convention (`1` = linear). |
| `items` | Required for `"drop"` only: an array of label strings: the callback value is the selected index, `0`..`#items - 1`. |
| `unit` | Optional display unit string, e.g. `"Hz"`. Shown as a suffix on a knob's value text box; not shown for a drop/switch. |
| `description` | Optional; used for the widget's accessibility description. Falls back to `name` if omitted. |
| `default` | Required; must fall within the resolved range (your declared `range` for a knob, or `0..#items - 1`/`0..1` for a drop/switch). |

Calling `UICreateParameterSet` again on a script reload replaces the previous set entirely -
a parameter not re-declared is unclaimed and disappears from the UI. There are only
`kMaxLuaParams` (8) slots in the underlying pool shared by every script; requesting more than
that, a duplicate `id`, or a malformed descriptor rejects the whole script at Apply time with
an error, the same as any other script error - the previously running script keeps playing
underneath.

## Music, velocity, and rhythm helpers

Beyond the plain `math`/`table`/`string` standard libraries above, the engine also binds a
small built-in library for the things a note-generating script typically needs: Hz/note
conversion, scale and chord tables, rhythm note-values, and velocity-to-gain shaping curves.
These are plain Lua tables/functions, always present, nothing to require.

```lua
Music.NoteToHz(69)             -- 440.0
Music.HzToNote(440)            -- 69.0
Music.IntervalToRatio(12)      -- 2.0  (up an octave)
Music.RatioToInterval(0.5)     -- -12.0 (down an octave)
Music.Scales.Major             -- { 0, 2, 4, 5, 7, 9, 11 }
Music.Chords.Minor7            -- { 0, 3, 7, 10 }
Music.Harmonics(110, 4)        -- { 110, 220, 330, 440 } - fundamental*1..fundamental*count
Music.HarmonizeToScale(61, 60, "Major")  -- 60.0 - snaps to the nearest note in the named
                                          -- Music.Scales entry, relative to the given root

Vel.Exponential(0.5)           -- exponential 0..1 -> 0..1 curve, default bend
Vel.Exponential(0.5, 8)        -- steeper bend
Vel.Cubic(0.5)                 -- v^3

Rr.Next(current, count)        -- round-robin: (current + 1) wrapped into [0, count)
Rr.Advance(current, step, count)

Rhythm.NoteValues.Eighth       -- 0.5 (beat-multiplier relative to a quarter note)
Rhythm.BeatsToMs(1, 120)       -- 500.0, at 120 bpm
```

`Music.Scales` and `Music.Chords` cover the common set (major/minor scales and modes,
pentatonics, blues, whole-tone, chromatic; major/minor/diminished/augmented triads, 6ths,
7ths, sus2/sus4, add9, 9ths) - see `LuaMusicMathLib.h` for the full list of names. These
tables are handed to you as regular, writable Lua tables (not read-only) purely for
simplicity; treat them as constants rather than mutating them.

`Music.Harmonics(fundamentalHz, count)` clamps `count` to 32. `Music.HarmonizeToScale`
preserves the input note's octave; passing an unknown scale name returns the note
unchanged.

## Timers

`Timer.After`/`Timer.Every` schedule a callback to run after some number of milliseconds have
elapsed, ticked once per audio block - the closest thing this sandbox has to a wall clock.
Useful for animating your own script-side state over time.

```lua
Timer.After(2000, function()
    -- runs once, about 2 seconds after this call
end)

local id = Timer.Every(250, function()
    -- runs repeatedly, about every 250 ms
end)

Timer.Cancel(id)  -- stops a still-pending Timer.After, or a running Timer.Every
```

`Timer.After`/`Timer.Every` return an id (a small positive integer) usable with
`Timer.Cancel`, or `0` if all 8 available timer slots are already in use. Timers do not
survive a script reload (Apply, or loading a saved script): every pending/repeating timer is
cleared before the new script runs, since a timer's callback is a closure over the old
script's own state.

Note: `Timer` schedules script-side callbacks only. It has no way to push a value back into
one of the plugin's own host-automatable parameters - those still only flow host/UI -> script
via `On<Id>Changed`, not the other way.

## Pitch tracking

An example that feeds its incoming audio into a YIN pitch tracker (currently `resonik`;
opt-in per example on the C++ side via `feedPitchAnalysis()`) fires `OnPitchDetected` once
per analysis hop and exposes the same values on demand through `Pitch.*`:

```lua
function OnPitchDetected(hz, confidence)
    -- hz: detected frequency, or 0 if nothing pitched was found this hop.
    -- confidence: 0..1, how periodic the signal looked - low near silence/noise/onsets.
end

Pitch.Hz()          -- the same hz as the last OnPitchDetected call
Pitch.Confidence()  -- the same confidence as the last OnPitchDetected call
```

Both default to `0` before the first hop. The hop rate is set on the C++ side
(`setPitchAnalysisGranularity()`, default 100 ms) - a script has no control over it.
`Music.HzToNote`/`Music.HarmonizeToScale` (see Music helpers above) are the usual next step
for turning `hz` into something musical.

## Playhead and transport

The `Transport` table reads the host's own playhead, independent of whatever tempo/play
controls a particular example defines (its own `OnStart`/`OnStop` above follow *that
example's* effective play state, not the host's raw transport).

```lua
Transport.TimeInSeconds()        -- decimal seconds since the host's playhead origin
Transport.TimeInQuarterNotes()   -- decimal quarter notes (PPQ) since the host's playhead origin
Transport.Tempo()                -- host tempo in bpm
Transport.TimeSignature()        -- two values: numerator, denominator
Transport.PlayingState()         -- true/false
Transport.LoopingState()         -- true/false
Transport.RecordingState()       -- true/false
```

Not every host reports every field. A field the host doesn't provide simply stays at its
default:

| Field | Default |
|---|---|
| `TimeInSeconds` | `0.0` |
| `TimeInQuarterNotes` | `0.0` |
| `Tempo` | `120.0` |
| `TimeSignature` | `4, 4` |
| `PlayingState` | `false` |
| `LoopingState` | `false` |
| `RecordingState` | `false` |

The standalone app (no host) always reports these defaults, since there is no playhead to
read.

A script can also react to a transport change directly, instead of polling `Transport.*`
every call:

```lua
function OnTempoChanged(bpm) end                             -- fires when Transport.Tempo() changes
function OnTimeSignatureChanged(numerator, denominator) end  -- fires when Transport.TimeSignature() changes
function OnPlayingStart() end                                 -- fires when Transport.PlayingState() becomes true
function OnPlayingStop() end                                  -- fires when Transport.PlayingState() becomes false
```

## Example: math.random and math.sin

```lua
function OnCC(channel, ccNumber, value)
    local jitter = math.random(-2, 2)
    local wobble = 8 * math.sin(value)
    -- do something with jitter/wobble, e.g. set a global your example's own per-tick
    -- function reads (dronesequencer's NextNotes(), for instance)
end
```

## Notes on the sandbox

- A script that fails to *compile* (a syntax error, or an error in code that runs
  immediately when the script loads) is rejected at Apply time; the popup shows the error
  inline and stays open so you can fix it without losing your edit.
- A script that compiles fine but errors *when actually called* later can't be caught at
  Apply time - Lua doesn't know that in advance. That kind of error shows up in the status
  bar instead, the first time it happens.
- Either way, whatever script was running before (a stub script, on a fresh instance) keeps
  playing underneath a rejected Apply - a bad script never leaves you with nothing.
