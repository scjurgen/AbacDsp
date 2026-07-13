#include <array>
#include <cmath>

#include "gtest/gtest.h"

#include "Reverbs/FdnTankSpicedBase.h"

namespace AbacDsp::Test
{

TEST(FdnTankSpicedBase, processBlockProducesFiniteDecayingOutput)
{
    constexpr size_t maxSizePerElement{4096};
    constexpr size_t order{4};
    constexpr size_t blockSize{16};
    FdnTankSpicedBase<maxSizePerElement, order, blockSize> tank(48000.f);
    tank.setDecay(200.f);
    tank.setMinSize(0.1f);
    tank.setMaxSize(1.0f);
    tank.setSpreadBulge(-0.4f);

    std::array<float, blockSize> in{};
    in[0] = 1.f;
    std::array<float, blockSize> out{};

    bool sawNonZero = false;
    for (int block = 0; block < 20; ++block)
    {
        tank.processBlock(in.data(), out.data());
        for (const float v : out)
        {
            ASSERT_TRUE(std::isfinite(v));
            sawNonZero = sawNonZero || v != 0.f;
        }
        in.fill(0.f);
    }
    EXPECT_TRUE(sawNonZero);
}

TEST(FdnTankSpicedBase, processBlockSplitKeepsBothChannelsFinite)
{
    constexpr size_t maxSizePerElement{4096};
    constexpr size_t order{4};
    constexpr size_t blockSize{16};
    FdnTankSpicedBase<maxSizePerElement, order, blockSize> tank(48000.f);
    tank.setDecay(200.f);
    tank.initUniformItd();

    std::array<float, blockSize> in{};
    in[0] = 1.f;
    std::array<float, blockSize> left{};
    std::array<float, blockSize> right{};

    for (int block = 0; block < 10; ++block)
    {
        tank.processBlockSplit(in.data(), left.data(), right.data());
        for (size_t i = 0; i < blockSize; ++i)
        {
            EXPECT_TRUE(std::isfinite(left[i]));
            EXPECT_TRUE(std::isfinite(right[i]));
        }
        in.fill(0.f);
    }
}

}
