#include <cmath>
#include <numeric>
#include <tuple>
#include <vector>

#include "gtest/gtest.h"

#include "Generators/BrownModulation.h"

namespace AbacDsp::Test
{

namespace
{
constexpr float kSampleRate = 48000.0f;
}

TEST(BrownModulation, outputStaysFiniteAndBounded)
{
    BrownModulation brown{kSampleRate};
    brown.setCenterFrequency(2.0f);
    for (int i = 0; i < 100000; ++i)
    {
        const auto v = brown.step();
        ASSERT_TRUE(std::isfinite(v));
        ASSERT_GE(v, -5.0f);
        ASSERT_LE(v, 5.0f);
    }
}

TEST(BrownModulation, outputHasNearZeroMeanOverTime)
{
    BrownModulation brown{kSampleRate};
    brown.seed(1234u);
    brown.setCenterFrequency(5.0f);

    for (int i = 0; i < 10000; ++i)
    {
        std::ignore = brown.step();
    }

    std::vector<float> samples;
    samples.reserve(50000);
    for (int i = 0; i < 50000; ++i)
    {
        samples.push_back(brown.step());
    }
    const float mean = std::accumulate(samples.begin(), samples.end(), 0.0f) / static_cast<float>(samples.size());
    EXPECT_NEAR(mean, 0.0f, 0.05f);
}

TEST(BrownModulation, sameSeedProducesSameSequence)
{
    BrownModulation a{kSampleRate};
    BrownModulation b{kSampleRate};
    a.seed(42u);
    b.seed(42u);
    a.setCenterFrequency(3.0f);
    b.setCenterFrequency(3.0f);

    for (int i = 0; i < 1000; ++i)
    {
        EXPECT_FLOAT_EQ(a.step(), b.step());
    }
}

TEST(BrownModulation, lowerCenterFrequencyProducesSmootherOutput)
{
    BrownModulation slow{kSampleRate};
    BrownModulation fast{kSampleRate};
    slow.seed(7u);
    fast.seed(7u);
    slow.setCenterFrequency(0.5f);
    fast.setCenterFrequency(50.0f);

    auto meanAbsDelta = [](BrownModulation& brown)
    {
        float previous = brown.step();
        float sum = 0.0f;
        constexpr int n = 20000;
        for (int i = 0; i < n; ++i)
        {
            const auto v = brown.step();
            sum += std::abs(v - previous);
            previous = v;
        }
        return sum / static_cast<float>(n);
    };

    EXPECT_LT(meanAbsDelta(slow), meanAbsDelta(fast));
}

}
