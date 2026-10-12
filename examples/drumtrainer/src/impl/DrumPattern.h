#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>

namespace DrumPattern
{

enum class Instrument : uint8_t
{
    Kick,
    Snare,
    HihatClosed,
    HihatOpen,
    Crash,
    TomHigh,
    TomLow,
    TomFloor,
};

// Indexed by Instrument; the Lua constant of each entry is its index.
inline constexpr auto kInstrumentNames = std::to_array<std::string_view>(
    {"Kick", "Snare", "HihatClosed", "HihatOpen", "Crash", "TomHigh", "TomLow", "TomFloor"});
static_assert(kInstrumentNames.size() == static_cast<size_t>(Instrument::TomFloor) + 1);

[[nodiscard]] constexpr std::string_view luaName(const Instrument instrument) noexcept
{
    return kInstrumentNames[static_cast<size_t>(instrument)];
}

[[nodiscard]] constexpr std::optional<Instrument> instrumentFromIndex(const long long index) noexcept
{
    if (index < 0 || index >= static_cast<long long>(kInstrumentNames.size()))
    {
        return std::nullopt;
    }
    return static_cast<Instrument>(index);
}

// One drummer cannot play the closed and the open hihat at the same moment.
[[nodiscard]] constexpr bool sharesALimb(const Instrument a, const Instrument b) noexcept
{
    const auto isHihat = [](const Instrument i) { return i == Instrument::HihatClosed || i == Instrument::HihatOpen; };
    return a == b || (isHihat(a) && isHihat(b));
}

inline constexpr size_t kMaxHits{512};
inline constexpr size_t kMaxBeatsPerBar{16};
inline constexpr size_t kMaxPatternBars{8};
inline constexpr size_t kMaxNameBytes{64};
inline constexpr uint8_t kDefaultVelocity{100};
// Hits closer than this (in beats) count as the same position.
inline constexpr float kSamePositionBeats{1e-4f};

// One expected drummer hit: where in the pattern (beats from the pattern start), what, how hard.
struct Hit
{
    float positionBeats{0.f};
    Instrument instrument{Instrument::Kick};
    uint8_t velocity{kDefaultVelocity};
};

// The target pattern, hits sorted by position; it spans patternBars bars. Inactive means the
// script never defined one. beatsPerBar 0 means the script left the bar length to the Preset.
struct HitPattern
{
    std::array<Hit, kMaxHits> hits{};
    size_t hitCount{0};
    size_t beatsPerBar{0};
    size_t patternBars{1};
    bool active{false};
};

// Wait-free single-producer, single-consumer hand-off: the producer always writes a
// slot the consumer cannot be reading, the consumer sees only whole published values.
template <typename T>
class TripleBuffer
{
  public:
    [[nodiscard]] T& backSlot() noexcept
    {
        return m_slots[m_back];
    }

    void publish() noexcept
    {
        m_back = m_shared.exchange(m_back | kFreshBit, std::memory_order_acq_rel) & kIndexMask;
    }

    // The newest published value, or nullptr when nothing new arrived since the last call.
    [[nodiscard]] const T* acquireNewest() noexcept
    {
        if ((m_shared.load(std::memory_order_relaxed) & kFreshBit) == 0)
        {
            return nullptr;
        }
        m_front = m_shared.exchange(m_front, std::memory_order_acq_rel) & kIndexMask;
        return &m_slots[m_front];
    }

  private:
    static constexpr unsigned kFreshBit{4};
    static constexpr unsigned kIndexMask{3};

    std::array<T, 3> m_slots{};
    std::atomic<unsigned> m_shared{2};
    unsigned m_back{0};
    unsigned m_front{1};
};

}
