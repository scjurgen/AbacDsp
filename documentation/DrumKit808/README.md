# DrumKit808

Generates a complete, copyright-free drum kit for `samples/drums/` (see
`samples/README.md`): every piece is synthesized from oscillators and
filtered noise, so - unlike a real recorded pack such as
`samples/drums/reggae/` - there is nothing in it that needs a license.

Run:

```
python3 generate_808_kit.py
```

from anywhere; it writes `samples/drums/808/<code>_<n>.wav` (four round-robin
takes per code, plain 48 kHz stereo IEEE-float WAV, no numpy/soundfile
dependency - same file format as `documentation/Metronome/generate_click_samples.py`'s
output). `samples/drums/` is gitignored, so regenerate locally after a fresh
clone rather than expecting the output to already be there.

## Voices

`generate_808_kit.py` reuses the reggae kit's own `<code>` vocabulary (see
`samples/README.md`), so the generated kit drops into
`ABACDSP_DRUM_SAMPLES_DIR` with no C++ changes. Each code is built from one of
a few shared voice factories:

- **`pitched_voice`** - a sine with an exponential pitch sweep and amplitude
  decay, plus an optional highpassed noise transient at the attack. Used for
  the kick, toms, timbales, woodblock, and the two metronome click pieces.
- **`metallic_voice`** - six detuned oscillators summed and highpassed, at
  the TR-808 hihat circuit's approximate fixed partial frequencies (205.3,
  304.4, 369.6, 522.7, 540.0, 800.0 Hz), decaying exponentially. Used for
  every hihat, cymbal, ride, and china piece, with per-code decay time,
  highpass cutoff, and an optional noise "shimmer" layer or sine "ping"
  attack telling the codes apart.
- **`snare_voice`** / **`rim_voice`** / **`clap_voice`** / **`shaker_voice`**
  - small bespoke tone+noise composites for the remaining percussive pieces.

Every voice factory returns a `render(rng)` closure; the round-robin loop in
`main()` seeds one `random.Random` per `(code, take)`, so each take applies a
small, deterministic amount of pitch/decay jitter - the same round-robin
variety a struck instrument's takes have, without ever ringing identically
twice, the way `generate_click_samples.py`'s click takes already do.

Each code's final peak level is authored relative to the others (kick and
snare hit hard, `hhghost` stays quiet) rather than normalized to one shared
constant - `GrooveDrumPlayer` trusts a kit's levels as-is, so flattening them
during generation would erase the dynamics a real kit has.
