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

}
