# Metronome

A JUCE standalone metronome with damped-sine click sounds, odd-meter support, and a drop-bars
mute feature for timing training.

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
| Swing | 1.0 – 2.0 | Swing ratio (only shown for shuffle/swing presets) |
| Drop Bars | see table below | Bars heard vs. bars silent |
| Metro Volume | −60 – 0 dB | Click loudness |
| Sub Volume | −60 – 0 dB | Subdivision tick loudness |
| Input Volume | −60 – +12 dB | Pass-through instrument level |
| Start | on/off | Starts or stops the metronome |

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
