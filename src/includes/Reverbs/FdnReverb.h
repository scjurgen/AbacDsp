#pragma once

#include "Delays/ModulationDelay.h"
#include "Delays/DispersionDelay.h"
#include "Filters/Biquad.h"
#include "Filters/OnePoleFilter.h"
#include "HadamardFeed.h"
#include "Delays/PitchFadeWindowDelay.h"
#include "Helpers/ConstructArray.h"
#include "Numbers/PrimeDispatcher.h"

#include <random>

namespace AbacDsp
{
template <size_t MaxSizePerElement, size_t MAXORDER, size_t BlockSize>
class FdnTank
{
    static constexpr size_t processingSize = 16;
    static constexpr size_t NumPitchDelays = 2;
    static constexpr size_t MaxSpecialFilters = 4;

  public:
    using LowPassFilter = OnePoleFilter<OnePoleFilterCharacteristic::LowPass>;
    using HighPassFilter = OnePoleFilter<OnePoleFilterCharacteristic::HighPass>;
    using AllPassFilter = OnePoleFilter<OnePoleFilterCharacteristic::AllPass>;
    using SimpleDelay = DispersionDelay<MaxSizePerElement>;
    using ModDelay = ModulatingDelayPitchedAdjust<MaxSizePerElement>;
    explicit FdnTank(const float sampleRate)
        : m_feedBackGain(1.0f / std::sqrt(static_cast<float>(MAXORDER)))
        , m_sampleRate(sampleRate)
        , m_basicDelay(constructArray<SimpleDelay, MAXORDER>(sampleRate))
        , m_delay(constructArray<ModDelay, MAXORDER>(sampleRate))
        , m_lp(constructArray<LowPassFilter, MaxSpecialFilters>(sampleRate))
        , m_hp(constructArray<HighPassFilter, MaxSpecialFilters>(sampleRate))
    {
        setMinSize(10);
        setMaxSize(20);
        setLowpass();
        setHighpass();
        setAllpass();
        setModulationDepth();
        setModulationSpeed();
    }

    void setOrder(const size_t order)
    {
        m_order = order;
        m_feedBackGain = 1.0f / std::sqrt(static_cast<float>(m_order));
        computeDelaySizes();
        setModulationDepth();
        setModulationSpeed();
        setAllpass();
        setLowpass();
        setHighpass();
    }

    void setUniqueDelay(const bool value)
    {
        m_avoidEqualLengthDelay = value;
        computeDelaySizes();
    }

    void setAllpassCount(const size_t value)
    {
        m_countAllpass = value;
        setAllpass();
    }

    void setLowpassCount(const size_t value)
    {
        m_countLowpass = value;
    }

    void setHighpassCount(const size_t value)
    {
        m_countHighpass = value;
    }

    void setModulationCount(const size_t value)
    {
        m_countModulation = value;
        setModulationDepth();
        setModulationSpeed();
    }

    void setPitch(size_t index, const float value)
    {
        m_pitch[index] = std::abs(value) < 0.01f ? 0.f : value;
        m_fusedPitch[index].setPitch(value);
        m_usePitch = false;
        for (const auto& p : m_pitch)
        {
            if (p != 0.0f)
            {
                m_usePitch = true;
            }
        }
    }


    void setModulationSpeed()
    {
        if (!m_countModulation)
        {
            return;
        }
        for (size_t i = 0; i < m_countModulation; ++i)
        {
            m_delay[i].setModSpeed(m_modSpeed + m_modSpeed * static_cast<float>(i) / m_countModulation);
        }
        for (size_t i = m_countModulation; i < m_delay.size(); ++i)
        {
            m_delay[i].setModSpeed(m_modSpeed + m_modSpeed * static_cast<float>(i) / m_delay.size());
        }
    }

    void setModSpeed(const float value)
    {
        m_modSpeed = value;
        setModulationSpeed();
    }

    void setModulationDepth()
    {
        for (size_t i = 0; i < m_countModulation; ++i)
        {
            m_delay[i].setModDepth(m_modDepth);
        }
        for (size_t i = m_countModulation; i < m_delay.size(); ++i)
        {
            m_delay[i].setModDepth(0.f);
        }
    }

    void setModDepth(const float value)
    {
        m_modDepth = value;
        setModulationDepth();
    }

    void setSampleRate(const float rate)
    {
        m_sampleRate = rate;
    }

