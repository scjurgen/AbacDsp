# Metronome

A JUCE standalone metronome with damped-sine click sounds, odd-meter support, and a drop-bars
mute feature for timing training. A Lua script can replace the played pattern (several
instruments per beat, each with its own dB level) and the positions the timing analysis
measures against; see Scripting below.

## Purpose

| Use case | How |
|---|---|
| Timing accuracy | Lock to the click; use the waveform display to see how tightly you land on the beat |
| Feel & groove | Switch to a shuffle or swing preset and adjust the swing ratio |
| Laid-back / anticipation | Use the waveform display — it shows one full beat of context so you can see whether you consistently play ahead or behind |
| Drop-bar practice | Set a drop-bars mode; the metronome goes silent for N bars so you must keep internal time, then checks you on the return |

## Controls

| Control | Range | Description |
|---|---|---|
| BPM | 40 – 250 | Tempo |
| Preset | see table below | Rhythm and subdivision feel |
| Voicing | see table below | Click sound, or a drum-kit voicing for beats and subdivisions |
| Drum Kit | 808 / Reggae / Pocket | Which `samples/drums/<kit>` sample set the voicing plays through |
| Swing | 1.0 – 2.0 | Swing ratio (only shown for shuffle/swing presets) |
| Drop Bars | see table below | Bars heard vs. bars silent |
| Metro Volume | −60 – 0 dB | Click loudness |
| Sub Volume | −60 – 0 dB | Subdivision tick loudness |
| Input Volume | −60 – +12 dB | Pass-through instrument level |
| Start | on/off | Starts or stops the metronome |
| Analysis | on/off | Starts or stops a timing-analysis take (see below) |
| Analysis Grid | Quarter / 8th / Triplet / Shuffle / 16th | Grid the analysis measures against (ignored while a script defines analysis positions) |
| Scripts menu | - | Edit, load, save and manage the pattern script (see Scripting) |

## Rhythm Presets

Each felt beat is one pulse at the set BPM. For compound and odd meters the pulse is an eighth
note; for 3/4 and 4/4 it is a quarter note.

| Preset | Beats | Subdivisions | Notes |
|---|---|---|---|
| 3/4 | 3 | none | plain quarter-note waltz |
| 3/4 8th | 3 | 8th | adds an eighth between each beat |
| 3/4 16th | 3 | 16th | adds three sixteenths between each beat |
| 3/4 shuffle | 3 | swing 8th | swing ratio adjustable |
| 3/4 triplet | 3 | triplet | two triplet subdivisions per beat |
| 4/4 | 4 | none | |
| 4/4 8th | 4 | 8th | |
| 4/4 16th | 4 | 16th | |
| 4/4 shuffle | 4 | swing 8th | |
| 4/4 triplet | 4 | triplet | |
| 4/4 swing | 4 | swing 8th | alias for shuffle with swing ratio focus |
| 5/4 (3+2) | 5 | none | accent groups: 3 then 2 |
| 5/4 8th (3+2) | 5 | 8th | |
| 5/4 (2+3) | 5 | none | accent groups: 2 then 3 |
| 5/4 8th (2+3) | 5 | 8th | |
| 6/8 in-2 | 2 | compound (×3) | felt as two dotted-quarter beats |
| 6/8 in-6 | 6 | none | felt as six eighth notes; D·s·s·B·s·s |
| 7/8 (2+2+3) | 7 | none | D·s·B·s·B·s·s |
| 7/8 (2+3+2) | 7 | none | D·s·B·s·s·B·s |
| 7/8 (3+2+2) | 7 | none | D·s·s·B·s·B·s |
| 9/8 in-3 | 3 | compound (×3) | felt as three dotted-quarter beats |
| 9/8 in-9 | 9 | none | D·s·s·B·s·s·B·s·s |
| 11/8 (3+3+3+2) | 11 | none | D·s·s·B·s·s·B·s·s·B·s |
| 11/8 (3+3+2+3) | 11 | none | D·s·s·B·s·s·B·s·B·s·s |
| 13/8 (3+3+3+2+2) | 13 | none | D·s·s·B·s·s·B·s·s·B·s·B·s |
| 13/8 (3+4+3+3) | 13 | none | D·s·s·B·s·s·s·B·s·s·B·s·s |

Legend: **D** = downbeat, **B** = beat, **s** = subdivision tick

## Drum Voicings

