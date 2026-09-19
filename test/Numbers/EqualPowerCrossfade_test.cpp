#include <array>
#include <cmath>
#include <numbers>

#include "gtest/gtest.h"

#include "Numbers/EqualPowerCrossfade.h"

namespace AbacDsp::Test
{
namespace
{
constexpr float kGainTolerance{1E-4f};
constexpr float kPowerTolerance{2E-4f};
}

TEST(EqualPowerCrossfadeTests, IdleCrossfadeYieldsIncomingOnly)
{
    EqualPowerCrossfade sut;
    EXPECT_TRUE(sut.isDone());
    const auto gains = sut.step();
    EXPECT_EQ(gains.outgoing, 0.f);
    EXPECT_EQ(gains.incoming, 1.f);
}

TEST(EqualPowerCrossfadeTests, ZeroStepsFinishesImmediately)
{
    EqualPowerCrossfade sut;
    sut.start(0);
    EXPECT_TRUE(sut.isDone());
    EXPECT_EQ(sut.step().incoming, 1.f);
}

TEST(EqualPowerCrossfadeTests, FirstSampleIsOutgoingAndFadeEndsAfterExactlyTheGivenSteps)
{
    EqualPowerCrossfade sut;
    sut.start(8);
    const auto first = sut.step();
    EXPECT_NEAR(first.outgoing, 1.f, kGainTolerance);
    EXPECT_NEAR(first.incoming, 0.f, kGainTolerance);

    for (size_t i = 1; i < 8; ++i)
    {
        EXPECT_FALSE(sut.isDone());
        static_cast<void>(sut.step());
    }
    EXPECT_TRUE(sut.isDone());
    EXPECT_EQ(sut.step().outgoing, 0.f);
}

TEST(EqualPowerCrossfadeTests, GainsFollowQuarterTurnCosAndSin)
{
    constexpr size_t kSteps{100};
    EqualPowerCrossfade sut;
    sut.start(kSteps);
    for (size_t i = 0; i < kSteps; ++i)
    {
        const auto progress = static_cast<float>(i) / static_cast<float>(kSteps);
        const auto gains = sut.step();
        EXPECT_NEAR(gains.outgoing, std::cos(progress * std::numbers::pi_v<float> / 2.f), kGainTolerance);
        EXPECT_NEAR(gains.incoming, std::sin(progress * std::numbers::pi_v<float> / 2.f), kGainTolerance);
    }
}

TEST(EqualPowerCrossfadeTests, PowerStaysConstantAcrossTheFade)
{
    constexpr size_t kSteps{257};
    EqualPowerCrossfade sut;
    sut.start(kSteps);
    for (size_t i = 0; i < kSteps; ++i)
    {
        const auto gains = sut.step();
        EXPECT_NEAR(gains.outgoing * gains.outgoing + gains.incoming * gains.incoming, 1.f, kPowerTolerance);
    }
}

TEST(EqualPowerCrossfadeTests, MixMatchesPerSampleStepAndContinuesAcrossCalls)
{
    constexpr size_t kSteps{10};
    const std::array<float, 6> outgoing{1.f, 2.f, 3.f, 4.f, 5.f, 6.f};
    const std::array<float, 6> incoming{-1.f, 0.5f, 2.f, -3.f, 1.f, 8.f};

    EqualPowerCrossfade reference;
    reference.start(kSteps);
    std::array<float, 12> expected{};
    for (size_t i = 0; i < 6; ++i)
    {
        const auto gains = reference.step();
        expected[i] = outgoing[i] * gains.outgoing + incoming[i] * gains.incoming;
    }
    for (size_t i = 0; i < 6; ++i)
    {
        const auto gains = reference.step();
        expected[6 + i] = outgoing[i] * gains.outgoing + incoming[i] * gains.incoming;
    }

    EqualPowerCrossfade sut;
    sut.start(kSteps);
    std::array<float, 12> actual{};
    sut.mix(outgoing, incoming, std::span<float>{actual}.first(6));
    sut.mix(outgoing, incoming, std::span<float>{actual}.last(6));
    for (size_t i = 0; i < actual.size(); ++i)
    {
        EXPECT_EQ(actual[i], expected[i]) << "index " << i;
    }
}

TEST(EqualPowerCrossfadeTests, SkipAdvancesLikeStepsAndStopsAtTheEnd)
{
    EqualPowerCrossfade skipped;
    skipped.start(10);
    skipped.skip(4);
    EqualPowerCrossfade stepped;
    stepped.start(10);
    for (size_t i = 0; i < 4; ++i)
    {
        static_cast<void>(stepped.step());
    }
    EXPECT_EQ(skipped.step().incoming, stepped.step().incoming);

    skipped.skip(1000);
    EXPECT_TRUE(skipped.isDone());
}

TEST(EqualPowerCrossfadeTests, MixMayWriteIntoItsOutgoingInput)
{
    std::array<float, 4> outgoing{1.f, 1.f, 1.f, 1.f};
    const std::array<float, 4> incoming{2.f, 2.f, 2.f, 2.f};

    EqualPowerCrossfade reference;
    reference.start(4);
    std::array<float, 4> expected{};
    for (size_t i = 0; i < 4; ++i)
    {
        const auto gains = reference.step();
        expected[i] = outgoing[i] * gains.outgoing + incoming[i] * gains.incoming;
    }

    EqualPowerCrossfade sut;
    sut.start(4);
    sut.mix(outgoing, incoming, outgoing);
    for (size_t i = 0; i < 4; ++i)
    {
        EXPECT_EQ(outgoing[i], expected[i]);
    }
}

}
