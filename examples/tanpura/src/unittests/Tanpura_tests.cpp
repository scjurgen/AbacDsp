#include <cmath>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <vector>

#include "Audio/AudioBuffer.h"
#include "impl/PluckSequencer.h"
#include "impl/TanpuraImpl.h"

namespace
{
constexpr float kSampleRate{48000.f};
constexpr size_t kMaxStringLength{10000};
using TestSequencer = PluckSequencer<kMaxStringLength>;
using TestEnsemble = AbacDsp::KarplusStrongEnsemble<TestSequencer::kNumVoices, kMaxStringLength>;
}

TEST(PluckSequencer, PatternTableMatchesSpec)
{
    ASSERT_EQ(TestSequencer::kPatterns.size(), 4u);

    const auto& p0 = TestSequencer::kPatterns[0]; // "H1 H2 1 -"
    EXPECT_EQ(p0.stepCount, 3u);
    EXPECT_EQ(p0.steps[0].role, PluckRole::Harmonic1);
    EXPECT_EQ(p0.steps[0].voiceIndex, 0u);
    EXPECT_EQ(p0.steps[1].role, PluckRole::Harmonic2);
    EXPECT_EQ(p0.steps[1].voiceIndex, 1u);
    EXPECT_EQ(p0.steps[2].role, PluckRole::Root);
    EXPECT_EQ(p0.steps[2].voiceIndex, 2u);

    const auto& p1 = TestSequencer::kPatterns[1]; // "H1 H2 8 1 -"
    EXPECT_EQ(p1.stepCount, 4u);
    EXPECT_EQ(p1.steps[2].role, PluckRole::Octave);
    EXPECT_EQ(p1.steps[2].voiceIndex, 2u);
    EXPECT_EQ(p1.steps[3].role, PluckRole::Root);
    EXPECT_EQ(p1.steps[3].voiceIndex, 3u);

    const auto& p2 = TestSequencer::kPatterns[2]; // "H1 H2 8 8 1 -"
    EXPECT_EQ(p2.stepCount, 5u);
    EXPECT_EQ(p2.steps[3].role, PluckRole::Octave);
    EXPECT_EQ(p2.steps[3].voiceIndex, 3u);
    EXPECT_EQ(p2.steps[4].role, PluckRole::Root);
    EXPECT_EQ(p2.steps[4].voiceIndex, 4u);

    const auto& p3 = TestSequencer::kPatterns[3]; // "H1 H2 - 8 8 1 -"
    EXPECT_EQ(p3.stepCount, 6u);
    EXPECT_EQ(p3.steps[2].role, PluckRole::Pause);
    EXPECT_EQ(p3.steps[3].role, PluckRole::Octave);
    EXPECT_EQ(p3.steps[3].voiceIndex, 2u);
    EXPECT_EQ(p3.steps[4].role, PluckRole::Octave);
    EXPECT_EQ(p3.steps[4].voiceIndex, 3u);
    EXPECT_EQ(p3.steps[5].role, PluckRole::Root);
    EXPECT_EQ(p3.steps[5].voiceIndex, 4u);
}

TEST(PluckSequencer, DashUsesNormalIntervalAndWrapAddsIntervalPlusPauseGap)
{
    TestSequencer seq(kSampleRate);
    TestEnsemble ensemble(kSampleRate);
    seq.setPattern(3);        // "H1 H2 - 8 8 1 -"
    seq.setIntervalMs(10.f);  // 10 ms/step -> 480 samples at 48 kHz
    seq.setPauseGapMs(100.f); // 4800 samples, deliberately not a multiple of intervalSamples

    constexpr size_t intervalSamples{480};
    constexpr size_t pauseSamples{4800};

    seq.setPlaying(true);
    std::vector<size_t> callsBetweenSteps;
    size_t previousStepIndex = seq.stepIndex();
    size_t callsSinceLastChange = 0;
    while (callsBetweenSteps.size() < 7)
    {
        seq.step(ensemble);
        ++callsSinceLastChange;
        if (seq.stepIndex() != previousStepIndex)
        {
            callsBetweenSteps.push_back(callsSinceLastChange);
            callsSinceLastChange = 0;
            previousStepIndex = seq.stepIndex();
        }
    }

    // step0 fires immediately; H1,H2,Pause,Octave,Octave all take one normal interval each
    // (the inner dash is not longer than a real step); the wrap after the last step (Root)
    // reaches the trailing dash's own virtual-pluck slot (one more interval) and then adds
    // the pause gap on top of that before the next cycle's H1.
    const std::vector<size_t> expected{1,
                                       intervalSamples + 1,
                                       intervalSamples + 1,
                                       intervalSamples + 1,
                                       intervalSamples + 1,
                                       intervalSamples + 1,
                                       intervalSamples + pauseSamples + 1};
    EXPECT_EQ(callsBetweenSteps, expected);
}

