# Documentation TODO

Algorithms and folders in `src/includes/` that still lack verification
plots, or where the existing documentation is out of date.

## Missing plots

- **Modulation** (`Modulation/Wow.h`, `Flutter.h`, `Tremolo.h`,
  `RingModulator.h`) - no documentation folder. Wow/Flutter in particular
  are only exercised indirectly (via `VariSpeedTapeDelay.h` in tapelooper);
  a standalone plot of their LFO characteristics (rate, depth, waveform)
  would help future tuning.

- **Delays** (`Delays/VariSpeedTapeDelay.h`, `MultiTapDelay.h`) - no
  documentation folder; tape-delay wow/flutter interaction and multi-tap
  spacing/decay would be useful alongside the Modulation plots above.

- **Biquads** 
  there is no documentation at all for them although they a part of important designs.
  check here, there is already some stuff: ~/projects/modabacad/ZY/documentation

## Missing README

- `VelvetNoise/` has a working `VelvetNoise.cpp` + `CMakeLists.txt` but no
  `README.md` describing what it explores or plots, unlike every other
  sibling folder.
