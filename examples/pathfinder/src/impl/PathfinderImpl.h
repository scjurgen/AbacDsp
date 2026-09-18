#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <memory>
#include <vector>

#include "Audio/AudioBuffer.h"
#include "Delays/WobbleDelay.h"
#include "EffectBase.h"
#include "Filters/Sinc/sinc_4.h"

/**
 * @brief Minimal "Tape Vibrato" graph: a shared stereo WobbleDelay read head,
 * modulated by tape-like wow and flutter, 100% wet.
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
    static constexpr float kCorrectionThresholdSamples{190.f};
    static constexpr float kFlutterRateFloorHz{0.6f};
    using Transport = AbacDsp::WobbleDelay<kBufferSize, 2, 1, BlockSize>;

    explicit PathfinderImpl(const float sampleRate)
        : EffectBase(sampleRate)
        , m_transport(sampleRate, std::make_shared<AbacDsp::SincFilter>(sinc4))
    {
        m_visualWavedata.resize(6000);
        m_transport.setReadHeadSafetyMargin(kSafetyMarginSamples);
        m_transport.setReadHeadCorrectionThreshold(0, kCorrectionThresholdSamples);
        m_transport.setReadHead(0, kBaseDelayMs * 0.001f * sampleRate, true);
        m_transport.setRatio(1.f, true);
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
        m_transport.setRatio(ratio, false);
    }

    void processBlock(const AbacDsp::AudioBuffer<2, BlockSize>& in, AbacDsp::AudioBuffer<2, BlockSize>& out)
    {
        std::array<float, 2 * BlockSize> interleavedIn{};
        for (size_t i = 0; i < BlockSize; ++i)
        {
            interleavedIn[2 * i] = in(i, 0);
            interleavedIn[2 * i + 1] = in(i, 1);
        }
        m_transport.feed(interleavedIn);

        std::array<float, 2 * BlockSize> interleavedOut{};
        m_transport.readBlock(0, interleavedOut);
        for (size_t i = 0; i < BlockSize; ++i)
        {
            out(i, 0) = interleavedOut[2 * i];
            out(i, 1) = interleavedOut[2 * i + 1];
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
        m_transport.setWowRate(m_speedHz);
        m_transport.setFlutterRate(std::max(m_speedHz, kFlutterRateFloorHz));
        m_transport.setWowDepth(kWowDepthAtZero + (kWowDepthAtOne - kWowDepthAtZero) * m_depth);
        m_transport.setFlutterDepth(kFlutterDepthAtZero + (kFlutterDepthAtOne - kFlutterDepthAtZero) * m_depth);
        m_transport.setWowVariance(m_aggressivity * kMaxWowVariance);
        m_transport.setWowDrift(m_aggressivity * kMaxWowDrift);
    }

    Transport m_transport;
    float m_depth{0.35f};
    float m_speedHz{0.8f};
    float m_aggressivity{0.15f};

    std::vector<float> m_visualWavedata;
    std::vector<float> m_preparedWavedata;
    size_t m_currentSample{0};
};
