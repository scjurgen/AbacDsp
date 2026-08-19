#include <cmath>
#include <tuple>
#include <vector>

#include "gtest/gtest.h"

#include "Analysis/ZeroCrossings.h"
#include "Filters/CombResonator.h"

namespace AbacDsp::Test
{
constexpr float sampleRate{48000.f};
constexpr size_t maxSize{2560};

TEST(CombResonatorTest, RingingPeriodMatchesRequestedFrequency)
{
    constexpr float freq = 220.f;
    CombResonator<maxSize> sut{sampleRate};
    sut.setDamping(0.f);
    sut.setByDecay(freq, 2.f);

    std::vector<float> out(4000);
    out[0] = sut.step(1.f);
    for (size_t i = 1; i < out.size(); ++i)
    {
        out[i] = sut.step(0.f);
    }

    const float period = periodLengthByZeroCrossingAverage(out.data(), out.size(), true);
    EXPECT_NEAR(period, sampleRate / freq, 1.5f);
}

TEST(CombResonatorTest, ShorterDecayRingsDownFaster)
{
    // A comb fed a single impulse rings as a decaying *impulse train*, not a smooth tone -
    // between spikes the output is near zero regardless of decay time - so the envelope at
    // sampleIndex is the peak over one period's worth of samples around it, not one raw
    // sample (which would just measure how close sampleIndex happens to land to a spike).
    const auto envelopeAt = [](const float decaySeconds, const size_t sampleIndex)
    {
        CombResonator<maxSize> sut{sampleRate};
        sut.setDamping(0.f);
        sut.setByDecay(440.f, decaySeconds);
        std::ignore = sut.step(1.f);
        constexpr size_t windowSamples = 120; // > one period at 440 Hz / 48 kHz (~109)
        float peak = 0.f;
        for (size_t i = 1; i <= sampleIndex; ++i)
        {
            const float result = sut.step(0.f);
            if (i > sampleIndex - windowSamples)
            {
                peak = std::max(peak, std::abs(result));
            }
        }
        return peak;
    };

    constexpr size_t probeIndex = 4000;
    const float shortDecay = envelopeAt(0.05f, probeIndex);
    const float longDecay = envelopeAt(5.f, probeIndex);
    EXPECT_LT(shortDecay, longDecay);
}

TEST(CombResonatorTest, StableUnderSustainedLoudExcitation)
{
    CombResonator<maxSize> sut{sampleRate};
    sut.setByDecay(220.f, 20.f); // max decay per SpectraltapScriptEngine::kMaxDecaySeconds

    for (size_t block = 0; block < 200; ++block)
    {
        for (size_t i = 0; i < 64; ++i)
        {
            const float out = sut.step(block % 8 == 0 ? 2.f : 0.f);
            ASSERT_TRUE(std::isfinite(out));
            ASSERT_LT(std::abs(out), 10.f);
        }
    }
}

TEST(CombResonatorTest, ResetClearsState)
{
    CombResonator<maxSize> sut{sampleRate};
    sut.setByDecay(220.f, 2.f);
    for (size_t i = 0; i < 200; ++i)
    {
        std::ignore = sut.step(i == 0 ? 1.f : 0.f);
    }
    sut.reset();
    EXPECT_FLOAT_EQ(sut.step(0.f), 0.f);
}

}
