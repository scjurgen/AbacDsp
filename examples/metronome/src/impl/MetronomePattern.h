#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <string_view>

namespace MetronomePattern
{

enum class Instrument : uint8_t
{
    Kick,
    Snare,
    Rimshot,
    Sidestick,
    Hihat,
    HihatOpen,
    HihatGhost,
    Wood,
    Clap,
    Shaker,
    Tamb,
    Tom1,
    Tom2,
    Tom3,
    TomLow,
    Timbale1,
    Timbale2,
    TimbaleDamp,
    Ride,
    Crash,
    ClickLow,
    ClickHigh,
};

struct InstrumentInfo
{
    std::string_view luaName;
    std::string_view sampleCode;
};

// Indexed by Instrument; the Lua constant of each entry is its index.
// clang-format off
inline constexpr auto kInstruments = std::to_array<InstrumentInfo>({
    {"Kick",        "bd"},
    {"Snare",       "sd"},
    {"Rimshot",     "rs"},
    {"Sidestick",   "sstick"},
    {"Hihat",       "hh"},
    {"HihatOpen",   "hhopen"},
    {"HihatGhost",  "hhghost"},
    {"Wood",        "wood"},
    {"Clap",        "clap"},
    {"Shaker",      "shaker"},
    {"Tamb",        "tamb"},
    {"Tom1",        "tom1"},
    {"Tom2",        "tom2"},
    {"Tom3",        "tom3"},
    {"TomLow",      "tomlo"},
    {"Timbale1",    "timb1"},
    {"Timbale2",    "timb2"},
    {"TimbaleDamp", "timbdmp"},
    {"Ride",        "ride"},
    {"Crash",       "crash"},
    {"ClickLow",    "clicklow"},
    {"ClickHigh",   "clickhigh"},
});
// clang-format on
static_assert(kInstruments.size() == static_cast<size_t>(Instrument::ClickHigh) + 1);

[[nodiscard]] constexpr std::string_view sampleCode(const Instrument instrument) noexcept
{
    return kInstruments[static_cast<size_t>(instrument)].sampleCode;
}

[[nodiscard]] constexpr std::optional<Instrument> instrumentFromIndex(const long long index) noexcept
{
    if (index < 0 || index >= static_cast<long long>(kInstruments.size()))
    {
        return std::nullopt;
    }
    return static_cast<Instrument>(index);
}

inline constexpr size_t kMaxHits{256};
inline constexpr size_t kMaxAnalysisPositions{128};
inline constexpr size_t kMaxBeatsPerBar{16};
inline constexpr float kMinLevelDb{-96.f};
inline constexpr float kMaxLevelDb{12.f};

// One drum hit: where in the bar (beats from the bar start), what, and how loud (linear).
struct Hit
{
    float positionBeats{0.f};
    Instrument instrument{Instrument::Kick};
    float gain{1.f};
};

// The scripted pattern, hits sorted by position. Inactive means the script never defined one.
// beatsPerBar 0 means the script left the bar length to the Preset.
struct HitPattern
{
    std::array<Hit, kMaxHits> hits{};
    size_t hitCount{0};
    size_t beatsPerBar{0};
    bool active{false};
};

// Bar positions (in beats, sorted) that analysed onsets are measured against.
struct AnalysisPositions
{
    std::array<float, kMaxAnalysisPositions> positions{};
    size_t count{0};
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

// Walks a HitPattern in step with a beat sequencer and reports each hit on its sample.
// Any jump in beat, position or tempo re-seeks: hits already behind the new position
// are skipped, never replayed in a burst.
class PatternScheduler
{
  public:
    void setPattern(const HitPattern* pattern) noexcept
    {
        m_pattern = pattern;
        m_beat = kNoBeat;
    }

    [[nodiscard]] bool active() const noexcept
    {
        return m_pattern != nullptr && m_pattern->active;
    }

    template <typename Trigger>
    void step(const size_t beatIndexInBar, const size_t beatSamplePos, const size_t samplesPerBeat,
              Trigger&& trigger) noexcept
    {
        if (!active())
        {
            return;
        }
        if (beatIndexInBar != m_beat || beatSamplePos != m_expectedPos || samplesPerBeat != m_samplesPerBeat)
        {
            seek(beatIndexInBar, beatSamplePos, samplesPerBeat);
        }
        while (m_cursor < m_beatEnd && m_nextOffset <= beatSamplePos)
        {
            trigger(m_pattern->hits[m_cursor]);
            ++m_cursor;
            refreshNextOffset();
        }
        m_expectedPos = beatSamplePos + 1;
    }

  private:
    static constexpr size_t kNoBeat{std::numeric_limits<size_t>::max()};

    [[nodiscard]] size_t offsetInBeat(const Hit& hit) const noexcept
    {
        const float fraction = hit.positionBeats - std::floor(hit.positionBeats);
        const auto offset = static_cast<size_t>(std::lround(fraction * static_cast<float>(m_samplesPerBeat)));
        return std::min(offset, m_samplesPerBeat - 1);
    }

    void refreshNextOffset() noexcept
    {
        m_nextOffset = m_cursor < m_beatEnd ? offsetInBeat(m_pattern->hits[m_cursor]) : kNoBeat;
    }

    [[nodiscard]] size_t firstHitAtOrAfter(const float positionBeats) const noexcept
    {
        const std::span<const Hit> hits{m_pattern->hits.data(), m_pattern->hitCount};
        return static_cast<size_t>(std::ranges::lower_bound(hits, positionBeats, {}, &Hit::positionBeats) -
                                   hits.begin());
    }

    void seek(const size_t beatIndexInBar, const size_t beatSamplePos, const size_t samplesPerBeat) noexcept
    {
        m_beat = beatIndexInBar;
        m_samplesPerBeat = std::max<size_t>(samplesPerBeat, 1);
        m_cursor = firstHitAtOrAfter(static_cast<float>(beatIndexInBar));
        m_beatEnd = firstHitAtOrAfter(static_cast<float>(beatIndexInBar + 1));
        while (m_cursor < m_beatEnd && offsetInBeat(m_pattern->hits[m_cursor]) < beatSamplePos)
        {
            ++m_cursor;
        }
        refreshNextOffset();
    }

    const HitPattern* m_pattern{nullptr};
    size_t m_beat{kNoBeat};
    size_t m_expectedPos{0};
    size_t m_samplesPerBeat{1};
    size_t m_cursor{0};
    size_t m_beatEnd{0};
    size_t m_nextOffset{kNoBeat};
};

}
