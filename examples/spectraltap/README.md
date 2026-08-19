# Spectraltap

A Lua-scripted multitap delay: up to 24 taps share one delay buffer, each independently
timed, panned, and shaped by one of seven spectral voice types (bandpass, lowpass,
highpass, notch, resonator, formant, or comb resonator), plus a plain bypass tap. A
patch's script owns topology (how many taps, where, what type) and every tap's
frequency/resonance/formant/gain/pan target; the engine owns Hz-to-coefficient mapping,
smoothing, and DSP safety.

## Controls

| Control | Range | Description |
|---|---|---|
| Dry | -100 - 12 dB | Dry (unprocessed input) level |
| Wet | -100 - 12 dB | Wet (tap bank) level |
| BPM | 40 - 250 | Manual tempo dial, disabled and forced to the host's own tempo while Host Sync is on. Feeds the script's `OnTiming` hook - see Scripting below. |
| Host Sync | on/off | When on, `OnTiming`'s `bpm` follows the host's transport tempo instead of the BPM dial (which greys out and displays the host value). |
| Division | 13 choices (1/1 .. 1/16T) | Passed to `OnTiming` as a 0-based index; the script owns interpreting it (see the stub's `kDivisionBeats` table below). |
| Script | (button) | Opens the popup editor for the current patch's script. While LLM-Assist is active it opens read-only instead (Apply/Reset disabled) so a manual edit can't race a watched-folder pull, and its text stays live-updated as pulls happen. The editor's own Reset button replaces the text with a full skeleton (every available hook, stubbed out) - Cancel discards it, Apply commits it. A dropdown in the editor also lets you view any installed library script, always read-only. |

**Settings > Scripts** manages a named pool of saved scripts (Load / Save As / Delete / Rename),
separate from the script embedded in the current patch. **Settings > Patches** saves/loads full
patches, including whichever script is currently applied.

A script can also pull in a shared library script with `import "name"` (see `../../LUA.md`) -
useful for boilerplate reused across several patches.

## Scripting

This section covers what's specific to Spectraltap: the topology calls (`SetMaxTaps`/`SetTap`),
the five real-time per-tap setters (`SetFrequency`/`SetResonance`/`SetFormant`/`SetPan`/
`SetGain`), and the `OnTiming` hook the BPM/Host Sync/Division controls drive. For everything
shared with any other Lua-scripted example - MIDI handlers, `OnStart`/`OnStop`, dynamic UI
parameters, the `Music`/`Vel`/`Rr`/`Rhythm` helper library, `Timer`, `Transport`, and
sandbox/error-handling notes - see `../../LUA.md`.

### `OnTiming`: BPM / Host Sync / Division

```lua
function OnTiming(bpm, divisionIndex) end
```

Fires once at startup and again whenever the BPM dial, Host Sync switch, or Division
dropdown changes (not every block, so a host BPM-automation ramp doesn't run it
constantly). `bpm` is the *effective* tempo those three controls produce - the manual BPM
dial value, or the host's own transport tempo while Host Sync is on (the dial itself greys
out and shows that value) - which is a **different number from the shared
`Transport.Tempo()`** every Lua example gets (always the raw host tempo, regardless of
Host Sync). `divisionIndex` is 0-based into the Division dropdown (`0` = "1/1" through
`12` = "1/16T"); the engine only uses it to gate this dial's own enable state, so a script
that wants the actual beat multiplier keeps its own lookup table - see the stub script's
`kDivisionBeats` below.

### DSP architecture

Every tap reads the same shared delay buffer at its own configured offset (up to 24 taps,
`kMaxTaps`), runs its own spectral voice on that tap-delayed signal, then gets
constant-power-panned and summed:

```
wet = sum(tapGain[i] * Pan[i](Voice[i](delayedInput[i])))
out = dryGain * input + wetGain * wet
```

There is no feedback between taps - every tap is a parallel, feed-forward voice off the
one shared buffer. Dry stays centered regardless of any tap's pan.

### Topology: `SetMaxTaps` / `SetTap`

