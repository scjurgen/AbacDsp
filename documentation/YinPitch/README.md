# YIN pitch detection and note tracking

Checks `StampedYinPitchDetector` (`src/includes/Analysis/StampedYinPitchDetector.h`) and `PitchNoteTracker`
(`src/includes/Analysis/PitchNoteTracker.h`) from 40 Hz to 2 kHz, and `YinPitchDetector`
(`src/includes/Analysis/YinPitchDetector.h`) on quiet noise. The `yin_*.png` files in this folder are checked-in samples of the
plots below.

## Two detectors, and why both exist

`YinPitchDetector` and `StampedYinPitchDetector` are the same algorithm (the cumulative mean normalised difference function, an
absolute threshold of 0.1, parabolic interpolation) for different jobs.

| | `YinPitchDetector` | `StampedYinPitchDetector` |
|---|---|---|
| Window | four periods of the lowest frequency, the first half integrated | 2 * maxLag samples |
| Lags computed | every lag, every hop | lag by lag, stopping at the first dip below the threshold |
| Cost control | none | integer decimation, cost falls with its square |
| Per reading | pitch and confidence | pitch, confidence, level and the count of input samples consumed |
| Reset | none | `reset()` |
| Used by | `NoteSegmenter`, `NoteVerifier`, `PitchFadeWindowDelay` | `PitchNoteTracker` through a stream of readings |

The first is used by analysis and effects with thresholds tuned to its window; the second exists because turning readings into
note events needs a level and a time stamp with every reading, a bounded cost on a small CPU, and a low range. `PitchNoteTracker`
is the streaming counterpart of `NoteSegmenter`: it takes one reading at a time and reports a note after three steady readings, a
release after three unvoiced ones, and a new strike when the level rises by 6 dB.
With `changeNeedsAttack` a pitch away from the playing note starts a note only after a rise in level (see "One pluck, one note").

## Quick start

`./generate.sh` runs `StampedYinExplore.cpp` through `dev-scripts/dev-explore.sh`, writes the plot data into a gitignored
`generated/` folder, sets up a local `.venv` from `requirements.txt` and renders the PNGs with `documentation/Plot/PyConPlot.py`.
`GUITAR_WAV=<recording.wav> ./generate.sh` adds the guitar plot (`StampedYinGuitarExplore.cpp`; the score is
`documentation/NoteSegmenter/scores/guitar_c_major_scale.score`, the recording is not in the repository). Both programs are
targets behind `EXPLORE_STUFF` too. By hand, from the repo root:

```bash
EXPLORE_OUT_DIR=$PWD/documentation/YinPitch/generated \
    ./dev-scripts/dev-explore.sh documentation/YinPitch/StampedYinExplore.cpp
```

## Pitch error over the range

![Pitch error of a steady sine, full rate and decimated by 4](yin_range.png)

A sine of amplitude 0.5 read after one second, detector range 40 Hz to 2 kHz, hop 50 Hz, at 48 kHz. At full rate the error is
below 0.1 cent up to 500 Hz, 0.3 cent at 1 kHz and 1.2 cent at 1.9 kHz. Decimated by 4 it is 0.9 cent up to 500 Hz, 5.4 cent at
1 kHz and 21 cent at 1.9 kHz, where a period is six samples of the decimated rate. The arithmetic falls with the square of the
decimation, and the tracker accepts readings within 50 cent of each other, so the note number is still right (see the tests).

## Time to the first note

![Time of the first note after a tone starts](yin_latency.png)

The tone starts at sample 0; the time is the stamp of the reading that completed the note, the end of the analysis window of the
third steady reading. Above about 200 Hz it is 80 ms; the window of a 40 Hz range is 50 ms long, so tones below about 200 Hz
need one more reading and are reported at 100 ms (41.2 Hz, E1, is note 28; 82.4 Hz, E2, note 40). The step is the hop of 20 ms;
the full rate and the decimated detector differ only at the edge of it.

## Noise

