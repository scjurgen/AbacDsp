#pragma once

#include <array>
#include <atomic>
#include <bit>
#include <cstddef>
#include <cstdint>

#include "DrumMidiDecoder.h"
#include "DrumPattern.h"

namespace DrumDisplay
{

// A played hit placed in the pattern: revolution counts whole passes through the pattern,
// patternBeats is the position within that pass, 0 <= patternBeats < pattern length.
struct DisplayHit
{
    float patternBeats{0.f};
    uint32_t revolution{0};
    DrumInput::PlayedVoice voice{DrumInput::PlayedVoice::Kick};
    uint8_t velocity{0};
};

// Single-producer, single-consumer ring the audio thread writes and the UI drains. A reader
// more than kCapacity behind (or holding the index of an earlier ring) skips everything.
class DisplayHitRing
{
  public:
    static constexpr uint32_t kCapacity{256};

    void push(const DisplayHit& hit) noexcept
    {
        const uint32_t written = m_written.load(std::memory_order_relaxed);
        m_hits[written % kCapacity] = hit;
        m_written.store(written + 1, std::memory_order_release);
    }

    template <typename Visit>
    void read(uint32_t& readIndex, Visit&& visit) const
    {
        const uint32_t written = m_written.load(std::memory_order_acquire);
        if (written - readIndex > kCapacity)
        {
            readIndex = written;
        }
        for (; readIndex != written; ++readIndex)
        {
            visit(m_hits[readIndex % kCapacity]);
        }
    }

  private:
    std::array<DisplayHit, kCapacity> m_hits{};
    std::atomic<uint32_t> m_written{0};
};

// Where the playhead is: phase 0..1 through the pattern, which revolution, and whether the
// click is running (the sequencer keeps counting silently while stopped).
struct PatternPosition
{
    float phase{0.f};
    uint32_t revolution{0};
    bool running{false};

    [[nodiscard]] uint64_t pack() const noexcept
    {
        return uint64_t{std::bit_cast<uint32_t>(phase)} | (uint64_t{revolution & 0x7FFFFFFFu} << 32) |
               (running ? uint64_t{1} << 63 : 0);
    }

    [[nodiscard]] static PatternPosition unpack(const uint64_t packed) noexcept
    {
        return {std::bit_cast<float>(static_cast<uint32_t>(packed & 0xFFFFFFFFu)),
                static_cast<uint32_t>((packed >> 32) & 0x7FFFFFFFu), (packed >> 63) != 0};
    }
};

// The target pattern as the UI shows it; version changes whenever pattern or bar length do.
struct DisplayPattern
{
    DrumPattern::HitPattern pattern{};
    size_t beatsPerBar{4};
    uint64_t version{0};
};

}
