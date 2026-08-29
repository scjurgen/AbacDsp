# Plan: metronome / rhythm-guide SMF library

Create a Python project that generates a curated library of Standard MIDI File Type 0 (.mid)
metronome, rhythm-guide, looper, and performance patterns, landing in this folder
(`documentation/Metronome/generated/`) for review before being copied into `MidiDrums/` (the
folder the looper's `GrooveKit` actually scans - confirmed below that its recursive scan and
"style = relative path" convention accept this plan's `<area>/<family>/<pattern-file>.mid`
nesting as-is, so no restructuring is needed at copy time).

This started as a separate, much larger catalog alongside `generate_metronome_midi.py`'s
earlier two-note-click-only "Metronome" groove generator. That script and its output have since
been retired (superseded, not kept in parallel) - `rhythm_library/` is now the only MIDI-pattern
generator in this folder, covering a broad educational/looper/performance rhythm-guide library
that uses the project's real drum-kit voices, not just two click notes. The click_low/click_high
WAV samples from `generate_click_samples.py` remain (see README.md) and are still available to
any pattern that wants a pure click sound.

## Project conventions adopted from this codebase (read, not assumed)

The original draft of this plan (see git history) assumed a generic/GM-flavored MIDI toolkit.
This codebase has its own conventions; the plan below is written against them:

- **Note numbers**: this project uses its own note map (`src/includes/Sampler/GrooveNoteMap.h`,
  mirrored in `MidiDrums/README.md`), not literal General MIDI drum notes. Every note choice in
  this plan is drawn from that map - see "Instrument palette" below. In particular there is no
  dedicated "claves" voice; clave/cascara patterns use Woodblock (56) and Rimshot (37) /
  Sidestick (71) as the closest existing timbres.
- **PPQN**: 480 ticks per quarter note. Checked: `GrooveMidiFile` (the C++ reader) takes
  ticks-per-quarter straight from each file's own header - any value works, so this isn't a
  hard requirement. The existing MidiDrums corpus mostly uses 9600; `generate_metronome_midi.py`
  uses 480. 480 is plenty of resolution (at 60 BPM, one tick is 1000/480 ≈ 2.08 ms, well under
  human timing-placement jitter) without the extra digits, so this library uses 480 too, matching
  the folder's existing script.
- **Section markers** (`track_name`, `text`, `marker` meta events - COUNT-IN, LOOP START,
  BAR N, TURNAROUND, FILL CUE, RETURN, etc.): included as originally planned. Checked:
  `GrooveMidiFile` only captures `set_tempo` and `time_signature` meta events; every other meta
  type parses without error but is discarded, so these markers currently have no effect on
  tapelooper's playback. Generate them anyway - they're free, correct, useful to a human opening
  the file in a DAW, and cost nothing if a future consumer wants to read them.
- **Mixed meter** (time-signature changes mid-file) is fully supported by the reader, which
  collects a full list of time-signature events, not just one at tick 0.
- **`dominantSounds`**: `MidiDrums/README.md` states this field's values must come from a fixed
  six-category vocabulary (`rimshot, snare, tom, hihat, cymbal, percussion`). This library
  doesn't enforce that - these are just standard MIDI files; how (or whether) a downstream
  consumer like the looper interprets sidecar fields is out of scope for the generator itself.
  Populate `dominantSounds` freely per pattern (can include `click_low`/`click_high` for the
  two click-only families, or instrument names outside the six-category list) without forcing
  compliance.
- **Output location for this pass**: `documentation/Metronome/generated/<area>/<family>/` -
  staged here for review, gitignored, same staging approach as `generate_metronome_midi.py`'s
  `generated/`. Moving the reviewed library into `MidiDrums/` is a later, separate step.

### Instrument palette (replaces the original draft's GM note table)

