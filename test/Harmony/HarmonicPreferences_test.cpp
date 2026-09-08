#include "gtest/gtest.h"

#include "Harmony/HarmonicPreferences.h"

namespace AbacDsp::Test
{

TEST(WishWeightsTest, neutralIsHalfForEveryWish)
{
    const auto weights = neutralWishWeights();
    for (size_t i = 0; i < kNumWishKinds; ++i)
    {
        EXPECT_FLOAT_EQ(weights[i], 0.5f);
    }
}

TEST(WishWeightsTest, setWishWeightClampsToUnitRange)
{
    auto weights = neutralWishWeights();
    setWishWeight(weights, WishKind::Tension, 1.5f);
    EXPECT_FLOAT_EQ(wishWeight(weights, WishKind::Tension), 1.f);
    setWishWeight(weights, WishKind::Tension, -0.5f);
    EXPECT_FLOAT_EQ(wishWeight(weights, WishKind::Tension), 0.f);
}

TEST(ApplyVowTest, unviolatedVowLeavesScoreUnchangedRegardlessOfStrength)
{
    for (const auto strength : {VowStrength::Never, VowStrength::Usually, VowStrength::Sometimes})
    {
        const Vow vow{VowKind::KeepHomeAudible, strength};
        const auto result = applyVow(vow, false, 0.8f);
        ASSERT_TRUE(result.has_value());
        EXPECT_FLOAT_EQ(*result, 0.8f);
    }
}

TEST(ApplyVowTest, violatedNeverVowRejectsTheCandidate)
{
    const Vow vow{VowKind::NoLargeVoiceJumps, VowStrength::Never};
    const auto result = applyVow(vow, true, 0.8f);
    EXPECT_FALSE(result.has_value());
}

TEST(ApplyVowTest, violatedUsuallyVowPenalizesMoreThanSometimes)
{
    const Vow usually{VowKind::LimitBrightness, VowStrength::Usually};
    const Vow sometimes{VowKind::LimitBrightness, VowStrength::Sometimes};
    const auto usuallyResult = applyVow(usually, true, 1.f);
    const auto sometimesResult = applyVow(sometimes, true, 1.f);
    ASSERT_TRUE(usuallyResult.has_value());
    ASSERT_TRUE(sometimesResult.has_value());
    EXPECT_LT(*usuallyResult, *sometimesResult);
}

TEST(ActiveImpulseTest, envelopeRisesLinearlyDuringAttack)
{
    ActiveImpulse impulse{ImpulseKind::Arrive};
    EXPECT_FLOAT_EQ(impulse.envelopeValue(), 0.f);
    impulse.advance(ActiveImpulse::kAttackSeconds * 0.5f);
    EXPECT_NEAR(impulse.envelopeValue(), 0.5f, 1e-4f);
}

TEST(ActiveImpulseTest, envelopeHoldsAtFullStrength)
{
    ActiveImpulse impulse{ImpulseKind::Arrive};
    impulse.advance(ActiveImpulse::kAttackSeconds + ActiveImpulse::kHoldSeconds * 0.5f);
    EXPECT_FLOAT_EQ(impulse.envelopeValue(), 1.f);
    EXPECT_FALSE(impulse.isFinished());
}

TEST(ActiveImpulseTest, envelopeDecaysToZeroAndFinishes)
{
    ActiveImpulse impulse{ImpulseKind::Arrive};
    impulse.advance(ActiveImpulse::kAttackSeconds + ActiveImpulse::kHoldSeconds + ActiveImpulse::kDecaySeconds * 0.5f);
    EXPECT_NEAR(impulse.envelopeValue(), 0.5f, 1e-4f);
    EXPECT_FALSE(impulse.isFinished());

    impulse.advance(ActiveImpulse::kDecaySeconds);
    EXPECT_FLOAT_EQ(impulse.envelopeValue(), 0.f);
    EXPECT_TRUE(impulse.isFinished());
}

TEST(ActiveImpulseTest, releaseEarlyJumpsStraightToDecay)
{
    ActiveImpulse impulse{ImpulseKind::Stay};
    impulse.advance(ActiveImpulse::kAttackSeconds + ActiveImpulse::kHoldSeconds * 0.2f);
    ASSERT_FLOAT_EQ(impulse.envelopeValue(), 1.f);

    impulse.releaseEarly();
    EXPECT_FLOAT_EQ(impulse.envelopeValue(), 1.f); // decay phase starts at full strength
    impulse.advance(ActiveImpulse::kDecaySeconds);
    EXPECT_FLOAT_EQ(impulse.envelopeValue(), 0.f);
}

TEST(ApplyImpulseTest, releaseCarriesNoWishBiasOfItsOwn)
{
    EXPECT_EQ(impulseBiases(ImpulseKind::Release).count, 0u);
}

TEST(ApplyImpulseTest, zeroStrengthLeavesWeightsUnchanged)
{
    auto weights = neutralWishWeights();
    const ActiveImpulse impulse{ImpulseKind::Darken}; // envelopeValue() == 0 at t = 0
    applyImpulse(impulse, weights);
    EXPECT_EQ(weights, neutralWishWeights());
}

TEST(ApplyImpulseTest, fullStrengthAppliesTheCuratedBias)
{
    auto weights = neutralWishWeights();
    ActiveImpulse impulse{ImpulseKind::Darken};
    impulse.advance(ActiveImpulse::kAttackSeconds); // full strength

    applyImpulse(impulse, weights);

    const auto biasSet = impulseBiases(ImpulseKind::Darken);
    for (size_t i = 0; i < biasSet.count; ++i)
    {
        const auto& bias = biasSet.biases[i];
        EXPECT_FLOAT_EQ(wishWeight(weights, bias.kind), 0.5f + bias.delta);
    }
}

}
