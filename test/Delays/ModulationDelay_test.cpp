
#include "Delays/ModulationDelay.h"

#include "gtest/gtest.h"
#include "gmock/gmock.h"

namespace AbacDsp::Test
{
TEST(ModulatingDelayPitchedAdjustTest, simpleFeedAndEat)
{
    ModulatingDelayPitchedAdjust<1000> sut(48000.f);
    sut.setSize(100);
    // settle pitching to correct position
    for (size_t i = 0; i < 100; ++i)
    {
        sut.step(0);
    }

    sut.step(1);
    for (size_t i = 0; i < 96; ++i)
    {
        EXPECT_EQ(sut.step(0), 0.f) << "failed at step " << i;
    }
    EXPECT_NE(sut.step(0), 0.99f);
}

}