![White noise: readings that report a pitch, and those that pass the tracker's confidence gate](yin_noise.png)

White noise of amplitude 1e-4 to 1, 96000 samples per point. `YinPitchDetector` and `StampedYinPitchDetector` at full rate
report a pitch on none of the readings at any level. Decimated by 4, the lowpass in front makes noise look slightly periodic and
16.8 percent of the readings carry a pitch, at every level; their confidence stays below 0.85, so none passes the gate of
`PitchNoteTracker` (lower plot) and the tracker never reports a note from noise (the chain tests run the five tones and six noise
levels at both settings).

Before the cumulative sum of `YinPitchDetector` started at lag 1 (it started at `d(0) = 1`, a constant that dominates for quiet
signals), the upper plot's red line was at 100 percent for noise of amplitude 0.01 and below: every reading was a pitch. The
test `QuietNoiseIsNotReadAsPitched` fails before that fix (95 of 95 readings) and passes after it.

## A guitar recording

![The score of a guitar scale and the notes heard](yin_guitar.png)

The recording of a guitar scale (C3 to C4, then C4 to C5, quarter notes at 120 BPM) through the decimated detector and the
tracker: all 16 notes are heard with the right note number and nothing else is reported, 80 to 140 ms after the expected onset
(the first note is played 43 ms late, see the `NoteVerifier` section). The blue line joins the times of the reports. There is no
annotated bass recording in the repository; the 41.2 Hz tone of the tests stands in for a low E.

## One pluck, one note

A fast setting (a reading every 5 ms, a note after four steady readings) reports more than one note for some plucks.
Logging every reading of the 7-string and the 5-string bass recordings shows where they come from:

- The first 25 ms after a pluck are unreliable: readings jump between octaves and harmonics and are unvoiced or have a
  confidence of 0.3 to 0.7. A stable run starts at about 25 to 30 ms, confidence 0.7 rising to 0.99.
- Inside a ringing note the detector can answer with the octave for 20 ms at a confidence of 0.91 to 0.94. Four
  readings of 5 ms agree, so the tracker starts a note; 60 ms of agreement (three readings of 20 ms) would have ignored it.
- In the tail of a low note (a fundamental of 31 Hz at -29 dB) the detector reads the fourth harmonic at a confidence of
  0.97 and the tracker starts a note just before the next pluck.
- A finger pressing the next fret before the pluck, and a bend, move the pitch by more than the hysteresis.

None of these comes with a rise in level, and a pluck always does. `PitchNoteTrackerSettings::changeNeedsAttack` makes
the tracker ignore a reading away from the playing note unless the level rose by `changeAttackRiseDb` (4 dB) over the
lowest of the last `attackLookbackHops` readings no more than `changeAttackHops` readings ago (16, 80 ms at 5 ms). The
note after a release needs no attack. A hammer-on or pull-off without a rise in level is ignored until the first note
is released.

Measured on recordings of the author (a 7-string guitar, a 5-string bass, palm muted notes, bends, and the scale above;
120 BPM; the folder `guitar-samples` is not in the repository), hop 5 ms, four readings, release after twelve, lowest
note at the instrument's lowest string:

| | Notes | Exact | Off by up to 3 semitones or an octave | Missed | Extra note-ons | Median latency |
|---|---|---|---|---|---|---|
| guitars, without the rule | 144 | 133 | 9 | 2 | 104 | 50 ms |
| guitars, with the rule | 144 | 137 | 5 | 2 | 15 | 52 ms |
| 5-string bass, without | 40 | 32 | 8 | 0 | 27 | 79 ms |
| 5-string bass, with | 40 | 33 | 7 | 0 | 5 | 79 ms |

Without the rule 63 of the extra note-ons of the guitars come from the bends and 28 from the 7-string recording; with it
6 and 4 remain. A rise of 3 to 6 dB makes no difference to these figures. A memory of 12 readings loses 3 of the 40 bass
notes, one of 24 readings leaves 7 extra note-ons on the bass instead of 5. The first note-on of a pluck is still wrong by
one to three semitones for about 3 percent of the notes: that is the pluck transient and not an extra note.

## The first readings after an attack

The first 25 ms of a pluck are the part of a note that a detector reads wrongly. With `attackSettleHops` the tracker
ignores that many readings from the attack on (found as for `changeNeedsAttack`): they neither confirm a candidate
nor break one, and do not count towards a release. A note cannot be reported before the readings are reliable at
about 25 to 30 ms, so ignoring the first 15 ms does not delay it. On the recordings above (three readings, 15 ms at a
hop of 5 ms):

| | Notes | Exact | Off by up to 3 semitones or an octave | Missed | Extra note-ons | Median latency |
|---|---|---|---|---|---|---|
| guitars, `attackSettleHops` 0 | 144 | 137 | 5 | 2 | 15 | 52 ms |
| guitars, 2 (10 ms) | 144 | 141 | 1 | 2 | 7 | 52 ms |
| guitars, 3 (15 ms) | 144 | 142 | 0 | 2 | 6 | 52 ms |
| 5-string bass, 0 | 40 | 33 | 7 | 0 | 5 | 79 ms |
| 5-string bass, 3 | 40 | 35 | 5 | 0 | 3 | 79 ms |

Four readings (20 ms) give the same figures as three. A lowpass in front of the detector did not help: a 4th-order
Butterworth at 600 Hz, 1 kHz or 2 kHz left the guitar figures as they were (600 Hz added 3 ms), and at 300 Hz on the
bass it was worse (31 of 40 exact against 33). The harmonics a lowpass would remove are what carries the pitch of a
weak fundamental, and the transient is not only high-frequency noise.

## Refining the pitch

YIN integrates over the first half of its window, `maxLag` samples. For a bass range (29 Hz, window 69 ms) that region
still holds the previous note and the pluck when the new note has sounded for three periods, so the reading converges
slowly towards the new pitch: after a step of one semitone at 61.7 Hz the reading is 85 cents off after 30 ms and still
8 cents off after 60 ms. With `StampedYinOptions::refinePeriods` (3) the pitch YIN found is refined by the normalised
autocorrelation over the newest periods only, with its peak searched within 12 percent of the YIN lag and interpolated
(a parabola through three lags). The refinement is skipped when less than one period of history is left or the
correlation is below 0.8, and confidence and stamps stay those of YIN.

Cents against the new pitch, after a hard cut from one sine to the sine a semitone higher (lowest note 29 Hz, decimated
by 2):

| Step | refinePeriods | 10 ms | 20 ms | 30 ms | 40 ms | 60 ms |
|---|---|---|---|---|---|---|
| 110 Hz to 116.5 Hz | 0 | -100 | -100 | -96 | -68 | -14 |
| | 3 | -77 | -41 | -8 | 0 | 0 |
| 196 Hz to 207.7 Hz | 0 | -100 | -100 | -100 | -75 | -17 |
| | 3 | -49 | 0 | 0 | 0 | 0 |
| 330 Hz to 349.6 Hz | 0 | -100 | -100 | -100 | -79 | -21 |
| | 3 | -8 | 0 | 0 | 0 | 0 |

Two periods are faster still and a little more sensitive to noise, four are slower. Near the lowest note of the range
the buffer holds less than three periods and the refinement helps little: at 41.2 Hz it is 32 cents off after 40 ms
against 54 without. For a steady sine it is also more accurate: the error at 1500 Hz is 0.06 cent instead of 3.25 cents
(decimated by 2).

On the recordings (hop 5 ms, four readings, three ignored after an attack, attack rule on) the guitars were already
exact (142 of 144, 2 missed), and on the 5-string bass 38 of the 40 notes are exact instead of 35 with `refinePeriods`
3 (37 with 2 or 4). The other two bass notes lie within 31 cents of the rounding boundary to the next semitone, and are
no detector errors. The latency does not change. The refinement costs time: 24 ms per second of audio for the bass range
(13 ms without), 5 ms for the guitars (3 ms without), single thread, decimated by 2.

