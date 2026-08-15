#pragma once

#include <algorithm>
#include <array>
#include <string_view>

#include "Audio/AudioBuffer.h"
#include "Delays/ModulationDelay.h"
#include "EffectBase.h"
#include "Filters/OnePoleFilter.h"
#include "Helpers/ConstructArray.h"
#include "Parameters/SmoothingParameter.h"

template <size_t BlockSize>
class DelayImpl final : public EffectBase
{
  public:
    static constexpr size_t MaxDelaySamples{10 * 48000};

    struct SyncDivision
    {
        std::string_view name;
        float quarterNotes;
    };

    // clang-format off
    static constexpr auto kSyncDivisions = std::to_array<SyncDivision>({
        {"1/1",   4.f},      {"1/2",   2.f},      {"1/2.",  3.f},      {"1/2T",  4.f / 3.f},
        {"1/4",   1.f},      {"1/4.",  1.5f},     {"1/4T",  2.f / 3.f},
        {"1/8",   0.5f},     {"1/8.",  0.75f},    {"1/8T",  1.f / 3.f},
        {"1/16",  0.25f},    {"1/16.", 0.375f},   {"1/16T", 1.f / 6.f},
    });
    // clang-format on
    using Delay = AbacDsp::ModulatingDelayPitchedAdjust<MaxDelaySamples>;
    using LowPass = AbacDsp::OnePoleFilter<AbacDsp::OnePoleFilterCharacteristic::LowPass>;
    using HighPass = AbacDsp::OnePoleFilter<AbacDsp::OnePoleFilterCharacteristic::HighPass>;
    using AllPass = AbacDsp::OnePoleFilter<AbacDsp::OnePoleFilterCharacteristic::AllPass>;

    explicit DelayImpl(const float sampleRate)
        : EffectBase(sampleRate)
        , m_delay{AbacDsp::constructArray<Delay, 2>(sampleRate)}
        , m_lowPass{AbacDsp::constructArray<LowPass, 2>(sampleRate)}
        , m_highPass{AbacDsp::constructArray<HighPass, 2>(sampleRate)}
        , m_allPass{AbacDsp::constructArray<AllPass, 2>(sampleRate)}
    {
        setTimeInMs(200);
        for (auto& f : m_lowPass)
        {
            f.setCutoff(16000);
        }
        for (auto& f : m_highPass)
        {
            f.setCutoff(50.f);
        }
        for (auto& f : m_allPass)
        {
            f.setCutoff(1500.f);
        }
        m_visualWavedata.resize(6000);
    }

    void setGain(const float value)
    {
        m_gain = std::pow(10.f, value / 20.f);
    }

    void setDry(const float value)
    {
        m_dryGain.newTransition(std::pow(10.f, value / 20.f), kParamSmoothingSeconds, sampleRate());
    }

    void setFeedback(const float valueInPercentage)
    {
        m_feedBack = valueInPercentage * 0.01f;
    }

    void setLowPass(const float cutoff)
    {
        for (auto& f : m_lowPass)
        {
            f.setCutoff(cutoff);
        }
    }

    void setHighPass(const float cutoff)
    {
        for (auto& f : m_highPass)
        {
            f.setCutoff(cutoff);
        }
    }

    void setAllPass(const float cutoff)
    {
        for (auto& f : m_allPass)
        {
            f.setCutoff(cutoff);
        }
    }

    void setModDepth(const float depthPercentage)
    {
        for (auto& d : m_delay)
        {
            d.setModDepth(depthPercentage * 0.01f);
        }
    }

    void setModSpeed(const float speed)
    {
        float factor = 1.f;
        for (auto& d : m_delay)
        {
            d.setModSpeed(speed * factor);
            factor *= 1.01f;
        }
    }

    void setTimeInMs(const float value)
    {
        for (auto& d : m_delay)
        {
            d.setSize(static_cast<size_t>(sampleRate() * value * 0.001f));
        }
    }

    void setHostSync(const bool value) noexcept
    {
        m_hostSync = value;
    }

    void setSyncDivision(const int index) noexcept
    {
        m_syncDivisionIndex = static_cast<size_t>(std::clamp(index, 0, static_cast<int>(kSyncDivisions.size()) - 1));
    }

    void setWet(const float value)
    {
        m_wetGain.newTransition(std::pow(10.f, value / 20.f), kParamSmoothingSeconds, sampleRate());
    }

    void setLowPassCutoff(const float cutoff)
    {
        for (auto& f : m_lowPass)
        {
            f.setCutoff(cutoff);
        }
    }
    void setHighPassCutoff(const float cutoff)
    {
        for (auto& f : m_highPass)
        {
            f.setCutoff(cutoff);
        }
    }
    void setAllPassCutoff(const float cutoff)
    {
        for (auto& f : m_allPass)
        {
            f.setCutoff(cutoff);
        }
    }
    void processBlock(const AbacDsp::AudioBuffer<2, BlockSize>& in, AbacDsp::AudioBuffer<2, BlockSize>& out)
    {
        if (m_hostSync)
        {
            applyHostSyncedTime();
        }

        for (size_t i = 0; i < BlockSize; ++i)
        {
            const float wetGain = m_wetGain.getValue();
            const float dryGain = m_dryGain.getValue();
            for (size_t c = 0; c < 2; ++c)
            {
                const float dry = in(i, c);
                const float filteredValue = m_allPass[c].step(
                    m_highPass[c].step(m_lowPass[c].step(dry * m_gain + m_feedBack * m_lastValue[c])));
                const auto value = m_delay[c].step(filteredValue);
                m_lastValue[c] = value;
                out(i, c) = value * wetGain + dryGain * dry;
            }
        }
        m_visualWavedata[m_currentSample] = out(0, 0);
        m_currentSample++;
        if (m_currentSample >= m_visualWavedata.size())
        {
            m_currentSample = 0;
        }
    }
    const std::vector<float>& visualizeWaveData()
    {
        m_preparedWavedata.resize(m_visualWavedata.size());
        m_preparedWavedata = m_visualWavedata;
        return m_preparedWavedata;
    }

  private:
    void applyHostSyncedTime() noexcept
    {
        const float bpm = std::clamp(static_cast<float>(hostTransport().bpm), 20.f, 300.f);
        const float msPerQuarterNote = 60000.f / bpm;
        setTimeInMs(kSyncDivisions[m_syncDivisionIndex].quarterNotes * msPerQuarterNote);
    }

    float m_gain{};
    std::array<float, 2> m_lastValue{};
    std::array<Delay, 2> m_delay;
    std::array<LowPass, 2> m_lowPass;
    std::array<HighPass, 2> m_highPass;
    std::array<AllPass, 2> m_allPass;
    std::vector<float> m_visualWavedata;
    std::vector<float> m_preparedWavedata;
    size_t m_currentSample = 0;
    static constexpr float kParamSmoothingSeconds{0.01f};
    AbacDsp::LinearSmoothing m_wetGain{0.2f};
    AbacDsp::LinearSmoothing m_dryGain{1.f};
    float m_feedBack{0.f};
    bool m_hostSync{false};
    size_t m_syncDivisionIndex{4};
};