```lua
SetMaxTaps(n)                              -- active tap count, 0..24 (kMaxTaps)
SetTap(index, delayMs, type, level, pan)   -- 0-based index, up to kMaxTaps - 1
```

`SetMaxTaps`/`SetTap` are topology-tier: not sample-accurate, so call them when
(re)configuring a patch (on load, on a scale/BPM change), not as a per-sample automation
path. `SetTap` itself is still click-free, though: each logical tap owns two delay-read/
voice/comb slots under the hood; a retune configures the currently-inactive slot fresh and
crossfades it in (~30 ms) while the previous one fades out, rather than resetting the
delay position and filter/comb state in place. A tap's very first `SetTap` applies directly
(nothing audible to fade from yet). All state for both slots of all 24 taps is preallocated
at construction; `SetMaxTaps` only raises/lowers how many logical taps are active, it never
allocates. Shrinking the active tap count still stops any dropped tap abruptly, not faded -
only a *retune* of an already-active tap is click-free.

`delayMs` is milliseconds, clamped to `[0, 4000]`. `level` and `pan` seed the same smoothed
targets `SetGain`/`SetPan` update in real time (see below) - they don't jump instantly, they
ramp the same way a real-time call would. An out-of-range `index` (>= 24) or unknown `type`
is silently ignored, leaving whatever topology was already active untouched - it never
corrupts another tap's state.

`type` is a plain integer, not a string:

| `type` | Voice | Tuned by |
|---|---|---|
| 0 | Bypass | (none - passes the delayed tap through unshaped) |
| 1 | LowPass | `SetFrequency`/`SetResonance` |
| 2 | HighPass | `SetFrequency`/`SetResonance` |
| 3 | BandPass | `SetFrequency`/`SetResonance` |
| 4 | Notch | `SetFrequency`/`SetResonance` |
| 5 | Resonator | `SetFrequency`/`SetResonance` |
| 6 | Formant | `SetFormant` |
| 7 | CombResonator | `SetFrequency`/`SetResonance` |

### Real-time per-tap setters

Safe to call every block: validated/clamped, smoothed, never allocate.

```lua
SetFrequency(index, fHz)                                     -- centre/fundamental Hz
SetResonance(index, fHz, decayTimeSeconds)                    -- frequency + decay time
SetFormant(index, fHz, f1Factor, f1Gain, f2Factor, f2Gain)    -- Formant only
SetPan(index, pan)                                             -- -1..1
SetGain(index, gain)                                           -- linear
```

| Parameter | Unit / range | Notes |
|---|---|---|
| `fHz` | Hz, clamped `[1, 20000]` | Smoothed in log2 space, once per block (not per sample) - so a frequency sweep never zippers, and coefficient recomputation stays cheap even with 24 taps active. |
| `decayTimeSeconds` | seconds, clamped `[0.001, 20]` | Filter taps: maps to Q via the same `pi*f*decay*k` relation `SvfResoBP` uses elsewhere in this repo, so "decay" means the same thing on every resonant tap type; Q itself is clamped to `[0.05, 40]`. CombResonator: passed straight to `AbacDsp::CombResonator::setByDecay()` in `src/includes/Filters/CombResonator.h` - `feedback = exp(-D / (decay * sampleRate))`, `D` the loop delay in samples, feedback clamped below 1, with loop damping and a `tanh` soft limiter always active in the feedback path. |
| `f1Factor`/`f2Factor` | clamped `[0.1, 16]` | `F1 = f1Factor * F0`, `F2 = f2Factor * F0`; each derived frequency is independently log-smoothed and clamped to the same 20 kHz ceiling as `fHz`, so a factor that would push a section above Nyquist is safely capped rather than left to diverge. |
| `f1Gain`/`f2Gain` | linear, clamped `[0, 4]` | Not dB. `F0`'s own gain is a fixed unity; the three sections sum and normalize by `1 / (1 + f1Gain + f2Gain)` so overall loudness doesn't grow as gains change. |
| `gain` | linear, clamped `[0, 4]` | Short linear smoothing. |
| `pan` | clamped `[-1, 1]` | Constant-power: `gL = cos((pan+1)*pi/4)`, `gR = sin((pan+1)*pi/4)` - the two channel gains are what's smoothed, not the raw pan value, so a pan sweep never dips in level. |

