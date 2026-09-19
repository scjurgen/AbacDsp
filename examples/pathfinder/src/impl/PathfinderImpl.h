#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <memory>
#include <random>
#include <vector>

#include "Audio/AudioBuffer.h"
#include "Delays/WobbleDelay.h"
#include "EffectBase.h"
#include "Helpers/ConstructArray.h"
#include "SamplerateConverter/UpDownSampler.h"

/**
 * @brief Minimal "Tape Vibrato" graph: one WobbleDelay per channel inside an UpDownSampler,
 * both identically seeded so they share the same tape-like wow and flutter, 100% wet.
 *
 * The seed of a planned Lua tape-modulation toolbox (chorus.md); base delay, buffer size
 * and safety margin are fixed static config here rather than exposed controls.
 */
template <size_t BlockSize>
class PathfinderImpl final : public EffectBase
{
  public:
    static constexpr size_t kBufferSize{8192};
    static constexpr float kBaseDelayMs{8.f};
    // Reused from organicchorus's Classic config; re-confirmed for this stereo/single-head
    // use via explore/pathfinder_vibrato.cpp before wiring into the blueprint.
    static constexpr float kSafetyMarginSamples{250.f};
    static constexpr float kFlutterRateFloorHz{0.6f};
    static constexpr std::mt19937::result_type kSharedSeed{1};
    using Delay = AbacDsp::WobbleDelay<kBufferSize, BlockSize>;
    using Transport = AbacDsp::UpDownSampler<Delay, BlockSize>;

    explicit PathfinderImpl(const float sampleRate)
        : EffectBase(sampleRate)
        , m_transports(AbacDsp::constructArray<Transport, 2>(BlockSize, sampleRate))
    {
        m_visualWavedata.resize(6000);
        for (auto& transport : m_transports)
        {
            auto& delay = transport.processor();
            delay.seed(kSharedSeed);
            delay.setSafetyMargin(kSafetyMarginSamples);
            delay.setDelay(kBaseDelayMs * 0.001f * sampleRate, true);
        }
        applyMacros();
    }

    void setDepth(const float percent)
    {
        m_depth = percent * 0.01f;
        applyMacros();
    }

    void setSpeed(const float hz)
    {
        m_speedHz = hz;
        applyMacros();
    }

    void setAggressivity(const float percent)
    {
        m_aggressivity = percent * 0.01f;
        applyMacros();
    }

    // 0% is tape-oriented (below nominal transport speed), 50% is nominal, 100% is a
    // clean, host-rate-equivalent transport - see chorus.md's Character mapping.
    void setCharacter(const float percent)
    {
        const auto fraction = percent * 0.01f;
        const auto ratio = std::exp2((fraction - 0.5f) * 2.f * kCharacterOctaves);
        for (auto& transport : m_transports)
        {
            transport.setRatio(std::clamp(ratio, Transport::kMinRatio, Transport::kMaxRatio));
        }
    }

    void processBlock(const AbacDsp::AudioBuffer<2, BlockSize>& in, AbacDsp::AudioBuffer<2, BlockSize>& out)
    {
        for (size_t channel = 0; channel < 2; ++channel)
        {
            std::array<float, BlockSize> channelIn{};
            std::array<float, BlockSize> channelOut{};
            for (size_t i = 0; i < BlockSize; ++i)
            {
                channelIn[i] = in(i, channel);
            }
            m_transports[channel].processBlock(channelIn, channelOut);
            for (size_t i = 0; i < BlockSize; ++i)
            {
                out(i, channel) = channelOut[i];
            }
        }
        for (size_t i = 0; i < BlockSize; ++i)
        {
            m_visualWavedata[m_currentSample] = out(i, 0) + out(i, 1);
            m_currentSample = (m_currentSample + 1) % m_visualWavedata.size();
        }
    }

    const std::vector<float>& visualizeWaveData()
    {
        m_preparedWavedata = m_visualWavedata;
        return m_preparedWavedata;
    }

  private:
    static constexpr float kCharacterOctaves{1.f};
    // Wow/flutter depth ranges reused from organicchorus's Classic config; flutter's is
    // the gentler one, per chorus.md's "wow depth primarily, flutter depth more gently".
    static constexpr float kWowDepthAtZero{0.15f};
    static constexpr float kWowDepthAtOne{0.45f};
    static constexpr float kFlutterDepthAtZero{0.1f};
    static constexpr float kFlutterDepthAtOne{0.5f};
    // OU Aggressivity's wow-variance/wow-drift scale, validated up to this value against
    // kSafetyMarginSamples via explore/pathfinder_vibrato.cpp.
    static constexpr float kMaxWowVariance{0.6f};
    static constexpr float kMaxWowDrift{0.6f};

    void applyMacros()
    {
        for (auto& transport : m_transports)
        {
            auto& delay = transport.processor();
            delay.setWowRate(m_speedHz);
            delay.setFlutterRate(std::max(m_speedHz, kFlutterRateFloorHz));
            delay.setWowDepth(kWowDepthAtZero + (kWowDepthAtOne - kWowDepthAtZero) * m_depth);
            delay.setFlutterDepth(kFlutterDepthAtZero + (kFlutterDepthAtOne - kFlutterDepthAtZero) * m_depth);
            delay.setWowVariance(m_aggressivity * kMaxWowVariance);
            delay.setWowDrift(m_aggressivity * kMaxWowDrift);
        }
    }

    std::array<Transport, 2> m_transports;
    float m_depth{0.35f};
    float m_speedHz{0.8f};
    float m_aggressivity{0.15f};

    std::vector<float> m_visualWavedata;
    std::vector<float> m_preparedWavedata;
    size_t m_currentSample{0};
};
