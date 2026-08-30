# Documentation TODO

Algorithms and folders in `src/includes/` that still lack verification
plots, or where the existing documentation is out of date.

## Missing plots

- **Diffuser** (`Diffuser/DiffusorDelayChain.h`, `AllpassDelay.h`,
  `SchroederDiffuser.h`) - no documentation folder exists yet. Needs:
  - spectral characteristics: magnitude/phase flatness (an allpass diffuser
    should be flat in magnitude), density of the impulse response over time
  - decay behaviour: how quickly the diffuser smears a transient into a dense
    tail, and how that varies with `SizeSpreadControl`/allpass count

- **Reverbs** (`Reverbs/FdnTankGlide.h`, `FdnTankSpiced.h`,
  `FdnTankSpicedBase.h`, `FdnTankBlockDelayWalshSIMD.h`) - `Reverbs/FDN/`
  only covers generating the Hadamard matrices these tanks are built from;
  none of these tank classes actually used by examples (dronesequencer,
  maxdiffuser, minireverb, pingsynth, sampleplayer, spectraltap, tanpura,
  tapelooper) have verification plots of their own yet. Needs:
  - spectral characteristics: modal density/echo density over frequency,
    flatness of the late-reverb spectrum
  - decay behaviour: RT60 measurement per band, comparison of `FdnTankGlide`
    vs `FdnTankSpiced` decay curves, effect of the "glide"/"spice" parameters
    on decay shape

- **Modulation** (`Modulation/Wow.h`, `Flutter.h`, `Tremolo.h`,
  `RingModulator.h`) - no documentation folder. Wow/Flutter in particular
  are only exercised indirectly (via `VariSpeedTapeDelay.h` in tapelooper);
  a standalone plot of their LFO characteristics (rate, depth, waveform)
  would help future tuning.

- **Delays** (`Delays/VariSpeedTapeDelay.h`, `MultiTapDelay.h`) - no
  documentation folder; tape-delay wow/flutter interaction and multi-tap
  spacing/decay would be useful alongside the Modulation plots above.

## Missing README

- `VelvetNoise/` has a working `VelvetNoise.cpp` + `CMakeLists.txt` but no
  `README.md` describing what it explores or plots, unlike every other
  sibling folder.
