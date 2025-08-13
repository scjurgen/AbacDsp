#pragma once

#include <concepts>
#include <cstddef>


template <typename T>
concept InterpolatorConcept = requires(T interp, float frac, const float* in, float* out) {
    { interp.halfCoeffWidth() } -> std::convertible_to<int>;
    { interp.increment() } -> std::convertible_to<float>;
    // We'll assume the interpolate function takes: frac [0..1), input ptr, output ptr, and
    // channels known at compile time
    { interp.template process<2>(frac, in, out) };
};

#pragma once

#include <vector>
#include <cmath>
#include <array>
#include <algorithm>

namespace AbacDsp
{


class BSplineFilter4pt
{
  public:
    struct InitParam
    {
        int increment;
        std::vector<float> coeffs;
    };

    explicit BSplineFilter4pt(const InitParam& param) noexcept
        : m_increment(param.increment)
    {
    }

    [[nodiscard]] int getBufferSize(float maxRatio, int channels) const noexcept
    {
        auto width = 3 * lrint(4 / static_cast<float>(m_increment) * maxRatio) + 1;
        return 1 + channels * (1 + std::max<int>(width, 4096));
    }

    [[nodiscard]] int halfCoeffWidth() const noexcept
    {
        return 2; // 4 taps total, symmetric
    }

    [[nodiscard]] int filterWidth() const noexcept
    {
        return 4;
    }

    [[nodiscard]] float getFractionFromIndex(int /*idx*/, float fraction) const noexcept
    {
        // For cubic B‑spline we don’t use idx directly — weights are function of fraction only
        return fractionToWeight(0, fraction);
    }

    [[nodiscard]] int increment() const noexcept
    {
        return m_increment;
    }

    [[nodiscard]] float getFractionFromInterleaved(int /*idx*/, float fraction) const noexcept
    {
        return fractionToWeight(0, fraction);
    }

    [[nodiscard]] size_t getInterleavedIndex(size_t originalIndex) const noexcept
    {
        return originalIndex; // unused meaningful mapping for cubic, but API‑compatible
    }

    template <size_t NumChannels>
    void processFixUp(size_t items, const float* buffer, float fraction, size_t /*idx*/, float* result) const noexcept
    {
        std::array<float, 4> w = splineWeights(fraction);
        for (size_t i = 0; i < items; ++i)
        {
            for (auto c = 0u; c < NumChannels; ++c)
            {
                result[c] += w[0] * buffer[(i - 1) * NumChannels + c] + w[1] * buffer[i * NumChannels + c] +
                             w[2] * buffer[(i + 1) * NumChannels + c] + w[3] * buffer[(i + 2) * NumChannels + c];
            }
        }
    }

    template <size_t NumChannels>
    void processFixDown(size_t items, const float* buffer, float fraction, size_t /*idx*/, float* result) const noexcept
    {
        std::array<float, 4> w = splineWeights(fraction);
        for (size_t i = 0; i < items; ++i)
        {
            for (auto c = NumChannels; c > 0; --c)
            {
                result[c - 1] +=
                    w[0] * buffer[(1 - i) * NumChannels + (c - 1)] + w[1] * buffer[(-i) * NumChannels + (c - 1)] +
                    w[2] * buffer[(-i - 1) * NumChannels + (c - 1)] + w[3] * buffer[(-i - 2) * NumChannels + (c - 1)];
            }
        }
    }

    template <size_t NumChannels, size_t DiscreteSteps, ssize_t DataStep, ssize_t term>
    void processFilterHalf(int32_t filterIdx, float* buffer, int32_t filterInc, float* result) const noexcept
    {
        float intPart;
        const auto addFraction = std::modf(filterInc / static_cast<float>(DiscreteSteps), &intPart);
        float fraction = (filterIdx & (DiscreteSteps - 1)) / static_cast<float>(DiscreteSteps);
        while (filterIdx > term)
        {
            std::array<float, 4> w = splineWeights(fraction);
            for (auto c = 0u; c < NumChannels; ++c)
            {
                // assumes buffer[-1], buffer[0], buffer[1], buffer[2]
                result[c] += w[0] * buffer[(-1) * NumChannels + c] + w[1] * buffer[(0) * NumChannels + c] +
                             w[2] * buffer[(+1) * NumChannels + c] + w[3] * buffer[(+2) * NumChannels + c];
            }
            filterIdx -= filterInc;
            fraction -= addFraction;
            if (fraction < 0)
                fraction += 1.f;
            buffer += DataStep * NumChannels;
        }
    }

  private:
    std::array<float, 4> splineWeights(float t) noexcept
    {
        float t2 = t * t;
        float t3 = t2 * t;

        float w0 = (-t * (t * (t - 2.0f) + 1.0f)) / 6.0f;
        float w1 = t2 * (t - 2.0f) / 2.0f + 2.0f / 3.0f;
        float w2 = t2 * (2.0f - t) / 2.0f + 1.0f / 6.0f;
        float w3 = t3 / 6.0f;

        return {w0, w1, w2, w3};
    }

    [[nodiscard]] static float fractionToWeight(int /*tap*/, float fraction) noexcept
    {
        auto w = splineWeights(fraction);
        return w[0];
    }

    const int m_increment{};
};

}
