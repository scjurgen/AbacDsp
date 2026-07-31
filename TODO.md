# TODO


## Host sync 
- host sync must be disabled if we are not in a host.

## bug
loading a loop while play is on the metronome needs to be aligned. Probably we want another behaviour in the future which would be to schedule
the loaded loop and play it when the current loop is ending (with a fade in/out operation). For now we just stop the looper, load the file, and wait for a new play signal.

## UI
- visualise current bar with an overlay

- naming: setRecord is not correct it should be toggleRecordMode
- refactor switches and visualisations (red recording mode)

## Code quality
### Sanitizier

- Tried wiring up ASan+UBSan via a CMake ENABLE_SANITIZERS option (2026-07-11):
  AddressSanitizer's dynamic runtime hangs at process startup on this Mac
  (Apple clang 17 / macOS 26.5.1) even for a trivial hello-world binary,
  stuck in AsanInitFromRtl's shadow-memory init. UBSan alone works fine.
  Sticking with Valgrind (docker-unit-tests) for now. Revisit ASan later,
  either once Apple/LLVM fixes this, or by running it in the Linux Docker
  container instead of natively.

## Cleanup

## Project Generator

- Make the UI better (again)
  - automatic position stuff
  - integer parameters honored correctly
  - sliders?
- Enhanced save presets (with names)
- Synth modules without AudioIn
- 5.1
- Background silkmask
