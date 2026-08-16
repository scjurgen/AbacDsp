# AbacDsp Coverage Report

## 📂 Overall coverage

| Metric        | Coverage |
|---------------|----------|
| **Lines**     | 🟢 12543/13393 (93.7%) |
| **Functions** | 🟢 2154/2204 (97.7%) |
| **Branches**  | 🔴 4689/7131 (65.8%) |
| **Decisions** | 🟡 2144/2796 (76.7%) |

## 📄 File coverage

| File                   | Lines | Functions | Branches |
|------------------------|-------|-----------|----------|
| **`src/includes/Analysis/EnvelopeFollower.h`** | 🟢 83/86 (96.5%) | 🟢 23/23 (100.0%) | 🔴 25/42 (59.5%) |
| **`src/includes/Analysis/FftMisc.h`** | 🟡 386/463 (83.4%) | 🟢 61/63 (96.8%) | 🔴 204/330 (61.8%) |
| **`src/includes/Analysis/OctaveBandAnalyzer.h`** | 🟢 135/135 (100.0%) | 🟢 28/28 (100.0%) | 🟢 70/70 (100.0%) |
| **`src/includes/Analysis/SimpleStats.h`** | 🟢 177/180 (98.3%) | 🟢 41/41 (100.0%) | 🔴 24/34 (70.6%) |
| **`src/includes/Analysis/Slicer.h`** | 🟢 329/352 (93.5%) | 🟢 21/21 (100.0%) | 🔴 231/344 (67.2%) |
| **`src/includes/Analysis/Spectrogram.h`** | 🟢 289/293 (98.6%) | 🟢 38/38 (100.0%) | 🟡 74/94 (78.7%) |
| **`src/includes/Analysis/YinPitchDetector.h`** | 🟢 105/106 (99.1%) | 🟢 13/13 (100.0%) | 🟡 33/42 (78.6%) |
| **`src/includes/Analysis/ZeroCrossings.h`** | 🟢 199/209 (95.2%) | 🟢 70/70 (100.0%) | 🔴 351/542 (64.8%) |
| **`src/includes/Audio/AudioBuffer.h`** | 🟢 84/85 (98.8%) | 🟢 45/45 (100.0%) | 🔴 26/36 (72.2%) |
| **`src/includes/Audio/Fader.h`** | 🟢 40/43 (93.0%) | 🟢 11/11 (100.0%) | 🔴 14/22 (63.6%) |
| **`src/includes/Audio/FixedSizeProcessor.h`** | 🟢 61/61 (100.0%) | 🟢 10/10 (100.0%) | 🟢 24/26 (92.3%) |
| **`src/includes/AudioFile/LoadWav.h`** | 🟢 21/22 (95.5%) | 🟢 4/4 (100.0%) | 🔴 9/18 (50.0%) |
| **`src/includes/AudioFile/SaveWav.h`** | 🟡 27/31 (87.1%) | 🟡 4/5 (80.0%) | 🔴 13/28 (46.4%) |
| **`src/includes/BlockProcessors/BlockProcessorBase.h`** | 🟢 39/43 (90.7%) | 🟡 14/16 (87.5%) | 🟢 17/18 (94.4%) |
| **`src/includes/BlockProcessors/BlockProcHighpass.h`** | 🟢 15/15 (100.0%) | 🟡 3/4 (75.0%) | 🟢 2/2 (100.0%) |
| **`src/includes/BlockProcessors/BlockProcLowpass.h`** | 🟢 15/15 (100.0%) | 🟡 3/4 (75.0%) | 🟢 2/2 (100.0%) |
| **`src/includes/BlockProcessors/BlockProcPhaseVocoderPitch.h`** | 🟢 22/22 (100.0%) | 🟢 6/6 (100.0%) | 🟡 3/4 (75.0%) |
| **`src/includes/BlockProcessors/BlockProcPitch.h`** | 🟡 36/42 (85.7%) | 🟢 7/7 (100.0%) | 🔴 9/18 (50.0%) |
| **`src/includes/BlockProcessors/BlockProcVibrato.h`** | 🟢 36/36 (100.0%) | 🟡 7/8 (87.5%) | 🔴 5/8 (62.5%) |
| **`src/includes/Delays/DispersionDelay.h`** | 🟡 149/177 (84.2%) | 🟢 11/11 (100.0%) | 🔴 33/48 (68.8%) |
| **`src/includes/Delays/FracReadHead.h`** | 🟡 52/61 (85.2%) | 🟢 13/13 (100.0%) | 🔴 20/28 (71.4%) |
| **`src/includes/Delays/ModulationDelay.h`** | 🟢 203/223 (91.0%) | 🟢 43/46 (93.5%) | 🔴 130/178 (73.0%) |
| **`src/includes/Delays/NaiveDelay.h`** | 🟡 41/46 (89.1%) | 🟢 16/16 (100.0%) | 🔴 20/32 (62.5%) |
| **`src/includes/Delays/ParallelPlainDelay.h`** | 🟡 374/430 (87.0%) | 🟢 37/37 (100.0%) | 🔴 251/364 (69.0%) |
| **`src/includes/Delays/PitchFadeWindowDelay.h`** | 🔴 217/358 (60.6%) | 🔴 29/45 (64.4%) | 🔴 76/172 (44.2%) |
| **`src/includes/Delays/VariSpeedTapeDelay.h`** | 🟢 112/113 (99.1%) | 🟢 13/13 (100.0%) | 🔴 32/44 (72.7%) |
| **`src/includes/Diffuser/AllpassDelay.h`** | 🟢 153/160 (95.6%) | 🟢 25/25 (100.0%) | 🟡 38/50 (76.0%) |
| **`src/includes/Diffuser/DiffusorDelayChain.h`** | 🟢 371/404 (91.8%) | 🟢 41/42 (97.6%) | 🟡 133/176 (75.6%) |
| **`src/includes/Diffuser/SchroederAllpass.h`** | 🟢 111/120 (92.5%) | 🟢 19/20 (95.0%) | 🔴 26/42 (61.9%) |
| **`src/includes/Diffuser/SchroederDiffuser.h`** | 🟢 40/41 (97.6%) | 🟢 8/8 (100.0%) | 🟡 8/10 (80.0%) |
| **`src/includes/Diffuser/SizeSpreadControl.h`** | 🟢 16/16 (100.0%) | 🟢 6/6 (100.0%) | 🟢 21/22 (95.5%) |
| **`src/includes/Filters/Biquad.h`** | 🟢 492/531 (92.7%) | 🟢 103/110 (93.6%) | 🟡 85/103 (82.5%) |
| **`src/includes/Filters/BiquadResoBandPassParallel.h`** | 🟢 37/37 (100.0%) | 🟢 4/4 (100.0%) | 🟡 7/8 (87.5%) |
| **`src/includes/Filters/BiquadResoBP.h`** | 🟡 42/52 (80.8%) | 🟢 5/5 (100.0%) | 🔴 1/2 (50.0%) |
| **`src/includes/Filters/BiquadResoBPParallelSIMD.h`** | 🟢 62/62 (100.0%) | 🟢 4/4 (100.0%) | 🟡 14/18 (77.8%) |
| **`src/includes/Filters/OnePoleFilter.h`** | 🟢 256/260 (98.5%) | 🟢 133/133 (100.0%) | 🔴 155/228 (68.0%) |
| **`src/includes/Filters/PinkFilter.h`** | 🟢 10/10 (100.0%) | 🟢 4/4 (100.0%) | 🟢 8/8 (100.0%) |
| **`src/includes/Filters/PoleMixingCorrections_generated.h`** | 🟢 8/8 (100.0%) | 🟢 2/2 (100.0%) | 🔴 3/6 (50.0%) |
| **`src/includes/Filters/PoleMixingFilter.h`** | 🟢 308/310 (99.4%) | 🟢 61/62 (98.4%) | 🟡 45/52 (86.5%) |
| **`src/includes/Filters/Sinc/SincFilter.h`** | 🟢 125/130 (96.2%) | 🟢 20/20 (100.0%) | 🔴 75/110 (68.2%) |
| **`src/includes/Filters/SvfResoBP.h`** | 🟢 135/135 (100.0%) | 🟢 21/21 (100.0%) | 🔴 16/26 (61.5%) |
| **`src/includes/Generators/AdsEnvelope.h`** | 🟢 141/152 (92.8%) | 🟢 22/22 (100.0%) | 🔴 25/40 (62.5%) |
| **`src/includes/Generators/AttackRamp.h`** | 🟢 64/65 (98.5%) | 🟢 9/9 (100.0%) | 🟡 13/16 (81.2%) |
| **`src/includes/Generators/BeatSequencer.h`** | 🟢 112/119 (94.1%) | 🟢 20/20 (100.0%) | 🔴 27/39 (69.2%) |
| **`src/includes/Generators/ClickGenerator.h`** | 🟢 34/34 (100.0%) | 🟢 7/7 (100.0%) | 🟡 4/5 (80.0%) |
| **`src/includes/Generators/Excitation.h`** | 🟢 80/81 (98.8%) | 🟢 13/13 (100.0%) | 🔴 20/30 (66.7%) |
| **`src/includes/Generators/FileIo/ReadResoVoice.h`** | 🟢 18/18 (100.0%) | 🟢 1/1 (100.0%) | 🔴 23/36 (63.9%) |
| **`src/includes/Generators/HarmonicGenerator.h`** | 🟢 137/138 (99.3%) | 🟢 50/51 (98.0%) | 🔴 26/50 (52.0%) |
| **`src/includes/Generators/JuliaWalk.h`** | 🟢 61/61 (100.0%) | 🟢 13/13 (100.0%) | 🟢 4/4 (100.0%) |
| **`src/includes/Generators/KarplusStrongEnsemble.h`** | 🟢 15/15 (100.0%) | 🟢 7/7 (100.0%) | 🟡 3/4 (75.0%) |
| **`src/includes/Generators/KarplusStrongString.h`** | 🟡 263/294 (89.5%) | 🟢 38/40 (95.0%) | 🔴 57/103 (55.3%) |
| **`src/includes/Generators/KarplusStrongVoice.h`** | 🟢 196/196 (100.0%) | 🟢 31/31 (100.0%) | 🔴 44/61 (72.1%) |
| **`src/includes/Generators/MeterTimeline.h`** | 🟢 54/56 (96.4%) | 🟢 11/11 (100.0%) | 🟡 24/28 (85.7%) |
| **`src/includes/Generators/OrnsteinUhlenbeckProcess.h`** | 🟡 29/34 (85.3%) | 🟢 5/5 (100.0%) | 🔴 6/16 (37.5%) |
| **`src/includes/Generators/RandomStyle/VelvetCrackle.h`** | 🟢 78/78 (100.0%) | 🟢 7/7 (100.0%) | 🟡 11/14 (78.6%) |
| **`src/includes/Generators/ResoGenerator.h`** | 🟢 86/87 (98.9%) | 🟢 11/11 (100.0%) | 🔴 58/88 (65.9%) |
| **`src/includes/Generators/ResoParallelSIMD.h`** | 🟢 89/94 (94.7%) | 🟢 15/15 (100.0%) | 🔴 21/32 (65.6%) |
| **`src/includes/Helpers/ConstructArray.h`** | 🟡 55/65 (84.6%) | 🟢 84/84 (100.0%) | 🔴 146/292 (50.0%) |
| **`src/includes/Helpers/CreateExpectedSet.h`** | 🟡 61/69 (88.4%) | 🟢 3/3 (100.0%) | 🔴 37/62 (59.7%) |
| **`src/includes/Modulation/Flutter.h`** | 🟢 43/45 (95.6%) | 🟢 7/7 (100.0%) | 🟢 9/10 (90.0%) |
| **`src/includes/Modulation/Modulation.h`** | 🟢 52/52 (100.0%) | 🟢 6/6 (100.0%) | 🟡 14/16 (87.5%) |
| **`src/includes/Modulation/SineModulation.h`** | 🟢 47/47 (100.0%) | 🟢 8/8 (100.0%) | 🟢 9/10 (90.0%) |
| **`src/includes/Modulation/Wow.h`** | 🟢 64/64 (100.0%) | 🟢 8/8 (100.0%) | 🔴 11/16 (68.8%) |
| **`src/includes/NaiveGenerators/Generator.h`** | 🟢 142/148 (95.9%) | 🟢 29/29 (100.0%) | 🔴 98/156 (62.8%) |
| **`src/includes/NonLinear/SimpleHysteresis.h`** | 🟢 26/26 (100.0%) | 🟢 4/4 (100.0%) | 🟢 4/4 (100.0%) |
| **`src/includes/Numbers/Approximation.h`** | 🟢 78/78 (100.0%) | 🟢 24/24 (100.0%) | ⚫ 0/0 (0.0%) |
| **`src/includes/Numbers/BulgeControl.h`** | 🟢 43/47 (91.5%) | 🟢 7/7 (100.0%) | 🟡 10/12 (83.3%) |
| **`src/includes/Numbers/Convert.h`** | 🟢 24/24 (100.0%) | 🟢 9/9 (100.0%) | ⚫ 0/0 (0.0%) |
| **`src/includes/Numbers/EasyingFunctions.h`** | 🟢 12/12 (100.0%) | 🟢 3/3 (100.0%) | ⚫ 0/0 (0.0%) |
| **`src/includes/Numbers/Interpolation.h`** | 🟢 127/127 (100.0%) | 🟢 15/15 (100.0%) | ⚫ 0/0 (0.0%) |
| **`src/includes/Numbers/MultichannelInterpolation.h`** | 🟢 68/68 (100.0%) | 🟢 15/15 (100.0%) | 🟢 26/26 (100.0%) |
| **`src/includes/Numbers/PrimeDispatcher.h`** | 🟢 106/115 (92.2%) | 🟢 12/12 (100.0%) | 🔴 57/92 (62.0%) |
| **`src/includes/Numbers/TimeDistanceSmoother.h`** | 🟢 169/184 (91.8%) | 🟢 36/37 (97.3%) | 🟡 70/90 (77.8%) |
| **`src/includes/Parameters/LinearParameter.h`** | 🟢 97/99 (98.0%) | 🟢 23/23 (100.0%) | 🔴 17/28 (60.7%) |
| **`src/includes/Parameters/SmoothingParameter.h`** | 🟢 35/36 (97.2%) | 🟢 5/5 (100.0%) | 🟡 11/14 (78.6%) |
| **`src/includes/Parameters/VelocityMapping.h`** | 🟢 8/8 (100.0%) | 🟢 2/2 (100.0%) | 🔴 1/2 (50.0%) |
| **`src/includes/Reverbs/FdnReverb.h`** | 🟡 162/193 (83.9%) | 🟢 18/18 (100.0%) | 🔴 52/84 (61.9%) |
| **`src/includes/Reverbs/FdnTankBlockDelayWalshSIMD.h`** | 🟢 377/408 (92.4%) | 🟢 52/52 (100.0%) | 🔴 89/120 (74.2%) |
| **`src/includes/Reverbs/FdnTankGlide.h`** | 🟢 131/139 (94.2%) | 🟢 44/44 (100.0%) | 🔴 94/128 (73.4%) |
| **`src/includes/Reverbs/FdnTankSpiced.h`** | 🟢 129/137 (94.2%) | 🟢 16/16 (100.0%) | 🔴 34/48 (70.8%) |
| **`src/includes/Reverbs/FdnTankSpicedBase.h`** | 🟢 158/166 (95.2%) | 🟢 18/18 (100.0%) | 🟡 46/60 (76.7%) |
| **`src/includes/Reverbs/HadamardFeed.h`** | 🟢 403/408 (98.8%) | 🟢 1/1 (100.0%) | 🟡 8/10 (80.0%) |
| **`src/includes/Reverbs/HadamardWalsh4.h`** | 🟢 31/31 (100.0%) | 🟢 4/4 (100.0%) | ⚫ 0/0 (0.0%) |
| **`src/includes/Reverbs/HadamardWalsh8.h`** | 🟢 52/52 (100.0%) | 🟢 4/4 (100.0%) | ⚫ 0/0 (0.0%) |
| **`src/includes/Reverbs/HadamardWalsh16.h`** | 🟢 66/66 (100.0%) | 🟢 4/4 (100.0%) | 🟢 10/10 (100.0%) |
| **`src/includes/Reverbs/HadamardWalsh32.h`** | 🟢 117/117 (100.0%) | 🟢 4/4 (100.0%) | 🟢 14/14 (100.0%) |
| **`src/includes/Reverbs/ModulationDelayNoFeedback.h`** | 🟢 379/398 (95.2%) | 🟢 100/108 (92.6%) | 🔴 249/518 (48.1%) |
| **`src/includes/Sampler/LoopFile.h`** | 🟢 29/30 (96.7%) | 🟢 6/6 (100.0%) | 🔴 19/34 (55.9%) |
| **`src/includes/Sampler/LoopRecorder.h`** | 🟢 271/284 (95.4%) | 🟢 38/38 (100.0%) | 🔴 100/143 (69.9%) |
| **`src/includes/Sampler/MidiFile.h`** | 🟢 159/169 (94.1%) | 🟢 21/21 (100.0%) | 🔴 69/122 (56.6%) |
| **`src/includes/Sampler/ResamplingPitchShifter.h`** | 🟢 69/70 (98.6%) | 🟢 9/9 (100.0%) | 🟡 16/18 (88.9%) |
| **`src/includes/Sampler/SamplePlayerBasic.h`** | 🟢 68/72 (94.4%) | 🟢 6/6 (100.0%) | 🔴 11/16 (68.8%) |
| **`src/includes/Sampler/SequencePattern.h`** | 🟢 43/43 (100.0%) | 🟢 13/13 (100.0%) | 🟡 10/12 (83.3%) |
| **`src/includes/Sampler/SequencerEngine.h`** | 🟢 140/141 (99.3%) | 🟢 33/34 (97.1%) | 🔴 105/164 (64.0%) |
| **`src/includes/Sampler/SliceLibrary.h`** | 🟢 82/87 (94.3%) | 🟢 15/15 (100.0%) | 🔴 28/40 (70.0%) |
| **`src/includes/Sampler/SlicePlayer.h`** | 🟢 85/86 (98.8%) | 🟢 12/12 (100.0%) | 🔴 41/56 (73.2%) |
| **`src/includes/Sampler/StretchedSampleProducer.h`** | 🟢 201/220 (91.4%) | 🟢 17/17 (100.0%) | 🟡 52/68 (76.5%) |
| **`src/includes/SamplerateConverter/ConvertSampleBuffer.h`** | 🟢 6/6 (100.0%) | 🟢 1/1 (100.0%) | 🔴 3/6 (50.0%) |
| **`src/includes/SamplerateConverter/InternalRateNormalizingProcessor.h`** | 🟢 109/109 (100.0%) | 🟢 16/16 (100.0%) | 🟡 31/36 (86.1%) |
| **`src/includes/SamplerateConverter/SrPullConverter.h`** | 🟡 163/193 (84.5%) | 🟢 11/11 (100.0%) | 🔴 44/78 (56.4%) |
| **`src/includes/SamplerateConverter/SrPushConverter.h`** | 🟢 304/316 (96.2%) | 🟢 20/20 (100.0%) | 🔴 84/192 (43.8%) |
| **`src/includes/Spectral/PhaseVocoderPitcher.h`** | 🟢 204/207 (98.6%) | 🟢 20/20 (100.0%) | 🟡 72/96 (75.0%) |
| **`src/includes/SynthHandling/SustainPedalHandler.h`** | 🟢 100/106 (94.3%) | 🟢 10/10 (100.0%) | 🔴 47/70 (67.1%) |
| **`src/includes/Wavetables/WaveTableOscillator.h`** | 🟢 194/194 (100.0%) | 🟢 23/23 (100.0%) | 🟡 62/70 (88.6%) |
| **`src/includes/Wavetables/WaveTableStorage.cpp`** | 🟢 86/87 (98.9%) | 🟢 6/6 (100.0%) | 🔴 32/44 (72.7%) |
| **`src/includes/Wavetables/WaveTableStorage.h`** | 🟢 98/99 (99.0%) | 🟢 25/25 (100.0%) | 🔴 48/71 (67.6%) |
