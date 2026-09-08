# Classic-beats reference-groove profiles

Aggregate statistics only, derived from copyrighted reference packs via `analyze_reference_grooves.py` - no note sequence from those packs is reproduced here or in the generated grooves; these numbers only inform what's genre-idiomatic (typical kick/snare/hihat placement, tempo range, ghost-note and fill density). See `.claude/classic-beats-full-kit.md` for the genre-to-source-folder mapping rationale.

Grid columns are 16th-note steps 1-16 of a 4/4 bar; symbols are the fraction of analyzed files with at least one hit of that category at that step: `#` >=80%, `+` >=50%, `:` >=20%, `.` <20%. Non-4/4 files are folded into the same 16-step grid on a proportional basis, so treat the grid as a rough shape, not a literal 16th-note transcription.

## Rock
Source folders: MidiDrums/High Energy Grooves, MidiDrums/British Invasion Grooves, MidiDrums/AOR Grooves, MidiDrums/Action Drums, /Users/scjurgen/projects/recover/modabacad/MidiDrums/000329@ACTION_DRUMS, /Users/scjurgen/projects/recover/modabacad/MidiDrums/000330@BRITISH_INVASION_GROOVES, /Users/scjurgen/projects/recover/modabacad/MidiDrums/000336@HIGH_ENERGY_GROOVES, /Users/scjurgen/projects/recover/modabacad/MidiDrums/000329@AOR_GROOVES
Files analyzed: 367 (367 variation-1 blueprints)
BPM: 55-240 (median 120)
Feel distribution: straight:83%, swing:17%
Snare/rimshot ghost-note rate: 41%
Hihat open ratio: 23%
Last-bar density vs. other bars: 1.0x
Hit-probability grid:
  snare      `:::++::++::++:+:`
  tom        `...........::::.`
  hihat      `#:+++::++:+++::+`
  cymbal     `:...:..::.:.:...`
  percussion `................`

## Blues
Source folders: /Users/scjurgen/projects/recover/modabacad/MidiDrums/000352@THE_BLUES, /Users/scjurgen/projects/recover/modabacad/MidiDrums/000330@BLUES
Files analyzed: 250 (154 variation-1 blueprints)
BPM: 68-230 (median 122)
Feel distribution: unknown:100%
Snare/rimshot ghost-note rate: 27%
Hihat open ratio: 30%
Last-bar density vs. other bars: 1.3x
Hit-probability grid:
  snare      `::+++:+::+++#++:`
  tom        `..........:.:...`
  hihat      `#.:+#.::+::+#.::`
  cymbal     `:.:.:.:.:.:.:.:.`
  percussion `................`

## Jazz Swing
Source folders: MidiDrums/Urban Jazz Grooves, /Users/scjurgen/projects/recover/modabacad/MidiDrums/000354@URBAN_JAZZ_GROOVES
Files analyzed: 250 (56 variation-1 blueprints)
BPM: 75-120 (median 95)
Feel distribution: straight:78%, swing:22%
Snare/rimshot ghost-note rate: 55%
Hihat open ratio: 5%
Last-bar density vs. other bars: 1.0x
Hit-probability grid:
  snare      `++:+++::++:++++:`
  tom        `................`
  hihat      `+:+:+:+:+:+:::+:`
  cymbal     `................`
  percussion `................`

## Latin
Source folders: /Users/scjurgen/projects/recover/modabacad/MidiDrums/000341@LATIN_JAZZ_GROOVES
Files analyzed: 250 (58 variation-1 blueprints)
BPM: 108-130 (median 111)
Feel distribution: straight:88%, swing:12%
Snare/rimshot ghost-note rate: 50%
Hihat open ratio: 18%
Last-bar density vs. other bars: 0.9x
Hit-probability grid:
  snare      `:::::::+::::::::`
  tom        `.:...:.::::::::.`
  hihat      `+::+:+:::+:+:+::`
  cymbal     `+:::::::::::::::`
  percussion `................`

## Funk
Source folders: MidiDrums/Modern Funk Grooves, MidiDrums/Funk, /Users/scjurgen/projects/recover/modabacad/MidiDrums/000343@MODERN_FUNK_GROOVES, /Users/scjurgen/projects/recover/modabacad/MidiDrums/000334@FUNK
Files analyzed: 250 (183 variation-1 blueprints)
BPM: 63-198 (median 115)
Feel distribution: straight:76%, swing:24%
Snare/rimshot ghost-note rate: 57%
Hihat open ratio: 9%
Last-bar density vs. other bars: 1.0x
Hit-probability grid:
  snare      `:+:+++++++:+++++`
  tom        `................`
  hihat      `++++++++++++++++`
  cymbal     `::::::::::::::::`
  percussion `................`

## Fusion
Source folders: MidiDrums/Metal Fusion, MidiDrums/Deathlike Fusion, MidiDrums/Fusion Grooves, MidiDrums/Progressive Fusion, MidiDrums/Linear Fusion, /Users/scjurgen/projects/recover/modabacad/MidiDrums/000343@METAL_FUSION, /Users/scjurgen/projects/recover/modabacad/MidiDrums/000332@DEATHLIKE_FUSION, /Users/scjurgen/projects/recover/modabacad/MidiDrums/000334@FUSION_GROOVES, /Users/scjurgen/projects/recover/modabacad/MidiDrums/000346@PROGRESSIVE_FUSION, /Users/scjurgen/projects/recover/modabacad/MidiDrums/000342@LINEAR_FUSION
Files analyzed: 416 (416 variation-1 blueprints)
BPM: 78-218 (median 117)
Feel distribution: straight:77%, swing:23%
Snare/rimshot ghost-note rate: 63%
Hihat open ratio: 10%
Last-bar density vs. other bars: 1.0x
Hit-probability grid:
  snare      `++++++++++++++++`
  tom        `................`
  hihat      `+:::::::+:::::::`
  cymbal     `:.:::::::.::::::`
  percussion `................`

