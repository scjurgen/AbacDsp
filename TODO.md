# TODO

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
