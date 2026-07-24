# TODO

## Looper

Next steps:
slices material after recording (in parallel to recording in another process).

play out the extracted samples when looping.
 

future additions:
- extract BPM
- pitch shift slices (saves new sample, needs good memory handling, we are realtime)
- shuffle slices
- reverse play, the beginning lands on the beat (so the playout position is before the beat with the length of the sample)

Hints: slicing, pitchshifting, beat extraction, click, sequencer will be added to the DSP library as testable includes.
use modern c++20, realtime, speed optimised 


## Maxdiffuser: add Phasevocoder pitch shifter
- Rework the src/Spectral/StretchedSampleProducer.h and create a realtime PhaseVocoderPitcher.h
- Test thoroughly with unit-tests in a closed development cycle
- the phasevocoder is a third option for the pitch shifter in the maxdiffuser (so we change to a drop box)

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
