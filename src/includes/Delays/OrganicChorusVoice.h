#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <random>

#include "Delays/WobbleDelay.h"
#include "Filters/Distortion.h"
#include "Filters/OnePoleFilter.h"
#include "Generators/OrnsteinUhlenbeckProcess.h"
#include "Parameters/OctaveGlide.h"
#include "SamplerateConverter/UpDownSampler.h"

namespace AbacDsp
{

/**
 * @ingroup delays
 * @brief One BBD-style chorus/flanger voice: a `WobbleDelay` running inside an `UpDownSampler`
 * at the tape-speed ratio, wrapped in a feedback loop and pre/post tone shaping.
 *
 * Tape speed is the sampler's ratio: the base ratio glides at the accel/brake rate, and a
 * zero-mean `OrnsteinUhlenbeckProcess` adds real mechanical wander every tile. Wow and Flutter
 * modulate the read head only, so what is delayed stays clean. Feedback runs one tile behind
 * the output. The effective delay is the centre delay over the ratio plus the sampler's
 * latency, which is not compensated.
 *
 * Using low speed brings the BBD character, e.g. at 10% it runs at 4800 buckets per second.
 */
template <size_t BufferSize, size_t TileSize>
class OrganicChorusVoice
{
  public:
    explicit OrganicChorusVoice(const float sampleRate)
        : m_transport(TileSize, sampleRate)
        , m_baseRatio(sampleRate)
        , m_speedDrift(sampleRate / static_cast<float>(TileSize))
        , m_preHighPass(sampleRate)
        , m_preLowPass(sampleRate)
        , m_postLowPass(sampleRate)
        , m_feedbackDamp(sampleRate)
    {
        m_transport.processor().setDelay(sampleRate * 0.01f, true);
        m_preHighPass.setCutoff(1.f);
        m_preLowPass.setCutoff(sampleRate * 0.5f);
        m_postLowPass.setCutoff(sampleRate * 0.5f);
        m_feedbackDamp.setCutoff(sampleRate * 0.5f);
    }

    void seed(const std::mt19937::result_type value) noexcept
    {
        m_transport.processor().seed(value);
        m_speedDrift.seed(value + 1);
    }

    void setReadHeadSafetyMargin(const float samples) noexcept
    {
        m_transport.processor().setSafetyMargin(samples);
    }

    void setCentreDelay(const float samples, const bool force = false) noexcept
    {
        m_transport.processor().setDelay(samples, force);
    }

    // The transport's own baseline speed (1.0 = unity), glided at the accel/brake rate.
    // Speed-drift below is independent of this: it perturbs the ratio each tile instead.
    void setTapeSpeedBaseRatio(const float ratio) noexcept
    {
        m_baseRatio.setTarget(std::clamp(ratio, Transport::kMinRatio, Transport::kMaxRatio));
    }

    // Amplitude (an OrnsteinUhlenbeckProcess sigma) of real mechanical speed wander on
    // the transport's own clock - independent of Depth's sine-LFO wobble.
    void setSpeedDriftAmplitude(const float sigma) noexcept
    {
        m_speedDrift.setSigma(sigma);
        m_speedDriftMean = sigma;
    }

    void setWowRate(const float hz) noexcept
    {
        m_transport.processor().setWowRate(hz);
    }

    void setWowDepth(const float depth) noexcept
    {
        m_transport.processor().setWowDepth(depth);
    }

    void setWowVariance(const float value) noexcept
    {
        m_transport.processor().setWowVariance(value);
    }

    void setWowDrift(const float value) noexcept
    {
        m_transport.processor().setWowDrift(value);
    }

    void setFlutterRate(const float hz) noexcept
    {
        m_transport.processor().setFlutterRate(hz);
    }

    void setFlutterDepth(const float depth) noexcept
    {
        m_transport.processor().setFlutterDepth(depth);
    }

    void setToneHighPass(const float hz) noexcept
    {
        m_preHighPass.setCutoff(hz);
    }

    void setTonePreLowPass(const float hz) noexcept
    {
        m_preLowPass.setCutoff(hz);
    }

    void setTonePostLowPass(const float hz) noexcept
    {
        m_postLowPass.setCutoff(hz);
    }

    void setSaturation(const float drive) noexcept
    {
        m_saturation.setDrive(drive);
    }

    void setFeedback(const float gain, const float dampCutoffHz) noexcept
    {
        m_feedbackGain = std::clamp(gain, -0.98f, 0.98f);
        m_feedbackDamp.setCutoff(dampCutoffHz);
    }

    void processBlock(const std::array<float, TileSize>& in, std::array<float, TileSize>& out) noexcept
    {
        // Subtracting the mean (setSigma() sets it to sigma, not 0) keeps the drift a
        // wobble, not a pitch bend.
        const auto drift = m_speedDrift.step() - m_speedDriftMean;
        const auto ratio = m_baseRatio.getValue(TileSize) * (1.f + drift);
        // Forced: the base glide and the drift are applied here, the sampler must not smooth them.
        m_transport.setRatio(std::clamp(ratio, Transport::kMinRatio, Transport::kMaxRatio), true);

        std::array<float, TileSize> driven{};
        for (size_t i = 0; i < TileSize; ++i)
        {
            driven[i] = m_preHighPass.step(in[i] + m_feedbackDamp.step(m_prevWet[i]) * m_feedbackGain);
        }
        m_preLowPass.processBlock(driven.data(), TileSize);
        m_transport.processBlock(driven, m_prevWet);

        m_postLowPass.processBlock(m_prevWet.data(), out.data(), TileSize);
        m_saturation.processBlock(out.data(), out.data(), TileSize);
    }

    void reset() noexcept
    {
        m_transport.processor().reset();
        m_transport.reset();
        m_speedDrift.reset();
        m_preHighPass.reset();
        m_preLowPass.reset();
        m_postLowPass.reset();
        m_feedbackDamp.reset();
        m_prevWet.fill(0.f);
    }

  private:
    using Transport = UpDownSampler<WobbleDelay<BufferSize, TileSize>, TileSize>;

    Transport m_transport;
    OctaveGlide m_baseRatio;
    OrnsteinUhlenbeckProcess m_speedDrift;
    float m_speedDriftMean{0.f};
    OnePoleFilter<OnePoleFilterCharacteristic::HighPass> m_preHighPass;
    OnePoleFilter<OnePoleFilterCharacteristic::LowPass> m_preLowPass;
    OnePoleFilter<OnePoleFilterCharacteristic::LowPass> m_postLowPass;
    OnePoleFilter<OnePoleFilterCharacteristic::LowPass> m_feedbackDamp;
    AtanhDrive m_saturation;
    float m_feedbackGain{0.f};
    std::array<float, TileSize> m_prevWet{};
};

}