Calling `SetFrequency`/`SetResonance`/`SetFormant`/`SetGain`/`SetPan` on a tap whose type
doesn't use that parameter is harmless (e.g. `SetFormant` on a BandPass tap is simply never
read) - each tap only consults the setters relevant to its current `type`.

A non-finite (NaN/Inf) argument to any call above is rejected outright, leaving whatever
was already active in place - the same "invalid input is a no-op, not a crash" contract
`SetTap` has for a bad index/type.

### Default (stub) script

Out of the box, a fresh patch sets up 4 scale-sequenced taps and keeps them synced to
BPM/Host Sync/Division:

```lua
UICreateParameterSet({
    { id = "root", name = "Root", type = "drop",
      items = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" }, default = 0 },
    { id = "scale", name = "Scale", type = "drop",
      items = { "Major", "NaturalMinor", "Dorian", "MajorPentatonic", "MinorPentatonic", "Blues", "WholeTone", "Chromatic" },
      default = 0 },
})

local kScaleNames = { "Major", "NaturalMinor", "Dorian", "MajorPentatonic", "MinorPentatonic", "Blues", "WholeTone",
                      "Chromatic" }

Root = 60          -- MIDI note (C4)
Scale = kScaleNames[1]
LastBpm = 120
LastDivisionIndex = 4

local kDivisionBeats = {
    [0] = 4, [1] = 2, [2] = 3, [3] = 4 / 3,
    [4] = 1, [5] = 1.5, [6] = 2 / 3,
    [7] = 0.5, [8] = 0.75, [9] = 1 / 3,
    [10] = 0.25, [11] = 0.375, [12] = 1 / 6,
}

function RetuneTaps(bpm, divisionIndex)
    SetMaxTaps(4)
    local beats = kDivisionBeats[divisionIndex] or 1
    local scale = Music.Scales[Scale]
    for i = 1, 4 do
        local delayMs = Rhythm.BeatsToMs(beats * i, bpm)
        local hz = Music.NoteToHz(Root + scale[degreeIndex[i]])
        SetTap(i - 1, delayMs, types[i], 0.8, pans[i])
        if types[i] == TapType.Formant then
            SetFormant(i - 1, hz, 2.0, 0.6, 3.5, 0.35)
        else
            SetResonance(i - 1, hz, 1.2)
        end
    end
end

function OnRootChanged(index)
    Root = 60 + index
    RetuneTaps(LastBpm, LastDivisionIndex)
end

function OnScaleChanged(index)
    Scale = kScaleNames[index + 1]
    RetuneTaps(LastBpm, LastDivisionIndex)
end

function OnTiming(bpm, divisionIndex)
    LastBpm = bpm
    LastDivisionIndex = divisionIndex
    RetuneTaps(bpm, divisionIndex)
end
```

Tap 0 is BandPass, tap 1 Resonator, tap 2 Formant, tap 3 Notch - one of each of the four
most commonly reached-for types - at the 1st/3rd/5th/7th degree of `Scale` above `Root`,
delayed at increasing multiples of the Division dropdown's beat value against the
effective BPM, alternating hard left/right pan. `Root` and `Scale` are exposed as Lua
Controls (the `UICreateParameterSet` call above) - that's what populates the Lua Controls
area on the performance page - so they're reachable without opening the script editor;
changing either, or just turning the BPM dial / toggling Host Sync / changing Division
(all of which retune automatically via `OnTiming`), retimes and retunes every tap, click-free,
without any audio-thread allocation. See `impl/SpectraltapScriptEngine.h`'s
`kStubScript`/`kSpectraltapSkeletonHooks` for the exact source.

The same script is also shipped as `base-scripts/scale-sequenced-multitap.lua`, so it shows
up as a regular file in the script editor's library dropdown and can be loaded directly via
**Settings > Scripts** instead of only being the compiled-in default - handy as a starting
point to copy and diverge from without losing the original.
