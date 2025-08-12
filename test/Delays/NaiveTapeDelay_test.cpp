#include "gtest/gtest.h"

#include <cmath>
#include "Delays/NaiveTapeDelay.h"

TEST(NaiveTapeDelayTests, simpleVersion)
{
    AbacDsp::NaiveTapeDelay<2, 100> sut{48000.f};
    constexpr std::array feedFirst{1.f, -1.f};
    constexpr std::array feedSecond{-1.f, 1.f};
    std::array eat{0.f, 0.f};
    sut.setFeedFactor(1.f);
    sut.setHoldFactor(0.45f);
    sut.setFeedbackFactor(0.45f);
    sut.step(feedFirst.data(), eat.data());
    sut.step(feedSecond.data(), eat.data());
    std::array feedNothing{0.f, 0.f};
    for (size_t i = 0; i < 50; ++i)
    {
        sut.step(feedNothing.data(), eat.data());
        std::cout << i << "\t" << eat[0] << "\t " << eat[1] << std::endl;
    }
}
