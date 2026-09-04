# SampleAnalysis

Two small tools for working with rendered synth-patch WAV files kept in `../../SynthSamples/`
(source material for ear-matching patches on other synths): cutting a multi-note render
into individual notes, and estimating each note's harmonic/envelope/modulation content.

```
python3 -m venv .venv
.venv/bin/pip install -r requirements.txt
```

## split_on_silence.py

Splits a WAV file into separate WAV files at points of detected silence - preprocessing
for `analyze_sample.py`, since a render with more than one note/take back to back needs
cutting apart before per-note analysis makes sense.

```
.venv/bin/python split_on_silence.py "../../SynthSamples/some_render.wav"
```

Writes `<stem>_001.wav`, `<stem>_002.wav`, ... flat into `SynthSamples/` by default (pass
`-o/--output-dir DIR` to write elsewhere). Segments shorter than `--min-segment-sec`
(default 0.15s) are dropped as noise; tune `--silence-db` (default -50 dBFS) if a quiet
passage is wrongly being cut, or kept where it should have been split.

## analyze_sample.py

Estimates harmonic content, envelope shape, and modulation from a WAV file, and writes a
JSON report plus an illustrative PNG plot to `generated/` (gitignored).

```
.venv/bin/python analyze_sample.py "../../SynthSamples/some_render_001.wav"
```

Writes `generated/<name>.analysis.json` and `generated/<name>.analysis.png`. Pass
`-o/--output-dir DIR` to write elsewhere.

Everything reported is heuristic - peak-picking, slope thresholds, and periodicity
detection on a detrended signal, not a source-separation model. Read the numbers as a
starting point for identifying a patch's shape, not a certified measurement. Use the
plot to sanity-check anything the JSON claims: a bad attack/release split or an
implausible harmonic will usually be visible there immediately.

### Plot

Three stacked panels, cropped to the sounding region of the file:
1. Waveform with the estimated dB envelope overlaid, and dashed lines marking the
   detected attack-start / peak / decay-end / release-start boundaries.
2. STFT spectrogram (dB) with the same boundary markers and the tracked f0 contour
   (cyan) overlaid.
3. FFT slices from the attack and release windows (a sustain slice is added too when
   the file has a genuine plateau), with the first 8 detected harmonic positions marked
   against the release trace.

### JSON report fields

Top level:

| Field | Meaning |
|---|---|
| `sample_rate`, `channels`, `duration_sec` | file properties |
| `level` | peak/RMS/noise-floor stats, see below |
| `envelope` | attack/decay/sustain/release segmentation, see below |
| `harmonics` | fundamental + partial levels, see below, or `null` if no fundamental could be tracked |
| `modulation` | tremolo/vibrato/filter-sweep/stereo-phase estimates, see below |
| `notes` | short plain-English flags summarizing anything notable found above |

`level`:
- `peak_dbfs`, `rms_dbfs`, `crest_factor_db` - whole-file levels.
- `noise_floor_dbfs` / `noise_floor_method` - the quietest 100ms window found anywhere
  in the file, or the pre-onset silence if that's quieter (`pre_onset_silence` vs
  `quietest_window_scan`/`whole_file_fallback`).
- `segment_rms_dbfs` - RMS dB per detected segment; `null` where that segment wasn't
  found (e.g. no sustain plateau).

`envelope`:
- `peak_time_sec` - time of the envelope's global maximum.
- `attack` / `decay` / `sustain` / `release` - each with its own `start_sec`/`end_sec`
  (names vary slightly per stage) and `duration_sec`. `attack`, `decay`, and `release`
  carry a `shape` object: `{"shape": "linear"|"exponential", "rate_db_per_sec": ...,
  linear_fit_rmse, exponential_fit_rmse}` - whichever fit had lower error decides the
  reported shape. `decay`/`sustain`/`release` are `null` when that stage wasn't found
  (e.g. a patch with no flat sustain plateau reports the whole post-peak tail as one
  `release` stage instead).
- `release.time_to_minus_60db_sec` - time from release start to -60dB relative to the
  release-start level; `null` if the file ends (or trails into silence) before reaching it.
- `onset_count` - from `librosa.onset.onset_detect`. The whole envelope/harmonic model
  assumes a single sustained note; when this is > 1, treat the rest of the report with
  extra caution (see `notes`).

`harmonics`:
- `fundamental_hz`, `nearest_note` - the tracked fundamental and its nearest 12-TET note
  name + cents offset.
- `partials` - list of `{n, freq_hz, ideal_freq_hz, level_db}`, `level_db` relative to
  the loudest bin in the analysis window. Stops after 5 consecutive partials below
  -70dB, or at 60 partials, or at Nyquist.
- `inharmonicity_pct` - mean fractional deviation of found partials (above -60dB) from
  an ideal harmonic series.
- `odd_even_energy_ratio_db` - odd-harmonic power vs even-harmonic power; strongly
  positive suggests a squarish/hollow timbre.
- `spectral_centroid_hz`, `spectral_rolloff_hz` (85% energy point) of the analysis window.
- `analysis_window_sec` / `analysis_window_source` - where in the file this was
  measured: the sustain plateau if one was found, else the longest voiced run, else the
  middle half of the file.

`modulation`:
- `amplitude` - tremolo: `{detected, rate_hz, depth_db}`, measured only within the
  sustain plateau (skipped if there isn't one).
- `pitch` - vibrato or drift: `{detected, type: "vibrato"|"drift", rate_hz,
  depth_cents}` or `{..., drift_semitones}`, measured over the longest voiced run.
- `unison_beating` - cross-check between the two above: if amplitude modulation is
  found with no matching vibrato rate, flags `possible_unison_beating` with an
  `estimated_detune_cents` guess (from the beat-frequency approximation); if the rates
  roughly match, flags `lfo_linked_vibrato_and_tremolo` instead.
- `filter_sweep` - spectral-centroid trend over the whole sounding region:
  `direction: "static"|"closing"|"opening"|"oscillating"`, with start/end/range
  centroid values and an `oscillation_rate_hz` when periodic.
- `stereo_phase` - `null` for mono files. Otherwise inter-channel correlation overall
  and per frequency band (`bands`, five roughly-octave-spaced bands up to Nyquist), plus
  `phase_cancellation_likely` and the `flagged_band` when the worst band's correlation
  drops below 0.3.

All periodicity checks (`amplitude`, `pitch` vibrato, `filter_sweep` oscillation)
require at least 2 full cycles inside the observed window before reporting a rate, to
avoid mistaking a single transient wobble for real modulation.
