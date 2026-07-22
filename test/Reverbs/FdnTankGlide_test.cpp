#include <algorithm>
#include <array>
#include <cmath>

#include "gtest/gtest.h"

#include "Reverbs/FdnTankGlide.h"

namespace AbacDsp::Test
{

TEST(FdnTankGlide, processBlockProducesFiniteDecayingOutput)
{
    constexpr size_t maxSizePerElement{4096};
    constexpr size_t order{4};
    constexpr size_t blockSize{16};
    FdnTankGlide<maxSizePerElement, order, blockSize> tank(48000.f);
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

TEST(FdnTankGlide, processBlockSplitKeepsBothChannelsFinite)
{
    constexpr size_t maxSizePerElement{4096};
    constexpr size_t order{4};
    constexpr size_t blockSize{16};
    FdnTankGlide<maxSizePerElement, order, blockSize> tank(48000.f);
    tank.setDecay(200.f);

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

// The whole point of FdnTankGlide over the ParallelPlainDelay-based tanks is that a size change
// pitch-glides instead of snapping the read pointer, so it must never produce a sample-to-sample
// jump much larger than what the steady-state signal itself already produces.
TEST(FdnTankGlide, resizingDuringSignalStaysArtifactFree)
{
    constexpr size_t maxSizePerElement{16384};
    constexpr size_t order{4};
    constexpr size_t blockSize{16};
    FdnTankGlide<maxSizePerElement, order, blockSize> tank(48000.f);
    tank.setDecay(500.f);
    tank.setMinSize(0.3f);
    tank.setMaxSize(0.6f);

    const auto sinAt = [](const size_t n) noexcept { return 0.3f * std::sin(static_cast<float>(n) * 0.1f); };

    float prev = 0.f;
    float maxSteadyDelta = 0.f;
    size_t n = 0;
    std::array<float, blockSize> out{};

    // Let the tank fill up and settle into a steady-state response first.
    for (int block = 0; block < 400; ++block)
    {
        std::array<float, blockSize> in{};
        for (auto& s : in)
        {
            s = sinAt(n++);
        }
        tank.processBlock(in.data(), out.data());
        for (const float v : out)
        {
            maxSteadyDelta = std::max(maxSteadyDelta, std::abs(v - prev));
            prev = v;
        }
    }

    // Trigger a large size change mid-stream, well within the pitch-glide's convergence window.
    tank.setMaxSize(6.0f);

    float maxDuringResize = 0.f;
    for (int block = 0; block < 800; ++block)
    {
        std::array<float, blockSize> in{};
        for (auto& s : in)
        {
            s = sinAt(n++);
        }
        tank.processBlock(in.data(), out.data());
        for (const float v : out)
        {
            ASSERT_TRUE(std::isfinite(v));
            maxDuringResize = std::max(maxDuringResize, std::abs(v - prev));
            prev = v;
        }
    }

    // A hard pointer jump reads uncorrelated buffer content and spikes far above the steady-state
    // slope; a pitch glide only ever gradually shifts frequency content, so it stays close to it.
    EXPECT_LT(maxDuringResize, maxSteadyDelta * 4.f + 1E-4f);
}

// setModulation() was previously never called by any caller in the codebase, so this path -
// including its interaction with a very short tank line - had no test coverage at all. 1m /
// 2.3 spread (MaxDiffuserImpl::FdnSizeSpread) is the shortest line reachable via the "FDN Size"
// UI dial at its minimum (~0.43m, well inside the collision-prone range fixed alongside the
// diffuser's ModulatingAllPassDelay).
TEST(FdnTankGlide, setModulationAtShortestReachableSizeStaysFiniteAndBounded)
{
    constexpr size_t maxSizePerElement{100000};
    constexpr size_t order{32};
    constexpr size_t blockSize{16};
    FdnTankGlide<maxSizePerElement, order, blockSize> tank(48000.f);
    tank.setDecay(1000.f);
    tank.setMinSize(1.0f / 2.3f);
    tank.setMaxSize(1.0f * 2.3f);
    tank.setModulation(1.0f, 2.0f);

    std::array<float, blockSize> in{};
    std::array<float, blockSize> left{};
    std::array<float, blockSize> right{};

    float maxAbs = 0.f;
    for (int block = 0; block < 3000; ++block)
    {
        in[0] = block == 0 ? 1.f : 0.f;
        tank.processBlockSplit(in.data(), left.data(), right.data());
        for (size_t i = 0; i < blockSize; ++i)
        {
            ASSERT_TRUE(std::isfinite(left[i]));
            ASSERT_TRUE(std::isfinite(right[i]));
            maxAbs = std::max({maxAbs, std::abs(left[i]), std::abs(right[i])});
        }
    }
    EXPECT_LT(maxAbs, 50.f);
}

}
