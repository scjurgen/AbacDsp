# ClassicBeats

Generates original, copyright-free, full-drumset MIDI grooves - meant to ship with TapeLooper/
Looper eventually - covering 11 classic genres: Rock, Blues, Jazz Swing, Latin, Funk, Fusion,
Hip-Hop, Progressive, Reggae, Pop, Soul.

This is deliberately separate from `documentation/Metronome/rhythm_library`, whose own `PLAN.md`
rules out a full drum groove library (that package stays a sparse pedagogical click/guide-track
library). Every groove here is the opposite: a real-sounding full-kit pattern - kick, snare
(with ghost notes), hihat or ride, crash, toms - with the groove itself running right up to a
short (last 1-2 beats only), genre-specific fill device that closes each loop, rather than a
generic fill replacing a large chunk of the final bar.

### Fill devices

Real drummers reach for a different device depending on genre and mood, not one generic ending -
see general drumming-pedagogy sources (not transcriptions) cited in
`.claude/classic-beats-full-kit.md`: [MusicRadar - how drum fills work](https://www.musicradar.com/tuition/tech/learn-how-drum-fills-work-in-5-easy-steps-639154),
[MusicRadar - one-drop reggae fills](https://www.musicradar.com/how-to/how-to-program-a-typical-one-drop-reggae-beat-and-add-fills),
[Drumeo - guide to funk](https://www.drumeo.com/beat/a-drummers-guide-to-funk/).

| Device (`*_ending` in the script) | Used by | What it is |
|---|---|---|
| `tom_cascade_ending` | Rock, Blues, Fusion, Progressive, Pop, Latin songo | high-to-low tom run landing on the next downbeat's crash |
| `ghost_snare_ending` | Funk, Soul | syncopated ghost/accent snare jabs - funk's own signature device, not a tom run |
| `hihat_triplet_ending` | Reggae (via `reggae_expert.json`), Latin bossa | laid-back triplet hihat into a sidestick accent, the way a real one-drop fill is built |
| `snare_comping_ending` | Jazz Swing | light syncopated snare jabs; the ride (already ringing) carries the landing instead of a crash |
| `tom_kick_punch_ending` | Hip-Hop boom-bap | a single low-tom-then-kick punch, no crash - loops cut clean |
| `snare_roll_flourish_ending` | Hip-Hop busy-hihat-roll | a short velocity-ramped snare-roll crescendo, no crash |

Every device is parameterized (where the window starts, how many beats) rather than fixed
content - each is an original phrase built from a named, well-documented technique, never a
transcription of anything.

## Three tools

### 1. `analyze_reference_grooves.py` - reference-groove statistics

Reads only aggregate numbers (16th-grid hit-probability, BPM/feel distribution, ghost-note and
hihat-open ratios, fill density) from copyrighted reference MIDI packs via the `midicsv` CLI -
never reproduces or stores any actual note sequence. Run it against any MidiDrums-shaped root
(this repo's own gitignored `MidiDrums/<Genre>/` packs, or an external reference library):

```
python3 analyze_reference_grooves.py --root ../../MidiDrums --root /path/to/other/library
```

Writes `genre_profiles.md` (committed - aggregate numbers only, safe to keep even though the
source material itself isn't). The genre-to-source-folder mapping it uses
(`GENRE_SOURCE_FOLDERS` in the script) is a small curated table, not automatic keyword
matching - see `.claude/classic-beats-full-kit.md` for the reasoning behind each entry. A genre
with no reasonable source-folder match gets no profile; its grooves below were then composed
from general genre knowledge instead (noted per-groove in the table below).

### 2. `analyze_fill_archetypes.py` - fill structure statistics

Same idea, applied to the dedicated "FILLS"/"INTRO_FILL(S)" subfolders reference packs already
carry alongside their verse/chorus grooves: classifies each fill file's structure (hit count,
which instrument dominates, whether velocity ramps up) into one of four named archetypes -
never reproduces a note sequence. Run it the same way:

```
python3 analyze_fill_archetypes.py --root ../../MidiDrums --root /path/to/other/library
```

Writes `fill_archetypes.md` (committed). The actual fill code in `generate_classic_beats.py`
was informed more by general drumming pedagogy than by this specific corpus, though - see
`.claude/classic-beats-full-kit.md` for why, and the "Fill devices" section below for the
cited sources.

### 3. `generate_classic_beats.py` - the groove generator

Pure Python stdlib, no dependencies - a hand-written Standard MIDI File writer (format 0,
channel 9/GM percussion), matching `documentation/DrumKit808`'s zero-dependency convention
rather than `rhythm_library`'s `mido`-based one, since here the output is fully authored, not
parsed from arbitrary third-party files.

```
python3 generate_classic_beats.py [--out DIR]
```

Writes `<out>/<Genre>/<slug>_v1.mid` and `<slug>_v2.mid` plus a `.json` sidecar for each,
following `MidiDrums/README.md`'s documented sidecar fields. Default `--out` is `generated/`
next to this script - **not** `MidiDrums/`. Copying reviewed grooves into `MidiDrums/<Genre>/`
(and from there into the shipped app, the way `DrumKit808` -> `samples/drums/808/` works) is a
deliberate, separate follow-up step, not done by this script.

For Rock/Blues/Jazz Swing/Latin/Funk/Fusion/Hip-Hop/Progressive/Pop/Soul, `_v2` is a humanized
take of `_v1` (small deterministic velocity/micro-timing jitter on the same notes) - see
"Reggae: expert-config pilot" below for why Reggae works differently.

## Note palette

Real kit voices from `MidiDrums/README.md` / `src/includes/Sampler/GrooveNoteMap.h`, not the
reduced palette `rhythm_library/instruments.py` uses:

| Voice | Note | Voice | Note |
|---|---|---|---|
| Kick | 36 | Ride | 51 |
| Snare | 38 | Ride bell | 53 |
| Rimshot | 37 | Tom 1 | 43 |
| Sidestick | 71 | Tom 2 | 45 |
| Hihat closed | 42 | Tom 3 | 47 |
| Hihat open | 49 | Tom (low) | 41 |
| Crash | 27 | Woodblock | 56 |

Latin grooves additionally use Timbale 1-4 (66-69) for percussion colour - the same
claves/cowbell/conga substitution `rhythm_library/patterns/cultural.py` already documents and
accepts, since this project's note map has no dedicated voice for those.

## Groove list

Two core grooves per genre (one per prevalent `feel` shown in `genre_profiles.md`, or - where
the profile carries no usable feel signal - one representative pair chosen from general genre
knowledge), each written as a clean take (`_v1`) and a humanized take (`_v2`). Reggae isn't in
this table - see "Reggae: expert-config pilot" below.

| Genre | Groove | BPM | Feel | Notes |
|---|---|---:|---|---|
| Rock | Driving straight backbeat | 128 | even | dominant feel in profile (83%) |
| Rock | Shuffle backbeat | 116 | shuffle | secondary feel in profile (17%) |
| Blues | Shuffle blues | 84 | shuffle | genre-defining feel; profile carried no feel tag |
| Blues | Straight blues-rock backbeat | 100 | even | straighter alternative for verses |
| Jazz Swing | Ride spang-a-lang groove | 132 | swing | classic ride-cymbal swing pattern |
| Jazz Swing | Brush-style comping groove | 96 | swing | lighter, sidestick-based comping |
| Latin | Bossa nova groove | 118 | even | son-clave sidestick over a light kick |
| Latin | Songo-style groove | 104 | even | timbale-driven, dominant feel in profile |
| Funk | Straight 16th-note funk | 104 | even | dominant feel in profile (76%), high ghost rate |
| Funk | Swung funk (New-Orleans style) | 92 | swing | secondary feel in profile (24%) |
| Fusion | Syncopated straight fusion | 112 | even | dominant feel in profile (77%), high ghost rate |
| Fusion | Swung fusion | 100 | swing | secondary feel in profile (23%) |
| Hip-Hop | Boom-bap groove | 90 | even | genre-defining feel; profile carried no feel tag |
| Hip-Hop | Busy hihat-roll groove | 140 | even | denser modern variant |
| Progressive | Straight odd-accented groove | 118 | even | dominant feel in profile (72%) |
| Progressive | Swung progressive groove | 100 | swing | secondary feel in profile (16%) |
| Pop | Straight pop backbeat | 118 | even | dominant feel in profile (77%) |
| Pop | Swung pop shuffle | 100 | swing | secondary feel in profile (23%) |
| Soul | Deep-pocket straight soul | 92 | even | dominant feel in profile (60%) |
| Soul | Swung soul groove | 84 | swing | near-as-prevalent secondary feel (40%) |

## Reggae: expert-config pilot

Reggae's grooves don't come from `GROOVES`/`_add()` at all - checking the corpus-derived
one-drop groove against real drumming sources found it was idiomatically wrong (it used a full
snare hit for the beat-3 accent; the style's defining sound is a cross-stick/rimshot click), and
generating `_v2` by humanizing `_v1` never reflected how a drummer actually plays a second take.
Reggae is now the pilot for a different approach: an explicit, citable "expert config" -
`reggae_expert.json` - naming the three canonical styles (one-drop, rockers, steppers per
[Wikipedia](https://en.wikipedia.org/wiki/One_drop_rhythm),
[bassculture.substack.com](https://bassculture.substack.com/p/the-one-drop-understanding-reggaes),
Modern Drummer's steppers article) and, for each, the *genuinely different* arrangement move a
real second take uses - swapping rimshot for a full snare, adding a cross-stick on the "and" of
beat 4, opening the hihat for a dubbier chorus - rather than timing/velocity noise on the same
notes. `build_expert_bar()`/`write_expert_groove()` in `generate_classic_beats.py` interpret
that config; `_v1` is the config's `verse` variation, `_v2` its `chorus` variation. This is
piloted on Reggae only for now - the other 10 genres stay on the `GROOVES`/`_add()` approach.

| Style | BPM | Kick | Verse (`_v1`) | Chorus (`_v2`) |
|---|---:|---|---|---|
| One-drop | 76 | beat 3 only (beat 1 empty) | rimshot on 3 | + cross-stick on the "and" of 4 |
| Rockers | 82 | beats 1 and 3 | rimshot on 3 | full snare on 3 (swapped articulation) |
| Steppers | 88 | all four beats | rimshot on 3 | + open hihat on an "and" |

## Loop length

Each groove's `end_of_track` meta event is pinned to the full `bar_count * bar_ticks` length
(not the last note's own tick), matching `rhythm_library/midi_writer.py`'s convention - a
consumer that derives loop length from the track's final event (see
`documentation/Metronome/rhythm_library/patterns/looper.py`'s note on tapelooper's own loop
engine) then gets the intended full bar count instead of a loop truncated at the last drum hit.
