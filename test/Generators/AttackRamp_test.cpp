#include <gtest/gtest.h>
#include <array>

#include "Generators/AttackRamp.h"

namespace AbacDsp::test
{

class AttackRampTest : public ::testing::Test
{
  protected:
    AttackRamp<64> ramp{48000.0f};
};

TEST_F(AttackRampTest, InitiallyIdle)
{
    EXPECT_FALSE(ramp.isActive());
}

TEST_F(AttackRampTest, LinearRampMonotonic)
{
    ramp.setMode(AttackRamp<64>::RampMode::Linear);
    ramp.setAttackTimeMs(10.0f);
    ramp.trigger();

    float prev = 0.0f;
    std::array<float, 64> block;
    block.fill(1.0f);

    for (int i = 0; i < 10; ++i)
    {
        ramp.processBlock(block);
        for (size_t j = 0; j < 64; ++j)
        {
            EXPECT_GE(block[j], prev);
            prev = block[j];
        }
        block.fill(1.0f);
    }

    EXPECT_FLOAT_EQ(block[63], 1.0f);
}

TEST_F(AttackRampTest, ExponentialRampMonotonic)
{
    ramp.setMode(AttackRamp<64>::RampMode::Exponential);
    ramp.setAttackTimeMs(10.0f);
    ramp.trigger();

    float prev = 0.0f;
    std::array<float, 64> block;
    block.fill(1.0f);

    for (int i = 0; i < 10; ++i)
    {
        ramp.processBlock(block);
        for (size_t j = 0; j < 64; ++j)
        {
            EXPECT_GE(block[j], prev);
            prev = block[j];
        }
        block.fill(1.0f);
    }

    EXPECT_FLOAT_EQ(block[63], 1.0f);
}

TEST_F(AttackRampTest, BecomesActiveAfterRamp)
{
    ramp.setAttackTimeMs(2.0f);
    ramp.trigger();

    std::array<float, 64> block;
    block.fill(1.0f);

    for (int i = 0; i < 10; ++i)
    {
        ramp.processBlock(block);
        if (ramp.isActive())
        {
            break;
        }
        block.fill(1.0f);
    }

    EXPECT_TRUE(ramp.isActive());
}

} // namespace AbacDsp::test
