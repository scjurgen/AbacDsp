#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <numeric>

#include "Filters/OnePoleFilter.h"
#include "HadamardWalsh16.h"
#include "HadamardWalsh32.h"
#include "HadamardWalsh4.h"
#include "HadamardWalsh8.h"
#include "Helpers/ConstructArray.h"
#include "ModulationDelayNoFeedback.h"
#include "Numbers/Convert.h"
#include "Numbers/PrimeDispatcher.h"

namespace AbacDsp
{

// FDN reverb tank whose per-line delay lengths never jump or crossfade when changed: they
// pitch-glide to the new length (ModulationDelayNoFeedback, ChangeSizeMode::PITCH), so a resize
// produces a brief, intentional pitch-bend rather than a click. Leaner than FdnTankSpiced: no interaural time
// difference ITD/stereo taps, no callback manager. Up to NumDampedLines lines get a lowpass in the feedback path to
// absorb highs; up to NumModulatedLines lines keep their delay's built-in modulation for extra dispersion.
template <size_t MaxSizePerElement, size_t ORDER, size_t BlockSize, size_t NumDampedLines = 4,
          size_t NumModulatedLines = 4>
    requires(ORDER == 4 || ORDER == 8 || ORDER == 16 || ORDER == 32) && (NumDampedLines <= ORDER) &&
            (NumModulatedLines <= ORDER)
class FdnTankGlide
{
  public:
    struct DelayWarp
    {
        [[nodiscard]] float getBulgeValue(const float x, const float bulgePower = 4.0f) const noexcept
        {
            return bulge < 0 ? 1 - std::pow(1 - x, std::pow(bulgePower, bulge))
                             : std::pow(x, std::pow(bulgePower, -bulge));
        }

        float minSize{100};
        float maxSize{210};
        float bulge{-0.6f};
        float spreadLines{0.f};
    };

    using Delay = ModulationDelayNoFeedback<MaxSizePerElement>;
    using Damper = OnePoleFilter<OnePoleFilterCharacteristic::LowPass>;

    static constexpr float kDefaultDampingCutoffHz{6000.f};

    explicit FdnTankGlide(const float sampleRate)
        : m_feedBackGain(1.0f / std::sqrt(static_cast<float>(ORDER)))
        , m_sampleRate(sampleRate)
        , m_delay{constructArray<Delay, ORDER>(sampleRate)}
        , m_damping{constructArray<Damper, NumDampedLines>(sampleRate, kDefaultDampingCutoffHz)}
    {
        for (size_t o = NumModulatedLines; o < ORDER; ++o)
        {
            m_delay[o].setModDepth(0.f);
        }
        // ModulationDelayNoFeedback defaults to ChangeSizeMode::FADE, not instant. Force
        // HARDSWITCH for this initial setup call; PITCH only switches on once real audio starts
        // flowing (see tick()), so any later resize glides instead.
        for (auto& line : m_delay)
        {
            line.setChangeSizeMode(ChangeSizeMode::HARDSWITCH);
        }
        setMinSize(10);
        setMaxSize(20);
    }

    void setDamping(const float cutoffHz)
    {
        for (auto& damper : m_damping)
        {
            damper.setCutoff(cutoffHz);
        }
    }

    void setModulation(const float depth, const float speedHz)
    {
        for (size_t o = 0; o < NumModulatedLines; ++o)
        {
            m_delay[o].setModDepth(depth);
            m_delay[o].setModSpeed(speedHz);
        }
    }

    void setUniqueDelay(const bool value)
    {
        m_avoidEqualLengthDelay = value;
        computeDelaySizes();
    }

    [[nodiscard]] size_t computeSizeFromMeters(const float meters) const
    {
        auto w = static_cast<size_t>(Convert::metersToSamples(meters, m_sampleRate));
        w = std::clamp<size_t>(w, 11, MaxSizePerElement);
        w = getUsefulPrime<11>(w);
        return w;
    }

    size_t setSize(const size_t index, const float meters)
    {
        const auto w = computeSizeFromMeters(meters);
        setDirectSize(index, w);
        return w;
    }

