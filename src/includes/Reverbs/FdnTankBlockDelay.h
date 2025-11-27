#pragma once

#include "HadamardFeed.h"
#include "Delays/ParallelPlainDelay.h"
#include "Helpers/ConstructArray.h"
#include "Numbers/PrimeDispatcher.h"

namespace AbacDsp
{

struct DelayWarp
{
    float getBulgeValue(const float x, const float bulgePower = 4.0f)
    {
        return bulge < 0 ? 1 - std::pow(1 - x, std::pow(bulgePower, bulge)) : std::pow(x, std::pow(bulgePower, -bulge));
    }

    float minSize{100};
    float maxSize{210};
    float bulge{-0.6f};
    float spreadLines{0.f};
};

template <size_t MaxSizePerElement, size_t ORDER, size_t BlockSize>
    requires(MaxSizePerElement % BlockSize == 0)
class FdnTankBlockDelay
{
  public:
    using Delay = ParallelPlainDelay<BlockSize, ORDER, MaxSizePerElement>;
    explicit FdnTankBlockDelay(const float sampleRate)
        : m_feedBackGain(1.0f / std::sqrt(static_cast<float>(ORDER)))
        , m_sampleRate(sampleRate)
    {
        setMinSize(10);
        setMaxSize(20);
    }

    void setUniqueDelay(const bool value)
    {
        m_avoidEqualLengthDelay = value;
        computeDelaySizes();
    }

    [[nodiscard]] size_t computeSizeFromMeters(const float meters) const
    {
        auto w = static_cast<size_t>(m_sampleRate * meters / 333.3f);
        w = std::clamp<size_t>(w, 11, MaxSizePerElement);
        w = getUsefulPrime<11>(w);
        return w;
    }

    size_t setSize(const size_t index, const float meters)
    {
        auto w = static_cast<size_t>(m_sampleRate * meters / 333.3f);
        w = std::clamp<size_t>(w, 11, MaxSizePerElement);
        w = getUsefulPrime<11>(w);
        m_currentWidth[index] = w;
        m_delay.setSize(index, w - 1);
        const auto tmp = powf(0.001f, m_currentWidth[index] / m_sampleRate / (m_msecs / 1000.0f));
        m_gain[index] = tmp * m_feedBackGain;
        return w;
    }

    void setDirectSize(const size_t index, const size_t value)
    {
        if (index >= ORDER)
        {
            return;
        }
        m_currentWidth[index] = value;
        m_delay.setSize(index, value - 2 * BlockSize);
        const auto tmp = powf(0.001f, m_currentWidth[index] / m_sampleRate / (m_msecs / 1000.0f));
        m_gain[index] = tmp * m_feedBackGain;
    }

    void computeDelaySizes()
    {
        std::array<float, ORDER> m_meters;
        std::array<size_t, ORDER> m_discreteSize;
        for (size_t i = 0; i < ORDER; ++i)
        {
            const auto frac = static_cast<float>(i) / static_cast<float>(ORDER - 1);
            auto x = m_warp.getBulgeValue(frac);
            const auto meters = m_warp.minSize + x * (m_warp.maxSize - m_warp.minSize);
            m_meters[i] =
                std::clamp(meters, std::min(m_warp.minSize, m_warp.maxSize), std::max(m_warp.minSize, m_warp.maxSize));
            m_discreteSize[i] = computeSizeFromMeters(m_meters[i]);
        }
        if (m_avoidEqualLengthDelay)
        {
            ensureUniqueDiscreteSize(m_discreteSize.data(), ORDER);
        }
        for (size_t i = 0; i < ORDER; ++i)
        {
            setDirectSize(i, m_discreteSize[i]);
        }
    }

    void setSpreadStereo(const float value)
    {
        m_mono = 1.f - value;
    }

    void setSpreadBulge(const float value)
    {
        m_warp.bulge = value;
        computeDelaySizes();
    }

