#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

#include "DrumMidiDecoder.h"
#include "DrumPattern.h"

namespace DrumAnalysis
{

// One played hit of a take; takeBeats counts from the downbeat capture started on and is
// negative for hits played in the last count-in bar.
struct CapturedHit
{
    float takeBeats{0.f};
    DrumInput::PlayedVoice voice{DrumInput::PlayedVoice::Kick};
    uint8_t velocity{0};
    uint8_t pedal{0};
};

// Fixed-capacity, allocation-free collector; once full, a long take stops recording.
class CapturedHitCollector
{
  public:
    static constexpr size_t kCapacity{8192};

    void reset() noexcept
    {
        m_count = 0;
    }

    void push(const CapturedHit& hit) noexcept
    {
        if (m_count < kCapacity)
        {
            m_hits[m_count++] = hit;
        }
    }

    [[nodiscard]] std::span<const CapturedHit> hits() const noexcept
    {
        return {m_hits.data(), m_count};
    }

  private:
    std::array<CapturedHit, kCapacity> m_hits{};
    size_t m_count{0};
};

// What a take was played against, frozen when capture started.
struct TakeSetup
{
    DrumPattern::HitPattern pattern{};
    size_t beatsPerBar{4};
    size_t capturedBars{0};
    float bpm{120.f};
};

// Half-open hihat is scored as closed; the pedal chick is not scored at all.
[[nodiscard]] constexpr std::optional<DrumPattern::Instrument> scoredInstrument(
    const DrumInput::PlayedVoice voice) noexcept
{
    using DrumInput::PlayedVoice;
    using DrumPattern::Instrument;
    switch (voice)
    {
        case PlayedVoice::Kick:
            return Instrument::Kick;
        case PlayedVoice::Snare:
            return Instrument::Snare;
        case PlayedVoice::HihatClosed:
        case PlayedVoice::HihatHalfOpen:
            return Instrument::HihatClosed;
        case PlayedVoice::HihatOpen:
            return Instrument::HihatOpen;
        case PlayedVoice::HihatPedal:
            return std::nullopt;
        case PlayedVoice::Crash:
            return Instrument::Crash;
        case PlayedVoice::TomHigh:
            return Instrument::TomHigh;
        case PlayedVoice::TomLow:
            return Instrument::TomLow;
        case PlayedVoice::TomFloor:
            return Instrument::TomFloor;
    }
    return std::nullopt;
}

// Closed and open hihat are one limb: matched against each other, a mismatch is articulation.
[[nodiscard]] constexpr DrumPattern::Instrument limbOf(const DrumPattern::Instrument instrument) noexcept
{
    return instrument == DrumPattern::Instrument::HihatOpen ? DrumPattern::Instrument::HihatClosed : instrument;
}

// deviationMs > 0 means early, < 0 late (the same sign as the Metronome report).
struct MatchedHit
{
    size_t patternHit{0};
    size_t cycle{0};
    float deviationMs{0.f};
    uint8_t velocity{0};
    DrumInput::PlayedVoice voice{DrumInput::PlayedVoice::Kick};
    bool wrongArticulation{false};
};

struct MissedHit
{
    size_t patternHit{0};
    size_t cycle{0};
};

struct ExtraHit
{
    float takeBeats{0.f};
    DrumPattern::Instrument instrument{DrumPattern::Instrument::Kick};
};

struct MatchResult
{
    std::vector<MatchedHit> matched;
    std::vector<MissedHit> missed;
    std::vector<ExtraHit> extras;
    size_t expectedCount{0};
    size_t pedalHits{0};
};

inline constexpr float kMaxMatchWindowMs{70.f};

namespace detail
{
struct Expected
{
    double takeBeats{0.0};
    size_t patternHit{0};
    size_t cycle{0};
    double windowBeats{0.0};
    std::optional<size_t> playedIndex;
};

struct Candidate
{
    double distanceBeats{0.0};
    size_t played{0};
    size_t expected{0};
};

[[nodiscard]] inline std::vector<Expected> expectedForLimb(const TakeSetup& setup, const DrumPattern::Instrument limb,
                                                           const double maxWindowBeats)
{
    const auto& pattern = setup.pattern;
    const double patternBeats = static_cast<double>(pattern.patternBars * setup.beatsPerBar);
    const double capturedBeats = static_cast<double>(setup.capturedBars * setup.beatsPerBar);
    std::vector<Expected> expected;
    for (size_t cycle = 0; patternBeats > 0.0 && static_cast<double>(cycle) * patternBeats < capturedBeats; ++cycle)
    {
        for (size_t i = 0; i < pattern.hitCount; ++i)
        {
            const double at = static_cast<double>(cycle) * patternBeats + pattern.hits[i].positionBeats;
            if (limbOf(pattern.hits[i].instrument) == limb && at < capturedBeats)
            {
                expected.push_back({at, i, cycle, maxWindowBeats, std::nullopt});
            }
        }
    }
    std::ranges::sort(expected, {}, &Expected::takeBeats);
    for (size_t i = 0; i < expected.size(); ++i)
    {
        if (i > 0)
        {
            expected[i].windowBeats =
                std::min(expected[i].windowBeats, 0.5 * (expected[i].takeBeats - expected[i - 1].takeBeats));
        }
        if (i + 1 < expected.size())
        {
            expected[i].windowBeats =
                std::min(expected[i].windowBeats, 0.5 * (expected[i + 1].takeBeats - expected[i].takeBeats));
        }
    }
    return expected;
}

[[nodiscard]] inline std::vector<Candidate> candidatesFor(const std::span<const Expected> expected,
                                                          const std::span<const CapturedHit> played,
                                                          const std::span<const size_t> playedIndices,
                                                          const double maxWindowBeats)
{
    std::vector<Candidate> candidates;
    for (const size_t p : playedIndices)
    {
        const double at = played[p].takeBeats;
        const auto first = std::ranges::lower_bound(expected, at - maxWindowBeats, {}, &Expected::takeBeats);
        for (auto e = first; e != expected.end() && e->takeBeats <= at + maxWindowBeats; ++e)
        {
            const double distance = e->takeBeats - at;
            if (std::abs(distance) <= e->windowBeats)
            {
                candidates.push_back({distance, p, static_cast<size_t>(std::distance(expected.begin(), e))});
            }
        }
    }
    std::ranges::sort(candidates, {}, [](const Candidate& c) { return std::abs(c.distanceBeats); });
    return candidates;
}
}

// Pairs each played hit with at most one expected hit of its limb, closest pairs first, within
// min(kMaxMatchWindowMs, half the gap to the neighbouring expected hits). Unpaired played hits
// outside the captured bars (count-in, unfinished last bar) are dropped rather than extra.
[[nodiscard]] inline MatchResult matchTake(const TakeSetup& setup, const std::span<const CapturedHit> played)
{
    MatchResult result;
    if (!setup.pattern.active || setup.bpm <= 0.f)
    {
        return result;
    }
    const double msPerBeat = 60000.0 / static_cast<double>(setup.bpm);
    const double maxWindowBeats = static_cast<double>(kMaxMatchWindowMs) / msPerBeat;
    const double capturedBeats = static_cast<double>(setup.capturedBars * setup.beatsPerBar);

    for (size_t limbIndex = 0; limbIndex < DrumPattern::kInstrumentNames.size(); ++limbIndex)
    {
        const auto limb = static_cast<DrumPattern::Instrument>(limbIndex);
        if (limbOf(limb) != limb)
        {
            continue;
        }
        auto expected = detail::expectedForLimb(setup, limb, maxWindowBeats);
        result.expectedCount += expected.size();

        std::vector<size_t> playedIndices;
        for (size_t p = 0; p < played.size(); ++p)
        {
            const auto instrument = scoredInstrument(played[p].voice);
            if (instrument && limbOf(*instrument) == limb)
            {
                playedIndices.push_back(p);
            }
        }
        std::vector<bool> playedTaken(played.size(), false);
        for (const auto& c : detail::candidatesFor(expected, played, playedIndices, maxWindowBeats))
        {
            if (!playedTaken[c.played] && !expected[c.expected].playedIndex)
            {
                playedTaken[c.played] = true;
                expected[c.expected].playedIndex = c.played;
            }
        }

        for (const auto& e : expected)
        {
            if (!e.playedIndex)
            {
                result.missed.push_back({e.patternHit, e.cycle});
                continue;
            }
            const auto& hit = played[*e.playedIndex];
            const auto expectedInstrument = setup.pattern.hits[e.patternHit].instrument;
            const auto deviationMs = static_cast<float>((e.takeBeats - hit.takeBeats) * msPerBeat);
            result.matched.push_back({e.patternHit, e.cycle, deviationMs, hit.velocity, hit.voice,
                                      scoredInstrument(hit.voice) != expectedInstrument});
        }
        for (const size_t p : playedIndices)
        {
            const double at = played[p].takeBeats;
            if (!playedTaken[p] && at >= 0.0 && at < capturedBeats)
            {
                result.extras.push_back({played[p].takeBeats, *scoredInstrument(played[p].voice)});
            }
        }
    }
    result.pedalHits =
        static_cast<size_t>(std::ranges::count_if(played,
                                                  [&](const CapturedHit& hit)
                                                  {
                                                      return hit.voice == DrumInput::PlayedVoice::HihatPedal &&
                                                             hit.takeBeats >= 0.f && hit.takeBeats < capturedBeats;
                                                  }));
    return result;
}

}