    void silence()
    {
        for (auto& d : m_delay)
        {
            d.silence();
        }
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
        m_delay[index].setSize(w);
        m_basicDelay[index].setSize(w);
        const auto tmp = powf(0.001f, m_currentWidth[index] / m_sampleRate / (m_msecs / 1000.0f));
        m_gain[index] = tmp * m_feedBackGain;

        // for (size_t i = 0; i < NumPitchDelays; ++i)
        // {
        //     if (index == m_order - NumPitchDelays + i)
        //     {
        //         m_octaveDelay[i].setSize(w);
        //     }
        // }
        return w;
    }

    void setDirectSize(const size_t index, const size_t value)
    {
        if (index >= m_order)
        {
            return;
        }
        m_currentWidth[index] = value;
        m_delay[index].setSize(value);
        m_basicDelay[index].setSize(value);
        const auto tmp = powf(0.001f, m_currentWidth[index] / m_sampleRate / (m_msecs / 1000.0f));
        m_gain[index] = tmp * m_feedBackGain;
        for (size_t i = 0; i < NumPitchDelays; ++i)
        {
            if (index == m_order - NumPitchDelays + i)
            {
                //  m_octaveDelay[i].setSize(value);
            }
        }
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
        size_t sum = 0;
        for (size_t i = 0; i < m_order; ++i)
        {
            setDirectSize(i, m_discreteSize[i]);
            sum += m_discreteSize[i];
        }
    }

    void setPitchReverse(bool value)
    {
        m_reversePitch = value;
    }

    void setLowpass(const float value)
    {
        m_lowPass = value;
        setLowpass();
    }

    void setHighpass(const float value)
    {
        m_highPass = value;
        setHighpass();
    }

    void setAllpassFirstCutoff(const float value)
    {
        m_allPassFirst = value;
        setAllpass();
    }

    void setAllpassLastCutoff(const float value)
    {
        m_allPassLast = value;
        setAllpass();
    }

    void setSpreadStereo(const float value)
    {
        m_mono = 1.f - value;
    }

    float getBulgeValue(const float x, const float bulge, const float bulgePower = 4.0f)
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
    void setLowPassCount(const float value)
    { // m_v=value;
    }

    void setHighPassCount(const float value)
    { // m_v=value;
    }

    void setModulationDepth(const float value)
    {
        for (auto& d : m_delay)
        {
            d.setModDepth(value);
        }
    }

    void setModulationSpeed(const float value)
    {
        for (auto& d : m_delay)
        {
            d.setModSpeed(value);
        }
    }

    void setReversePitch(const float value)
    {
        for (auto& pd : m_fusedPitch)
        {
            pd.setReverse(value);
        }
    }


    void setPitchStrength(const float value)
    {
        m_pitchStrength = value;
    }

    void setPitch1Inplace(const float value)
    {
        setPitch(0, value);
    }

    void setPitch2Inplace(const float value)
    {
        setPitch(1, value);
    }


