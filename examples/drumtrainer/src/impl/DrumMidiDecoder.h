#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

#include "DrumMap.h"
#include "HihatClassifier.h"

namespace DrumInput
{

enum class PlayedVoice : uint8_t
{
    Kick,
    Snare,
    HihatClosed,
    HihatHalfOpen,
    HihatOpen,
    HihatPedal,
    Crash,
    TomHigh,
    TomLow,
    TomFloor,
};

// Indexed by PlayedVoice.
inline constexpr auto kPlayedVoiceLabels =
    std::to_array<std::string_view>({"Kick", "Snare", "Hihat closed", "Hihat half-open", "Hihat open", "Hihat pedal",
                                     "Crash", "Tom high", "Tom low", "Tom floor"});
static_assert(kPlayedVoiceLabels.size() == static_cast<size_t>(PlayedVoice::TomFloor) + 1);

[[nodiscard]] constexpr std::string_view label(const PlayedVoice voice) noexcept
{
    return kPlayedVoiceLabels[static_cast<size_t>(voice)];
}

[[nodiscard]] constexpr bool isHihat(const PlayedVoice voice) noexcept
{
    return voice == PlayedVoice::HihatClosed || voice == PlayedVoice::HihatHalfOpen ||
           voice == PlayedVoice::HihatOpen || voice == PlayedVoice::HihatPedal;
}

// pedal is the hihat pedal CC value in effect at the hit, for every voice.
struct DrumHit
{
    int64_t time{0};
    PlayedVoice voice{PlayedVoice::Kick};
    uint8_t velocity{0};
    uint8_t pedal{0};
};

/**
 * Turns raw MIDI from the drum module into drum hits using a DrumMap: note-ons of mapped pads
 * become hits, the hihat stick note is classified by HihatClassifier, the pedal CC feeds it.
 * Note-offs, velocity-0 note-ons, aftertouch and every other message are ignored. Hihat stick
 * hits are emitted with a delay of up to the grace window, so hits can arrive out of time order
 * by that much; call advance() regularly.
 */
class DrumMidiDecoder
{
  public:
    void setMap(const DrumMap& map, const float sampleRate) noexcept
    {
        m_map = map;
        m_hihat.configure(map.hihat, sampleRate);
    }

    template <typename Emit>
    void process(const std::span<const uint8_t> bytes, const int64_t time, Emit&& emit)
    {
        if (bytes.size() < 3)
        {
            return;
        }
        const auto type = static_cast<uint8_t>(bytes[0] & 0xF0);
        const auto channel = static_cast<uint8_t>((bytes[0] & 0x0F) + 1);
        if (!m_map.acceptsChannel(channel))
        {
            return;
        }
        if (type == 0x90 && bytes[2] > 0)
        {
            noteOn(bytes[1], bytes[2], time, emit);
        }
        else if (type == 0xB0 && bytes[1] == m_map.hihat.pedalCc)
        {
            m_hihat.onPedal(bytes[2], time, hihatEmitter(emit));
        }
    }

    template <typename Emit>
    void advance(const int64_t now, Emit&& emit)
    {
        m_hihat.advance(now, hihatEmitter(emit));
    }

  private:
    [[nodiscard]] static constexpr PlayedVoice voiceForPad(const Pad pad) noexcept
    {
        switch (pad)
        {
            case Pad::Kick:
                return PlayedVoice::Kick;
            case Pad::Snare:
                return PlayedVoice::Snare;
            case Pad::Hihat:
                return PlayedVoice::HihatClosed;
            case Pad::HihatPedal:
                return PlayedVoice::HihatPedal;
            case Pad::Crash:
                return PlayedVoice::Crash;
            case Pad::TomHigh:
                return PlayedVoice::TomHigh;
            case Pad::TomLow:
                return PlayedVoice::TomLow;
            case Pad::TomFloor:
                return PlayedVoice::TomFloor;
        }
        return PlayedVoice::Kick;
    }

    [[nodiscard]] static constexpr PlayedVoice voiceForArticulation(const HihatArticulation articulation) noexcept
    {
        switch (articulation)
        {
            case HihatArticulation::Closed:
                return PlayedVoice::HihatClosed;
            case HihatArticulation::HalfOpen:
                return PlayedVoice::HihatHalfOpen;
            case HihatArticulation::Open:
                return PlayedVoice::HihatOpen;
        }
        return PlayedVoice::HihatClosed;
    }

    template <typename Emit>
    [[nodiscard]] static auto hihatEmitter(Emit& emit)
    {
        return [&emit](const ClassifiedHihat& hit)
        { emit(DrumHit{hit.time, voiceForArticulation(hit.articulation), hit.velocity, hit.pedal}); };
    }

    template <typename Emit>
    void noteOn(const uint8_t note, const uint8_t velocity, const int64_t time, Emit& emit)
    {
        const auto pad = m_map.padForNote(note);
        if (!pad)
        {
            return;
        }
        if (*pad == Pad::Hihat)
        {
            m_hihat.onStick(velocity, time, hihatEmitter(emit));
            return;
        }
        emit(DrumHit{time, voiceForPad(*pad), velocity, m_hihat.pedal()});
    }

    DrumMap m_map{};
    HihatClassifier m_hihat;
};

}
