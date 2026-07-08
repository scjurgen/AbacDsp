#pragma once

#include <algorithm>
#include <cstddef>

namespace AbacDsp
{

/*
 * Minimax polynomial for sin(x * pi/2) on [0, 1], accurate to about -100 dB:
 * good enough for equal-power fade factors without calling std::sin.
 */
[[nodiscard]] inline float quarterSineForFade(const float x) noexcept
{
    constexpr auto f0 = 0.00132429520515f;
    constexpr auto f1 = 1.56617254438f;
    constexpr auto f3 = -0.635995286743f;
    constexpr auto f5 = 0.0684984463761f;
    const auto sqrX = x * x;
    return ((f5 * sqrX + f3) * sqrX + f1) * x + f0;
}

enum class FadeMode
{
    In,
    Out
};

enum class FadeCurve
{
    Linear,
    Sine
};

template <FadeMode fadeMode, FadeCurve fadeCurve>
class Fader
{
  public:
    void reset(const size_t steps) noexcept
    {
        m_steps = steps;
        m_currentFactor = fadeMode == FadeMode::Out ? 1.f : 0.f;
        m_advance = 1.f / static_cast<float>(steps);
        m_isDone = false;
    }

    [[nodiscard]] float step(const float in) noexcept
    {
        if (m_isDone)
        {
            if constexpr (fadeMode == FadeMode::Out)
            {
                return 0.f;
            }
            else
            {
                return in;
            }
        }
        const auto returnValue = in * currentGain();
        if constexpr (fadeMode == FadeMode::Out)
        {
            m_currentFactor -= m_advance;
            if (m_currentFactor <= 0.f)
            {
                m_isDone = true;
            }
        }
        else
        {
            m_currentFactor += m_advance;
            if (m_currentFactor >= 1.f)
            {
                m_isDone = true;
            }
        }
        return returnValue;
    }

    void processBlock(float* inplace, const size_t numSamples) noexcept
    {
        processBlock(inplace, inplace, numSamples);
    }

    void processBlock(const float* in, float* target, const size_t numSamples) noexcept
    {
        if (m_isDone)
        {
            if constexpr (fadeMode == FadeMode::Out)
            {
                std::fill(target, target + numSamples, 0.f);
            }
            else
            {
                if (in != target)
                {
                    std::copy(in, in + numSamples, target);
                }
            }
            return;
        }
        for (size_t i = 0; i < numSamples; ++i)
        {
            if constexpr (fadeMode == FadeMode::Out)
            {
                m_currentFactor -= m_advance;
            }
            else
            {
                m_currentFactor += m_advance;
            }
            target[i] = in[i] * currentGain();
            if constexpr (fadeMode == FadeMode::Out)
            {
                if (m_currentFactor <= 0.f)
                {
                    std::fill(target + i, target + numSamples, 0.f);
                    m_isDone = true;
                    return;
                }
            }
            else
            {
                if (m_currentFactor >= 1.f)
                {
                    m_isDone = true;
                    std::copy(in + i, in + numSamples, target + i);
                    return;
                }
            }
        }
    }

    [[nodiscard]] size_t width() const noexcept
    {
        return m_steps;
    }

    [[nodiscard]] bool isDone() const noexcept
    {
        return m_isDone;
    }

  private:
    [[nodiscard]] float currentGain() const noexcept
    {
        if constexpr (fadeCurve == FadeCurve::Linear)
        {
            return m_currentFactor;
        }
        else
        {
            return quarterSineForFade(m_currentFactor);
        }
    }

    bool m_isDone{true};
    float m_currentFactor{0.f};
    float m_advance{0.f};
    size_t m_steps{0};
};

}
