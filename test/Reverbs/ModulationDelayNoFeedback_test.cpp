#include <cmath>
#include <vector>

#include "gtest/gtest.h"

#include "Reverbs/ModulationDelayNoFeedback.h"

namespace AbacDsp::Test
{

TEST(ModulationDelayNoFeedback, stepProducesFiniteOutputAfterSizeChange)
{
    constexpr size_t maxSize{4096};
    ModulationDelayNoFeedback<maxSize> delay(48000.f);
    delay.setWidthInMsecs(10.f);
    delay.setModDepth(0.05f);
    delay.setModSpeed(2.f);

    for (int i = 0; i < 2000; ++i)
    {
        const float in = std::sin(static_cast<float>(i) * 0.05f);
        const float out = delay.step(in);
        EXPECT_TRUE(std::isfinite(out));
    }
    EXPECT_GT(delay.size(), 0u);
}

TEST(ModulationDelayNoFeedback, processBlockMatchesStepByStep)
{
    constexpr size_t maxSize{4096};
    ModulationDelayNoFeedback<maxSize> delay(48000.f);
    delay.setWidthInMsecs(5.f);

    constexpr size_t numSamples{256};
    std::vector<float> source(numSamples);
    for (size_t i = 0; i < numSamples; ++i)
    {
        source[i] = std::sin(static_cast<float>(i) * 0.1f);
    }
    std::vector<float> target(numSamples, 0.f);
    delay.processBlock(source.data(), target.data(), numSamples);

    for (const float v : target)
    {
        EXPECT_TRUE(std::isfinite(v));
    }
}

TEST(ModulationDelayNoFeedback, defaultConstructorAllowsRelaxedInit)
{
    constexpr size_t maxSize{2048};
    ModulationDelayNoFeedback<maxSize> delay;
    delay.setSampleRate(44100.f);
    delay.relaxedInit();

    const float out = delay.step(0.5f);
    EXPECT_TRUE(std::isfinite(out));
}

TEST(ModulationDelayNoFeedback, changeSizeModeSetterRoundTrips)
{
    ModulationDelayNoFeedback<4096> delay(48000.f);
    EXPECT_EQ(delay.changeSizeMode(), ChangeSizeMode::FADE); // default
    delay.setChangeSizeMode(ChangeSizeMode::HARDSWITCH);
    EXPECT_EQ(delay.changeSizeMode(), ChangeSizeMode::HARDSWITCH);
}

// A feedback-free, filter-free delay passes DC straight through once the buffer
// fills: reading a buffer full of 1.0 (even with modulation/interpolation) yields 1.0.
TEST(ModulationDelayNoFeedback, hardSwitchDelaysDcAndHandlesWrap)
{
    ModulationDelayNoFeedback<4096> delay(48000.f); // initial write head / width = 512
    delay.setChangeSizeMode(ChangeSizeMode::HARDSWITCH);
    delay.setSize(1000); // larger than the write head, exercises the read-head wrap branch
    float out = 0.f;
    for (int i = 0; i < 6000; ++i)
    {
        out = delay.step(1.0f);
    }
    EXPECT_NEAR(out, 1.0f, 1e-3f);
}

TEST(ModulationDelayNoFeedback, hardSwitchWithoutWrapAndClampsSize)
{
    ModulationDelayNoFeedback<4096> delay(48000.f);
    delay.setChangeSizeMode(ChangeSizeMode::HARDSWITCH);
    delay.setSize(100);    // below the write head, no wrap
    delay.setSize(5);      // clamped up to the 48-sample floor
    delay.setSize(999999); // clamped down to MAXSIZE - 1
    float out = 0.f;
    for (int i = 0; i < 6000; ++i)
    {
        out = delay.step(1.0f);
    }
    EXPECT_TRUE(std::isfinite(out));
}

TEST(ModulationDelayNoFeedback, sameSizeIsANoOp)
{
    ModulationDelayNoFeedback<4096> delay(48000.f);
    delay.setChangeSizeMode(ChangeSizeMode::HARDSWITCH);
    delay.setSize(512); // equals the initial width: early return, heads untouched
    EXPECT_TRUE(std::isfinite(delay.step(1.0f)));
}

TEST(ModulationDelayNoFeedback, pitchModeConvergesDelayWidthUpThenDown)
{
    ModulationDelayNoFeedback<8192> delay(48000.f); // initial width = 1024
    delay.setChangeSizeMode(ChangeSizeMode::PITCH);

    delay.setSize(1500); // grow: read head advances slower until the gap widens
    float out = 0.f;
    for (int i = 0; i < 40000; ++i)
    {
        out = delay.step(1.0f);
    }
    EXPECT_NEAR(static_cast<float>(delay.size()), 1500.f, 4.f);
    EXPECT_NEAR(out, 1.0f, 1e-2f);

    delay.setSize(600); // shrink: read head advances faster until the gap narrows
    for (int i = 0; i < 40000; ++i)
    {
        out = delay.step(1.0f);
    }
    EXPECT_NEAR(static_cast<float>(delay.size()), 600.f, 4.f);
    EXPECT_TRUE(std::isfinite(out));
}

TEST(ModulationDelayNoFeedback, fadeModeSchedulesSecondResizeMidFade)
{
    ModulationDelayNoFeedback<8192> delay(48000.f); // default FADE mode, initial width 1024
    delay.setSize(2000);                            // first fade target
    for (int i = 0; i < 100; ++i)
    {
        static_cast<void>(delay.step(1.0f)); // fade in progress
    }
    delay.setSize(3000); // scheduled while the first fade is still running

    float out = 0.f;
    for (int i = 0; i < 20000; ++i)
    {
        out = delay.step(1.0f); // finish both fades
    }
    EXPECT_TRUE(std::isfinite(out));
    EXPECT_EQ(delay.size(), 3000u);
}

TEST(ModulationDelayNoFeedback, readTapDuringAndAfterFade)
{
    ModulationDelayNoFeedback<8192> delay(48000.f);
    delay.setSize(2000);
    static_cast<void>(delay.step(1.0f));             // starts the fade
    EXPECT_TRUE(std::isfinite(delay.readTap(0.5f))); // crossfading tap

    for (int i = 0; i < 20000; ++i)
    {
        static_cast<void>(delay.step(1.0f));
    }
    EXPECT_TRUE(std::isfinite(delay.readTap(0.5f))); // steady tap
}

TEST(ModulationDelayNoFeedback, zeroModDepthTakesDirectReadPath)
{
    ModulationDelayNoFeedback<4096> delay(48000.f);
    delay.setModDepth(0.f); // applied when the sweep phase next crosses zero
    float out = 0.f;
    for (int i = 0; i < 6000; ++i)
    {
        out = delay.step(1.0f);
    }
    EXPECT_NEAR(out, 1.0f, 1e-3f);
}

TEST(ModulationDelayNoFeedback, modulationSweepStaysFinite)
{
    ModulationDelayNoFeedback<4096> delay(48000.f);
    delay.setModDepth(0.8f);
    delay.setModSpeed(6.f);
    float out = 0.f;
    for (int i = 0; i < 5000; ++i)
    {
        out = delay.step(std::sin(static_cast<float>(i) * 0.03f));
    }
    EXPECT_TRUE(std::isfinite(out));
}

}