Voicing swaps the damped-sine click for real drum-kit hits from the synthesized,
copyright-free 808 kit (`samples/drums/808`, see `documentation/DrumKit808`). Click is the
default and unchanged; every other voicing replaces the click and subdivision tick with
samples, played through the existing accent hierarchy (Downbeat 1.0, Beat 0.85, Sub 0.55
gain) instead of a different sound.

A single alternation idiom drives every voicing without any per-meter special case: a
counter increments on every beat and resets each bar, so the downbeat is always count 0,
even counts play "Beat A" and odd counts play "Beat B". For a two-instrument voicing on a
4/4 preset that lands kick on 1 and 3, snare on 2 and 4, the way a real drummer plays it. On
an odd-beats-per-bar preset the alternation still applies beat to beat, just without that
clean 2-and-4 grouping; which beat opens on A vs. B then also flips from bar to bar.

| Voicing | Beat A | Beat B | Subdivision |
|---|---|---|---|
| Click | - | - | - |
| Kick | Kick | Kick | - |
| Kick + HH | Kick | Kick | Closed hihat |
| HH only | Closed hihat | Closed hihat | Ghost hihat |
| Kick Snare HH | Kick | Snare | Closed hihat |
| Timbal | Timbale 1 | Timbale 2 | Damped timbale |
| Tom | Low tom | Mid tom | High tom |
| Wood | Woodblock | Woodblock | Woodblock |
| Sticks | Sidestick | Sidestick | Sidestick |
| Shaker offbeat | - | - | Shaker |

Each sample code round-robins through its own take pool on every trigger, so repeated hits
don't sound mechanically identical. "Wood" and "Sticks" use the same instrument for Beat A
and Beat B, so the alternation idiom has no audible effect for them. "Shaker offbeat" leaves
the beat itself silent; only the subdivision grid sounds.

## Drop Bars

Silence the click for N bars to train internal pulse.

| Setting       | Cycle (■ = heard, □ = silent) |
|---------------|---|
| Drop none     | ■ ■ ■ ■ ■ ■ ■ ■ ■ ■ ■ ■ … |
| Play 1 Drop 1 | ■ □ ■ □ ■ □ ■ □ ■ □ ■ □ … |
| Play 3 Drop 1 | ■ ■ ■ □ ■ ■ ■ □ ■ ■ ■ □ … |
| Play 2 Drop 2 | ■ ■ □ □ ■ ■ □ □ ■ ■ □ □ … |
| Play 1 Drop 3 | ■ □ □ □ ■ □ □ □ ■ □ □ □ … |

The cycle always restarts on the downbeat of the next heard bar.

## Timing Analysis

