#pragma once

#include "../BlockProcessors/BlockProcessorBase.h"
#include "HadamardWalsh4.h"
#include "HadamardWalsh8.h"
#include "HadamardWalsh16.h"
#include "HadamardWalsh32.h"

#include "Delays/ParallelPlainDelay.h"
#include "Numbers/PrimeDispatcher.h"

#include <cmath>
#include <random>
#include <array>
#include <algorithm>
#include <memory>

namespace AbacDsp
{

/*
 * ITD = Interaural Time Difference
 *
 * Enhanced FDN with callback system for delay line processing
 */
template <size_t MaxSizePerElement, size_t ORDER, size_t BlockSize>
    requires(ORDER == 4 || ORDER == 8 || ORDER == 16 || ORDER == 32)
class FdnTankSpiced
{
  public:
    struct DelayWarp
    {
        float getBulgeValue(const float x, const float bulgePower = 4.0f)
        {
            return bulge < 0 ? 1 - std::pow(1 - x, std::pow(bulgePower, bulge))
                             : std::pow(x, std::pow(bulgePower, -bulge));
        }

        float minSize{100};
        float maxSize{210};
        float bulge{-0.6f};
        float spreadLines{0.f};
    };

    using DelayWithTaps = ParallelPlainDelay<BlockSize, ORDER, MaxSizePerElement, 3>;
    using CallbackMgr = CallbackManager<ORDER, BlockSize>;

    explicit FdnTankSpiced(const float sampleRate)
        : m_feedBackGain(1.0f / std::sqrt(static_cast<float>(ORDER)))
        , m_sampleRate(sampleRate)
    {
        setMinSize(10);
        setMaxSize(20);
        initRandomItd();
    }

    // Callback management
    void setDelayCallback(size_t delayIndex, std::shared_ptr<BlockProcessorBase<BlockSize>> processor)
    {
        m_callbacks.setCallback(delayIndex, std::move(processor));
    }

    void removeDelayCallback(size_t delayIndex)
    {
        m_callbacks.removeCallback(delayIndex);
    }

    void resetCallbacks()
    {
        m_callbacks.reset();
    }

    // Existing methods (unchanged interface)
    void initUniformItd()
    {
        constexpr float maxItdMs = 0.33f;
        const int maxItdSamples = static_cast<int>(std::round(maxItdMs / 1000.f * m_sampleRate));
        for (int i = 0; i < ORDER; ++i)
        {
            const float panPos = 2.f * static_cast<float>(i) / static_cast<float>(ORDER - 1) - 1.f;
            const int itd = static_cast<int>(std::round(panPos * maxItdSamples));
            if (itd > 0)
            {
                m_itdTaps[0][i] = 0;
                m_itdTaps[1][i] = itd;
            }
            else
            {
                m_itdTaps[0][i] = -itd;
                m_itdTaps[1][i] = 0;
            }
        }
        setItdTaps(m_itdTaps);
    }

    void initRandomItd()
    {
        constexpr float maxItdMs = 0.33f;
        const int maxItdSamples = static_cast<int>(std::round(maxItdMs / 1000.f * m_sampleRate));
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<int> dist(-maxItdSamples, maxItdSamples);
        for (int i = 0; i < ORDER; ++i)
        {
            const int itd = dist(gen);
            if (itd > 0)
            {
                m_itdTaps[0][i] = 0;
                m_itdTaps[1][i] = itd;
            }
            else
            {
                m_itdTaps[0][i] = -itd;
                m_itdTaps[1][i] = 0;
            }
        }
        setItdTaps(m_itdTaps);
    }

    void setUniqueDelay(const bool value)
    {
        m_avoidEqualLengthDelay = value;
        computeDelaySizes();
    }

    [[nodiscard]] size_t computeSizeFromMeters(const float meters) const
    {
        auto w = static_cast<size_t>(m_sampleRate * meters / 333.3f);
        w = std::clamp(w, size_t{11}, MaxSizePerElement);
        w = getUsefulPrime<11>(w);
        return w;
    }

    void setItdTaps(std::array<std::array<size_t, ORDER>, 2>& taps)
    {
        m_itdTaps = taps;
        for (size_t i = 0; i < ORDER; ++i)
        {
            m_delay.setRelativeHead(1, i, m_itdTaps[0][i]);
            m_delay.setRelativeHead(2, i, m_itdTaps[1][i]);
        }
    }

    size_t setSize(const size_t index, const float meters)
    {
        auto w = static_cast<size_t>(m_sampleRate * meters / 333.3f);
        w = std::clamp(w, size_t{11}, MaxSizePerElement);
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

    void setMinSize(const float meters)
    {
        m_warp.minSize = meters;
        computeDelaySizes();
    }

    void setMaxSize(const float meters)
    {
        m_warp.maxSize = meters;
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
        if constexpr (ORDER == 4)
        {
            hadamardWalsh4_simd(m_lastValue[idx].data(), m_feedValue[idx].data());
        }
        if constexpr (ORDER == 8)
        {
            hadamardWalsh8_simd(m_lastValue[idx].data(), m_feedValue[idx].data());
        }
        if constexpr (ORDER == 16)
        {
            hadamardWalsh16_simd(m_lastValue[idx].data(), m_feedValue[idx].data());
        }
        if constexpr (ORDER == 32)
        {
            hadamardWalsh32_simd(m_lastValue[idx].data(), m_feedValue[idx].data());
        }

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

        m_callbacks.processCallbacks(m_outValue);

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

    void processBlockSplit(const float* in, float* left, float* right)
    {
        std::fill_n(left, BlockSize, 0.f);
        std::fill_n(right, BlockSize, 0.f);
        processBlockSplitAdd(in, left, right);
    }

    void processBlockSplitAdd(const float* in, float* left, float* right)
    {
        constexpr float factorOrder = 1.f / static_cast<float>(ORDER);
        for (size_t o = 0; o < ORDER; ++o)
        {
            for (size_t s = 0; s < BlockSize; s++)
            {
                m_inValue[o][s] = in[s] - m_feedValue[s][o] * m_gain[o];
            }
        }

        m_delay.processBlock(m_inValue, m_outValue);

        m_callbacks.processCallbacks(m_outValue);

        std::array<std::array<float, BlockSize>, ORDER> decorrelatedLeft;
        std::array<std::array<float, BlockSize>, ORDER> decorrelatedRight;
        m_delay.processHead(1, decorrelatedLeft);
        m_delay.processHead(2, decorrelatedRight);

        for (uint32_t s = 0; s < BlockSize; s++)
        {
            matrixFeed(s);
            for (size_t o = 0; o < ORDER; ++o)
            {
                left[s] += decorrelatedLeft[o][s];
                right[s] += decorrelatedRight[o][s];
            }
        }
    }

  private:
    float m_feedBackGain;
    float m_sampleRate;
    DelayWarp m_warp;
    float m_mono{0.0};

    alignas(16) std::array<size_t, ORDER> m_currentWidth{};
    alignas(16) std::array<std::array<float, BlockSize>, ORDER> m_inValue{};
    alignas(16) std::array<std::array<float, BlockSize>, ORDER> m_outValue{};
    alignas(16) std::array<std::array<float, ORDER>, BlockSize> m_lastValue{};
    alignas(16) std::array<std::array<float, ORDER>, BlockSize> m_feedValue{};
    alignas(16) std::array<float, ORDER> m_gain{};
    alignas(16) std::array<std::array<size_t, ORDER>, 2> m_itdTaps{};

    float m_msecs{100.0f};
    DelayWithTaps m_delay{};
    bool m_avoidEqualLengthDelay{false};
    CallbackMgr m_callbacks{};
};

}