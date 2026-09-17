#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <memory>

#include "Delays/OrganicChorusVoice.h"
#include "Helpers/ConstructArray.h"

namespace AbacDsp
{

/**
 * @ingroup delays
 * @brief Up to MaxVoices organic chorus/flanger voices, panned into a stereo bus and
 * mixed with the dry signal.
 *
 * Every voice is fed the same mono sum of the stereo input; only inactive voices (beyond
 * the currently active count) are skipped in the mix. Per-voice DSP values (delay, Wow/
 * Flutter, tone, feedback, pan, gain) are plain setters - macro curves and per-
 * configuration policy are the caller's concern, not this class's, so it stays testable
 * and configuration-agnostic.
 */
template <size_t BufferSize, size_t TileSize, size_t MaxVoices = 4>
class OrganicChorusEngine
{
  public:
    OrganicChorusEngine(const float sampleRate, const std::shared_ptr<SincFilter>& sincFilter)
        : m_voices(constructArray<OrganicChorusVoice<BufferSize, TileSize>, MaxVoices>(sampleRate, sincFilter))
    {
        m_gain.fill(1.f);
        for (size_t v = 0; v < MaxVoices; ++v)
        {
            m_voices[v].seed(static_cast<std::mt19937::result_type>(v + 1));
            m_pan[v] = MaxVoices > 1 ? -1.f + 2.f * static_cast<float>(v) / static_cast<float>(MaxVoices - 1) : 0.f;
        }
    }

    void setActiveVoiceCount(const size_t count) noexcept
    {
        m_activeVoices = std::clamp(count, size_t{1}, MaxVoices);
    }

    void setVoiceSeed(const size_t voice, const std::mt19937::result_type value) noexcept
    {
        m_voices[voice].seed(value);
    }

    void setVoicePan(const size_t voice, const float pan) noexcept
    {
        m_pan[voice] = std::clamp(pan, -1.f, 1.f);
    }

    void setVoiceGain(const size_t voice, const float linearGain) noexcept
    {
        m_gain[voice] = linearGain;
    }

    void setVoiceCentreDelay(const size_t voice, const float samples, const bool force = false) noexcept
    {
        m_voices[voice].setCentreDelay(samples, force);
    }

    void setVoiceTapeSpeedBaseRatio(const size_t voice, const float ratio) noexcept
    {
        m_voices[voice].setTapeSpeedBaseRatio(ratio);
    }

    void setVoiceSpeedDriftAmplitude(const size_t voice, const float sigma) noexcept
    {
        m_voices[voice].setSpeedDriftAmplitude(sigma);
    }

    void setVoiceReadHeadSafetyMargin(const size_t voice, const float samples) noexcept
    {
        m_voices[voice].setReadHeadSafetyMargin(samples);
    }

    void setVoiceReadHeadCorrectionThreshold(const size_t voice, const float samples) noexcept
    {
        m_voices[voice].setReadHeadCorrectionThreshold(samples);
    }

    void setVoiceWow(const size_t voice, const float rateHz, const float depth, const float variance,
                     const float drift) noexcept
    {
        m_voices[voice].setWowRate(rateHz);
        m_voices[voice].setWowDepth(depth);
        m_voices[voice].setWowVariance(variance);
        m_voices[voice].setWowDrift(drift);
    }

    void setVoiceFlutter(const size_t voice, const float rateHz, const float depth) noexcept
    {
        m_voices[voice].setFlutterRate(rateHz);
        m_voices[voice].setFlutterDepth(depth);
    }

    void setVoiceTone(const size_t voice, const float highPassHz, const float preLowPassHz,
                      const float postLowPassHz) noexcept
    {
        m_voices[voice].setToneHighPass(highPassHz);
        m_voices[voice].setTonePreLowPass(preLowPassHz);
        m_voices[voice].setTonePostLowPass(postLowPassHz);
    }

    void setVoiceSaturation(const size_t voice, const float drive) noexcept
    {
        m_voices[voice].setSaturation(drive);
    }

    void setVoiceFeedback(const size_t voice, const float gain, const float dampCutoffHz) noexcept
    {
        m_voices[voice].setFeedback(gain, dampCutoffHz);
    }

    void setMix(const float wetFraction) noexcept
    {
        m_wet = std::clamp(wetFraction, 0.f, 1.f);
    }

    void processBlock(const std::array<float, 2 * TileSize>& in, std::array<float, 2 * TileSize>& out) noexcept
    {
        std::array<float, TileSize> monoIn{};
        for (size_t i = 0; i < TileSize; ++i)
        {
            monoIn[i] = 0.5f * (in[2 * i] + in[2 * i + 1]);
        }

        std::array<float, TileSize> wetLeft{};
        std::array<float, TileSize> wetRight{};
        std::array<float, TileSize> voiceOut{};
        for (size_t v = 0; v < m_activeVoices; ++v)
        {
            m_voices[v].processBlock(monoIn, voiceOut);
            const auto leftGain = m_gain[v] * 0.5f * (1.f - m_pan[v]);
            const auto rightGain = m_gain[v] * 0.5f * (1.f + m_pan[v]);
            for (size_t i = 0; i < TileSize; ++i)
            {
                wetLeft[i] += voiceOut[i] * leftGain;
                wetRight[i] += voiceOut[i] * rightGain;
            }
        }

        const auto dry = 1.f - m_wet;
        for (size_t i = 0; i < TileSize; ++i)
        {
            out[2 * i] = dry * in[2 * i] + m_wet * wetLeft[i];
            out[2 * i + 1] = dry * in[2 * i + 1] + m_wet * wetRight[i];
        }
    }

    void reset() noexcept
    {
        for (auto& voice : m_voices)
        {
            voice.reset();
        }
    }

  private:
    std::array<OrganicChorusVoice<BufferSize, TileSize>, MaxVoices> m_voices;
    std::array<float, MaxVoices> m_pan{};
    std::array<float, MaxVoices> m_gain{};
    size_t m_activeVoices{MaxVoices};
    float m_wet{0.5f};
};

}
