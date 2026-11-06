#include <array>
#include <cmath>
#include <cstddef>

#include "gtest/gtest.h"

#include "Wavetables/MorphCarrierLevel.h"

namespace AbacDsp::Test
{

namespace
{

constexpr std::array defaultWaves{BasicWave::Sine, BasicWave::Square, BasicWave::Saw};

/// The RMS of (1 - f) a + f b over the table, summed directly from the samples of the store.
float bruteForceRms(const BasicWave first, const BasicWave second, const float fraction, const float phaseIncrement)
{
    const WaveTableSet& a = WaveTableStore::getTableSet(first);
    const WaveTableSet& b = WaveTableStore::getTableSet(second);
    const WaveTable& ta = a.tables[a.getIndexByFrequency(phaseIncrement * 2)];
    const WaveTable& tb = b.tables[b.getIndexByFrequency(phaseIncrement * 2)];
    double sum{0.0};
    for (size_t i = 0; i < WaveTableSize; ++i)
    {
        const double v = (1.0 - static_cast<double>(fraction)) * static_cast<double>(ta.data[i]) +
                         static_cast<double>(fraction) * static_cast<double>(tb.data[i]);
        sum += v * v;
    }
    return static_cast<float>(std::sqrt(sum / static_cast<double>(WaveTableSize)));
}

} // namespace

TEST(MorphCarrierLevel, thePositionMirrorsTheMorphOfTheOscillator)
{
    EXPECT_EQ(0u, MorphCarrierLevel::positionOf(-1.f).pair);
    EXPECT_FLOAT_EQ(0.f, MorphCarrierLevel::positionOf(-1.f).fraction);
    EXPECT_EQ(0u, MorphCarrierLevel::positionOf(-0.5f).pair);
    EXPECT_FLOAT_EQ(0.5f, MorphCarrierLevel::positionOf(-0.5f).fraction);
    EXPECT_EQ(1u, MorphCarrierLevel::positionOf(0.f).pair);
    EXPECT_FLOAT_EQ(0.f, MorphCarrierLevel::positionOf(0.f).fraction);
    EXPECT_EQ(1u, MorphCarrierLevel::positionOf(1.f).pair);
    EXPECT_FLOAT_EQ(0.999999f, MorphCarrierLevel::positionOf(1.f).fraction);
}

TEST(MorphCarrierLevel, aMorphOutsideTheRangeIsClamped)
{
    EXPECT_EQ(0u, MorphCarrierLevel::positionOf(-5.f).pair);
    EXPECT_FLOAT_EQ(0.f, MorphCarrierLevel::positionOf(-5.f).fraction);
    EXPECT_EQ(1u, MorphCarrierLevel::positionOf(5.f).pair);
    EXPECT_FLOAT_EQ(0.999999f, MorphCarrierLevel::positionOf(5.f).fraction);
}

TEST(MorphCarrierLevel, theLowestLevelOfEveryDefaultWaveHasTheRmsOfTheStore)
{
    const MorphCarrierLevel level{defaultWaves};
    constexpr float lowPitch = 20.f / 48000.f;
    EXPECT_NEAR(0.5f, level.rms(-1.f, lowPitch), 1e-3f);
    EXPECT_NEAR(0.5f, level.rms(0.f, lowPitch), 1e-3f);
    EXPECT_NEAR(0.5f, level.rms(1.f, lowPitch), 1e-3f);
}

TEST(MorphCarrierLevel, theLevelOfAMorphEqualsTheRmsSummedFromTheTables)
{
    const MorphCarrierLevel level{defaultWaves};
    for (const float pitchHz : {50.f, 440.f, 2000.f, 5000.f, 11000.f})
    {
        const float increment = pitchHz / 48000.f;
        for (const float fraction : {0.f, 0.25f, 0.5f, 0.9f})
        {
            const float morphFirstPair = -1.f + fraction;
            const float morphSecondPair = fraction;
            EXPECT_NEAR(bruteForceRms(BasicWave::Sine, BasicWave::Square, fraction, increment),
                        level.rms(morphFirstPair, increment), 1e-5f)
                << pitchHz << " Hz first pair " << fraction;
            EXPECT_NEAR(bruteForceRms(BasicWave::Square, BasicWave::Saw, fraction, increment),
                        level.rms(morphSecondPair, increment), 1e-5f)
                << pitchHz << " Hz second pair " << fraction;
        }
    }
}

TEST(MorphCarrierLevel, aCrossfadeOfCorrelatedWavesDipsInTheMiddle)
{
    const MorphCarrierLevel level{defaultWaves};
    constexpr float increment = 100.f / 48000.f;
    const float middle = level.rms(-0.5f, increment);
    EXPECT_LT(middle, level.rms(-1.f, increment));
    EXPECT_LT(middle, level.rms(0.f, increment));
    EXPECT_GT(middle, 0.45f);
}

TEST(MorphCarrierLevel, aHigherLevelLosesHarmonicsAndSoRms)
{
    const MorphCarrierLevel level{defaultWaves};
    EXPECT_LT(level.rms(0.f, 5000.f / 48000.f), level.rms(0.f, 100.f / 48000.f));
}

TEST(MorphCarrierLevel, replacingASlotChangesThePairsThatUseIt)
{
    MorphCarrierLevel level{defaultWaves};
    constexpr float increment = 100.f / 48000.f;
    const float before = level.rms(0.5f, increment);
    level.setWave(2, BasicWave::Sine);
    EXPECT_NEAR(bruteForceRms(BasicWave::Square, BasicWave::Sine, 0.5f, increment), level.rms(0.5f, increment), 1e-5f);
    EXPECT_NE(before, level.rms(0.5f, increment));
}

TEST(MorphCarrierLevel, aSawNextToASquareNullsTheFundamentalAndASharkFinDoesNot)
{
    constexpr float increment = 100.f / 48000.f;
    const auto worstDipDb = [&](const std::array<BasicWave, 3>& waves)
    {
        const MorphCarrierLevel level{waves};
        float lowest{1.f};
        for (int step = 0; step <= 100; ++step)
        {
            lowest = std::min(lowest, level.rms(0.02f * static_cast<float>(step) - 1.f, increment) / 0.5f);
        }
        return 20.f * std::log10(lowest);
    };
    EXPECT_LT(worstDipDb({BasicWave::Sine, BasicWave::Square, BasicWave::Saw}), -10.f);
    EXPECT_GT(worstDipDb({BasicWave::Sine, BasicWave::Square, BasicWave::SharkFin}), -0.5f);
}

TEST(MorphCarrierLevel, everyLevelOfTheLadderIsCovered)
{
    const MorphCarrierLevel level{defaultWaves};
    EXPECT_EQ(WaveTableStore::getTableSet(BasicWave::Sine).tables.size(), level.levelCount());
    EXPECT_GT(level.levelCount(), 1u);
}

} // namespace AbacDsp::Test