    void matrixFeed(float in)
    {
        hadamardFeed(m_order, m_lastValue.data(), m_feedValue.data());

        if (m_usePitch)
        {
            size_t indexS{0};
            if (!m_useFusedPitch)
            {
                const float inPitched = in + m_pitchStrength * (m_fusedPitch[0].step(in) + m_fusedPitch[1].step(in));
                for (size_t s = 0; s < m_order - m_countModulation; ++s)
                {
                    m_lastValue[s] = m_basicDelay[s].step(inPitched - m_feedValue[s] * m_gain[s]);
                }
                for (size_t s = 0; s < m_countModulation; ++s)
                {
                    m_lastValue[m_order - 1 - s] =
                        m_delay[s].step(inPitched - m_feedValue[m_order - 1 - s] * m_gain[m_order - 1 - s]);
                }
            }
            else
            {
                for (size_t s = 0; s < m_order - NumPitchDelays; ++s, ++indexS)
                {
                    m_lastValue[indexS] = m_delay[indexS].step(in - m_feedValue[indexS] * m_gain[indexS]);
                }
                std::array<float, NumPitchDelays> pitchOut{};
                for (size_t s = 0; s < m_pitch.size() && s < m_order; ++s, ++indexS)
                {
                    if (m_pitch[s] != 0.f)
                    {
                        pitchOut[s] = m_fusedPitch[s].step(in - m_feedValue[indexS] * m_gain[indexS]);
                        m_lastValue[indexS] =
                            pitchOut[s] * m_pitchStrength +
                            m_delay[indexS].step(in - m_feedValue[indexS] * m_gain[indexS]) * (1 - m_pitchStrength);
                    }
                    else
                    {
                        m_lastValue[indexS] = m_delay[indexS].step(in - m_feedValue[indexS] * m_gain[indexS]);
                    }
                }
            }
        }
        else
        {
            for (size_t s = 0; s < m_order - m_countModulation; ++s)
            {
                m_lastValue[s] = m_basicDelay[s].step(in - m_feedValue[s] * m_gain[s]);
            }
            for (size_t s = 0; s < m_countModulation; ++s)
            {
                m_lastValue[m_order - 1 - s] =
                    m_delay[s].step(in - m_feedValue[m_order - 1 - s] * m_gain[m_order - 1 - s]);
            }
        }
        // for (size_t s = 0; s < m_order && s < m_countSaturation; ++s)
        // {
        //     size_t idx = s + (m_order - m_countSaturation) / 2;
        //     m_lastValue[idx] = m_invSaturationDepth * std::atan(m_saturationDepth * m_lastValue[idx]);
        // }
        //
        // for (size_t s = 0; s < m_countLowpass; ++s)
        // {
        //     size_t idx = s + (m_order - m_countLowpass) / 2;
        //     m_lastValue[idx] = m_lp[s].step(m_lastValue[idx]);
        // }
        // for (size_t s = 0; s < m_countHighpass; ++s)
        // {
        //     size_t idx = s + (m_order - m_countHighpass) / 2;
        //     m_lastValue[idx] = m_hp[idx].step(m_lastValue[idx]);
        // }
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
    void processBlock(const float* in, float* out, const uint32_t numSamples)
    {
        for (uint32_t pos = 0; pos < numSamples; pos++)
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

    void processBlockSplit(const float* in, float* left, float* right, const uint32_t numSamples)
    {
        std::fill(left, left + numSamples, 0.f);
        std::fill(right, right + numSamples, 0.f);
        processBlockSplitAdd(in, left, right, numSamples);
    }

    void processBlockSplitAdd(const float* in, float* left, float* right, const uint32_t numSamples)
    {
        const float factorOrder = 1.f / static_cast<float>(m_order);
        for (uint32_t pos = 0; pos < numSamples; pos++)
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

    void processBlockSum(const float* in, float* out, const uint32_t numSamples)
    {
        for (uint32_t pos = 0; pos < numSamples; pos++)
        {
            matrixFeed(in[pos]);
            for (size_t s = 0; s < m_order; ++s)
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
    void setLowpass()
    {
        for (auto& lp : m_lp)
        {
            lp.setCutoff(m_lowPass);
        }
    }

    void setHighpass()
    {
        for (auto& hp : m_hp)
        {
            hp.setCutoff(m_highPass);
        }
    }

    void setAllpass()
    {
        for (size_t i = 0; i < m_order; ++i)
        {
            const auto x = static_cast<float>(i) / static_cast<float>(m_order - 1);
            const auto value = m_allPassFirst * std::pow(m_allPassLast / m_allPassFirst, x);
            m_basicDelay[i].setAllPassCutoff(value);
        }
    }

    float m_allPassFirst{1400.f}, m_allPassLast{600.f};
    float m_feedBackGain;

    float m_sampleRate;
    float m_minSize{100};
    float m_maxSize{210};
    float m_lowPass{20000};
    float m_highPass{20};
    float m_modSpeed{0.2f};
    float m_modDepth{0.f};
    float m_bulge{-0.6f};
    float m_mono{0.0};
    std::array<size_t, MAXORDER> m_currentWidth{};
    std::array<float, MAXORDER> m_lastValue{};
    std::array<float, MAXORDER> m_feedValue{};
    float m_msecs{100.0f};
    std::array<float, MAXORDER> m_gain{};
    std::array<SimpleDelay, MAXORDER> m_basicDelay;
    std::array<ModDelay, MAXORDER> m_delay;
    std::array<PitchFadeWindowDelay<MaxSizePerElement>, NumPitchDelays> m_fusedPitch{};
    std::array<PitchFadeWindowDelay<MaxSizePerElement>, NumPitchDelays> m_feedPitch{};
    std::array<LowPassFilter, MaxSpecialFilters> m_lp;
    std::array<HighPassFilter, MaxSpecialFilters> m_hp;

    size_t m_countAllpass{0};
    size_t m_countLowpass{0};
    size_t m_countHighpass{0};
    size_t m_countModulation{2};
    size_t m_order{MAXORDER};
    bool m_spreadSymmetric{false};
    bool m_avoidEqualLengthDelay{false};
    float m_spreadLines{0.f};
    bool m_usePitch{false};
    bool m_useFusedPitch{true};
    bool m_reversePitch{false};
    float m_pitchStrength{0.5f};
    std::array<float, NumPitchDelays> m_pitch{};
    std::array<float, NumPitchDelays> m_pitchOut{};
};
}
