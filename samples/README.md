Due to copyright reasons, sample audio content is not shipped in this repository.
`samples/*` is gitignored except this file - populate `samples/drums/` locally
yourself, either with the synthesized kit below (no licensing concerns: nothing
in it is sampled or recorded) or with your own licensed material.

## Directory layout

`GrooveKit` (`src/includes/Sampler/GrooveKit.h`) scans one flat directory per
drum kit for round-robin sample files:

```
samples/drums/<kit-name>/<code>_<n>.wav
```

- `<code>` names one kit piece (`bd` for kick, `sd` for snare, `hh` for closed
  hihat, ...) - see the vocabulary table below.
- `<n>` is a 1-based round-robin take number (`bd_1.wav`, `bd_2.wav`, ...); any
  count works per code, `GrooveKit` just cycles through whatever takes exist.
- Every file must be **stereo**: `AudioFile::LoadWav::loadStereoFromFile`
  silently skips anything with fewer than 2 channels, so a mono source needs
  duplicating to stereo, not just renaming.
- Match the rest of the kit's sample rate. The shipped tooling (this repo's
  own generator, and `documentation/Metronome/generate_click_samples.py`)
  writes plain 48 kHz stereo IEEE-float WAV; there is no per-voice resampling
  in `GrooveDrumPlayer`, so a mismatched rate plays back pitch- and
  time-shifted.
- The looper/groover/tapelooper examples point at one such directory via
  `ABACDSP_DRUM_SAMPLES_DIR` in their `CMakeLists.txt` (default:
  `samples/drums/reggae`); point it at a different `<kit-name>` folder, or
  copy a kit's files into `samples/drums/reggae/`, to switch kits without
  touching C++.

A kit does not need every code below - `GrooveNoteMap.h` resolves each groove
note through a fallback chain of tags, most specific first (e.g. a missing
`hhstep` falls back to `hh`), and a missing `bd`/`sd`/`rs`/`wood` simply stays
silent since those have no broader tag to fall back to.

## Code vocabulary

Which loaded `<code>` satisfies which `GrooveTag` is
`GrooveKit::classifyReggaeKit()` in `GrooveKit.h` - despite the name, it keys
purely on these code strings, not on anything about the reggae kit's actual
audio, so any kit (including a newly generated or newly sourced one) that
reuses this vocabulary plugs in with zero C++ changes.

| code | instrument | code | instrument | code | instrument |
|---|---|---|---|---|---|
| bd | kick | hhhalf | hihat (half open) | timb1 | timbale 1 |
| rs | rimshot | hhstop | hihat (stopped) | timb2 | timbale 2 |
| sstick | sidestick | hhsoft | hihat (soft step) | timb3 | timbale 3 |
| sd | snare | hhghost | hihat (ghost) | timb4 | timbale 4 |
| sd2 | snare (alt) | hhopen | hihat (open) | timbdmp | timbale (damped) |
| sdroll | snare (roll) | crash | crash | wood | woodblock |
| tomlo | tom (low) | crstop / crstop2 | crash (stopped) | clap | handclap |
| tom1 | tom 1 | crlong / crlong2 | crash (long) | shaker | shaker |
| tom2 | tom 2 | china | china cymbal | tamb | tambourine |
| tom3 | tom 3 | chinasht | china (short) | clicklow | metronome, low |
| tomlt | tom (left) | ride / ride2 | ride | clickhigh | metronome, high |
| hhstep / hhstep2 / hhstep3 | hihat (step) | ridebell | ride bell | | |

This is the reggae kit's own vocabulary (see its `README.md` once you have a
kit in place); GM note numbers and the full tag-fallback chain are in
`src/includes/Sampler/GrooveNoteMap.h`.

## Option A: generate the built-in synthesized kit

```
python3 documentation/DrumKit808/generate_808_kit.py
```

Writes a complete TR-808-style kit - every code above, four round-robin takes
each - to `samples/drums/808/`. It is pure Python stdlib (no numpy/soundfile),
runs in under two minutes, and everything in it is synthesized from
oscillators and filtered noise, so there is nothing to license. See
`documentation/DrumKit808/README.md` for how each voice is built.

## Option B: adding a real sample pack (notes for an LLM assistant populating this folder)

- Only use material you can confirm is public domain, CC0, or otherwise
  explicitly licensed for this use (a pack's own license file or page, not a
  site's general reputation). Never add a commercial/copyrighted pack (drum
  machine ROM rips, a named vendor's sample library) here, even though
  `samples/*` is gitignored on this machine - that ignore rule exists
  precisely so such material never ends up in a commit; don't rely on it as
  the only safeguard (a stray `git add -f` would defeat it).
- Put a new pack in its own `samples/drums/<pack-name>/` folder, converted to
  stereo 48 kHz WAV and renamed to `<code>_<n>.wav` per the table above - a
  partial kit covering only the codes you have material for is fine (see the
  fallback-chain note above). If the source pack has no round-robin takes,
  a single `_1.wav` per code is enough.
- Write that folder's own `README.md` recording the source, its license, and
  the code-to-instrument mapping you used - the folder itself stays
  gitignored, so this is the only record of provenance that survives.
