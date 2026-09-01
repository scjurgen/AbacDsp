#include <cmath>

#include "gtest/gtest.h"

#include "NonLinear/WaveShaperTables.h"

namespace AbacDsp::Test
{

TEST(WaveShaperTables, storeBuildsOnePresetPerTableEntry)
{
    const WaveShaperTableStore store;
    for (size_t i = 0; i < kDistortionWaveTableSet.size(); ++i)
    {
        const auto v = store.getValue(i, 0.0f);
        EXPECT_TRUE(std::isfinite(v)) << "preset " << kDistortionWaveTableSet[i].name;
    }
}

TEST(WaveShaperTables, everyPresetStaysFiniteAndBoundedAcrossInputRange)
{
    const WaveShaperTableStore store;
    for (size_t i = 0; i < kDistortionWaveTableSet.size(); ++i)
    {
        for (int step = -200; step <= 200; ++step)
        {
            const auto x = static_cast<float>(step) * 0.02f;
            const auto v = store.getValue(i, x);
            ASSERT_TRUE(std::isfinite(v)) << "preset " << kDistortionWaveTableSet[i].name << " at x=" << x;
            ASSERT_GE(v, -8.0f) << "preset " << kDistortionWaveTableSet[i].name << " at x=" << x;
            ASSERT_LE(v, 8.0f) << "preset " << kDistortionWaveTableSet[i].name << " at x=" << x;
        }
    }
}

TEST(WaveShaperTables, classicSiliconIsClampedLinearIdentity)
{
    const WaveShaperTableStore store;
    const auto siliconIndex = static_cast<size_t>(std::distance(
        kDistortionWaveTableSet.begin(),
        std::ranges::find_if(kDistortionWaveTableSet, [](const auto& p) { return p.name == "classic silicon"; })));
    EXPECT_NEAR(store.getValue(siliconIndex, 0.0f), 0.0f, 0.01f);
    EXPECT_NEAR(store.getValue(siliconIndex, 0.5f), 0.5f, 0.02f);
    EXPECT_NEAR(store.getValue(siliconIndex, -0.5f), -0.5f, 0.02f);
    EXPECT_NEAR(store.getValue(siliconIndex, 2.0f), 1.0f, 0.02f);
    EXPECT_NEAR(store.getValue(siliconIndex, -2.0f), -1.0f, 0.02f);
}

TEST(WaveShaperTables, processBlockMatchesPerSampleGetValue)
{
    const WaveShaperTableStore store;
    constexpr size_t presetIndex = 0;
    std::array<float, 8> samples{-1.0f, -0.7f, -0.3f, -0.1f, 0.1f, 0.3f, 0.7f, 1.0f};
    std::array<float, 8> expected{};
    for (size_t i = 0; i < samples.size(); ++i)
    {
        expected[i] = store.getValue(presetIndex, samples[i]);
    }

    store.processBlock(presetIndex, samples.data(), samples.size());

    for (size_t i = 0; i < samples.size(); ++i)
    {
        EXPECT_FLOAT_EQ(samples[i], expected[i]);
    }
}

TEST(InterpolateWithEquidistantControlPoints, linearRampReproducesEndpoints)
{
    InterpolateWithEquidistantControlPoints ipl;
    ipl.setInterpolationFunction(Interpolation::linearPt2, 0);
    ipl.setControlVector(false, {-1.0f, 1.0f}, 2.0f);

    EXPECT_NEAR(ipl.getValue(0.0f), 0.0f, 1e-4f);
    EXPECT_NEAR(ipl.getValue(1.0f), 1.0f, 1e-4f);
    EXPECT_NEAR(ipl.getValue(-1.0f), -1.0f, 1e-4f);
}

}
