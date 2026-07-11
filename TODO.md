# TODO

## Cleanup

- Fix pre-existing unit test failures (unrelated to the diffuser work, found 2026-07-08):
  - DelaysTests: ModulatingDelayPitchedAdjustTest.simpleFeedAndEat
  - ModulationTests: WowTest.RateAffectsFrequency, WowTest.OutputRangeIsReasonable,
    WowTest.DriftAffectsFrequencyStability
- Rename test/Diffuser/DiffuserDelayChain_test.cpp: despite its name it tests
  SchroederDiffuser (should be SchroederDiffuser_test.cpp). The actual chain is
  covered by test/Diffuser/DiffusorDelayChain_test.cpp.



## Project Generator

- Make the UI better (again)
  - automatic position stuff
  - integer parameters honored correctly
  - sliders?
- Enhanced save presets (with names)
- Synth modules without AudioIn
- 5.1
