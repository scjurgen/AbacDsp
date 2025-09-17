#include <gtest/gtest.h>
#include <vector>
#include <numeric>
#include <cmath>

#include "Generators/JuliaWalk.h"
#include "Analysis/SimpleStats.h"

namespace AbacDsp::Test
{


TEST(JuliaWalk, iterates)
{
    JuliaIter sut;
    constexpr float cx = -0.1565684555158167f;
    constexpr float cy = -1.032499968259872f;
    sut.setCx(cx);
    sut.setCy(cy);
    sut.setPower(2);
    EXPECT_FLOAT_EQ(sut.getIter(0.f, 0.f), 0);
    EXPECT_NEAR(sut.getIter(0.f, 0.1f), 19.3112f, 1E-4f);
    EXPECT_NEAR(sut.getIter(0.1f, 0.f), 18.88909f, 1E-4f);
    EXPECT_NEAR(sut.getIter(0.1f, 0.1f), 15.20783f, 1E-4f);
}

TEST(JuliaWalk, walk)
{
    JuliaWalk sut{48000.f};
    constexpr float cx = -1.6f;
    constexpr float cy = -0.5f;
    sut.setCx(cx);
    sut.setCy(cy);
    sut.setPower(2);
    sut.setFrequency(440.f);
    sut.setJx(0);
    sut.setJy(0);
    sut.setJxRad(2.f);
    sut.setJyRad(2.f);
    for (size_t i = 0; i < 1000; ++i)
    {
        std::cout << i << "\t" << sut.next() << "\n";
    }
}
}