TEST(PluckSequencer, PlayStopGatesNewPlucksAndResumesFromStart)
{
    TestSequencer seq(kSampleRate);
    TestEnsemble ensemble(kSampleRate);
    seq.setPattern(0);
    seq.setIntervalMs(10.f);
    seq.setPlaying(true);
    seq.step(ensemble); // fires H1, stepIndex -> 1
    EXPECT_EQ(seq.stepIndex(), 1u);

    seq.setPlaying(false);
    for (size_t i = 0; i < 10000; ++i)
    {
        seq.step(ensemble);
    }
    EXPECT_EQ(seq.stepIndex(), 1u); // frozen while stopped

    seq.setPlaying(true);
    EXPECT_EQ(seq.stepIndex(), 0u); // resuming restarts the pattern
    seq.step(ensemble);
    EXPECT_EQ(seq.stepIndex(), 1u);
}

TEST(PluckSequencer, ZeroSlidePercentNeverSlides)
{
    TestSequencer seq(kSampleRate);
    TestEnsemble ensemble(kSampleRate);
    seq.setPattern(0);
    seq.setIntervalMs(10.f);
    seq.setPauseGapMs(1.f);
    seq.setHarmonicFirst(19); // offset != 0, so a slide would be audible if it happened
    seq.setSlidePercent(0.f);
    seq.setPlaying(true);

    for (size_t i = 0; i < 20000; ++i)
    {
        seq.step(ensemble);
        EXPECT_FALSE(seq.isSliding());
    }
}

TEST(PluckSequencer, HundredPercentSlideAlwaysSlides)
{
    TestSequencer seq(kSampleRate);
    TestEnsemble ensemble(kSampleRate);
    seq.setPattern(0);
    seq.setIntervalMs(10.f);
    seq.setPauseGapMs(1.f);
    seq.setHarmonicFirst(19);
    seq.setSlideTimeMs(1.f); // short, so each slide finishes well before the next H1 pluck
    seq.setSlidePercent(100.f);
    seq.setPlaying(true);

    bool everSlid = false;
    for (size_t i = 0; i < 5000; ++i)
    {
        seq.step(ensemble);
        everSlid = everSlid || seq.isSliding();
    }
    EXPECT_TRUE(everSlid);
}

TEST(PluckSequencer, SlidePercentControlsBendFrequency)
{
    TestSequencer seq(kSampleRate);
    TestEnsemble ensemble(kSampleRate);
    seq.setPattern(0); // "H1 H2 1 -", 3 steps, fast cycling
    seq.setIntervalMs(10.f);
    seq.setPauseGapMs(1.f);
    seq.setHarmonicFirst(19);
    seq.setSlideTimeMs(1.f); // finishes long before the next H1 pluck comes around
    seq.setSlidePercent(50.f);
    seq.setPlaying(true);

    size_t h1Triggers{0};
    size_t slidesObserved{0};
    size_t previousStepIndex = seq.stepIndex();
    while (h1Triggers < 2000)
    {
        seq.step(ensemble);
        if (seq.stepIndex() != previousStepIndex)
        {
            if (previousStepIndex == 0) // the step that just completed was H1
            {
                ++h1Triggers;
                slidesObserved += seq.isSliding() ? 1 : 0;
            }
            previousStepIndex = seq.stepIndex();
        }
    }

    const double ratio = static_cast<double>(slidesObserved) / static_cast<double>(h1Triggers);
    EXPECT_NEAR(ratio, 0.5, 0.05);
}

TEST(TanpuraImpl, IntervalMsForDivisionMatchesMusicalRatios)
{
    using Impl = TanpuraImpl<32>;
    EXPECT_FLOAT_EQ(Impl::intervalMsForDivision(120.f, 0), 2000.f); // "1/1" at 120 BPM
    EXPECT_FLOAT_EQ(Impl::intervalMsForDivision(120.f, 4), 500.f);  // "1/4" at 120 BPM
    EXPECT_FLOAT_EQ(Impl::intervalMsForDivision(120.f, 7), 250.f);  // "1/8" at 120 BPM
    EXPECT_FLOAT_EQ(Impl::intervalMsForDivision(120.f, 100), Impl::intervalMsForDivision(120.f, 12));
}

