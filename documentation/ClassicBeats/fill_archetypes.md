# Classic-beats fill archetypes

Aggregate/structural statistics only, derived from the dedicated "FILLS" (and "INTRO_FILL(S)") subfolders reference packs already carry - no note sequence from those packs is reproduced here or in the generated grooves. Each fill file is classified into one of four archetypes by its hit count, which instrument dominates, and whether velocity ramps up across the bar - see analyze_fill_archetypes.py's classify(). generate_classic_beats.py implements each archetype as an original, parameterized fill generator, picked per groove by what's idiomatic here.

Archetypes: **crescendo_roll** (one voice, e.g. snare, with velocity ramping up across the bar - the classic drum-roll buildup), **descending_tom_run** (two or more toms, no strong single-voice dominance), **sparse_punctuation** (6 or fewer hits - a couple of accented hits, not a dense run), **busy_groove_variation** (denser, mixed kick/snare/hihat - closer to a varied groove bar than a distinct fill gesture).

## Rock
Files analyzed: 200 (from 47 FILLS folder(s))
Archetype breakdown: busy_groove_variation:177 (88%), descending_tom_run:21 (10%), crescendo_roll:2 (1%)
Median hit count: 22 (range 7-46)
Mean tom fraction: 19%, mean snare fraction: 40%

## Blues
Files analyzed: 200 (from 43 FILLS folder(s))
Archetype breakdown: busy_groove_variation:175 (88%), descending_tom_run:15 (8%), crescendo_roll:10 (5%)
Median hit count: 26 (range 6-81)
Mean tom fraction: 14%, mean snare fraction: 41%

## Jazz Swing
Files analyzed: 123 (from 7 FILLS folder(s))
Archetype breakdown: busy_groove_variation:100 (81%), descending_tom_run:17 (14%), crescendo_roll:6 (5%)
Median hit count: 17 (range 9-32)
Mean tom fraction: 19%, mean snare fraction: 51%

## Latin
Files analyzed: 120 (from 10 FILLS folder(s))
Archetype breakdown: busy_groove_variation:78 (65%), descending_tom_run:42 (35%)
Median hit count: 19 (range 9-38)
Mean tom fraction: 33%, mean snare fraction: 30%

## Funk
Files analyzed: 200 (from 25 FILLS folder(s))
Archetype breakdown: busy_groove_variation:145 (72%), descending_tom_run:28 (14%), crescendo_roll:27 (14%)
Median hit count: 20 (range 7-68)
Mean tom fraction: 19%, mean snare fraction: 48%

## Fusion
Files analyzed: 200 (from 52 FILLS folder(s))
Archetype breakdown: busy_groove_variation:166 (83%), descending_tom_run:25 (12%), crescendo_roll:9 (4%)
Median hit count: 26 (range 10-56)
Mean tom fraction: 17%, mean snare fraction: 45%

## Hip-Hop
No FILLS-labeled reference material found.

## Progressive
Files analyzed: 200 (from 31 FILLS folder(s))
Archetype breakdown: busy_groove_variation:174 (87%), crescendo_roll:19 (10%), descending_tom_run:7 (4%)
Median hit count: 20 (range 9-43)
Mean tom fraction: 14%, mean snare fraction: 52%

## Reggae
Files analyzed: 157 (from 34 FILLS folder(s))
Archetype breakdown: busy_groove_variation:93 (59%), descending_tom_run:38 (24%), crescendo_roll:19 (12%), sparse_punctuation:7 (4%)
Median hit count: 10 (range 2-37)
Mean tom fraction: 26%, mean snare fraction: 33%

## Pop
Files analyzed: 200 (from 40 FILLS folder(s))
Archetype breakdown: busy_groove_variation:185 (92%), descending_tom_run:14 (7%), crescendo_roll:1 (0%)
Median hit count: 19 (range 7-50)
Mean tom fraction: 19%, mean snare fraction: 35%

## Soul
Files analyzed: 200 (from 42 FILLS folder(s))
Archetype breakdown: busy_groove_variation:136 (68%), descending_tom_run:60 (30%), crescendo_roll:4 (2%)
Median hit count: 20 (range 3-52)
Mean tom fraction: 29%, mean snare fraction: 33%

## Overall (all genres combined)
- busy_groove_variation: 1429 (79%)
- descending_tom_run: 267 (15%)
- crescendo_roll: 97 (5%)
- sparse_punctuation: 7 (0%)
