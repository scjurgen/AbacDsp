#include <cmath>
#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "Delays/FracReadHead.h"

namespace AbacDsp::Test
{


constexpr size_t WrapSize{2048};
constexpr float SampleRate{48000.f};

TEST(FracReadHeadTest, advancesLinearWhenNotAdjusting)
{
    FracReadHead<WrapSize> sut(SampleRate);
    size_t hd = 0;
    for (size_t i = 0; i < 100; ++i)
    {
        const auto p = sut.step((hd++) % WrapSize);
        EXPECT_FLOAT_EQ(p, static_cast<float>(i + 1));
        EXPECT_FALSE(sut.isAdjusting());
        EXPECT_EQ(sut.getCurrentPhase(), TransitionPhase::Idle);
    }
}

TEST(FracReadHeadTest, adjustNearing)
{
    FracReadHead<WrapSize, false> sut(SampleRate);
    size_t hd = 0;
    sut.setNewDelta(1000);
    for (size_t i = 0; i < 5500; ++i)
    {
        sut.step((hd++) % WrapSize);
        if (!sut.isAdjusting())
        {
            break;
        }
    }
    EXPECT_FALSE(sut.isAdjusting());
    float previous = sut.step((hd++) % WrapSize);
    sut.setNewDelta(500);
    // EXPECT_EQ(sut.totalSteps(), 1500);
    for (size_t i = 0; i < 1497; ++i)
    {
        const auto p = sut.step((hd++) % WrapSize);
        float delta = p - previous;
        if (delta < 0)
        {
            delta += WrapSize;
        }
        EXPECT_GE(delta, 1.0f) << "failed at " << i;

        previous = p;
        EXPECT_TRUE(sut.isAdjusting()) << "failed at " << i;
        EXPECT_EQ(sut.getCurrentPhase(), TransitionPhase::Ramping) << "failed at " << i;
    }

    for (size_t i = 0; i < 9; ++i)
    {
        const auto p = sut.step((hd++) % WrapSize);
        EXPECT_LE(p - previous, 1.1f) << "failed at " << i;
        previous = p;
        EXPECT_FALSE(sut.isAdjusting());
        EXPECT_EQ(sut.getCurrentPhase(), TransitionPhase::Idle) << "failed at " << i;
    }
}


TEST(FracReadHeadTest, adjustDistancing)
{
    FracReadHead<WrapSize, false> sut(SampleRate);
    size_t hd = 0;
    sut.setNewDelta(500);
    for (size_t i = 0; i < 2500; ++i)
    {
        sut.step((hd++) % WrapSize);
        if (!sut.isAdjusting())
        {
            break;
        }
    }
    EXPECT_FALSE(sut.isAdjusting());
    float previous = sut.step((hd++) % WrapSize);
    sut.setNewDelta(1000);
    // EXPECT_EQ(sut.totalSteps(), 2251);
    for (size_t i = 0; i < 2253; ++i)
    {
        const auto p = sut.step((hd++) % WrapSize);
        float delta = p - previous;
        if (delta > WrapSize)
        {
            delta -= WrapSize;
        }
        EXPECT_LT(delta, 1.01f) << "failed at " << i;

        previous = p;
        EXPECT_TRUE(sut.isAdjusting()) << "failed at " << i;
        EXPECT_EQ(sut.getCurrentPhase(), TransitionPhase::Ramping) << "failed at " << i;
    }

    for (size_t i = 0; i < 9; ++i)
    {
        const auto p = sut.step((hd++) % WrapSize);
        EXPECT_LE(p - previous, 1.1f) << "failed at " << i;
        previous = p;
        EXPECT_FALSE(sut.isAdjusting());
        EXPECT_EQ(sut.getCurrentPhase(), TransitionPhase::Idle) << "failed at " << i;
    }
}
TEST(FracReadHeadTest, adjustNearingQuartic)
{
    FracReadHead<WrapSize, true> sut(SampleRate);
    size_t hd = 0;
    sut.setNewDelta(1000);
    for (size_t i = 0; i < 6000; ++i)
    {
        sut.step((hd++) % WrapSize);
        if (!sut.isAdjusting())
        {
            break;
        }
    }
    EXPECT_FALSE(sut.isAdjusting());
    float previous = sut.step((hd++) % WrapSize);
    sut.setNewDelta(500);
    EXPECT_EQ(sut.totalSteps(), 1872);
    for (size_t i = 0; i < 1871; ++i)
    {
        const auto p = sut.step((hd++) % WrapSize);
        float delta = p - previous;

        if (delta < 0)
        {
            delta += WrapSize;
        }
        EXPECT_GE(delta, 1.0f) << "failed at " << i;

        previous = p;
        EXPECT_TRUE(sut.isAdjusting()) << "failed at " << i;
        EXPECT_EQ(sut.getCurrentPhase(), TransitionPhase::Ramping) << "failed at " << i;
    }

    for (size_t i = 0; i < 9; ++i)
    {
        const auto p = sut.step((hd++) % WrapSize);
        EXPECT_LE(p - previous, 1.1f) << "failed at " << i;
        previous = p;
        EXPECT_FALSE(sut.isAdjusting());
        EXPECT_EQ(sut.getCurrentPhase(), TransitionPhase::Idle) << "failed at " << i;
    }
}

TEST(FracReadHeadTest, adjustDistancingQuartic)
{
    FracReadHead<WrapSize, true> sut(SampleRate);
    size_t hd = 0;
    sut.setNewDelta(500);
    for (size_t i = 0; i < 3000; ++i)
    {
        sut.step((hd++) % WrapSize);
        if (!sut.isAdjusting())
        {
            break;
        }
    }
    EXPECT_FALSE(sut.isAdjusting());
    float previous = sut.step((hd++) % WrapSize);
    sut.setNewDelta(1000);
    EXPECT_EQ(sut.totalSteps(), 2818);
    for (size_t i = 0; i < 2817; ++i)
    {
        const auto p = sut.step((hd++) % WrapSize);
        float delta = p - previous;
        if (delta > WrapSize)
        {
            delta -= WrapSize;
        }

        EXPECT_LT(delta, 1.01f) << "failed at " << i;

        previous = p;
        EXPECT_TRUE(sut.isAdjusting()) << "failed at " << i;
        EXPECT_EQ(sut.getCurrentPhase(), TransitionPhase::Ramping) << "failed at " << i;
    }

    for (size_t i = 0; i < 9; ++i)
    {
        const auto p = sut.step((hd++) % WrapSize);
        EXPECT_LE(p - previous, 1.1f) << "failed at " << i;
        previous = p;
        EXPECT_FALSE(sut.isAdjusting());
        EXPECT_EQ(sut.getCurrentPhase(), TransitionPhase::Idle) << "failed at " << i;
    }
}

// Regression test: setNewDelta() used to size every ramp for the quadratic
// curve's mean bump (2c/3) even when quartic=true, whose mean is 8c/15, so a
// quartic ramp stopped 20% short of the requested delta.
TEST(FracReadHeadTest, quarticRampReachesTargetDeltaWithoutUndershoot)
{
    for (const bool distancing : {true, false})
    {
        for (const float target : {200.f, 500.f, 1000.f, 1500.f})
        {
            FracReadHead<WrapSize, true> sut(SampleRate);
            size_t hd = 0;
            sut.setNewDelta(distancing ? target : 1800.f);
            while (sut.isAdjusting())
            {
                sut.step(static_cast<float>((hd++) % WrapSize));
            }

            if (!distancing)
            {
                sut.setNewDelta(target);
                while (sut.isAdjusting())
                {
                    sut.step(static_cast<float>((hd++) % WrapSize));
                }
            }

            EXPECT_NEAR(sut.getCurrentDelta(), target, 1.5f)
                << (distancing ? "distancing" : "nearing") << " to target " << target;
        }
    }
}
}
