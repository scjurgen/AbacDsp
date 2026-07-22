#include <algorithm>
#include <cmath>
#include <vector>

#include "gtest/gtest.h"

#include "Modulation/Modulation.h"

namespace AbacDsp::Test
{

namespace
{
// AllpassDelay/ModulatingAllPassDelay tick Modulation once every 16 audio samples, so its own
// "sample rate" is the audio sample rate divided by 16 - matches every real caller.
constexpr float kTickRate{48000.f / 16.f};

[[nodiscard]] std::vector<float> collectValues(Modulation& mod, const size_t numTicks)
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

// Sanity check for the hypothesis test below: the position itself moves by a small, bounded
// amount every tick and never jumps - confirming the turnaround bookkeeping in tick() is not
// itself glitchy (that would be a different, separate bug from the one under test here).
TEST(ModulationTest, PositionNeverJumpsMoreThanOneStepPerTick)
{
    constexpr float depth{64.f};
    constexpr float speedHz{2.f};
    Modulation mod(kTickRate);
    mod.setModulationSpeed(speedHz);
    mod.setModulationDepth(depth);

    static_cast<void>(collectValues(mod, 4)); // let setNewAdvance() take effect
    const auto values = collectValues(mod, 6000);

    const float expectedStep = depth * 2.f * speedHz / kTickRate; // per-tick step, see setNewAdvance()
    for (size_t i = 1; i < values.size(); ++i)
    {
        const float delta = values[i] - values[i - 1];
        EXPECT_LE(std::abs(delta), expectedStep + 1e-3f) << "at tick " << i;
    }
}

// The hypothesis under test: a triangle-shaped position sweep has a *constant* rate of change
// within each half-cycle that flips sign at the turnaround - a square wave, not a continuously
// varying one. Since the perceived pitch shift from delay modulation is proportional to that
// rate of change (not the position itself), this is exactly the "toggling between two extreme
// pitches" artifact described against ModulatingAllPassDelay: a smoothly-moving read position
// still produces an abruptly-flipping pitch shift, because the position's own derivative is
// abrupt. A sine-shaped sweep would not have this property (its derivative, a cosine, is itself
// smooth) - this test would need to be rewritten, not just re-tuned, if the LFO shape changes.
TEST(ModulationTest, RateOfChangeIsTwoConstantValuesNotASmoothCurve)
{
    constexpr float depth{64.f};
    constexpr float speedHz{2.f};
    Modulation mod(kTickRate);
    mod.setModulationSpeed(speedHz);
    mod.setModulationDepth(depth);
    static_cast<void>(collectValues(mod, 4)); // let setNewAdvance() take effect

    const auto values = collectValues(mod, 6000); // several full periods

    std::vector<float> deltas;
    for (size_t i = 1; i < values.size(); ++i)
    {
        const float delta = values[i] - values[i - 1];
        if (std::abs(delta) > 1e-6f)
        {
            deltas.push_back(delta);
        }
    }
    ASSERT_FALSE(deltas.empty());

    const auto [minIt, maxIt] = std::minmax_element(deltas.begin(), deltas.end());
    const float minDelta = *minIt;
    const float maxDelta = *maxIt;

    // A smoothly-varying (e.g. sine-shaped) rate of change would spread deltas continuously
    // between the two extremes; a triangle position sweep instead sits at (very close to) just
    // the two extreme values essentially all the time.
    size_t nearExtreme = 0;
    for (const auto d : deltas)
    {
        if (std::abs(d - minDelta) < 1e-3f || std::abs(d - maxDelta) < 1e-3f)
        {
            ++nearExtreme;
        }
    }
    const float fractionNearExtreme = static_cast<float>(nearExtreme) / static_cast<float>(deltas.size());
    EXPECT_GT(fractionNearExtreme, 0.99f)
        << "expected almost every delta to sit at one of exactly two values (a square-wave rate "
           "of change); found a spread instead, consistent with a smoothly-varying LFO";

    // The two extremes should be equal in magnitude and opposite in sign - a symmetric square
    // wave, matching a symmetric triangle position sweep.
    EXPECT_NEAR(minDelta, -maxDelta, 1e-3f);
}

}