TEST(TanpuraImpl, HostSyncSwitchesBetweenManualAndClampedHostBpm)
{
    TanpuraImpl<32> impl(kSampleRate);
    impl.setBpm(90.f);
    EXPECT_FLOAT_EQ(impl.currentBpm(), 90.f);

    EffectBase::HostTransport transport{};
    transport.bpm = 500.0; // above the clamp ceiling
    impl.setHostTransport(transport);
    EXPECT_FLOAT_EQ(impl.currentBpm(), 90.f); // still unsynced, host bpm ignored

    impl.setHostSync(true);
    EXPECT_TRUE(impl.isHostSynced());
    EXPECT_FLOAT_EQ(impl.currentBpm(), 300.f); // clamped to the ceiling

    transport.bpm = 5.0; // below the clamp floor
    impl.setHostTransport(transport);
    EXPECT_FLOAT_EQ(impl.currentBpm(), 20.f);
}

TEST(TanpuraImpl, ProcessBlockProducesBoundedFiniteOutput)
{
    constexpr size_t blockSize{32};
    TanpuraImpl<blockSize> impl(kSampleRate);
    impl.setKey(24);
    impl.setLevel(-6.f);
    impl.setPattern(2);
    impl.setBpm(200.f);
    impl.setPluckDivision(4);  // "1/4" -> 300 ms/step at 200 BPM
    impl.setPauseDivision(10); // "1/16" -> 75 ms pause gap
    impl.setAttack(5.f);
    impl.setDecay(200.f);
    impl.setLevelSustain(0.3f);
    impl.setReverbDry(0.f);
    impl.setReverbWet(-6.f);
    impl.setReverbSize(30.f);
    impl.setReverbDecay(2000.f);
    impl.setReverbShelfLow(6.f);
    impl.setReverbShelfHigh(-6.f);
    impl.setPlayStop(true);

    AbacDsp::AudioBuffer<2, blockSize> in{};
    AbacDsp::AudioBuffer<2, blockSize> out{};
    for (size_t block = 0; block < 200; ++block)
    {
        impl.processBlock(in, out);
        for (size_t i = 0; i < blockSize; ++i)
        {
            EXPECT_TRUE(std::isfinite(out(i, 0)));
            EXPECT_TRUE(std::isfinite(out(i, 1)));
            EXPECT_LE(std::abs(out(i, 0)), 10.f);
            EXPECT_LE(std::abs(out(i, 1)), 10.f);
        }
    }
}

namespace
{
// Plucks a handful of strings with a very short string decay, stops new plucks, lets the
// dry signal die out, then measures energy in a later window: with the reverb fully wet
// and a long reverb decay, that window should still be alive; with it fully dry, silent.
[[nodiscard]] double energyAfterDrySettles(const float reverbDryDb, const float reverbWetDb)
{
    constexpr size_t blockSize{64};
    TanpuraImpl<blockSize> impl(kSampleRate);
    impl.setKey(24);
    impl.setPattern(2); // "H1 H2 8 8 1 -", feeds all 5 strings into the tank
    impl.setBpm(250.f);
    impl.setPluckDivision(12); // "1/16T", the fastest division -> 40 ms/step at 250 BPM
    impl.setPauseDivision(12);
    impl.setAttack(1.f);
    impl.setDecay(5.f); // very short: dry signal is effectively silent within ~50 ms
    impl.setLevelSustain(0.f);
    impl.setReverbDry(reverbDryDb);
    impl.setReverbWet(reverbWetDb);
    impl.setReverbSize(30.f);
    impl.setReverbDecay(5000.f); // long: the tank is still alive well past the dry settle time

    AbacDsp::AudioBuffer<2, blockSize> in{};
    AbacDsp::AudioBuffer<2, blockSize> out{};

    impl.setPlayStop(true);
    for (size_t block = 0; block < 500; ++block) // ~2.8 pattern cycles at 40 ms/step
    {
        impl.processBlock(in, out);
    }
    impl.setPlayStop(false);
    for (size_t block = 0; block < 100; ++block) // let the short dry decay fully settle
    {
        impl.processBlock(in, out);
    }

    double energy = 0.0;
    for (size_t block = 0; block < 50; ++block)
    {
        impl.processBlock(in, out);
        for (size_t i = 0; i < blockSize; ++i)
        {
            energy += static_cast<double>(out(i, 0)) * out(i, 0);
        }
    }
    return energy;
}
}

TEST(TanpuraImpl, ReverbTailPersistsAfterDrySignalSettlesWhenWet)
{
    EXPECT_LT(energyAfterDrySettles(0.f, -100.f), 1e-6);
    EXPECT_GT(energyAfterDrySettles(-100.f, 0.f), 1e-4);
}
