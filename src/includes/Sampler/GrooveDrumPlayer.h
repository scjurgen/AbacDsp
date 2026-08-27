#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <format>
#include <iostream>
#include <random>
#include <span>
#include <string>
#include <vector>

#include "Helpers/DebugClock.h"
#include "Sampler/SliceLibrary.h"

namespace AbacDsp
{

/// @ingroup sampler
/// @brief One resolved MIDI-groove note: an exact tick position, a resolved
/// SliceLibrary track, and a linear playback gain.
struct GrooveTrigger
{
    uint32_t tick{0};
    size_t track{0};
    float gain{1.f};
};

/// @ingroup sampler
/// @brief A groove's trigger list plus the tick geometry needed to place them in
/// time. triggers must be sorted by tick ascending.
struct GrooveProgram
{
    std::vector<GrooveTrigger> triggers;
    uint32_t loopLengthTicks{1};
    uint16_t ticksPerQuarterNote{1};
};

/// @ingroup sampler
/// @brief Output of GrooveDrumPlayer::renderBurst(): pre-rendered audio plus the
/// tick state it ended at, for GrooveDrumPlayer::primeTickState() to resume from.
struct GrooveBurstResult
{
    std::vector<float> audio; // interleaved stereo
    double tickPos{0.0};
    size_t nextTriggerIndex{0};
};

/**
 * @ingroup sampler
 * @brief Polyphonic voice pool playing a resolved MIDI groove against a
 * sample library.
 *
 * Tracks its own tick position: a free-running loop of loopLengthTicks, advanced
 * each sample from the caller's live samplesPerBeat, independent of any bar/beat/
 * step grid. Every trigger plays a random slice from its track at unmodified gain
 * (no pitch/reverse/normalization - recorded levels are trusted as-is).
 * syncToPpq()/primeTickState() reposition tick state (host resync, or resuming a
 * renderBurst()-rendered burst); resetPosition() snaps to tick 0. Not thread-safe;
 * library/program pointers are borrowed and must outlive the player.
 *
 * @warning checkTriggers() logs once per loop repeat via std::cout - not
 *          realtime-safe, accepted deliberately.
 */
class GrooveDrumPlayer
{
  public:
    static constexpr size_t kChannels = 2;
    static constexpr size_t kMaxVoices = 16;
    // Headroom over any real kit's track count (the one shipped reggae kit has 39).
    static constexpr size_t kMaxTracks = 64;
    // syncToPpq()'s no-op-vs-resync threshold: tight enough to never fire on
    // ordinary floating-point drift, loose enough to ignore sub-audible jitter.
    static constexpr double kSyncToleranceTicks = 1.0;

    explicit GrooveDrumPlayer(const float sampleRate) noexcept
        : m_sampleRate(sampleRate)
    {
        setFadeMs(2.f);
    }

    void setLibrary(const SliceLibrary* library) noexcept
    {
        m_library = library;
    }

    // Optional per-track display names ("bd", "sd", "hh", ...) for the
    // trigger print; a track with no name, or an unset span, falls back to
    // its numeric index. Non-owning: the caller must keep storage alive.
    void setTrackNames(const std::span<const std::string> names) noexcept
    {
        m_trackNames = names;
    }

    void setGroove(const GrooveProgram* program) noexcept
    {
        if (program == m_program)
        {
            return;
        }
        m_program = program;
        resetPosition();
    }

    void resetPosition() noexcept
    {
        m_tickPos = 0.0;
        m_nextTriggerIndex = 0;
    }

    [[nodiscard]] double tickPosition() const noexcept
    {
        return m_tickPos;
    }

    void primeTickState(const double tickPos, const size_t nextTriggerIndex) noexcept
    {
        m_tickPos = tickPos;
        m_nextTriggerIndex = nextTriggerIndex;
    }

    // Phase-locks to a host's ppqPosition, called every host block while playing.
    // Most calls are a no-op sub-tick drift correction; returns true only on a
    // real jump (a host seek), which also repositions the trigger cursor.
    [[nodiscard]] bool syncToPpq(const double ppqPosition) noexcept
    {
        if (m_program == nullptr || m_program->ticksPerQuarterNote == 0 || m_program->loopLengthTicks == 0)
        {
            return false;
        }
        const double loopLengthTicks = static_cast<double>(m_program->loopLengthTicks);
        double newTickPos =
            std::fmod(ppqPosition * static_cast<double>(m_program->ticksPerQuarterNote), loopLengthTicks);
        if (newTickPos < 0.0)
        {
            newTickPos += loopLengthTicks;
        }
        const double forwardDistance = std::fmod(newTickPos - m_tickPos + loopLengthTicks, loopLengthTicks);
        const double wrapDistance = std::min(forwardDistance, loopLengthTicks - forwardDistance);
        if (wrapDistance <= kSyncToleranceTicks)
        {
            return false;
        }
        m_tickPos = newTickPos;
        const auto& triggers = m_program->triggers;
        m_nextTriggerIndex = static_cast<size_t>(
            std::distance(triggers.begin(), std::upper_bound(triggers.begin(), triggers.end(), newTickPos,
                                                             [](const double tick, const GrooveTrigger& trigger)
                                                             { return tick < static_cast<double>(trigger.tick); })));
        return true;
    }