Switching Analysis on starts listening to the input signal for onsets and measures each
onset's distance to the nearest grid position, in milliseconds (negative = late, positive =
early). The grid is the Analysis Grid control (quarters, 8ths, triplets, shuffle or 16ths of the
Preset's bar) or, when a script defines them, the script's analysis positions. Onset detection
uses a hysteresis (two-threshold) envelope: a hit fires when the level rises past the upper
threshold, and it only re-arms once the level has since dropped past a lower one. This keeps a
held or strummed chord's rippling sustain from re-triggering as several hits, at the cost of a
known tradeoff: a genuinely new attack played while the previous one is still loud (within the
hysteresis band) will not register as a separate hit.

Only onsets within 70 ms of a grid position (a 140 ms window centered on it) are measured.
Anything farther away, such as ghost notes or other material, is left out of all statistics and
the deviation histogram and is counted in the report's "Ignored" card instead. When grid
positions are less than 140 ms apart (16ths at 120 BPM or faster), every onset is within the
window and nothing is ignored.

Switching Analysis back off writes an HTML report and opens it in the default browser. The
report contains:

- Summary cards: hit count, mean and standard deviation of the measured deviations, tempo and
  preset, the analysis grid, and the number of ignored onsets.
- A full-bar hit distribution: onset density in 10 ms bins across the whole bar, with a solid line
  at every beat, a dashed line at every other grid position, and color-coded timing-quality zones
  (Locked, Very tight, Tight, Loose, Off) around each position. The ignored onsets are drawn in
  gray on top of the measured ones, so you can see where they happen.
- An overall deviation histogram in 10 ms bins. Its x-axis is fixed at +/- half a beat at the
  take's tempo (e.g. +/-333 ms at 90 BPM), so it never auto-zooms to the data.
- A table with one row per grid position (Beat 1, Beat 1 - Off-beat or Sub 1, Sub 2 for the
  built-in grids, Beat 2 + 0.5 for script positions): hit count, mean and standard deviation, and
  a comment combining tightness (spread) and direction (dragging, laid-back, pushing, rushing),
  so uneven timing on one beat or off-beat shows up on its own.

Each Analysis toggle resets the collected data, so one on/off cycle is one take (up to 4096 hits;
further hits are not recorded once that many have been collected). The page's charts are plain
inline SVG; its layout uses Bootstrap loaded from a CDN, so it needs network access to render
correctly. Reports are written to `~/Documents/Metronome Analysis/`, one timestamped file per take.

## Scripting

A script programs what the metronome plays and, independently, what the timing analysis
measures against. The script runs once on every Apply and on every Preset change; its top-level
code calls the four functions below. Nothing in a script runs on the audio thread. Edit scripts
from the Scripts menu; the library dropdown of the editor offers the shipped examples
(`base-scripts/`).

Without a script, or with a script that calls none of these functions, the Preset, Voicing and
Analysis Grid controls behave as described above. Calling `ClearPattern` or `AddInstrument`
replaces the Preset/Voicing sound with the script's pattern; calling `ClearAnalysis` or
`AddAnalysisPosition` replaces the Analysis Grid. An analysis grid left empty falls back to the
Analysis Grid control.

| Function | Meaning |
|---|---|
| `ClearPattern()` | Empties the played pattern; the metronome is silent until `AddInstrument` is called. |
| `AddInstrument(position, instrument, levelDb)` | One drum hit. Several hits at one position play together (kick and hihat on 1). |
| `ClearAnalysis()` | Empties the analysis positions. |
| `AddAnalysisPosition(position)` | One bar position the played onsets are measured against. Duplicates are ignored. |

`position` is in beats from the bar start: 0 is beat 1, 0.5 the "and" of 1, 1 is beat 2, 2.75 the
last 16th of beat 3. The bar length is the Preset's beats per bar, available as the global
`BarBeats`; a position outside `0 <= position < BarBeats` is a script error. Positions are
literal: the Swing control only affects the built-in subdivisions.

`levelDb` is relative to the sample's own level (0 = as stored) and limited to -96 .. +12. The
Metro Volume control is the master on top of it; Sub Volume has no role for script patterns.

`instrument` is one of these global constants. Each plays the like-named sample of the selected
Drum Kit; a sample the kit lacks stays silent.

| Constant | Sample | Constant | Sample |
|---|---|---|---|
| `Kick` | bd | `Tom3` | tom3 |
| `Snare` | sd | `TomLow` | tomlo |
| `Rimshot` | rs | `Timbale1` | timb1 |
| `Sidestick` | sstick | `Timbale2` | timb2 |
| `Hihat` | hh | `TimbaleDamp` | timbdmp |
| `HihatOpen` | hhopen | `Ride` | ride |
| `HihatGhost` | hhghost | `Crash` | crash |
| `Wood` | wood | `ClickLow` | clicklow |
| `Clap` | clap | `ClickHigh` | clickhigh |
| `Shaker` | shaker | `Tom1`, `Tom2` | tom1, tom2 |
| `Tamb` | tamb | | |

```lua
ClearPattern()
for beat = 0, BarBeats - 1 do
    AddInstrument(beat, Hihat, -6)
    AddInstrument(beat + 0.5, Hihat, -12)
end
AddInstrument(0, Kick, 0)
AddInstrument(2, Kick, -2)
AddInstrument(1, Snare, -1)
AddInstrument(3, Snare, -1)

ClearAnalysis()
for i = 0, BarBeats * 4 - 1 do
    AddAnalysisPosition(i / 4)
end
```

Rules and limits:

- A pattern holds at most 256 hits, an analysis grid at most 128 positions; exceeding either is
  a script error, as is an unknown instrument.
- A new pattern starts at the next bar boundary while the metronome is running, immediately when
  it is stopped. A script that fails to load leaves the previous pattern playing and shows its
  error.
- Drop Bars still mutes a script pattern. The beat and spectrogram displays stay driven by the
  Preset.
- Changing the Preset reloads the script for the new `BarBeats`. The reload is picked up while
  the editor window is open.
- The functions only work at the top level of the script, not from handlers or timers.
- The report lists one row per analysis position (labelled "Beat 2 + 0.5" for a position inside
  a beat) and measures each onset within 70 ms of its nearest position, wrapping at the bar end.

Shipped scripts in `base-scripts/`: `rock-8th-groove`, `bossa-clave`, `five-over-four`,
`quarters-analyse-16ths` and `reggae`.
