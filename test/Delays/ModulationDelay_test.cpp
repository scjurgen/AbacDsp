
#include <cmath>
#include <limits>
#include <tuple>

#include "gtest/gtest.h"

#include "Delays/ModulationDelay.h"

namespace AbacDsp::Test
{
TEST(ModulatingDelayPitchedAdjustTest, simpleFeedAndEat)
{
    constexpr auto epsilon = std::numeric_limits<float>::epsilon();

    ModulatingDelayPitchedAdjust<1000> sut(48000.f);
    sut.setSize(100);
    // settle pitching to correct position
    for (size_t i = 0; i < 100; ++i)
    {
        std::ignore = sut.step(0);
    }

    std::ignore = sut.step(1);
    for (size_t i = 0; i < 95; ++i)
    {
        EXPECT_TRUE(std::abs(sut.step(0)) <= epsilon) << "failed at step " << i;
    }
    EXPECT_TRUE(std::abs(sut.step(0) - 0.99f) > epsilon);
}

}