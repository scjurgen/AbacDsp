
#include "Delays/OuModDelay.h"

#include "gtest/gtest.h"
#include "gmock/gmock.h"

namespace AbacDsp::Test
{

TEST(OuModDelayTest, simpleFeedAndEat)
{
    OuModDelay<1000> sut(48000.f);
    sut.setSize(100);
    sut.setModDepth(0.f);
    for (size_t i = 0; i < 3000; ++i)
    {
        sut.step(0);
    }
    sut.step(1);
    // this is still off
    for (size_t i = 0; i < 96; ++i)
    {
        EXPECT_EQ(sut.step(0), 0.f) << "failed at " << i;
    }
    EXPECT_LT(sut.step(0), 0.f);
    const auto v1 = sut.step(0);
    EXPECT_GT(v1, 0.1f);
    const auto v2 = sut.step(0);
    EXPECT_GT(v2, 0.1f);
    EXPECT_GE(v1 + v2, 1.0f);
    EXPECT_LT(sut.step(0), 0.f);
}

TEST(OuModDelayTest, wrapProblem)
{
    OuModDelay<300> sut(48000.f);
    sut.setModDrift(0.f);
    sut.setModVariance(0.f);
    sut.setModSpeed(30.f);
    sut.setSize(100);
    sut.setModDepth(1.f);
    for (size_t i = 0; i < 500100; ++i)
    {
        sut.step(0);
    }
    sut.step(1);
    for (size_t i = 0; i < 4000; ++i)
    {
        sut.step(0);
    }
}
}