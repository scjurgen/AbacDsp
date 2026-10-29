#pragma once

#include <cassert>
#include <cmath>
#include <string_view>

#include "Harmony/ChordName.h"
#include "Numbers/Convert.h"

namespace AbacDsp
{

/// @ingroup analysis
/// @brief A frequency as the nearest equal-tempered note and its deviation from it in cents.
struct PitchReading
{
    float frequency{0.f};
    int midiNote{0};
    float cents{0.f};

    [[nodiscard]] std::string_view name() const noexcept
    {
        return noteName(midiNote);
    }

    // MIDI 60 is C4.
    [[nodiscard]] int octave() const noexcept
    {
        return static_cast<int>(std::floor(static_cast<float>(midiNote) / 12.f)) - 1;
    }
};

/// @ingroup analysis
/// @brief Cents lie in [-50, 50); exactly half a semitone above a note reads -50 to the next one.
[[nodiscard]] inline PitchReading pitchReadingOf(const float frequency, const float referenceA4 = 440.f) noexcept
{
    assert(frequency > 0.f && referenceA4 > 0.f);
    const float note = Convert::frequencyToNote(frequency, referenceA4);
    const float nearest = std::floor(note + 0.5f);
    return {frequency, static_cast<int>(nearest), (note - nearest) * 100.f};
}

}
