# Resonik

A Lua-scripted resonator bank: up to 100 parallel chains (a bandpass feeding a decaying
bandpass), spread across a frequency range and excited by whatever audio is fed into the
plugin, turning the input into ringing, decaying tones. The spread across chains - frequency,
decay, gain, delay, and per-chain fine detail - is entirely up to the script; there are no
fixed distribution dials left to fight with it.

## Controls

| Control | Range | Description |
|---|---|---|
| Chains | 1 - 100 | Number of parallel resonator chains active |
| Dry | -100 - 12 dB | Dry (unprocessed input) level |
| Wet | -100 - 12 dB | Wet (resonated) level |
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

This section covers what's specific to Resonik: the aggregate `SetFreqRange`/`SetDecayRange`/
`SetGainRange`/`SetDelayRange`/`SetQ` calls and the per-chain `SetResonanceBody`. For everything
shared with any other Lua-scripted example - MIDI handlers, `OnStart`/`OnStop`, dynamic UI
parameters, the `Music`/`Vel`/`Rr`/`Rhythm` helper library, `Timer`, `Transport`, pitch tracking
(`OnPitchDetected`/`Pitch.*` - Resonik is the example that feeds its input through the YIN
tracker), and sandbox/error-handling notes - see `../../LUA-MANUAL.md`.

### Aggregate controls

Each mimics one of the plugin's old, now-removed dials. Every chain not individually
overridden (see Per-chain fine control below) follows these; they take effect on the next
audio block:

```lua
SetFreqRange(low, high[, distribution])  -- Hz spread across every chain; distribution:
                                          -- 0 = linear, 1 = logarithmic (default if omitted
                                          -- is whatever the last call set, or the stub
                                          -- script's own default - see below)
SetDecayRange(min, max)                  -- seconds spread
SetGainRange(minDb, maxDb)               -- dB spread
SetDelayRange(minMs, maxMs)              -- ms spread
SetQ(value)                              -- shared Q for every chain not overridden below
```

Chain `i` (0-based, out of the active `Chains` count) sits at
`fraction = i / (Chains - 1)` along each range, either linearly interpolated or, for
frequency with `distribution = 1`, log-interpolated between `low` and `high`.

### Per-chain fine control

```lua
SetResonanceBody(index, { freq = .., decay = .., gainDb = .., q = .., delayMs = .. })
```

Overrides one chain (0-based `index`, up to `Chains - 1`) with any subset of fields; a field
left out stays under aggregate control for that chain. The override is **persistent**, not
drained - it keeps winning over the aggregate controls for whichever fields it set until that
same chain is overridden again (with a wholesale replacement, not a merge: a second
`SetResonanceBody` call on the same index clears every field the first call set that the
second doesn't repeat).

### Default (stub) script

Out of the box, a fresh patch mimics the plugin's old dial-driven behavior as plain Lua
variables you can edit directly, plus one live behavior: it retunes the whole frequency range
around whatever pitch it detects in the incoming audio.

```lua
DecayMin = 0.3
DecayMax = 4.0
GainMin = -18
GainMax = 0
DelayMin = 0
DelayMax = 300
Q = 6
Distribution = 1 -- 0 = linear, 1 = logarithmic

function OnPitchDetected(hz, confidence)
    if hz > 0 and confidence > 0.5 then
        SetFreqRange(hz, hz * 8, Distribution)
    end
end
```

### Example: `base-scripts/pitch-follow-resonance.lua`

A fuller showcase: harmonizes the resonator bank to the incoming signal's detected pitch,
quantized to a selectable key (`Root`/`Scale` dials declared via `UICreateParameterSet`, see
`../../LUA-MANUAL.md`), and sweeps a moving window of individually-tuned chains across the whole bank
each pitch-detection hop, so a large number of chains stay actively - and distinctly - engaged
over time instead of always retuning the same handful. Load it directly as a patch script to
try it, or read it as a template for a more elaborate per-chain scripting scheme than the
stub's aggregate-only defaults.
