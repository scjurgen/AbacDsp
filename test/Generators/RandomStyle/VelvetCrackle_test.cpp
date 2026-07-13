#include <cmath>
#include <tuple>

#include "gtest/gtest.h"

#include "Generators/RandomStyle/VelvetCrackle.h"

namespace AbacDsp::Test
{

TEST(VelvetCrackleGenerator, processProducesFiniteOutput)
{
    VelvetCrackleGenerator gen;
    bool sawNonZero = false;
    for (int i = 0; i < 2000; ++i)
    {
        const float v = gen.process(0.5f, 0.5f);
        ASSERT_TRUE(std::isfinite(v));
        sawNonZero = sawNonZero || v != 0.f;
    }
    EXPECT_TRUE(sawNonZero);
}

TEST(VelvetCrackleGenerator, resetReturnsToInitialState)
{
    VelvetCrackleGenerator gen;
    for (int i = 0; i < 100; ++i)
    {
        std::ignore = gen.process(0.8f, 0.8f);
    }
    gen.reset();
    const float v = gen.process(0.f, 0.f);
    EXPECT_TRUE(std::isfinite(v));
}

}
