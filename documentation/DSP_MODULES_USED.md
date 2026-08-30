# DSP Modules Used by Documentation

Companion to `examples/DSP_MODULES_USED.md`, which covers the JUCE plugin
examples. This one covers `documentation/`: the explore/benchmark programs
and design tools that live alongside the library's write-ups. Several
headers that no example reaches (direct or indirect) are exercised here
instead - this folder is where they're actually alive.

Scope: only `documentation/*/*.cpp` (and nested) files that `#include` a
`src/includes/` header. Python generator scripts (`documentation/Metronome`,
`fit*.py`, etc.) are out of scope - they don't touch the C++ library.

## Headers used by documentation programs

| Header | Used by | CMake target | Built? |
|---|---|---|---|
| Analysis/FftMisc.h | KarplusStrong/KarplusStrongExplore.cpp, Filters/PoleMixing/PoleMixingExplore.cpp | KarplusStrongExplore, PoleMixingExplore | yes |
| Analysis/SimpleStats.h | Filters/BandpassImpulses/BpImpulseExplore.cpp, ResoVoiceExplore.cpp | BandpassImpulseExplore, ResoVoiceExplore | yes |
| Analysis/Slicer.h | Slicer/SlicerExplore.cpp | SlicerExplore | yes |
| Analysis/ZeroCrossings.h | Filters/BandpassImpulses/BpImpulseExplore.cpp | BandpassImpulseExplore | yes |
| Filters/BiquadResoBP.h | Filters/BandpassImpulses/BpImpulseExplore.cpp, BpParallelPerformance.cpp | BandpassImpulseExplore, BandpassImpulsePerformance | yes |
| Filters/BiquadResoBPParallelSIMD.h | same two files | same two targets | yes |
| Filters/BiquadResoBandPassParallel.h | same two files | same two targets | yes |
| Filters/PoleMixingFilter.h | Filters/PoleMixing/PoleMixingExplore.cpp | PoleMixingExplore | yes |
| Filters/SvfResoBP.h | Filters/BandpassImpulses/BpImpulseExplore.cpp | BandpassImpulseExplore | yes |
| Generators/ExcitationTechnique.h | KarplusStrong/KarplusStrongExplore.cpp | KarplusStrongExplore | yes |
| Generators/KarplusStrongVoice.h | KarplusStrong/KarplusStrongExplore.cpp | KarplusStrongExplore | yes |
| Generators/Excitation.h | Filters/BandpassImpulses/BpImpulseExplore.cpp | BandpassImpulseExplore | yes |
| Generators/RandomStyle/VelvetCrackle.h | VelvetNoise/VelvetNoise.cpp | VelvetNoise | yes |
| Generators/ResoGenerator.h | Filters/BandpassImpulses/BpImpulseExplore.cpp, ResoVoiceExplore.cpp | BandpassImpulseExplore, ResoVoiceExplore | yes |
| Generators/OrnsteinUhlenbeckProcess.h | OrnsteinUhlenbeck/OrnsteinUhlenbeckTimeline.cpp | OrnsteinUhlenbeckTimeline | yes |
| Numbers/Convert.h | KarplusStrongExplore.cpp, BpImpulseExplore.cpp, ResoVoiceExplore.cpp | KarplusStrongExplore, BandpassImpulseExplore, ResoVoiceExplore | yes |

## Not used by examples, but alive here

Cross-referencing against `examples/DSP_MODULES_USED.md`'s "not used"
tables: these headers were marked unused by any example (direct or
indirect) but do get compiled and exercised by a documentation program.

| Header | Why it looked unused | Actually exercised by |
|---|---|---|
| Analysis/SimpleStats.h | no example includes it, nothing pulls it in transitively | BpImpulseExplore, ResoVoiceExplore |
| Analysis/ZeroCrossings.h | same | BpImpulseExplore |
| Filters/BiquadResoBPParallelSIMD.h | standalone SIMD alternative, no example uses it | BpImpulseExplore, BandpassImpulsePerformance (this is literally its performance-comparison benchmark) |
| Filters/BiquadResoBandPassParallel.h | same | BpImpulseExplore, BandpassImpulsePerformance |
| Generators/Excitation.h | superseded by ExcitationTechnique.h in the examples | BpImpulseExplore |
| Generators/RandomStyle/VelvetCrackle.h | no example includes it | VelvetNoise |

So of the 6 truly "not used by any example" headers previously flagged in
Generators/Analysis/Filters, only `Generators/AttackRamp.h`,
`Generators/FileIo/ReadResoVoice.h`, `Generators/JuliaWalk.h`, and
`Generators/ResoParallelSIMD.h` remain unreferenced anywhere in the repo
outside of tests.

## Removed as stale

Three dead files that used to live here were removed as part of a
documentation cleanup pass: `documentation/Reverbs/FDN/
performanceFdnWalshAndBlockDelay.cpp` (plus its `CMakeLists.txt`) compared
five now-deleted `FdnTank*` variants to find the fastest - a question
already answered by `FdnTankBlockDelayWalshSIMD` being the one that
survived into `src/includes/Reverbs/` - and it referenced headers that no
longer exist. `documentation/Filters/BandpassImpulses/
BpImpulseExplore.Old.cpp` and `BpParaPerformanceSize.cpp` were both
superseded, unbuilt drafts. None of the three ever counted as "used" here;
their removal doesn't change any row above. `Reverbs/FdnReverb.h` remains
unreferenced anywhere in the repo outside of tests.

## Generator tools, not consumers

Two documentation folders don't *use* the listed library headers so much as
*produce* them - worth noting so they aren't mistaken for consumers:

- `documentation/Filters/SincFilterDesign/` regenerates the windowed-sinc
  coefficient tables (`sinc_*.h`) that live in
  `src/includes/Filters/Sinc/`. Its own `sinc_*.h`/`SincFilter.h` files are
  local copies used only to develop the generator, not includes of the
  library's copies.
- `documentation/Filters/PoleMixing/fitPoleMixingCorrections.py` produces
  `src/includes/Filters/PoleMixingCorrections_generated.h`.

Both are Python tools with no CMake target of their own (aside from
`PoleMixingExplore.cpp`, which separately *does* consume the library's
`PoleMixingFilter.h`, as listed above).
