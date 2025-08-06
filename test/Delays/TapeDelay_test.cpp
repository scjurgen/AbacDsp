#include "gtest/gtest.h"

#include <cmath>
#include "Delays/TapeDelay.h"

TEST(TapeDelayTests, simpleVersion)
{
    AbacDsp::TapeDelay<2, 100> sut{48000.f};
    std::array feed{1.f, -1.f};
    std::array eat{0.f, 0.f};
    sut.setFeedFactor(1.f);
    sut.setHoldFactor(1.f);
    sut.setFeedbackFactor(1.f);
    sut.step(feed.data(), eat.data());
    std::array feedNothing{0.f, 0.f};
    for (size_t i = 0; i < 250; ++i)
    {
        sut.step(feedNothing.data(), eat.data());
        std::cout << i << "\t" << eat[0] << "\t " << eat[1] << std::endl;
    }
}
