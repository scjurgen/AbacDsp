#pragma once

#include <cstdint>
#include <optional>

namespace AbacDsp
{

/// @ingroup generators
/// @brief Playing technique used to excite a plucked-string voice, independent of PluckType's
/// noise-color selection.
enum class ExcitationType : uint8_t
{
    Pluck,
    Strike,
    Mute,
    PalmMute,
    Bow,
    Sympathetic,
    Wind,
    Rub,
};

/**
 * @ingroup generators
 * @brief A scheduled excitation technique, handed to a voice for unattended execution.
 *
 * startMs/endMs are measured from the moment the event becomes active, not from the call
 * that scheduled it. endMs is unused by Pluck/Strike, and repurposed as a fade duration
 * (not a hold-then-restore window) by Mute; see KarplusStrongVoice for the full mapping.
 */
struct ExcitationEvent
{
    float startMs{0.f};
    float endMs{0.f};
    ExcitationType type{ExcitationType::Pluck};
    float strength{1.f};
    std::optional<float> harmonic; // Sympathetic only: n-th harmonic of the string's fundamental
};

}
