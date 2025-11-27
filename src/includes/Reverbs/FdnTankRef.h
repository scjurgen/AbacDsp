#pragma once

#include "HadamardFeed.h"
#include "Delays/NaiveDelay.h"
#include "Helpers/ConstructArray.h"
#include "Numbers/PrimeDispatcher.h"

#include <random>

namespace AbacDsp
{
template <size_t MaxSizePerElement, size_t MAXORDER, size_t BlockSize>
class FdnTankRef
{
  public:
    using SimpleDelay = NaiveDelay<MaxSizePerElement>;
    explicit FdnTankRef(const float sampleRate)
        : m_sampleRate(sampleRate)
        , m_feedBackGain(1.0f / std::sqrt(static_cast<float>(MAXORDER)))
        , m_basicDelay(constructArray<SimpleDelay, MAXORDER>())
    {
        setMinSize(10);
        setMaxSize(20);
    }

    void setOrder(const size_t order)
    {
        m_order = order;
        m_feedBackGain = 1.0f / std::sqrt(static_cast<float>(m_order));
        computeDelaySizes();
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
        m_basicDelay[index].setSize(w - 1);
        const auto tmp = powf(0.001f, m_currentWidth[index] / m_sampleRate / (m_msecs / 1000.0f));
        m_gain[index] = tmp * m_feedBackGain;
        return w;
    }

    void setDirectSize(const size_t index, const size_t value)
    {
        if (index >= m_order)
        {
            return;
        }

        m_currentWidth[index] = value;
        m_basicDelay[index].setSize(value - 1);
        const auto tmp = powf(0.001f, m_currentWidth[index] / m_sampleRate / (m_msecs / 1000.0f));
        m_gain[index] = tmp * m_feedBackGain;
    }

    void computeDelaySizes()
    {
        std::mt19937 gen(m_order);
        std::normal_distribution distribution(0.0f, m_spreadLines / 30.0f);
        std::array<float, MAXORDER> m_meters;
        std::array<size_t, MAXORDER> m_discreteSize;
        for (size_t i = 0; i < m_order; ++i)
        {
            const auto frac = static_cast<float>(i) / static_cast<float>(m_order - 1);
            auto x = getBulgeValue(frac, -m_bulge);
            if (m_spreadLines)
            {
                x += distribution(gen);
            }
            const auto meters = m_minSize + x * (m_maxSize - m_minSize);
            m_meters[i] = std::clamp(meters, std::min(m_minSize, m_maxSize), std::max(m_minSize, m_maxSize));
            m_discreteSize[i] = computeSizeFromMeters(m_meters[i]);
        }
        if (m_avoidEqualLengthDelay)
        {
            ensureUniqueDiscreteSize(m_discreteSize.data(), m_order);
        }
        for (size_t i = 0; i < m_order; ++i)
        {
            setDirectSize(i, m_discreteSize[i]);
        }
    }

    void setSpreadStereo(const float value)
    {
        m_mono = 1.f - value;
    }

    static float getBulgeValue(const float x, const float bulge, const float bulgePower = 4.0f)
    {
        return bulge < 0 ? 1 - std::pow(1 - x, std::pow(bulgePower, -bulge)) : std::pow(x, std::pow(bulgePower, bulge));
    }

    static float getSymBulge(const float x, const float bulge, const float bulgePower = 4.0f)
    {
        if (x < 0.5)
        {
            return std::pow(2 * x, std::pow(bulgePower, bulge)) / 2;
        }
        return 1 - std::pow(2 - 2 * x, std::pow(bulgePower, bulge)) / 2;
    }

    void setSpreadBulge(const float value)
    {
        m_bulge = value;
        computeDelaySizes();
    }

    void setSpreadRandomFactor(const float factor)
    {
        m_spreadLines = factor;
        computeDelaySizes();
    }

    void setMinSize(const float s)
    {
        m_minSize = s;
        computeDelaySizes();
    }

    void setMaxSize(const float s)
    {
        m_maxSize = s;
        computeDelaySizes();
    }

    void setDecay(const float msecs)
    {
        m_msecs = msecs;
        if (m_msecs >= 99999.f)
        {
            for (size_t i = 0; i < m_gain.size(); ++i)
            {
                m_gain[i] = m_feedBackGain;
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
    void matrixFeed(float in)
    {
        hadamardFeed(m_order, m_lastValue.data(), m_feedValue.data());
        for (size_t s = 0; s < m_order; ++s)
        {
            m_lastValue[s] = m_basicDelay[s].step(in - m_feedValue[s] * m_gain[s]);
        }
    }

    void processBlock(const float* in, float* out)
    {
        for (uint32_t pos = 0; pos < BlockSize; pos++)
        {
            matrixFeed(in[pos]);
            float result = 0.0f;
            for (size_t s = 0; s < m_order; ++s)
            {
                result += m_lastValue[s];
            }
            out[pos] = result;
        }
    }

    void processBlockSplit(const float* in, float* left, float* right)
    {
        std::fill_n(left, BlockSize, 0.f);
        std::fill_n(right, BlockSize, 0.f);
        processBlockSplitAdd(in, left, right, BlockSize);
    }

    void processBlockSplitAdd(const float* in, float* left, float* right)
    {
        const float factorOrder = 1.f / static_cast<float>(m_order);
        for (uint32_t pos = 0; pos < BlockSize; pos++)
        {
            matrixFeed(in[pos]);
            float l = 0;
            float r = 0;
            for (size_t i = 0; i < m_order; i += 2)
            {
                l += m_lastValue[i];
                r += m_lastValue[i + 1];
            }
            l *= factorOrder;
            r *= factorOrder;
            left[pos] += l + r * m_mono; // if mono is one we have l+r on both channels
            right[pos] += r + l * m_mono;
        }
    }

    void processBlockSum(const float* in, float* out)
    {
        for (uint32_t pos = 0; pos < BlockSize; pos++)
        {
            matrixFeed(in[pos]);
            for (size_t s = 0; s < m_order; ++s)
            {
                out[pos] += m_lastValue[s];
            }
        }
    }

    void processBlock(float* inPlace)
    {
        processBlock(inPlace, inPlace);
    }

  private:
    float m_sampleRate;
    float m_feedBackGain;
    float m_minSize{100};
    float m_maxSize{210};
    float m_bulge{-0.6f};
    float m_mono{0.0};
    std::array<size_t, MAXORDER> m_currentWidth{};
    std::array<float, MAXORDER> m_lastValue{};
    std::array<float, MAXORDER> m_feedValue{};
    float m_msecs{100.0f};
    std::array<float, MAXORDER> m_gain{};
    std::array<SimpleDelay, MAXORDER> m_basicDelay;
    size_t m_order{MAXORDER};
    bool m_spreadSymmetric{false};
    bool m_avoidEqualLengthDelay{false};
    float m_spreadLines{0.f};
};
}
