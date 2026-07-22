#include <algorithm>
#include <cmath>
#include <numbers>
#include <vector>

#include "gtest/gtest.h"

#include "Modulation/SineModulation.h"

namespace AbacDsp::Test
{

namespace
{
// AllpassDelay/ModulatingAllPassDelay tick their position modulator once every 16 audio samples,
// so its own "sample rate" is the audio sample rate divided by 16 - matches every real caller.
constexpr float kTickRate{48000.f / 16.f};

[[nodiscard]] std::vector<float> collectValues(SineModulation& mod, const size_t numTicks)
{
    std::vector<float> values(numTicks);
    for (size_t i = 0; i < numTicks; ++i)
    {
        mod.tick();
        values[i] = mod.lastValue();
    }
    return values;
}
}

TEST(SineModulationTest, DepthZeroMeansNotModulating)
{
    SineModulation mod(kTickRate);
    mod.setModulationSpeed(2.f);
    mod.setModulationDepth(0.f);
    static_cast<void>(collectValues(mod, 4));
    EXPECT_FALSE(mod.isModulating());
}

TEST(SineModulationTest, PositiveDepthMeansModulating)
{
    SineModulation mod(kTickRate);
    mod.setModulationSpeed(2.f);
    mod.setModulationDepth(64.f);
    static_cast<void>(collectValues(mod, 4));
    EXPECT_TRUE(mod.isModulating());
}

TEST(SineModulationTest, PositionStaysWithinZeroToDepth)
{
    constexpr float depth{64.f};
    SineModulation mod(kTickRate);
    mod.setModulationSpeed(2.f);
    mod.setModulationDepth(depth);
    static_cast<void>(collectValues(mod, 4)); // let the pending depth/speed take effect

    const auto values = collectValues(mod, 6000); // several full periods
    for (const auto v : values)
    {
        EXPECT_GE(v, -1e-3f);
        EXPECT_LE(v, depth + 1e-3f);
    }
    const auto [minIt, maxIt] = std::minmax_element(values.begin(), values.end());
    EXPECT_NEAR(*minIt, 0.f, 1e-2f);
    EXPECT_NEAR(*maxIt, depth, 1e-2f);
}

// Sanity check mirroring ModulationTest.PositionNeverJumpsMoreThanOneStepPerTick: the position
// never jumps by more than the sweep's own theoretical peak velocity (depth/2 * phaseAdvance, at
// phase == pi/2), confirming the deferred-update bookkeeping in tick() is not itself glitchy.
TEST(SineModulationTest, PositionNeverJumpsMoreThanPeakVelocityPerTick)
{
    constexpr float depth{64.f};
    constexpr float speedHz{2.f};
    SineModulation mod(kTickRate);
    mod.setModulationSpeed(speedHz);
    mod.setModulationDepth(depth);
    static_cast<void>(collectValues(mod, 4));

    const auto values = collectValues(mod, 6000);

    constexpr float twoPi = 2.0f * std::numbers::pi_v<float>;
    const float phaseAdvance = twoPi * speedHz / kTickRate;
    const float peakVelocity = depth * 0.5f * phaseAdvance; // derivative peak, at phase == pi/2

    for (size_t i = 1; i < values.size(); ++i)
    {
        const float delta = values[i] - values[i - 1];
        EXPECT_LE(std::abs(delta), peakVelocity + 1e-3f) << "at tick " << i;
    }
}

// The whole point of this class over Modulation: the rate of change must vary smoothly across
// many distinct values (a sine), not collapse to just two constant extremes (a square wave) -
// see ModulationTest.RateOfChangeIsTwoConstantValuesNotASmoothCurve for the triangle case this
// is meant to fix.
TEST(SineModulationTest, RateOfChangeVariesSmoothlyAcrossManyValues)
{
    constexpr float depth{64.f};
    constexpr float speedHz{2.f};
    SineModulation mod(kTickRate);
    mod.setModulationSpeed(speedHz);
    mod.setModulationDepth(depth);
    static_cast<void>(collectValues(mod, 4));

    const auto values = collectValues(mod, 6000); // several full periods

    std::vector<float> roundedDeltas;
    for (size_t i = 1; i < values.size(); ++i)
    {
        const float delta = values[i] - values[i - 1];
        roundedDeltas.push_back(std::round(delta * 1000.0f) / 1000.0f);
    }
    std::sort(roundedDeltas.begin(), roundedDeltas.end());
    roundedDeltas.erase(std::unique(roundedDeltas.begin(), roundedDeltas.end()), roundedDeltas.end());

    // A square-wave rate of change (the bug being fixed) would collapse to 2 distinct values;
    // a sine's derivative sweeps through many. 20 is a generous floor - the prototype measured
    // 269 at this depth/speed.
    EXPECT_GT(roundedDeltas.size(), 20u);
}

// Changing depth or speed mid-sweep must not cause an audible jump: the pending change should
// only take effect once the position returns to its trough (phase == 0), matching Modulation's
// turnaround-deferred update semantics.
TEST(SineModulationTest, ChangingDepthMidSweepDoesNotJumpPosition)
{
    SineModulation mod(kTickRate);
    mod.setModulationSpeed(2.f);
    mod.setModulationDepth(32.f);
    static_cast<void>(collectValues(mod, 4));

    // Advance partway into the sweep, then request a much larger depth.
    static_cast<void>(collectValues(mod, 300));
    const float beforeChange = mod.lastValue();
    mod.setModulationDepth(128.f);
    mod.tick();
    const float afterChange = mod.lastValue();

    EXPECT_NEAR(beforeChange, afterChange, 0.2f) << "depth change should not jump the position mid-sweep";
}

}
