#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <memory>
#include <random>

#include "Delays/OrganicChorusTransport.h"
#include "Filters/Distortion.h"
#include "Filters/OnePoleFilter.h"
#include "Filters/Sinc/SincFilter.h"
#include "Generators/OrnsteinUhlenbeckProcess.h"

namespace AbacDsp
{

/**
 * @ingroup delays
 * @brief One BBD-style chorus/flanger voice built on `OrganicChorusTransport`, wrapped in a
 * feedback loop and pre/post tone shaping.
 *
 * The transport writes tape at its base ratio plus a separate, zero-mean
 * `OrnsteinUhlenbeckProcess` drift (real mechanical speed wander), and keeps its Wow/Flutter
 * confined to the read head instead - so what's on tape stays clean and the chorus wobble is
 * purely a playback-time effect. This class also adds what a real BBD circuit wires around
 * the chip: feedback and tone shaping. Feedback runs one tile behind the transport's own
 * output, since a tile's wet signal isn't known until after it has been fed to the transport.
 *
 * Using low speed brings the BBD character, e.g. at 10% you write/movefact 4800 buckets per second.
 */
template <size_t BufferSize, size_t TileSize>
class OrganicChorusVoice
{
  public:
    OrganicChorusVoice(const float sampleRate, const std::shared_ptr<SincFilter>& sincFilter)
        : m_transport(sampleRate, sincFilter)
        , m_speedDrift(sampleRate / static_cast<float>(TileSize))
        , m_preHighPass(sampleRate)
        , m_preLowPass(sampleRate)
        , m_postLowPass(sampleRate)
        , m_feedbackDamp(sampleRate)
    {
        m_transport.setRatio(1.f, true);
        m_transport.setReadHead(0, sampleRate * 0.01f, true);
        m_transport.setReadHeadCorrectionThreshold(0, sampleRate * 0.01f);
        m_preHighPass.setCutoff(1.f);
        m_preLowPass.setCutoff(sampleRate * 0.5f);
        m_postLowPass.setCutoff(sampleRate * 0.5f);
        m_feedbackDamp.setCutoff(sampleRate * 0.5f);
    }

    void seed(const std::mt19937::result_type value) noexcept
    {
        m_transport.seed(value);
        m_speedDrift.seed(value + 1);
    }

    void setReadHeadSafetyMargin(const float samples) noexcept
    {
        m_transport.setReadHeadSafetyMargin(samples);
    }

    // See OrganicChorusTransport::setReadHeadCorrectionThreshold(): must exceed this voice's
    // own Wow/Flutter excursion, or drift correction cancels the modulation itself.
    void setReadHeadCorrectionThreshold(const float samples) noexcept
    {
        m_transport.setReadHeadCorrectionThreshold(0, samples);
    }

    void setCentreDelay(const float samples, const bool force = false) noexcept
    {
        m_transport.setReadHead(0, samples, force);
    }

    // The transport's own baseline speed (1.0 = unity); glides smoothly via the
    // transport's own accel/brake model. Speed-drift below is independent of this - it
    // perturbs the ratio directly each tile rather than retargeting this glide.
    void setTapeSpeedBaseRatio(const float ratio) noexcept
    {
        m_transport.setRatio(ratio);
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
        m_transport.setWowRate(hz);
    }

    void setWowDepth(const float depth) noexcept
    {
        m_transport.setWowDepth(depth);
    }

    void setWowVariance(const float value) noexcept
    {
        m_transport.setWowVariance(value);
    }

    void setWowDrift(const float value) noexcept
    {
        m_transport.setWowDrift(value);
    }

    void setFlutterRate(const float hz) noexcept
    {
        m_transport.setFlutterRate(hz);
    }

    void setFlutterDepth(const float depth) noexcept
    {
        m_transport.setFlutterDepth(depth);
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
        // Direct perturbation, not setRatio(): continuous retargeting would be damped
        // out by the glide meant for occasional speed changes. Subtracting the mean
        // (setSigma() sets it to sigma, not 0) keeps this a wobble, not a pitch bend.
        m_transport.setExternalRatioPerturbation(m_speedDrift.step() - m_speedDriftMean);

        std::array<float, TileSize> driven{};
        for (size_t i = 0; i < TileSize; ++i)
        {
            driven[i] = m_preHighPass.step(in[i] + m_feedbackDamp.step(m_prevWet[i]) * m_feedbackGain);
        }
        m_preLowPass.processBlock(driven.data(), TileSize);
        m_transport.feed(driven);

        m_transport.readBlock(0, m_prevWet);
        m_postLowPass.processBlock(m_prevWet.data(), out.data(), TileSize);
        m_saturation.processBlock(out.data(), out.data(), TileSize);
    }

    void reset() noexcept
    {
        m_transport.reset();
        m_speedDrift.reset();
        m_preHighPass.reset();
        m_preLowPass.reset();
        m_postLowPass.reset();
        m_feedbackDamp.reset();
        m_prevWet.fill(0.f);
    }

  private:
    OrganicChorusTransport<BufferSize, 1, 1, TileSize> m_transport;
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
