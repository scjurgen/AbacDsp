# TODO

## Maxdiffuser

### Custom Delay Chain Visualisation
Introduce to the JuceStandAloneGenerator a generic visualisation/interaction object.
It should be a placeholder that then will be custom implemented in the impl/ part of the generated code.
Concrete example that we implment: Object ShowProcessingBins impl/ShowProcessingBins.h where we paint bins of the single diffuser elements (up to 50) levels (dB scale)
Bin 0 will be the actual input level, and bin 50 will be the final current output level.

### Phasevocoder pitch shifter
- Rework the src/Spectral/StretchedSampleProducer.h and create a realtime PhaseVocoderPitcher.h
- Test thoroughly with unit-tests in a closed development cycle

## Code quality
### Sanitizier

- Tried wiring up ASan+UBSan via a CMake ENABLE_SANITIZERS option (2026-07-11):
  AddressSanitizer's dynamic runtime hangs at process startup on this Mac
  (Apple clang 17 / macOS 26.5.1) even for a trivial hello-world binary,
  stuck in AsanInitFromRtl's shadow-memory init. UBSan alone works fine.
  Sticking with Valgrind (docker-unit-tests) for now. Revisit ASan later,
  either once Apple/LLVM fixes this, or by running it in the Linux Docker
  container instead of natively.

### Test coverage
- Discuss what to use for unit test coverage


## Cleanup

## Project Generator

- Make the UI better (again)
  - automatic position stuff
  - integer parameters honored correctly
  - sliders?
- Enhanced save presets (with names)
- Synth modules without AudioIn
- 5.1
