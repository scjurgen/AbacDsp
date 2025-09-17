
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
    for (size_t i = 0; i < 200; ++i)
    {
        sut.step(0);
    }
    sut.step(1);
    // this is still off
    for (size_t i = 0; i < 95; ++i)
    {
        EXPECT_EQ(sut.step(0), 0.f) << "failed at " << i;
    }
    EXPECT_LT(sut.step(0), 0.f);
    EXPECT_GT(sut.step(0), 0.5f);
    EXPECT_GT(sut.step(0), 0.5f);
    EXPECT_LT(sut.step(0), 0.f);
}
}