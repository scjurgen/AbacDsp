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
{"compiled": true, "error": "", "patchName": "", "scriptName": "cmaj9-arpeggio", "kind": "script"}
```

`error` is the Lua compile error message when `compiled` is `false`, empty otherwise.
`patchName` is whatever patch was active in the app at the time (often empty). `kind` is
`"script"` for a normal patch-script pull as above, or `"library"` for a library-script
pull - see the next section, since `compiled`/`error` mean something different there. Note:
this only catches *compile*-time errors (syntax errors, code that runs at load). A script
that compiles but throws inside `NextNotes()`/`OnTiming()` later reports `compiled: true`
here and only fails when the app actually calls it - there is no file-based signal for that
class of error, only the app's own status bar.

## Library scripts (generated/libraries/)

A script can `import "name"` a shared library instead of duplicating boilerplate (see root
`LUA.md`, "Importing shared library scripts"). To push a library update through this same
watched-folder workflow rather than editing it by hand:

1. Write `generated/libraries/<name>.lua` - same naming rule as `import "name"` itself:
   letters, digits, `_`, `-` (an accidental trailing `.lua` in the name is fine, it's
   stripped the same way `import` strips it). This is a plain library file - helper
   function/table definitions, not `NextNotes()`/`OnTiming()`.
2. The app saves it to `Library/User/<name>.lua`, then **automatically re-applies whatever
   patch script is currently active** - the same Apply path as a manual edit - so if that
   script has `import "<name>"`, the new content is picked up and genuinely validated, not
   just syntax-checked in isolation.
3. Wait for `generated/libraries/state-<name>.json`, same schema as above but with
   `"kind": "library"`. Importantly, `compiled`/`error` here describe **the current patch
   script's** recompile outcome after the library update, not the library file by itself -
   if the current script doesn't actually import this library, `compiled: true` just means
   "saved, and whatever was already running still compiles," not "this library is correct."
4. After processing, the source is renamed to `generated/libraries/pulled-<name>-<epoch-ms>.lua`,
   exactly like a patch-script pull - same "never reapplied, don't re-edit in place" rule.

**Ordering matters if a patch you're about to push imports a library you're also pushing:**
a pending `generated/` patch-script candidate is always processed before a pending
`generated/libraries/` one on every poll. Push the library first and wait for its own
`generated/libraries/state-<name>.json` to confirm it, *then* push the patch. Pushing both
at once risks the patch being applied (and permanently pulled) against a library that
doesn't exist yet - it fails with an `import "...": library script not found` error and,
per step 4 above, can't be retried under that same filename; you'd have to push it again
under a new name once the library is actually installed.

This always writes to `Library/User/`, never `Library/Base/` (the repo-synced tier, see
`LUA.md`) - there is no watched-folder path into `Base/` by design. Once a `Library/User/`
script is proven out and you want it to ship with the app for every patch (not just as a
personal addition), the way to "promote" it is a plain repo change: copy it into
`examples/dronesequencer/base-scripts/<name>.lua` and commit - ask Claude Code to do this
as a normal follow-up once you're happy with a library, rather than trying to push it there
through `generated/`.

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
- `slide` (optional, default 0): semitones the note bends in from, signed - positive starts
  above and descends in, negative starts below and rises in. `slideTime` (optional, default
  0): ms for the bend to resolve; only meaningful when `slide` is nonzero.
- Only `base`, `math`, `table`, `string` are loaded - no `io`, `os`, `require`. No wall
  clock; use a `local` counter across calls instead.

### Excitation techniques

`Excite(channel, params)` fires an abstract playing technique on a string, independent of
`NextNotes()`'s own note table - callable from any script context, at any time, including
mid-sustain. It runs to completion on its own once called.

```lua
Excite(0, { type = "bow", start = 0, ["end"] = 2000, strength = 0.5 })
```

`params`: `type` (see below), `start` (ms before it begins), `["end"]` (ms, meaning depends
on `type`; must be bracketed since `end` is a Lua keyword), `strength` (0..1), `harmonic`
(`"sympathetic"` only, 1 = fundamental). A string runs one technique at a time; calling
`Excite()` again queues the new call right after the current one ends (a third call replaces
the queued one).

| `type` | `end` means | Behavior |
|---|---|---|
| `pluck` | unused | Fresh pluck, `strength` is gain. |
| `strike` | unused | Like `pluck`, brighter/shorter attack. |
| `mute` | fade-out duration | Fades to silence over `end` ms instead of cutting off. |
| `palmmute` | window end | Raises the damper for `[start, end]`, then restores it. |
| `bow` | window end | Continuous excitation for `[start, end]`; wakes a stopped string. |
| `sympathetic` | window end | Like `bow`, excites the `harmonic`-th harmonic directly. |
| `wind` | window end | Like `bow`, slow wide random level fluctuation (gusting). |
| `rub` | window end | Like `bow`, fast tight random level fluctuation (scraping). |

See `examples/dronesequencer/README.md` under "Excitation techniques" for the full
description and `base-scripts/excitation-techniques-demo.lua` for a live example.

See root `LUA.md` for the full shared hook list (`OnStart`/`OnStop`, incoming-MIDI handlers,
`Music`/`Timer`/`Transport`), and `examples/dronesequencer/README.md` for the division-index
meaning and worked examples.
