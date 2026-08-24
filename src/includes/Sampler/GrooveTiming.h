#pragma once

#include <cmath>
#include <cstdint>
#include <vector>

#include "Sampler/MidiFile.h"

namespace AbacDsp
{

/// @ingroup sampler
/// @brief Grid resolution a groove is analyzed against.
enum class GridResolution : uint8_t
{
    Eighth,
    Sixteenth,
    ThirtySecond,
};

/// @ingroup sampler
/// @brief Whether a grid position lands on a quarter-note (beat) boundary.
enum class GridStrength : uint8_t
{
    Strong,
    Weak,
};

[[nodiscard]] constexpr uint32_t stepsPerBeat(const GridResolution resolution) noexcept
{
    switch (resolution)
    {
        case GridResolution::Eighth:
            return 2;
        case GridResolution::ThirtySecond:
            return 8;
        case GridResolution::Sixteenth:
        default:
            return 4;
    }
}

[[nodiscard]] constexpr uint32_t gridStepTicks(const uint16_t ticksPerQuarterNote,
                                               const GridResolution resolution) noexcept
{
    return ticksPerQuarterNote / stepsPerBeat(resolution);
}

// Rounds tick to the nearest grid step, as an absolute tick position.
[[nodiscard]] inline uint32_t inferredGridTick(const uint32_t tick, const uint32_t stepTicks) noexcept
{
    if (stepTicks == 0)
    {
        return tick;
    }
    return static_cast<uint32_t>(std::lround(static_cast<double>(tick) / static_cast<double>(stepTicks))) * stepTicks;
}

// Ticks per bar for the time-signature segment tick falls in ("N/M" -> N beats of
// (4/M) quarter notes each). timeSignatures may be empty (defaults to 4/4).
[[nodiscard]] inline uint32_t barLengthTicks(const std::vector<MidiTimeSignatureEvent>& timeSignatures,
                                             const uint16_t ticksPerQuarterNote, const uint32_t tick) noexcept
{
    MidiTimeSignatureEvent active{0, 4, 2};
    for (const auto& ts : timeSignatures)
    {
        if (ts.tick > tick)
        {
            break;
        }
        active = ts;
    }
    const double beatTicks =
        (4.0 * static_cast<double>(ticksPerQuarterNote)) / static_cast<double>(1u << active.denominatorPower);
    return static_cast<uint32_t>(std::lround(static_cast<double>(active.numerator) * beatTicks));
}

// 0-based index of the bar tick falls in, counting whole bars across every
// time-signature segment that starts at or before tick.
[[nodiscard]] inline uint32_t barIndexForTick(const std::vector<MidiTimeSignatureEvent>& timeSignatures,
                                              const uint16_t ticksPerQuarterNote, const uint32_t tick) noexcept
{
    if (timeSignatures.empty())
    {
        const uint32_t barTicks = barLengthTicks(timeSignatures, ticksPerQuarterNote, 0);
        return barTicks == 0 ? 0 : tick / barTicks;
    }
    uint32_t barIndex = 0;
    for (size_t i = 0; i < timeSignatures.size(); ++i)
    {
        const uint32_t segStart = timeSignatures[i].tick;
        if (segStart > tick)
        {
            break;
        }
        const uint32_t barTicks = barLengthTicks(timeSignatures, ticksPerQuarterNote, segStart);
        if (barTicks == 0)
        {
            continue;
        }
        const bool isLastSegment = i + 1 == timeSignatures.size();
        const uint32_t segEnd = isLastSegment ? tick + 1 : timeSignatures[i + 1].tick;
        if (isLastSegment || tick < segEnd)
        {
            return barIndex + (tick - segStart) / barTicks;
        }
        barIndex += (segEnd - segStart) / barTicks;
    }
    return barIndex;
}

// Absolute tick of the start of the bar containing tick.
[[nodiscard]] inline uint32_t barStartTick(const std::vector<MidiTimeSignatureEvent>& timeSignatures,
                                           const uint16_t ticksPerQuarterNote, const uint32_t tick) noexcept
{
    uint32_t segStart = 0;
    for (const auto& ts : timeSignatures)
    {
        if (ts.tick > tick)
        {
            break;
        }
        segStart = ts.tick;
    }
    const uint32_t barTicks = barLengthTicks(timeSignatures, ticksPerQuarterNote, tick);
    if (barTicks == 0)
    {
        return segStart;
    }
    const uint32_t tickInSegment = tick - segStart;
    return segStart + (tickInSegment / barTicks) * barTicks;
}

// Grid-step index of tick within its own bar (0 = downbeat), rounded to the
// nearest step - this is the recurring key voice-role bias rules key off.
[[nodiscard]] inline uint32_t patternPositionInBar(const std::vector<MidiTimeSignatureEvent>& timeSignatures,
                                                   const uint16_t ticksPerQuarterNote, const uint32_t tick,
                                                   const GridResolution resolution) noexcept
{
    const uint32_t barStart = barStartTick(timeSignatures, ticksPerQuarterNote, tick);
    const uint32_t stepTicks = gridStepTicks(ticksPerQuarterNote, resolution);
    if (stepTicks == 0)
    {
        return 0;
    }
    return static_cast<uint32_t>(std::lround(static_cast<double>(tick - barStart) / static_cast<double>(stepTicks)));
}

[[nodiscard]] constexpr GridStrength gridStrength(const uint32_t patternPosition,
                                                  const GridResolution resolution) noexcept
{
    return (patternPosition % stepsPerBeat(resolution) == 0) ? GridStrength::Strong : GridStrength::Weak;
}

// A strong position that isn't the bar's own downbeat - a 4/4-shaped
// generalization of "backbeat" (see groove-aware-cleanup.md's Risks section).
[[nodiscard]] constexpr bool isBackbeatPosition(const uint32_t patternPosition,
                                                const GridResolution resolution) noexcept
{
    return patternPosition != 0 && gridStrength(patternPosition, resolution) == GridStrength::Strong;
}

}
