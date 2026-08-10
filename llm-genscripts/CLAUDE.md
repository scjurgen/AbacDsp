# llm-genscripts

Drop folder for the DroneSequencer example's "LLM-Assist" watched-folder mode. Writing a
`.lua` file into `generated/` is how a Claude Code session hands a note-generating script
to a running DroneSequencer instance without going through its in-app popup editor. See
`README.md` for the folder layout.

Full Lua scripting API is documented in two places: the engine API shared by any Lua-scripted
example (MIDI handlers, `OnStart`/`OnStop`, dynamic UI parameters, the `Music`/`Vel`/`Rr`/
`Rhythm` helper library, `Timer`, `Transport`, sandboxed stdlib) in root `LUA.md`, and
DroneSequencer's own `NextNotes`/`OnTiming` contract and note-table fields in
`examples/dronesequencer/README.md` under "Scripting" - read both before writing a script,
they are the authoritative reference. This file only covers the watched-folder protocol
itself.

## Precondition

This only works while the DroneSequencer standalone/plugin app is actually running with
LLM-Assist enabled (Scripts menu) and pointed at `llm-genscripts/generated/`. If no
`state-*.json` shows up after writing a script, check that first before assuming a bug.

## Workflow

1. Write a new file in `generated/` named `<descriptive-name>.lua` (e.g.
   `generated/cmaj9-arpeggio.lua`). Must define at least `NextNotes()`; add
   `OnTiming(bpm, division)` if the script needs tempo/division. Never start the filename
   with `pulled-` (reserved, see below).
2. The running app polls `generated/` on its UI timer, picks the newest-mtime `*.lua` file
   that isn't already `pulled-*`, and applies it via the same path as the in-app editor's
   Apply button.
3. Wait for `generated/state-<name>.json` to appear next to it (should be near-instant
   once the app picks it up). Read `compiled`: `true` means it is live now; `false` means
   the previous script is still playing and `error` holds the compile error.
4. On failure, fix the script and write it under a **new** filename. Do not try to reuse
   or re-edit the original: it has already been renamed away (step 5) and will not be
   looked at again even if edited in place.
5. After applying (success or failure), the source file is renamed to
   `generated/pulled-<name>-<epoch-ms>.lua` so it is never picked up twice. Leave these
   alone; they are history, not live input. Same for old `state-*.json` files - each one
   reflects the result at the time it was written, not the folder's current state.

## state-<name>.json schema

```json
{"compiled": true, "error": "", "patchName": "", "scriptName": "cmaj9-arpeggio"}
```

`error` is the Lua compile error message when `compiled` is `false`, empty otherwise.
`patchName` is whatever patch was active in the app at the time (often empty). Note: this
only catches *compile*-time errors (syntax errors, code that runs at load). A script that
compiles but throws inside `NextNotes()`/`OnTiming()` later reports `compiled: true` here
and only fails when the app actually calls it - there is no file-based signal for that
class of error, only the app's own status bar.

## Quick script contract recap

```lua
function NextNotes()
    return {
        { note = 60, velocity = 0.8, channel = 0, length = 0, delay = 0 },
    }
end
```

- Return 0..8 note tables per call (extras beyond 8 are silently dropped); an empty table
  plays nothing this tick.
- `note`: MIDI-style note number, 60 = middle C. `velocity`: 0..1. `channel`: 0-based
  string index, clamped to however many Voices the patch has (1-8). `length`: ms before
  mute, 0 = ring out naturally. `delay`: ms offset from the beat, may be negative.
- Only `base`, `math`, `table`, `string` are loaded - no `io`, `os`, `require`. No wall
  clock; use a `local` counter across calls instead.

See root `LUA.md` for the full shared hook list (`OnStart`/`OnStop`, incoming-MIDI handlers,
`Music`/`Timer`/`Transport`), and `examples/dronesequencer/README.md` for the division-index
meaning and worked examples.
