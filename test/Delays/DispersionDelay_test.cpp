
#include "Delays/DispersionDelay.h"

#include "gtest/gtest.h"
#include "gmock/gmock.h"

namespace AbacDsp::Test
{

TEST(DispersionDelayTest, dispersionUp)
{
    DispersionDelay<1000> sut(48000.f);
    sut.setSize(100);
    sut.setAllPassCutoff(10.f);
    for (size_t i = 0; i < 1024; ++i)
    {
        sut.step(0);
    }
    sut.step(1);
    for (size_t i = 0; i < 100; ++i)
    {
        EXPECT_EQ(sut.step(0), 0.f) << "failed at " << i;
    }
    EXPECT_LT(sut.step(0), -0.99f);
}

TEST(DispersionDelayTest, simpleFeedAndEat)
{
    DispersionDelay<1000> sut(48000.f);
    sut.setSize(100);
    sut.setAllPassCutoff(10000.f);
    for (size_t i = 0; i < 1024; ++i)
    {
        sut.step(0);
    }
    sut.step(1);
    for (size_t i = 0; i < 100; ++i)
    {
        EXPECT_EQ(sut.step(0), 0.f) << "failed at " << i;
    }
    EXPECT_LT(sut.step(0), -0.1f);
    EXPECT_GT(sut.step(0), 0.98f);
}

TEST(DispersionDelayTest, changeDelayTime)
{
    DispersionDelay<1000> sut(48000.f);
    sut.setFadeWindowSize(150);
    sut.setSize(100);
    sut.setAllPassCutoff(20000.f);
    for (size_t i = 0; i < 160; ++i)
    {
        sut.step(0);
    }
    sut.step(1);
    for (size_t i = 0; i < 100; ++i)
    {
        EXPECT_NEAR(sut.step(0), 0.f, 1E-5f) << "failed at " << i;
    }
    EXPECT_GT(sut.step(0), 0.5f);
    EXPECT_GT(sut.step(0), 0.6f);
    for (size_t i = 0; i < 30; ++i)
    {
        sut.step(0);
    }
    for (size_t i = 0; i < 69; ++i)
    {
        EXPECT_NEAR(sut.step(0), 0.f, 1E-5f) << "failed at " << i;
    }
    sut.setSize(150);
    sut.step(1);
    for (size_t i = 0; i < 100; ++i)
    {
        sut.step(0);
    }
    EXPECT_GT(sut.step(0), 0.1f); // fade peak after 100 steps from old head
    EXPECT_GT(sut.step(0), 0.2f);
    for (size_t i = 0; i < 48; ++i)
    {
        sut.step(0);
    }
    EXPECT_GT(sut.step(0), 0.5); // fade peak after 150 steps from new head
    EXPECT_GT(sut.step(0), 0.6f);
}
}
