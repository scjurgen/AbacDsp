# Pingsynth

A Lua-scripted modal resonator synth: each voice is a bank of ringing bandpass resonators,
excited by an impulse (plus an optional soft noise-burst tail) on every note-on. There is no
fixed harmonic-series dropdown - a patch's script computes an arbitrary list of partials
(frequency, gain, decay, entry delay) per note and hands it to the resonator bank, so odd/even/
stretched/inharmonic/whatever-you-write timbres are all just different Lua loops, not different
C++ code paths.

## Controls

| Control | Range | Description |
|---|---|---|
| Mode | polyphonic / mpe | `polyphonic`: a shared voice pool with oldest-steals-newest stealing once all voices are busy. `mpe`: one voice per MIDI channel; a new note-on on a channel that already has an active voice retriggers it instead of stealing another one. |
| Vol | -100 - 12 dB | Dry voice output level |
| Reverb | -120 - 0 dB | Reverb send level (FDN tank, fixed size/decay) |
| Script | (button) | Opens the popup editor for the current patch's script. While LLM-Assist is active it opens read-only instead (Apply/Reset disabled) so a manual edit can't race a watched-folder pull, and its text stays live-updated as pulls happen. The editor's own Reset button replaces the text with a full skeleton (every available hook, stubbed out) - Cancel discards it, Apply commits it. A dropdown in the editor also lets you view any installed library script, always read-only. |

**Settings > Scripts** manages a named pool of saved scripts (Load / Save As / Delete / Rename),
separate from the script embedded in the current patch. **Settings > Patches** saves/loads full
patches, including whichever script is currently applied.

A script can also pull in a shared library script with `import "name"` (see `../../LUA-MANUAL.md`) -
useful for boilerplate reused across several patches. Built-in libraries live in this repo's
`base-scripts/` folder and are synced to disk on every launch; your own go alongside them in
`Library/User/`, under the same per-app data directory as the Scripts pool above. The script
editor's dropdown (see the Script control above) lets you view any of them read-only.

## Scripting

This section covers what's specific to Pingsynth: the `SetHarmonics`/`SetPitchBendRange` calls
and the `OnMpeModeChanged` hook. For everything shared with any other Lua-scripted example -
MIDI handlers, `OnStart`/`OnStop`, dynamic UI parameters, the `Music`/`Vel`/`Rr`/`Rhythm` helper
library, `Timer`, `Transport`, the available stdlib functions, and sandbox/error-handling notes
- see `../../LUA-MANUAL.md`.

The script decides what a note sounds like; C++ only handles voice bookkeeping (which of the
16 voices a note-on/note-off maps to, per the Mode dial above) and mixing.

```lua
function OnNoteOn(channel, note, velocity)
    local fundamental = Music.NoteToHz(note)
    local harmonics = {
        { freq = fundamental,       gain = 1.0, decay = 1.2, delayMs = 0 },
        { freq = fundamental * 3,   gain = 0.5, decay = 0.9, delayMs = 0 },
        { freq = fundamental * 5,   gain = 0.3, decay = 0.6, delayMs = 5 },
    }
    SetHarmonics(channel, note, velocity, { harmonics = harmonics, attackMs = 1, softExcitation = 0 })
end
```

`SetHarmonics(channel, note, velocity, params)` fires a fresh set of resonators for one note;
call it from `OnNoteOn` (or anywhere else - `OnCC`, a `Timer.After` callback, an imported
library function). `channel`/`note` pick which voice the harmonics land on, following the same
allocation rule as the Mode dial above. `params` is a table with:

| Field | Meaning |
|---|---|
| `harmonics` | Array of partials, see below. Required; an empty or missing array rings nothing. |
| `attackMs` | Linear fade-in for the whole voice, in ms. Default `0` (no fade). |
| `softExcitation` | `0..1`: blends in a continuous noise-burst tail alongside the impulse, instead of a pure click. Default `0` (pure impulse). |

Each entry in `harmonics` is a table with:

| Field | Meaning |
|---|---|
| `freq` | Absolute frequency in Hz. Required; an entry with `freq <= 0` (or missing) is silently dropped. |
| `gain` | Starting amplitude. Default `1`. |
| `decay` | Ring-out time in seconds. Default `0.3`. |
| `delayMs` | How long this partial waits before entering, in ms - staggered onsets are what make a struck/plucked sound read as physical rather than an additive chord firing all at once. Default `0`. |

Up to 60 harmonics per `SetHarmonics` call; extras beyond that are dropped. Up to 16
`SetHarmonics` calls are queued per audio block; a 17th before the block is processed is
dropped rather than blocking - fine for normal playing, a concern only for pathological
all-at-once MIDI dumps.

### Pitch bend

```lua
SetPitchBendRange(7)  -- semitones; call once, e.g. at the top level of the script
```

A MIDI pitch-bend message bends whichever voice(s) are on that channel (in `mpe` mode, at most
one; in `polyphonic` mode, every active voice a note-on last claimed on that channel) directly
- no `OnPitchBend` handling required for this to work, though the script still receives
`OnPitchBend` too (see `../../LUA-MANUAL.md`) if it wants to react separately. Without an explicit
`SetPitchBendRange` call, the range defaults to 12 semitones (a full octave) in `mpe` mode -
per-note bends are expected to be expressive there - and 2 semitones in `polyphonic` mode, the
conventional MIDI default. Note: the underlying resonator bank only bends the first entry of
whatever `harmonics` array was last sent for that voice, not every partial - put the note's
fundamental first if you want the audible pitch to track the bend.

```lua
function OnMpeModeChanged(mpeMode)
    -- Fires whenever the Mode dial's polyphonic/mpe setting changes. mpeMode is true/false.
end
```

### Example scripts

`base-scripts/odd-harmonics.lua` is the engine's own default script (loadable/importable as a
starting point): a plain odd-harmonic series with velocity-scaled falloff.
`base-scripts/inharmonic-bell.lua` demonstrates non-harmonic partial ratios (a bell/struck-metal
timbre no fixed formula dropdown could express) and reacting to `OnMpeModeChanged` to shift the
excitation character between modes.
