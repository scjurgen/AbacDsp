# AbacDsp Coverage Report

## 📂 Overall coverage

| Metric        | Coverage |
|---------------|----------|
| **Lines**     | 🟢 10819/11614 (93.2%) |
| **Functions** | 🟢 1730/1769 (97.8%) |
| **Branches**  | 🔴 3273/4911 (66.6%) |
| **Decisions** | 🔴 1747/2332 (74.9%) |

## 📄 File coverage

| File                   | Lines | Functions | Branches |
|------------------------|-------|-----------|----------|
| **`src/includes/Analysis/EnvelopeFollower.h`** | 🟢 83/86 (96.5%) | 🟢 23/23 (100.0%) | 🔴 25/42 (59.5%) |
| **`src/includes/Analysis/FftMisc.h`** | 🟡 369/444 (83.1%) | 🟢 61/63 (96.8%) | 🔴 191/308 (62.0%) |
| **`src/includes/Analysis/OctaveBandAnalyzer.h`** | 🟢 135/135 (100.0%) | 🟢 28/28 (100.0%) | 🟢 70/70 (100.0%) |
| **`src/includes/Analysis/SimpleStats.h`** | 🟢 177/180 (98.3%) | 🟢 41/41 (100.0%) | 🔴 24/34 (70.6%) |
| **`src/includes/Analysis/Spectrogram.h`** | 🟡 235/282 (83.3%) | 🟢 32/35 (91.4%) | 🔴 55/94 (58.5%) |
| **`src/includes/Analysis/YinPitchDetector.h`** | 🟢 99/100 (99.0%) | 🟢 12/12 (100.0%) | 🟡 33/42 (78.6%) |
| **`src/includes/Analysis/ZeroCrossings.h`** | 🟡 166/185 (89.7%) | 🟡 29/36 (80.6%) | 🔴 155/260 (59.6%) |
| **`src/includes/Audio/AudioBuffer.h`** | 🟢 84/84 (100.0%) | 🟢 45/45 (100.0%) | 🔴 26/36 (72.2%) |
| **`src/includes/Audio/Fader.h`** | 🟢 40/43 (93.0%) | 🟢 11/11 (100.0%) | 🔴 14/22 (63.6%) |
| **`src/includes/Audio/FixedSizeProcessor.h`** | 🟢 32/32 (100.0%) | 🟢 7/7 (100.0%) | 🟡 16/18 (88.9%) |
| **`src/includes/AudioFile/LoadWav.h`** | 🟢 16/17 (94.1%) | 🟢 3/3 (100.0%) | 🔴 8/16 (50.0%) |
| **`src/includes/AudioFile/SaveWav.h`** | 🟢 27/27 (100.0%) | 🟢 4/4 (100.0%) | 🔴 11/22 (50.0%) |
| **`src/includes/BlockProcessors/BlockProcessorBase.h`** | 🟢 39/43 (90.7%) | 🟡 14/16 (87.5%) | 🟢 17/18 (94.4%) |
| **`src/includes/BlockProcessors/BlockProcHighpass.h`** | 🟢 15/15 (100.0%) | 🟡 3/4 (75.0%) | 🟢 2/2 (100.0%) |
| **`src/includes/BlockProcessors/BlockProcLowpass.h`** | 🟢 15/15 (100.0%) | 🟡 3/4 (75.0%) | 🟢 2/2 (100.0%) |
| **`src/includes/BlockProcessors/BlockProcPitch.h`** | 🟢 29/29 (100.0%) | 🟢 7/7 (100.0%) | 🔴 7/12 (58.3%) |
| **`src/includes/BlockProcessors/BlockProcVibrato.h`** | 🟢 36/36 (100.0%) | 🟡 7/8 (87.5%) | 🔴 5/8 (62.5%) |
| **`src/includes/Delays/DispersionDelay.h`** | 🟡 149/177 (84.2%) | 🟢 11/11 (100.0%) | 🔴 33/48 (68.8%) |
| **`src/includes/Delays/FracReadHead.h`** | 🟡 55/64 (85.9%) | 🟢 13/13 (100.0%) | 🔴 20/28 (71.4%) |
| **`src/includes/Delays/ModulationDelay.h`** | 🟢 201/221 (91.0%) | 🟢 33/35 (94.3%) | 🔴 106/144 (73.6%) |
| **`src/includes/Delays/NaiveDelay.h`** | 🟡 41/46 (89.1%) | 🟢 16/16 (100.0%) | 🔴 20/32 (62.5%) |
| **`src/includes/Delays/ParallelPlainDelay.h`** | 🟡 340/385 (88.3%) | 🟢 32/32 (100.0%) | 🔴 207/300 (69.0%) |
| **`src/includes/Delays/PitchFadeWindowDelay.h`** | 🔴 113/172 (65.7%) | 🟡 16/21 (76.2%) | 🔴 38/74 (51.4%) |
| **`src/includes/Delays/VariSpeedTapeDelay.h`** | 🟢 112/113 (99.1%) | 🟢 13/13 (100.0%) | 🔴 32/44 (72.7%) |
| **`src/includes/Diffuser/AllpassDelay.h`** | 🟢 141/142 (99.3%) | 🟢 24/24 (100.0%) | 🟡 36/46 (78.3%) |
| **`src/includes/Diffuser/DiffusorDelayChain.h`** | 🟢 200/210 (95.2%) | 🟢 28/28 (100.0%) | 🟡 55/68 (80.9%) |
| **`src/includes/Diffuser/SchroederAllpass.h`** | 🟢 111/120 (92.5%) | 🟢 19/20 (95.0%) | 🔴 26/42 (61.9%) |
| **`src/includes/Diffuser/SchroederDiffuser.h`** | 🟢 40/41 (97.6%) | 🟢 8/8 (100.0%) | 🟡 8/10 (80.0%) |
| **`src/includes/Filters/Biquad.h`** | 🟢 477/515 (92.6%) | 🟢 99/106 (93.4%) | 🟡 85/103 (82.5%) |
| **`src/includes/Filters/BiquadReference.h`** | 🟢 134/135 (99.3%) | 🟢 13/13 (100.0%) | 🟢 12/13 (92.3%) |
| **`src/includes/Filters/BiquadResoBandPassParallel.h`** | 🟢 37/37 (100.0%) | 🟢 4/4 (100.0%) | 🟡 7/8 (87.5%) |
| **`src/includes/Filters/BiquadResoBP.h`** | 🟡 42/52 (80.8%) | 🟢 5/5 (100.0%) | 🔴 1/2 (50.0%) |
| **`src/includes/Filters/BiquadResoBPParallelSIMD.h`** | 🟢 62/62 (100.0%) | 🟢 4/4 (100.0%) | 🟡 14/18 (77.8%) |
| **`src/includes/Filters/OnePoleFilter.h`** | 🟢 234/240 (97.5%) | 🟢 113/113 (100.0%) | 🔴 127/188 (67.6%) |
| **`src/includes/Filters/PinkFilter.h`** | 🟢 10/10 (100.0%) | 🟢 4/4 (100.0%) | 🟢 8/8 (100.0%) |
| **`src/includes/Filters/PoleMixingFilter.h`** | 🟢 302/304 (99.3%) | 🟢 59/60 (98.3%) | 🟡 51/62 (82.3%) |
| **`src/includes/Filters/Sinc/SincFilter.h`** | 🟢 121/128 (94.5%) | 🟢 19/20 (95.0%) | 🔴 73/110 (66.4%) |
| **`src/includes/Filters/SvfResoBP.h`** | 🟢 104/106 (98.1%) | 🟢 15/15 (100.0%) | 🔴 14/24 (58.3%) |
| **`src/includes/Generators/AttackRamp.h`** | 🟢 64/65 (98.5%) | 🟢 9/9 (100.0%) | 🟡 13/16 (81.2%) |
| **`src/includes/Generators/Excitation.h`** | 🟢 80/81 (98.8%) | 🟢 13/13 (100.0%) | 🔴 20/30 (66.7%) |
| **`src/includes/Generators/FileIo/ReadResoVoice.h`** | 🟢 18/18 (100.0%) | 🟢 1/1 (100.0%) | 🔴 23/36 (63.9%) |
| **`src/includes/Generators/HarmonicGenerator.h`** | 🟢 137/138 (99.3%) | 🟢 50/51 (98.0%) | 🔴 26/50 (52.0%) |
| **`src/includes/Generators/JuliaWalk.h`** | 🟢 61/61 (100.0%) | 🟢 13/13 (100.0%) | 🟢 4/4 (100.0%) |
| **`src/includes/Generators/OrnsteinUhlenbeckProcess.h`** | 🟢 34/34 (100.0%) | 🟢 5/5 (100.0%) | 🔴 8/16 (50.0%) |
| **`src/includes/Generators/RandomStyle/VelvetCrackle.h`** | 🟢 78/78 (100.0%) | 🟢 7/7 (100.0%) | 🟡 11/14 (78.6%) |
| **`src/includes/Generators/ReferenceWave.h`** | 🟢 11/11 (100.0%) | 🟢 1/1 (100.0%) | 🟢 4/4 (100.0%) |
| **`src/includes/Generators/ResoGenerator.h`** | 🟢 86/87 (98.9%) | 🟢 11/11 (100.0%) | 🔴 58/88 (65.9%) |
| **`src/includes/Generators/ResoParallelSIMD.h`** | 🟢 89/94 (94.7%) | 🟢 15/15 (100.0%) | 🔴 21/32 (65.6%) |
| **`src/includes/Helpers/ConstructArray.h`** | 🟡 45/53 (84.9%) | 🟢 69/69 (100.0%) | 🔴 101/202 (50.0%) |
| **`src/includes/Helpers/CreateExpectedSet.h`** | 🟡 61/69 (88.4%) | 🟢 3/3 (100.0%) | 🔴 37/62 (59.7%) |
| **`src/includes/Modulation/Flutter.h`** | 🟢 43/45 (95.6%) | 🟢 7/7 (100.0%) | 🟢 9/10 (90.0%) |
| **`src/includes/Modulation/Modulation.h`** | 🟢 54/54 (100.0%) | 🟢 7/7 (100.0%) | 🟡 14/16 (87.5%) |
| **`src/includes/Modulation/Wow.h`** | 🟢 64/64 (100.0%) | 🟢 8/8 (100.0%) | 🔴 11/16 (68.8%) |
| **`src/includes/NaiveGenerators/Generator.h`** | 🟢 142/148 (95.9%) | 🟢 29/29 (100.0%) | 🔴 98/156 (62.8%) |
| **`src/includes/NonLinear/SimpleHysteresis.h`** | 🟢 26/26 (100.0%) | 🟢 4/4 (100.0%) | 🟢 4/4 (100.0%) |
| **`src/includes/Numbers/Approximation.h`** | 🟢 78/78 (100.0%) | 🟢 24/24 (100.0%) | ⚫ 0/0 (0.0%) |
| **`src/includes/Numbers/BulgeControl.h`** | 🟢 43/47 (91.5%) | 🟢 7/7 (100.0%) | 🟡 10/12 (83.3%) |
| **`src/includes/Numbers/Convert.h`** | 🟢 20/20 (100.0%) | 🟢 7/7 (100.0%) | ⚫ 0/0 (0.0%) |
| **`src/includes/Numbers/EasyingFunctions.h`** | 🟢 12/12 (100.0%) | 🟢 3/3 (100.0%) | ⚫ 0/0 (0.0%) |
| **`src/includes/Numbers/Interpolation.h`** | 🟢 127/127 (100.0%) | 🟢 15/15 (100.0%) | ⚫ 0/0 (0.0%) |
| **`src/includes/Numbers/MultichannelInterpolation.h`** | 🟢 68/68 (100.0%) | 🟢 15/15 (100.0%) | 🟢 26/26 (100.0%) |
| **`src/includes/Numbers/PrimeDispatcher.h`** | 🟢 88/95 (92.6%) | 🟢 10/10 (100.0%) | 🔴 51/80 (63.7%) |
| **`src/includes/Numbers/TimeDistanceSmoother.h`** | 🟢 169/184 (91.8%) | 🟢 36/37 (97.3%) | 🟡 70/90 (77.8%) |
| **`src/includes/Parameters/LinearParameter.h`** | 🟢 97/99 (98.0%) | 🟢 23/23 (100.0%) | 🔴 17/28 (60.7%) |
| **`src/includes/Parameters/SmoothingParameter.h`** | 🟢 35/36 (97.2%) | 🟢 5/5 (100.0%) | 🟡 10/12 (83.3%) |
| **`src/includes/Parameters/VelocityMapping.h`** | 🟢 8/8 (100.0%) | 🟢 2/2 (100.0%) | 🔴 1/2 (50.0%) |
| **`src/includes/Reverbs/FdnReverb.h`** | 🟡 162/193 (83.9%) | 🟢 18/18 (100.0%) | 🔴 52/84 (61.9%) |
| **`src/includes/Reverbs/FdnTankBlockDelay.h`** | 🟢 356/383 (93.0%) | 🟢 46/46 (100.0%) | 🟡 86/114 (75.4%) |
| **`src/includes/Reverbs/FdnTankBlockDelaySIMD.h`** | 🟢 377/408 (92.4%) | 🟢 52/52 (100.0%) | 🔴 89/120 (74.2%) |
| **`src/includes/Reverbs/FdnTankBlockDelayWalsh.h`** | 🟢 94/102 (92.2%) | 🟢 13/13 (100.0%) | 🔴 22/30 (73.3%) |
| **`src/includes/Reverbs/FdnTankBlockDelayWalshSIMD.h`** | 🟢 377/408 (92.4%) | 🟢 52/52 (100.0%) | 🔴 89/120 (74.2%) |
| **`src/includes/Reverbs/FdnTankRef.h`** | 🟢 373/408 (91.4%) | 🟢 52/52 (100.0%) | 🔴 77/112 (68.8%) |
| **`src/includes/Reverbs/FdnTankSpiced.h`** | 🟢 129/137 (94.2%) | 🟢 16/16 (100.0%) | 🔴 34/48 (70.8%) |
| **`src/includes/Reverbs/FdnTankSpicedBase.h`** | 🟢 158/166 (95.2%) | 🟢 18/18 (100.0%) | 🟡 46/60 (76.7%) |
| **`src/includes/Reverbs/Hadamard4.h`** | 🟢 18/18 (100.0%) | 🟢 3/3 (100.0%) | 🔴 4/8 (50.0%) |
| **`src/includes/Reverbs/Hadamard8.h`** | 🟢 34/34 (100.0%) | 🟢 4/4 (100.0%) | 🔴 8/16 (50.0%) |
| **`src/includes/Reverbs/Hadamard16.h`** | 🟢 68/68 (100.0%) | 🟢 4/4 (100.0%) | 🔴 16/32 (50.0%) |
| **`src/includes/Reverbs/Hadamard32.h`** | 🟢 136/136 (100.0%) | 🟢 3/3 (100.0%) | ⚫ 0/0 (0.0%) |
| **`src/includes/Reverbs/HadamardFeed.h`** | 🟢 403/408 (98.8%) | 🟢 1/1 (100.0%) | 🟡 8/10 (80.0%) |
| **`src/includes/Reverbs/HadamardWalsh4.h`** | 🟢 31/31 (100.0%) | 🟢 4/4 (100.0%) | ⚫ 0/0 (0.0%) |
| **`src/includes/Reverbs/HadamardWalsh8.h`** | 🟢 52/52 (100.0%) | 🟢 4/4 (100.0%) | ⚫ 0/0 (0.0%) |
| **`src/includes/Reverbs/HadamardWalsh16.h`** | 🟢 66/66 (100.0%) | 🟢 4/4 (100.0%) | 🟢 10/10 (100.0%) |
| **`src/includes/Reverbs/HadamardWalsh32.h`** | 🟢 117/117 (100.0%) | 🟢 4/4 (100.0%) | 🟢 14/14 (100.0%) |
| **`src/includes/Reverbs/ModulationDelayNoFeedback.h`** | 🔴 135/195 (69.2%) | 🟢 27/30 (90.0%) | 🔴 42/116 (36.2%) |
| **`src/includes/Sampler/ResamplingPitchShifter.h`** | 🟢 69/70 (98.6%) | 🟢 9/9 (100.0%) | 🟡 16/18 (88.9%) |
| **`src/includes/Sampler/SamplePlayerBasic.h`** | 🟢 68/72 (94.4%) | 🟢 6/6 (100.0%) | 🔴 11/16 (68.8%) |
| **`src/includes/Sampler/StretchedSampleProducer.h`** | 🟢 201/220 (91.4%) | 🟢 17/17 (100.0%) | 🟡 52/68 (76.5%) |
| **`src/includes/SamplerateConverter/ConvertSampleBuffer.h`** | 🟢 6/6 (100.0%) | 🟢 1/1 (100.0%) | 🔴 3/6 (50.0%) |
| **`src/includes/SamplerateConverter/SrPullConverter.h`** | 🟡 163/193 (84.5%) | 🟢 11/11 (100.0%) | 🔴 44/78 (56.4%) |
| **`src/includes/SamplerateConverter/SrPushConverter.h`** | 🟢 287/304 (94.4%) | 🟢 20/20 (100.0%) | 🔴 80/192 (41.7%) |
| **`src/includes/SynthHandling/SustainPedalHandler.h`** | 🟢 100/106 (94.3%) | 🟢 10/10 (100.0%) | 🔴 47/70 (67.1%) |
| **`src/includes/Wavetables/WaveTableOscillator.h`** | 🟢 194/194 (100.0%) | 🟢 23/23 (100.0%) | 🟡 62/70 (88.6%) |
| **`src/includes/Wavetables/WaveTableStorage.cpp`** | 🟢 86/87 (98.9%) | 🟢 6/6 (100.0%) | 🔴 32/44 (72.7%) |
| **`src/includes/Wavetables/WaveTableStorage.h`** | 🟢 98/99 (99.0%) | 🟢 25/25 (100.0%) | 🔴 48/71 (67.6%) |