| Role | Note | GrooveTag | Notes |
|---|---:|---|---|
| Downbeat / grounded low pulse | 36 | Kick | |
| Normal click (click-only families) | 100 / 101 | ClickLow / ClickHigh | reserved for the pure two-note click families, matching `generate_metronome_midi.py` |
| Primary accent / backbeat | 38 | Snare | |
| Secondary accent | 40 | SnareAlt | |
| Rim/stick click, clave surrogate | 37 | Rimshot | |
| Side-stick texture, clave surrogate | 71 | Sidestick | |
| Light subdivision | 42 | HihatClosed | |
| Open/loose subdivision accent | 49 | HihatOpen | |
| Prominent count-in / section cue | 56 | Woodblock | |
| Ride-based jazz guide | 51 | Ride | |
| Crash cue (section/start) | 27 | Crash | |
| Distinguishable extra guide layer | 43 / 66 | Tom1 / Timbale1 | pick one per pattern family, not both |
| Dum (Middle Eastern) | 36 | Kick | |
| Tak (Middle Eastern) | 37 / 38 | Rimshot / Snare | |

Suggested velocities keep the original draft's six-tier scale (these are editorial choices, not
a system requirement): section/start cue 118, primary downbeat accent 108, secondary accent 96,
normal click 78, light subdivision 54, ghost/subtle guide 38.

Default note duration: 30 ticks for click events, 45 ticks for cue events. Ensure note-offs
don't collide incorrectly with a note-on at the same timestamp.

## Everything below is unchanged in scope from the original draft