## Hip-Hop
Source folders: MidiDrums/Hip-Hop Offbeats, /Users/scjurgen/projects/recover/modabacad/MidiDrums/000336@HIP-HOP_OFFBEATS, /Users/scjurgen/projects/recover/modabacad/MidiDrums/000336@HIP-HOP_GROOVES, /Users/scjurgen/projects/recover/modabacad/MidiDrums/1909@EZX_HIP-HOP!
Files analyzed: 250 (176 variation-1 blueprints)
BPM: 62-158 (median 89)
Feel distribution: unknown:99%, swing:1%
Snare/rimshot ghost-note rate: 26%
Hihat open ratio: 5%
Last-bar density vs. other bars: 1.0x
Hit-probability grid:
  snare      `.::+#::::::+#:::`
  tom        `................`
  hihat      `+:+:+:+:+:+:+:+:`
  cymbal     `................`
  percussion `................`

## Progressive
Source folders: MidiDrums/Progressive Fusion, MidiDrums/The Progressive Foundry, MidiDrums/Progressive, /Users/scjurgen/projects/recover/modabacad/MidiDrums/000352@THE_PROGRESSIVE_FOUNDRY, /Users/scjurgen/projects/recover/modabacad/MidiDrums/000346@PROGRESSIVE_FUSION, /Users/scjurgen/projects/recover/modabacad/MidiDrums/1912@EZX_PROGRESSIVE, /Users/scjurgen/projects/recover/modabacad/MidiDrums/000346@PROGRESSIVE_METAL
Files analyzed: 250 (229 variation-1 blueprints)
BPM: 67-180 (median 100)
Feel distribution: straight:72%, swing:16%, unknown:13%
Snare/rimshot ghost-note rate: 53%
Hihat open ratio: 23%
Last-bar density vs. other bars: 1.0x
Hit-probability grid:
  snare      `++:+++++++++++++`
  tom        `................`
  hihat      `+::::::::::::::+`
  cymbal     `+:::::::::::::::`
  percussion `................`

## Reggae
Source folders: MidiDrums/Reggae Beats, /Users/scjurgen/projects/recover/modabacad/MidiDrums/1907@EZX_REGGAE, /Users/scjurgen/projects/recover/modabacad/MidiDrums/000350@REGGAE_BEATS
Files analyzed: 250 (66 variation-1 blueprints)
BPM: 100-148 (median 122)
Feel distribution: straight:56%, shuffle:44%
Snare/rimshot ghost-note rate: 24%
Hihat open ratio: 6%
Last-bar density vs. other bars: 1.0x
Hit-probability grid:
  snare      `:...:..:+...:.:.`
  tom        `................`
  hihat      `+:+++:+:+:::+:::`
  cymbal     `................`
  percussion `................`

## Pop
Source folders: MidiDrums/UK Pop Grooves, MidiDrums/Indiependent, MidiDrums/Sixties Pop Grooves, MidiDrums/Singer Songwriter Grooves, /Users/scjurgen/projects/recover/modabacad/MidiDrums/000351@SINGER_SONGWRITER_GROOVES, /Users/scjurgen/projects/recover/modabacad/MidiDrums/16@EZX_NUMBER_1_HITS, /Users/scjurgen/projects/recover/modabacad/MidiDrums/000351@SIXTIES_POP_GROOVES, /Users/scjurgen/projects/recover/modabacad/MidiDrums/000353@UK_POP_GROOVES, /Users/scjurgen/projects/recover/modabacad/MidiDrums/1916@EZX_DREAM_POP, /Users/scjurgen/projects/recover/modabacad/MidiDrums/000337@INDIEPENDENT
Files analyzed: 316 (316 variation-1 blueprints)
BPM: 60-178 (median 113)
Feel distribution: straight:77%, swing:23%
Snare/rimshot ghost-note rate: 38%
Hihat open ratio: 13%
Last-bar density vs. other bars: 1.0x
Hit-probability grid:
  snare      `:::++::::::++:::`
  tom        `...........::::.`
  hihat      `+:+++:+++:+++:++`
  cymbal     `:.............:.`
  percussion `................`

## Soul
Source folders: MidiDrums/Soul Grooves, MidiDrums/Gospel Grooves, MidiDrums/Urban Jazz Grooves, /Users/scjurgen/projects/recover/modabacad/MidiDrums/000335@GOSPEL_GROOVES, /Users/scjurgen/projects/recover/modabacad/MidiDrums/000331@CONTEMPORARY_R&B_GROOVES, /Users/scjurgen/projects/recover/modabacad/MidiDrums/000354@URBAN_JAZZ_GROOVES, /Users/scjurgen/projects/recover/modabacad/MidiDrums/000351@SOUL_GROOVES
Files analyzed: 291 (291 variation-1 blueprints)
BPM: 63-224 (median 102)
Feel distribution: straight:60%, swing:40%
Snare/rimshot ghost-note rate: 37%
Hihat open ratio: 12%
Last-bar density vs. other bars: 1.0x
Hit-probability grid:
  snare      `.:.++:.::::++:::`
  tom        `................`
  hihat      `+::++::++::+::::`
  cymbal     `................`
  percussion `................`