    void setFadeMs(const float ms) noexcept
    {
        m_fadeFrames = std::max<size_t>(1, static_cast<size_t>(ms / 1000.f * m_sampleRate));
    }

    void setTrackGain(const size_t track, const float gain) noexcept
    {
        if (track < kMaxTracks)
        {
            m_trackGain[track] = gain;
        }
    }

    [[nodiscard]] size_t activeVoiceCount() const noexcept
    {
        size_t count = 0;
        for (const Voice& voice : m_voices)
        {
            count += voice.active ? 1 : 0;
        }
        return count;
    }

    // Advances by one sample, triggers any groove events crossed, and returns
    // the mixed stereo output. perTrackOut, if non-null, also gets each track's own contribution.
    [[nodiscard]] std::array<float, kChannels> advanceSample(
        const size_t samplesPerBeat,
        std::array<std::array<float, kChannels>, kMaxTracks>* perTrackOut = nullptr) noexcept
    {
        checkTriggers(samplesPerBeat);
        ++m_sampleCounter;
        std::array<float, kChannels> out{0.f, 0.f};
        if (perTrackOut != nullptr)
        {
            perTrackOut->fill(std::array<float, kChannels>{0.f, 0.f});
        }
        for (Voice& voice : m_voices)
        {
            if (voice.active)
            {
                renderVoice(voice, out, perTrackOut);
            }
        }
        return out;
    }

    // Off-thread, non-realtime: renders frames samples of a fresh player against
    // library/program for later resumption via primeTickState(). Allocates.
    [[nodiscard]] static GrooveBurstResult renderBurst(const float sampleRate, const float bpm,
                                                       const SliceLibrary& library, const GrooveProgram& program,
                                                       const size_t frames)
    {
        GrooveDrumPlayer player(sampleRate);
        player.setLibrary(&library);
        player.setGroove(&program);
        const auto samplesPerBeat = static_cast<size_t>(sampleRate * 60.f / std::max(1.f, bpm));
        GrooveBurstResult result;
        result.audio.resize(frames * kChannels);
        for (size_t i = 0; i < frames; ++i)
        {
            const auto frame = player.advanceSample(samplesPerBeat);
            result.audio[i * kChannels] = frame[0];
            result.audio[i * kChannels + 1] = frame[1];
        }
        result.tickPos = player.m_tickPos;
        result.nextTriggerIndex = player.m_nextTriggerIndex;
        return result;
    }

  private:
    /// @brief One playing slice. startOrder is a monotonic counter, so stealing picks the oldest by comparison.
    struct Voice
    {
        bool active{false};
        size_t track{0};
        size_t indexInTrack{0};
        size_t lengthFrames{0};
        size_t pos{0};
        size_t effectiveFade{1};
        float gain{1.f};
        uint64_t startOrder{0};
    };

    // Advances the tick position by this sample's worth of ticks and fires
    // every trigger whose tick falls in the half-open interval crossed,
    // wrapping the trigger cursor back to the start on a loop wrap.
    void checkTriggers(const size_t samplesPerBeat) noexcept
    {
        if (m_program == nullptr || m_library == nullptr || m_program->triggers.empty() ||
            m_program->loopLengthTicks == 0 || samplesPerBeat == 0)
        {
            return;
        }
        const auto& triggers = m_program->triggers;
        const double loopLengthTicks = static_cast<double>(m_program->loopLengthTicks);
        const double ticksPerSample =
            static_cast<double>(m_program->ticksPerQuarterNote) / static_cast<double>(samplesPerBeat);
        double newTickPos = m_tickPos + ticksPerSample;

        while (m_nextTriggerIndex < triggers.size() &&
               static_cast<double>(triggers[m_nextTriggerIndex].tick) < newTickPos)
        {
            triggerVoice(triggers[m_nextTriggerIndex]);
            ++m_nextTriggerIndex;
        }
        if (newTickPos >= loopLengthTicks)
        {
            newTickPos -= loopLengthTicks;
            std::cout << std::format("{:8.3f}s  loop repeat\n",
                                     static_cast<double>(m_sampleCounter) / static_cast<double>(m_sampleRate));
            m_nextTriggerIndex = 0;
            while (m_nextTriggerIndex < triggers.size() &&
                   static_cast<double>(triggers[m_nextTriggerIndex].tick) < newTickPos)
            {
                triggerVoice(triggers[m_nextTriggerIndex]);
                ++m_nextTriggerIndex;
            }
        }
        m_tickPos = newTickPos;
    }