Nothing from the original pattern catalog, folder taxonomy, or documentation requirements is
dropped. Only the note-number table, PPQN, and the two clarifications above (markers are
generated regardless of current runtime use; `dominantSounds` isn't vocabulary-constrained)
differ from the original text.

### Output layout

```
generated/
  <area>/
    <family>/
      <pattern-file>.mid
```

Three top-level areas: `educational`, `looper`, `performance`. Lowercase kebab-case directory
and filenames, e.g. `generated/educational/four-four/conventional-bar-marker.mid`.

Also generate:
- `generated/manifest.json`
- `generated/README.md`
- `tests/` that verify every generated MIDI file structurally and musically

Do not hand-author binary MIDI files. All files must be produced by a Python generator.

### MIDI file requirements

Every output `.mid` file must:
- Be Standard MIDI File format 0, exactly one `MTrk`, 480 ticks per quarter note.
- Begin at tick 0 with: `track_name`, a `text` meta event with a concise human-readable
  description, `time_signature`, `set_tempo`, then `marker` events where relevant.
- End with `end_of_track`.
- Be valid when reloaded by `mido.MidiFile()`.
- Encode musical time as integer ticks only; never accumulate floating-point time.
- Use short percussion Note On / Note Off pairs on channel 9 (zero-based), never a
  note-on-with-zero-velocity as the only representation.

Default nominal tempo: 120 BPM unless the pattern explicitly belongs to a slower or faster
idiomatic preset. Tempo is metadata, easily changed by users in a DAW; don't bake tempo changes
into the initial collection except for a small, clearly labelled educational tempo-ramp family
if implemented.

Represent each pattern declaratively in Python, preferably with dataclasses: `PatternSpec`,
`Meter`, `EventSpec`, `SectionSpec`, `PatternCategory`. A `PatternSpec` should include at least:
id, display_name, description, area, family, meter numerator/denominator, bpm, number of bars,
grid resolution in ticks, events (absolute musical positions before serialization), expected
loop length in ticks, tags, optional count_in_bars, optional section markers, optional
recommended-use text.

Implement a generic event scheduler: author events in absolute ticks or beat/subdivision units,
sort by absolute tick, convert to MIDI delta times only during serialization, handle
simultaneous events correctly, emit marker/text events at absolute positions as configured,
never rely on event insertion order alone for timing correctness.

### Pattern design principles

Distinguish: Pulse (basic temporal reference), Accent (beat grouping/bar start/stress),
Subdivision (internal rhythmic placement), Cue (loop boundary/count-in/section/form event),
Silence (intentional removal of reference points for internal-time training), Groove guide (a
sparse, non-full-drum-kit musical rhythm establishing feel).

Do not create a full drum groove library - the goal is a click/guide-track library with
musically meaningful, sparse percussion patterns, even though real kit voices are now available.

For every file: descriptive name including the main musical concept; textual metadata so a DAW
user can identify it after moving the file; tags in manifest.json; preserve meter via
`time_signature`; predictably sortable filenames.

Two-bar phrases for patterns whose identity depends on 3-side/2-side clave direction. Four-bar
loops for standard performance patterns unless a different length is musically fundamental.
Section markers: `COUNT-IN`, `LOOP START`, `BAR 1`..`BAR 4`, `TURNAROUND`, `FILL CUE`, `RETURN`.

### Folder taxonomy

```
generated/
  educational/
    fundamentals/  four-four/  three-four/  compound-meter/  odd-meter/  mixed-meter/
    subdivisions/  swing-shuffle/  syncopation/  internal-time/
    afro-cuban/  afro-diasporic/  middle-eastern/
  looper/
    four-four/  three-four/  compound-meter/  odd-meter/  mixed-meter/
    count-ins/  phrase-cues/  groove-guides/
  performance/
    straight/  swing-shuffle/  compound-meter/  odd-meter/
    afro-cuban/  afro-diasporic/  middle-eastern/  mixed-meter/  count-ins/
```

Exactly `area / family / midi file` - no additional meter folder below family. Not every family
needs every possible pattern, but aim for a coherent starter collection of roughly 75-110 files.

### Educational patterns

**A. Fundamentals** - flat-mechanical-4-4, conventional-bar-marker-4-4, strong-weak-duple-4-4,
two-beat-cut-time, downbeat-only-4-4, two-bar-downbeat, four-bar-phrase-marker, backbeat-2-and-4,
jazz-two-and-four, beat-one-and-three, offbeat-eighths, sparse-beat-one-and-three.

**B. Four-four** - beat-1-only, beats-1-and-3, beats-2-and-4, conventional-bar-marker, backbeat,
eighth-note-subdivision, sixteenth-note-subdivision, sixteenth-landmarks-beat-and-and,
sixteenth-landmarks-e-and-a, eighth-note-accent-every-3-subdivisions, quarter-note-triplet-guide,
3-over-4-guide, 4-over-3-guide, 5-over-4-guide, syncopated-anticipated-and-of-4,
displaced-click-on-e, displaced-click-on-and, displaced-click-on-a.

**C. Three-four** - waltz-downbeat, waltz-beat-1-only, waltz-2-and-3,
hemiola-accent-over-two-bars (3/4 with 2+2+2 grouping across two bars),
three-four-eighth-subdivision, three-four-offbeat-eighths.

**D. Compound meter** - six-eight-two-dotted-quarter-pulses, six-eight-eighth-note-subdivision,
six-eight-two-main-pulses-only, six-eight-shuffle-guide, nine-eight-three-plus-three-plus-three,
twelve-eight-four-dotted-quarter-pulses, twelve-eight-blues-guide, twelve-eight-sparse-main-pulses.

**E. Odd meter** - five-four-three-plus-two, five-four-two-plus-three, five-eight-three-plus-two,
five-eight-two-plus-three, seven-eight-two-plus-two-plus-three, seven-eight-two-plus-three-plus-two,
seven-eight-three-plus-two-plus-two, seven-four-two-plus-two-plus-three,
nine-eight-two-plus-two-plus-two-plus-three, eleven-eight-three-plus-three-plus-three-plus-two,
eleven-eight-two-plus-two-plus-three-plus-two-plus-two.

**F. Mixed meter** (two- or four-bar patterns; `time_signature` changes exactly at bar boundaries) -
alternating-3-4-and-4-4, alternating-4-4-and-3-4, alternating-6-8-and-3-4,
alternating-5-8-and-7-8, alternating-7-8-and-4-4, additive-2-2-3-and-3-2-2 (two bars, 7/8),
combined-odd-even-5-4-and-4-4, combined-odd-even-7-8-and-4-4.

**G. Subdivisions and feel** - straight-eighths, straight-sixteenths, eighth-note-triplets,
sixteenth-note-triplets, shuffle-basic, shuffle-sparse, swing-light-55-percent,
swing-medium-60-percent, swing-heavy-66-percent, swing-two-and-four, swung-offbeat-guide.
Keep the notated time signature at 4/4; encode actual event positions with long-short eighth
timing (never merely label straight eighths as swing); store swing ratio as a tag and in
manifest metadata.

**H. Internal time / challenge** - one-bar-click-one-bar-silent, two-bars-click-two-bars-silent,
four-bars-click-four-bars-silent, beat-one-only-four-bars, one-click-every-two-bars,
one-click-every-four-bars, missing-beat-2, missing-beat-3, missing-beat-4,
rotating-missing-beat, barline-only, offbeat-only, random-but-deterministic-sparse-4-4,
random-but-deterministic-sparse-7-8. Use a fixed random seed for anything labelled deterministic.

### Afro-Cuban, Afro-diasporic, and Middle Eastern material

Represented respectfully and accurately as rhythmic guide patterns, not generic "world music"
presets - a concise source/context note in the README explains these are introductory
pulse/clave guides, not substitutes for learning the traditions and performance practice. Use
Woodblock/Rimshot/Sidestick and low percussion rather than an entire kit (see instrument
palette above for the no-claves-voice substitution).

**A. Afro-Cuban** (two-bar 4/4 where required) - son-clave-3-2, son-clave-2-3, rumba-clave-3-2,
rumba-clave-2-3, cascara-basic, bossa-nova-clave-guide, songo-style-pulse-guide,
mozambique-style-pulse-guide. Explicitly tag `clave-3-2` or `clave-2-3`; explain in text
metadata which bar is the 3-side and which is the 2-side; use exact 16th-note-grid placement;
verify son and rumba differ at the final note of the 3-side - this distinction is central.

**B. Afro-diasporic / Brazilian** - bossa-nova-two-bar-guide, samba-surdo-pulse-guide,
samba-clave-like-guide (labelled a learning guide, not "the samba clave"),
6-8-afro-bell-basic, 12-8-afro-bell-basic, tresillo-3-3-2, cinquillo-guide, habanera-guide.
Neutral, specific names; avoid implying a single canonical rhythm for an entire tradition.

**C. Middle Eastern / Arabic rhythmic cycles** - maqsum-4-4-guide, baladi-4-4-guide,
saidi-4-4-guide, malfuf-2-4-guide, wahda-4-4-slow-guide, samaai-thaqil-10-8-guide,
karsilama-9-8-two-two-two-three, aksak-9-8-two-two-two-three. Dum = Kick (36), tak = Rimshot
(37) or Snare (38), optional light subdivision = HihatClosed (42). Before implementing named
cultural patterns, consult authoritative/educational references and encode their conventional
basic forms; add a `source_context` field in the pattern definition and manifest; don't
overclaim universality since regional and stylistic variants exist.

### Looper patterns

**A. Count-ins** - one-bar-count-in-4-4, two-bar-count-in-4-4, one-bar-count-in-3-4,
one-bar-count-in-6-8, one-bar-count-in-5-4-three-plus-two, one-bar-count-in-7-8-two-two-three,
two-bar-count-in-with-last-beat-cue. A distinct Woodblock/high cue on count-in beat 1; an extra
distinctive final-beat/subdivision cue where labelled; `COUNT-IN` and `LOOP START` markers.
(Generated as planned - see the note above about these markers currently having no runtime
effect in tapelooper; still correct, useful files.)

**B. Phrase cues** - four-bar-loop-standard (bar 1 stronger cue, ordinary bars 2-4),
four-bar-loop-with-turnaround-cue, eight-bar-loop-with-halfway-cue,
twelve-bar-loop-with-form-markers, 16-bar-loop-with-4-bar-phrase-markers,
four-bar-loop-backbeat-guide, four-bar-loop-jazz-two-and-four, four-bar-loop-one-click-per-bar,
four-bar-loop-six-eight, four-bar-loop-five-four-three-plus-two,
four-bar-loop-seven-eight-two-two-three.

**C. Groove guides** - sparse-rock-guide-4-4, sparse-funk-guide-4-4, sparse-jazz-guide-4-4,
sparse-shuffle-guide-4-4, sparse-bossa-guide, sparse-clave-guide-3-2, sparse-clave-guide-2-3,
sparse-6-8-guide, sparse-12-8-guide.

Every looper pattern: a clear loop start; marker events at every bar boundary; loop length in
bars and ticks in manifest; avoid busy patterns that would interfere with monitoring/recording.

### Performance patterns

Longer running guide tracks for live use, rehearsal, or playback with a lead track. Default
length: four bars for 4/4 and most common meters, two bars when the cultural rhythm's essential
direction requires it, eight bars for selected phrase-oriented patterns; a short one-bar
count-in only when the filename says count-in.

Central design rule: for looping four-bar patterns, bar 4 includes a recognisable but sparse
turnaround/fill cue leading to bar 1 (a small increase in subdivision density or a compact
pickup gesture, not a full drummer fill); bar 1 has a clearly distinguishable loop-start cue;
the loop is musically seamless from the final event through bar 1.

**A. Straight** - four-bar-straight-bar-marker, four-bar-straight-backbeat-guide,
four-bar-straight-one-and-three-guide, four-bar-straight-sparse-rock-guide,
four-bar-straight-sparse-funk-guide, eight-bar-straight-phrase-guide,
four-bar-straight-with-turnaround, four-bar-straight-with-last-bar-subdivision-lift,
four-bar-straight-with-pickup-cue, four-bar-count-in-and-loop.

**B. Swing and shuffle** - four-bar-jazz-two-and-four, four-bar-jazz-ride-like-guide,
four-bar-shuffle-guide, four-bar-swing-light, four-bar-swing-medium,
four-bar-swing-with-turnaround, eight-bar-jazz-phrase-guide.

**C. Compound and odd meter** - four-bar-six-eight-guide, four-bar-twelve-eight-blues-guide,
four-bar-five-four-three-plus-two, four-bar-five-four-two-plus-three,
four-bar-seven-eight-two-two-three, four-bar-seven-eight-two-three-two,
four-bar-seven-eight-three-two-two, four-bar-nine-eight-aksak-grouping,
four-bar-eleven-eight-guide.

**D. Afro-Cuban and Afro-diasporic** - two-bar-son-clave-3-2-loop, two-bar-son-clave-2-3-loop,
two-bar-rumba-clave-3-2-loop, two-bar-rumba-clave-2-3-loop, four-bar-bossa-guide,
four-bar-samba-pulse-guide, four-bar-6-8-afro-bell-guide, four-bar-12-8-afro-bell-guide,
four-bar-tresillo-guide, four-bar-habanera-guide.

**E. Middle Eastern** - four-bar-maqsum-guide, four-bar-baladi-guide, four-bar-saidi-guide,
four-bar-malfuf-guide, two-bar-samaai-thaqil-guide, four-bar-karsilama-guide.

**F. Mixed meter** - four-bar-alternating-3-4-4-4, four-bar-alternating-6-8-3-4,
four-bar-alternating-5-8-7-8, four-bar-alternating-7-8-4-4, eight-bar-mixed-meter-phrase-guide.

### Implementation details

Project layout (inside this folder, alongside `generate_click_samples.py`):

```
documentation/Metronome/
  rhythm_library/
    __init__.py
    model.py            # PatternSpec, Meter, EventSpec, SectionSpec, PatternCategory
    midi_writer.py
    patterns/
      __init__.py
      educational.py
      looper.py
      performance.py
      cultural.py
    generate.py
    validation.py
    manifest.py
    documentation.py    # README generation, see below
  tests/
    test_generation.py
    test_midi_structure.py
    test_timing.py
    test_catalog.py
    test_documentation.py
  generated/             # gitignored staging output (this run's PLAN territory)
```

CLI commands:
- `python -m rhythm_library.generate`
- `python -m rhythm_library.generate --output generated`
- `python -m rhythm_library.generate --area educational`
- `python -m rhythm_library.generate --family odd-meter`
- `python -m rhythm_library.generate --clean`
- `python -m rhythm_library.validation generated`

Manifest.json entry per file: relative_path, id, display_name, area, family, description, meter
(or meter sequence), BPM, PPQN, bar_count, duration_ticks, duration_beats, count_in_bars,
loopable, grouping (e.g. `[2, 2, 3]`), swing_ratio if applicable, tags, instruments/note mapping
used, marker positions, source_context if applicable, recommended_use.

`generated/README.md`: installation/generation commands; sound symbol / note definitions
(using this project's own note map, not GM); folder taxonomy; a guide to choosing a pattern for
practice/looping/live performance; an explanation that SMF0 has one track but can still carry
channel and meta events; a catalog table linking every generated file; a note that tempos are
defaults; a cultural-context note.

### Testing and validation

Generate to a temporary directory and verify:

1. **File structure** - every file opens in mido, type 0, one track, ends in `end_of_track`,
   has `time_signature`/`set_tempo` at tick 0; every filename appears in manifest and vice versa.
2. **Timing** - all deltas non-negative integers; sum of deltas equals expected track duration;
   every note-off after its matching note-on; tick-0 events include required metadata; marker
   ticks within duration; loopable-pattern duration exactly equals the stated integral bar
   length; mixed-meter bar lengths correctly calculated at each time-signature change.
3. **Pattern-specific musical tests** - 4/4 conventional bar marker has beats 1-4 with beat 1
   accented; backbeat pattern stronger on beats 2 and 4; jazz 2-and-4 clicks only on beats 2/4;
   6/8 primary pulses on dotted-quarter positions; 5/4 and 7/8 grouping boundaries match actual
   accents; offbeat-only patterns have no primary click on quarter-note beats; gap patterns have
   actual silent bars; swing files have unequal eighth positions matching their declared ratio;
   son vs rumba clave differ at the intended 3-side final note; performance four-bar patterns
   have a loop-start marker at bar 1 and a turnaround/fill cue in bar 4; count-in patterns have
   count-in markers and their loop start occurs precisely after the declared count-in duration.
4. **Catalog quality** - no duplicate pattern IDs or output paths; every pattern has
   description/tags/recommended_use; cultural patterns have source_context; pattern count
   between 75 and 110; all three areas and all requested major families have at least one file.

### Hierarchical README requirements

Generated alongside the MIDI files (never maintained by hand, regenerated every run):
`generated/README.md`, `generated/<area>/README.md`, `generated/<area>/<family>/README.md` for
every populated family folder. Relative Markdown links only.

Root `generated/README.md`: title/purpose (SMF0, 480 PPQN, channel 10/zero-based channel 9,
default 120 BPM); how to browse the three areas with links; the click-language/note legend
table (this project's own notes, see instrument palette); general usage guidance; the
cultural-context note; a full navigation table (one row per family: area, family, purpose,
pattern count, doc link).

Area READMEs (`generated/<area>/README.md`): title, how the material progresses, a suggested
learning/use workflow, a table of family folders (what it teaches/purpose, difficulty
tendency, pattern count, doc link).

Family READMEs (`generated/<area>/<family>/README.md`): one paragraph on purpose/differences/
listening guidance/cautions; a "Quick selection" list; a pattern-catalog table (file, meter,
bars, main pattern/grouping, click language, difficulty, best use, notes); a "Pattern notes"
subsection per file (file link, meter/length, pulse/grouping, events, use, listening target,
loop/cue behaviour where relevant, cultural context where relevant, simplification note where
relevant) that adds explanatory value rather than repeating the table.

Special documentation cases: internal-time files state exactly which beats/bars are silent and
how a musician can check for drift when the click resumes; swing/shuffle files state the exact
ratio and which grid the click sits on; odd meters display grouping prominently (never as
equally accented individual notes); mixed meters give the bar-by-bar sequence and whether the
file loops seamlessly; count-ins state duration, the cue separating count-in from loop start,
and the exact tick/bar of `LOOP START`; looper/performance files show a compact form diagram and
state what bar 4 contains; Afro-Cuban files state clave direction and the son/rumba distinction;
Afro-diasporic files state whether they're a pulse/bell/surdo guide or a simplified learning
rhythm; Middle Eastern files show dum/tak notation and flag regional variation.

Implement documentation generation in `rhythm_library/documentation.py`
(`generate_root_readme`, `generate_area_readme`, `generate_family_readme`,
`render_pattern_table`, `render_pattern_notes`, `render_click_language`, `render_form_diagram`),
reusing the same `PatternSpec`/manifest data as the MIDI generator - no second, manually
maintained documentation database. Sort entries consistently (musical purpose, then filename);
escape Markdown table characters; portable forward-slash links; omit empty sections; only
populated folders; exact pattern counts; deterministic (same data -> byte-identical README).

Documentation tests verify: README presence for every populated folder; every `.mid` linked
from its family README, every family README linked from its area README, every area README
linked from the root, all local links resolve; catalog tables match the manifest exactly
(meter/bars/grouping/filename), cultural/swing/mixed-meter/loop-cue info present where
applicable; README generation is deterministic and works for a family with only one pattern.

### Delivery

Before finishing:
1. Install dependencies.
2. Run the generator.
3. Run all tests.
4. Run the validation CLI against `generated/`.
5. Report: exact generated MIDI file count, exact count per top-level area, location of
   manifest.json and README.md, test result, a few representative output paths, any
   intentionally simplified rhythmic representations (in particular: clave patterns using
   Woodblock/Rimshot as a substitute for a real claves voice).