    void setDirectSize(const size_t index, const size_t value)
    {
        if (index >= ORDER)
        {
            return;
        }
        m_currentWidth[index] = value;
        m_delay[index].setSize(value);
        const auto tmp = std::pow(0.001f, m_currentWidth[index] / m_sampleRate / (m_msecs / 1000.0f));
        m_gain[index] = tmp * m_feedBackGain;
    }

    void computeDelaySizes()
    {
        std::array<float, ORDER> meters{};
        std::array<size_t, ORDER> discreteSize{};
        for (size_t i = 0; i < ORDER; ++i)
        {
            const auto frac = static_cast<float>(i) / static_cast<float>(ORDER - 1);
            const auto x = m_warp.getBulgeValue(frac);
            const auto m = m_warp.minSize + x * (m_warp.maxSize - m_warp.minSize);
            meters[i] =
                std::clamp(m, std::min(m_warp.minSize, m_warp.maxSize), std::max(m_warp.minSize, m_warp.maxSize));
            discreteSize[i] = computeSizeFromMeters(meters[i]);
        }
        if (m_avoidEqualLengthDelay)
        {
            ensureUniqueDiscreteSize(discreteSize.data(), ORDER);
        }
        for (size_t i = 0; i < ORDER; ++i)
        {
            setDirectSize(i, discreteSize[i]);
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

    void processBlock(const float* in, float* out)
    {
        for (size_t s = 0; s < BlockSize; ++s)
        {
            const auto& delayOut = tick(in[s]);
            out[s] = std::accumulate(delayOut.begin(), delayOut.end(), 0.f);
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
        for (size_t s = 0; s < BlockSize; ++s)
        {
            const auto& delayOut = tick(in[s]);
            float l = 0.f;
            float r = 0.f;
            for (size_t o = 0; o < ORDER; o += 2)
            {
                l += delayOut[o];
                r += delayOut[o + 1];
            }
            left[s] += l + r * m_mono;
            right[s] += r + l * m_mono;
        }
    }

  private:
    void matrixMix(const float* in, float* out) const noexcept
    {
        if constexpr (ORDER == 4)
        {
            hadamardWalsh4_simd(in, out);
        }
        if constexpr (ORDER == 8)
        {
            hadamardWalsh8_simd(in, out);
        }
        if constexpr (ORDER == 16)
        {
            hadamardWalsh16_simd(in, out);
        }
        if constexpr (ORDER == 32)
        {
            hadamardWalsh32_simd(in, out);
        }
    }

    // One sample through every line: read (pitch-gliding towards its target length as needed),
    // damp the first NumDampedLines outputs, matrix-mix, and feed the mix back as the next
    // sample's input to each line. Returns the (damped) raw per-line outputs, which double as
    // both the audio output tap and the signal the matrix mixes for feedback.
    [[nodiscard]] const std::array<float, ORDER>& tick(const float in)
    {
        if (!m_started) [[unlikely]]
        {
            m_started = true;
            for (auto& line : m_delay)
            {
                line.setChangeSizeMode(ChangeSizeMode::PITCH);
            }
        }
        for (size_t o = 0; o < ORDER; ++o)
        {
            m_delayOut[o] = m_delay[o].step(m_feedInput[o]);
        }
        for (size_t o = 0; o < NumDampedLines; ++o)
        {
            m_delayOut[o] = m_damping[o].step(m_delayOut[o]);
        }
        matrixMix(m_delayOut.data(), m_mixed.data());
        for (size_t o = 0; o < ORDER; ++o)
        {
            m_feedInput[o] = in - m_mixed[o] * m_gain[o];
        }
        return m_delayOut;
    }

    float m_feedBackGain;
    float m_sampleRate;
    DelayWarp m_warp;
    float m_mono{0.0f};

    alignas(16) std::array<size_t, ORDER> m_currentWidth{};
    alignas(16) std::array<float, ORDER> m_gain{};
    alignas(16) std::array<float, ORDER> m_feedInput{};
    alignas(16) std::array<float, ORDER> m_delayOut{};
    alignas(16) std::array<float, ORDER> m_mixed{};

    float m_msecs{100.0f};
    std::array<Delay, ORDER> m_delay;
    std::array<Damper, NumDampedLines> m_damping;
    bool m_avoidEqualLengthDelay{false};
    bool m_started{false};
};

}
