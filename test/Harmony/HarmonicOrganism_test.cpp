#include "gtest/gtest.h"

#include "Harmony/HarmonicOrganism.h"

namespace AbacDsp::Test
{

namespace
{
constexpr float kSampleRate{48000.f};
}

TEST(WishAffinityTest, intrinsicAxisRewardsMatchingTag)
{
    const HarmonicState current{};
    const HarmonicState candidate{.luminosity = 0.8f};
    auto weights = neutralWishWeights();

    setWishWeight(weights, WishKind::Luminosity, 0.8f);
    EXPECT_NEAR(wishAffinity(current, candidate, WishKind::Luminosity, weights), 1.f, 1e-5f);

    setWishWeight(weights, WishKind::Luminosity, 0.2f);
    EXPECT_NEAR(wishAffinity(current, candidate, WishKind::Luminosity, weights), 0.4f, 1e-5f);
}

TEST(WishAffinityTest, closenessRewardsSharedPitchClassesWhenWishIsHigh)
{
    const HarmonicState current{.voicing = Voicing::fromSemitones(std::to_array<int>({0, 4, 7}))};
    const HarmonicState identical{.voicing = Voicing::fromSemitones(std::to_array<int>({0, 4, 7}))};
    const HarmonicState disjoint{.voicing = Voicing::fromSemitones(std::to_array<int>({1, 5, 8}))};

    auto weights = neutralWishWeights();
    setWishWeight(weights, WishKind::Closeness, 1.f);

    EXPECT_GT(wishAffinity(current, identical, WishKind::Closeness, weights),
              wishAffinity(current, disjoint, WishKind::Closeness, weights));
}

TEST(WishAffinityTest, mobilityRewardsMoreMotionWhenWishIsHigh)
{
    const HarmonicState current{.voicing = Voicing::fromSemitones(std::to_array<int>({0, 4, 7}))};
    const HarmonicState nearby{.voicing = Voicing::fromSemitones(std::to_array<int>({0, 4, 8}))};
    const HarmonicState distant{.voicing = Voicing::fromSemitones(std::to_array<int>({-12, 4, 19}))};

    auto weights = neutralWishWeights();
    setWishWeight(weights, WishKind::Mobility, 1.f);

    EXPECT_GT(wishAffinity(current, distant, WishKind::Mobility, weights),
              wishAffinity(current, nearby, WishKind::Mobility, weights));
}

TEST(VowViolatedTest, keepHomeAudibleChecksPitchClassPresence)
{
    const HarmonicState current{};
    const HarmonicState withHome{.voicing = Voicing::fromSemitones(std::to_array<int>({0, 4, 7}))};
    const HarmonicState withoutHome{.voicing = Voicing::fromSemitones(std::to_array<int>({1, 5, 8}))};
    EXPECT_FALSE(vowViolated(VowKind::KeepHomeAudible, 0, current, withHome));
    EXPECT_TRUE(vowViolated(VowKind::KeepHomeAudible, 0, current, withoutHome));
}

TEST(VowViolatedTest, noLargeVoiceJumpsComparesMotionAgainstCurrent)
{
    const HarmonicState current{.voicing = Voicing::fromSemitones(std::to_array<int>({0, 4, 7}))};
    const HarmonicState nearby{.voicing = Voicing::fromSemitones(std::to_array<int>({0, 4, 8}))};
    const HarmonicState farAway{.voicing = Voicing::fromSemitones(std::to_array<int>({0, 4, 19}))};
    EXPECT_FALSE(vowViolated(VowKind::NoLargeVoiceJumps, 0, current, nearby));
    EXPECT_TRUE(vowViolated(VowKind::NoLargeVoiceJumps, 0, current, farAway));
}

TEST(VowViolatedTest, limitDensityAndBrightnessReadTheCandidateAlone)
{
    const HarmonicState current{};
    const HarmonicState sparse{.voicing = Voicing::fromSemitones(std::to_array<int>({0, 4, 7})), .luminosity = 0.5f};
    const HarmonicState denseAndBright{.voicing = Voicing::fromSemitones(std::to_array<int>({0, 2, 4, 5, 7, 9, 11})),
                                       .luminosity = 0.95f};
    EXPECT_FALSE(vowViolated(VowKind::LimitDensity, 0, current, sparse));
    EXPECT_TRUE(vowViolated(VowKind::LimitDensity, 0, current, denseAndBright));
    EXPECT_FALSE(vowViolated(VowKind::LimitBrightness, 0, current, sparse));
    EXPECT_TRUE(vowViolated(VowKind::LimitBrightness, 0, current, denseAndBright));
}

TEST(VowViolatedTest, preserveOpenIntervalsFlagsAdjacentNotes)
{
    const HarmonicState current{};
    const HarmonicState open{.voicing = Voicing::fromSemitones(std::to_array<int>({0, 4, 7}))};
    const HarmonicState clustered{.voicing = Voicing::fromSemitones(std::to_array<int>({0, 1, 7}))};
    EXPECT_FALSE(vowViolated(VowKind::PreserveOpenIntervals, 0, current, open));
    EXPECT_TRUE(vowViolated(VowKind::PreserveOpenIntervals, 0, current, clustered));
}

TEST(VowViolatedTest, sparseLowEndFlagsMoreThanOneLowNote)
{
    const HarmonicState current{};
    const HarmonicState oneLowNote{.voicing = Voicing::fromSemitones(std::to_array<int>({-7, 0, 4}))};
    const HarmonicState twoLowNotes{.voicing = Voicing::fromSemitones(std::to_array<int>({-9, -7, 0}))};
    EXPECT_FALSE(vowViolated(VowKind::SparseLowEnd, 0, current, oneLowNote));
    EXPECT_TRUE(vowViolated(VowKind::SparseLowEnd, 0, current, twoLowNotes));
}

TEST(RegionBonusTest, matchingRegionGetsTheBonus)
{
    const HarmonicState candidate{.region = PaletteRegion::ChromaticWeather};
    EXPECT_FLOAT_EQ(regionBonus(candidate, PaletteRegion::ChromaticWeather), kPreferredRegionBonus);
}

TEST(RegionBonusTest, nonMatchingRegionGetsNothing)
{
    const HarmonicState candidate{.region = PaletteRegion::Home};
    EXPECT_FLOAT_EQ(regionBonus(candidate, PaletteRegion::ChromaticWeather), 0.f);
}

TEST(RegionBonusTest, noPreferenceGetsNothing)
{
    const HarmonicState candidate{.region = PaletteRegion::Home};
    EXPECT_FLOAT_EQ(regionBonus(candidate, std::nullopt), 0.f);
}

class HarmonicOrganismTest : public ::testing::Test
{
  protected:
    HarmonicOrganism organism{kSampleRate, 42u};
};

TEST_F(HarmonicOrganismTest, noTransitionBeforeDwellElapses)
{
    const auto samplesJustUnderDwell = static_cast<size_t>((HarmonicOrganism::kDwellSeconds - 0.5f) * kSampleRate);
    organism.step(samplesJustUnderDwell);
    EXPECT_FALSE(organism.takePendingTransition().has_value());
}

TEST_F(HarmonicOrganismTest, eventuallyTransitionsAcrossManyDwellCycles)
{
    const auto samplesPerDwell = static_cast<size_t>(HarmonicOrganism::kDwellSeconds * kSampleRate);
    bool sawTransition = false;
    for (int cycle = 0; cycle < 50 && !sawTransition; ++cycle)
    {
        organism.step(samplesPerDwell);
        sawTransition = organism.takePendingTransition().has_value();
    }
    EXPECT_TRUE(sawTransition);
}

TEST_F(HarmonicOrganismTest, preferredRegionShowsUpAmongVisitedStates)
{
    organism.setPreferredRegion(PaletteRegion::ChromaticWeather);
    const auto samplesPerDwell = static_cast<size_t>(HarmonicOrganism::kDwellSeconds * kSampleRate);
    bool sawPreferredRegion = false;
    for (int cycle = 0; cycle < 50 && !sawPreferredRegion; ++cycle)
    {
        organism.step(samplesPerDwell);
        if (const auto after = organism.takePendingTransition())
        {
            sawPreferredRegion = after->region == PaletteRegion::ChromaticWeather;
        }
    }
    EXPECT_TRUE(sawPreferredRegion);
}

TEST_F(HarmonicOrganismTest, neverVowIsNeverViolatedAcrossManySimulatedCycles)
{
    const auto samplesPerDwell = static_cast<size_t>(HarmonicOrganism::kDwellSeconds * kSampleRate);
    for (int cycle = 0; cycle < 80; ++cycle)
    {
        const auto before = organism.currentState();
        organism.step(samplesPerDwell);
        if (const auto after = organism.takePendingTransition())
        {
            const auto motion = voiceLeadingMotion(before.voicing, after->voicing);
            EXPECT_LE(motion.maxMotionSemitones, kLargeJumpSemitones);
        }
    }
}

TEST_F(HarmonicOrganismTest, arriveImpulseIncreasesClosenessAndReducesTensionAndMobilityWeights)
{
    const auto attackSamples = static_cast<size_t>(ActiveImpulse::kAttackSeconds * kSampleRate);
    const auto neutral = organism.currentWishWeights();
    organism.triggerImpulse(ImpulseKind::Arrive);
    organism.step(attackSamples); // reach full strength, well short of a dwell-period decision

    const auto biased = organism.currentWishWeights();
    EXPECT_GT(wishWeight(biased, WishKind::Closeness), wishWeight(neutral, WishKind::Closeness));
    EXPECT_LT(wishWeight(biased, WishKind::Tension), wishWeight(neutral, WishKind::Tension));
    EXPECT_LT(wishWeight(biased, WishKind::Mobility), wishWeight(neutral, WishKind::Mobility));
}

TEST_F(HarmonicOrganismTest, stayImpulseReducesMobilityAndIncreasesClosenessWeights)
{
    const auto attackSamples = static_cast<size_t>(ActiveImpulse::kAttackSeconds * kSampleRate);
    const auto neutral = organism.currentWishWeights();
    organism.triggerImpulse(ImpulseKind::Stay);
    organism.step(attackSamples);

    const auto biased = organism.currentWishWeights();
    EXPECT_LT(wishWeight(biased, WishKind::Mobility), wishWeight(neutral, WishKind::Mobility));
    EXPECT_GT(wishWeight(biased, WishKind::Closeness), wishWeight(neutral, WishKind::Closeness));
}

TEST_F(HarmonicOrganismTest, releaseFastDecaysAnActiveImpulse)
{
    organism.triggerImpulse(ImpulseKind::Stay);
    const auto attackSamples = static_cast<size_t>(ActiveImpulse::kAttackSeconds * kSampleRate);
    organism.step(attackSamples); // reach full strength
    const auto atFullStrength = organism.currentWishWeights();

    organism.triggerImpulse(ImpulseKind::Release);
    const auto fullDecaySamples = static_cast<size_t>(ActiveImpulse::kDecaySeconds * kSampleRate);
    organism.step(fullDecaySamples);
    const auto afterRelease = organism.currentWishWeights();

    EXPECT_GT(wishWeight(afterRelease, WishKind::Mobility), wishWeight(atFullStrength, WishKind::Mobility));
}

}