    void setSpreadRandomFactor(const float factor)
    {
        m_warp.spreadLines = factor;
        computeDelaySizes();
    }

    void setMinSize(const float s)
    {
        m_warp.minSize = s;
        computeDelaySizes();
    }

    void setMaxSize(const float s)
    {
        m_warp.maxSize = s;
        computeDelaySizes();
    }

    void setDecay(const float msecs)
    {
        m_msecs = msecs;
        if (m_msecs >= 99999.f)
        {
            for (size_t o = 0; o < m_gain.size(); ++o)
            {
                m_gain[o] = m_feedBackGain;
            }
        }
        else
        {
            for (size_t i = 0; i < m_gain.size(); ++i)
            {
                const auto tmp =
                    std::pow(0.001f, static_cast<float>(m_currentWidth[i]) / m_sampleRate / (m_msecs / 1000.0f));
                m_gain[i] = tmp * m_feedBackGain;
            }
        }
    }

    void matrixFeed(size_t idx)
    {
        hadamardFeed(ORDER, m_lastValue[idx].data(), m_feedValue[idx].data());
        for (size_t o = 0; o < ORDER; ++o)
        {
            m_lastValue[idx][o] = m_outValue[o][idx];
        }
    }

    void processBlock(const float* in, float* out)
    {
        for (size_t o = 0; o < ORDER; ++o)
        {
            for (size_t s = 0; s < BlockSize; s++)
            {
                m_inValue[o][s] = in[s] - m_feedValue[s][o] * m_gain[o];
            }
        }
        m_delay.processBlock(m_inValue, m_outValue);

        for (uint32_t s = 0; s < BlockSize; s++)
        {
            matrixFeed(s);
            float result = 0.0f;
            for (size_t o = 0; o < ORDER; ++o)
            {
                result += m_lastValue[s][o];
            }
            out[s] = result;
        }
    }

    void processBlockSplit(const float* in, float* left, float* right, const uint32_t numSamples)
    {
        std::fill_n(left, numSamples, 0.f);
        std::fill_n(right, numSamples, 0.f);
        processBlockSplitAdd(in, left, right, numSamples);
    }

    void processBlockSplitAdd(const float* in, float* left, float* right, const uint32_t numSamples)
    {
        constexpr float factorOrder = 1.f / static_cast<float>(ORDER);
        for (uint32_t s = 0; s < numSamples; s++)
        {
            matrixFeed(in[s]);
            float l = 0;
            float r = 0;
            for (size_t o = 0; o < ORDER; o += 2)
            {
                l += m_lastValue[s][o];
                r += m_lastValue[s][o + 1];
            }
            l *= factorOrder;
            r *= factorOrder;
            left[s] += l + r * m_mono; // if mono is one we have l+r on both channels
            right[s] += r + l * m_mono;
        }
    }

    void processBlockSum(const float* in, float* out, const uint32_t numSamples)
    {
        for (uint32_t pos = 0; pos < numSamples; pos++)
        {
            matrixFeed(in[pos]);
            for (size_t s = 0; s < ORDER; ++s)
            {
                out[pos] += m_lastValue[s];
            }
        }
    }

    void processBlock(float* inPlace, const uint32_t numSamples)
    {
        processBlock(inPlace, inPlace, numSamples);
    }

  private:
    float m_feedBackGain;

    float m_sampleRate;
    DelayWarp m_warp;

    float m_mono{0.0};
    std::array<size_t, ORDER> m_currentWidth{};
    std::array<std::array<float, BlockSize>, ORDER> m_inValue{};
    std::array<std::array<float, BlockSize>, ORDER> m_outValue{};
    std::array<std::array<float, ORDER>, BlockSize> m_lastValue{};
    std::array<std::array<float, ORDER>, BlockSize> m_feedValue{};
    float m_msecs{100.0f};
    std::array<float, ORDER> m_gain{};
    Delay m_delay{};

    bool m_spreadSymmetric{false};
    bool m_avoidEqualLengthDelay{false};
};
}
