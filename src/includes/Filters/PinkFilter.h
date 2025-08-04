#pragma once

#include <array>

class PinkFilter
{
public:
    PinkFilter()
    {
    }

    float step(const float in)
    {
        static constexpr auto scale{0.05f};
        static constexpr std::array coeffsA{0.99765f, 0.96300f, 0.57000f};
        static constexpr std::array coeffsB{0.0990460f, 0.2965164f, 1.0526913f};
        for (size_t i = 0; i < 3; ++i)
        {
            m_v[i] = coeffsA[i] * m_v[i] + coeffsB[i] * in;
        }
        return (m_v[0] + m_v[1] + m_v[2] + in * 0.1848f) * scale;
    }

    void processBlock(float* in, const size_t numSamples) noexcept
    {
        processBlock(in, in, numSamples);
    }

    void processBlock(const float* in, float* out, const size_t numSamples) noexcept
    {
        std::transform(in, in + numSamples, out, [this](const float x)
        {
            return this->step(x);
        });
    }

private:
    std::array<float, 3> m_v{};
};
