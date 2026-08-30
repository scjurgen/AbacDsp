# Documentation TODO

Algorithms and folders in `src/includes/` that still lack verification
plots, or where the existing documentation is out of date.

## Missing plots

- **Delays** (`Delays/VariSpeedTapeDelay.h`, `MultiTapDelay.h`) - no
  documentation folder; tape-delay wow/flutter interaction (now that
  `Modulation/` covers `Wow`/`Flutter` in isolation) and multi-tap
  spacing/decay would be worth a look.

- **Biquads** 
  there is no documentation at all for them although they a part of important designs.
  check here, there is already some stuff: ~/projects/modabacad/ZY/documentation

## Missing README

- `VelvetNoise/` has a working `VelvetNoise.cpp` + `CMakeLists.txt` but no
  `README.md` describing what it explores or plots, unlike every other
  sibling folder.