    void triggerVoice(const GrooveTrigger& trigger) noexcept
    {
        if (trigger.track >= m_library->trackCount())
        {
            return;
        }
        const size_t sliceCount = m_library->sliceCountInTrack(trigger.track);
        const size_t sliceIndex = randomSliceIndex(sliceCount);
        if (sliceIndex >= sliceCount)
        {
            return;
        }
        const auto& info = m_library->sliceInfo(trigger.track, sliceIndex);
        if (info.lengthFrames == 0)
        {
            return;
        }
        Voice& voice = allocateVoice();
        voice.active = true;
        voice.track = trigger.track;
        voice.indexInTrack = sliceIndex;
        voice.lengthFrames = info.lengthFrames;
        voice.pos = 0;
        voice.gain = trigger.gain;
        voice.effectiveFade = std::max<size_t>(1, std::min(m_fadeFrames, info.lengthFrames / 2));
        voice.startOrder = m_triggerCounter++;
        logTrigger(trigger.track);
    }

    // Debug-only, temporary: names each fired trigger with its absolute time
    // since process start, for correlating audible groove hits against the
    // display's own clock (see TapeLooperImpl::logBarBeatIfChanged()).
    void logTrigger(const size_t track) const
    {
        const std::string name = track < m_trackNames.size() ? m_trackNames[track] : std::to_string(track);
        std::cout << std::format("{:10} us  trigger {}\n", debugElapsedMicroseconds(), name);
    }

    [[nodiscard]] size_t randomSliceIndex(const size_t sliceCount) noexcept
    {
        if (sliceCount == 0)
        {
            return 0;
        }
        std::uniform_int_distribution<size_t> dist(0, sliceCount - 1);
        return dist(m_rng);
    }

    [[nodiscard]] Voice& allocateVoice() noexcept
    {
        for (Voice& voice : m_voices)
        {
            if (!voice.active)
            {
                return voice;
            }
        }
        Voice* oldest = &m_voices[0];
        for (Voice& voice : m_voices)
        {
            if (voice.startOrder < oldest->startOrder)
            {
                oldest = &voice;
            }
        }
        return *oldest;
    }

    void renderVoice(Voice& voice, std::array<float, kChannels>& out,
                     std::array<std::array<float, kChannels>, kMaxTracks>* perTrackOut) noexcept
    {
        const auto gain = edgeGain(voice) * voice.gain * trackGain(voice.track);
        for (size_t channel = 0; channel < kChannels; ++channel)
        {
            const float sample = m_library->sample(voice.track, voice.indexInTrack, voice.pos, channel) * gain;
            out[channel] += sample;
            if (perTrackOut != nullptr && voice.track < kMaxTracks)
            {
                (*perTrackOut)[voice.track][channel] += sample;
            }
        }
        if (++voice.pos >= voice.lengthFrames)
        {
            voice.active = false;
        }
    }

    [[nodiscard]] float trackGain(const size_t track) const noexcept
    {
        return track < kMaxTracks ? m_trackGain[track] : 1.f;
    }

    [[nodiscard]] static float edgeGain(const Voice& voice) noexcept
    {
        const auto fade = static_cast<float>(voice.effectiveFade);
        const auto fadeIn = static_cast<float>(voice.pos + 1) / fade;
        const auto fadeOut = static_cast<float>(voice.lengthFrames - voice.pos) / fade;
        return std::clamp(std::min(fadeIn, fadeOut), 0.f, 1.f);
    }

    float m_sampleRate;
    size_t m_fadeFrames{1};
    const SliceLibrary* m_library{nullptr};
    const GrooveProgram* m_program{nullptr};
    std::span<const std::string> m_trackNames{};
    double m_tickPos{0.0};
    size_t m_nextTriggerIndex{0};
    uint64_t m_sampleCounter{0}; // for the trigger-log timestamp only
    std::array<Voice, kMaxVoices> m_voices{};
    std::array<float, kMaxTracks> m_trackGain{[]
                                              {
                                                  std::array<float, kMaxTracks> gains{};
                                                  gains.fill(1.f);
                                                  return gains;
                                              }()};
    uint64_t m_triggerCounter{1};
    std::mt19937 m_rng{std::random_device{}()};
};

}
