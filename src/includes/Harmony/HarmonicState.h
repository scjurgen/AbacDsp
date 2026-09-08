#pragma once

#include <optional>
#include <string_view>

#include "Harmony/HarmonicPreferences.h"
#include "Harmony/PitchClassSet.h"

namespace AbacDsp
{

/// @ingroup harmony
/// @brief Which of the spec's named regions a HarmonicState belongs to - purely descriptive,
/// used to browse/group the palette rather than to drive scoring.
enum class PaletteRegion
{
    Home,
    MajorLight,
    ModalWarmth,
    OpenSuspended,
    ChromaticWeather
};

/**
 * @ingroup harmony
 * @brief One harmonic place: a concrete upper-structure voicing (home-relative, home = 0; the
 * pedal, if any, is handled separately - see AmbientPadImpl's pedal-channel handling) plus how
 * it scores against each state-intrinsic wish axis, precomputed by hand so scoring a candidate
 * is a handful of float comparisons rather than live interval analysis.
 */
struct HarmonicState
{
    std::string_view name; ///< e.g. "1m(add9)" (Nashville-number style) - diagnostics/logging only
    PaletteRegion region{PaletteRegion::Home};
    Voicing voicing;
    float luminosity{0.5f}; ///< 0 dark .. 1 bright
    float minorColor{0.5f}; ///< 0 major-leaning .. 1 minor-leaning
    float density{0.5f};    ///< 0 sparse .. 1 dense
    float ambiguity{0.5f};  ///< 0 clear/functional .. 1 ambiguous/open
    float tension{0.5f};    ///< 0 at rest .. 1 tense
};

/// @brief This state's own tag for a WishKind, or nullopt for the two relational axes
/// (Closeness, Mobility) that only make sense between two states, not on one alone.
[[nodiscard]] constexpr std::optional<float> stateTag(const HarmonicState& state, const WishKind kind) noexcept
{
    switch (kind)
    {
        case WishKind::Luminosity:
            return state.luminosity;
        case WishKind::MinorColor:
            return state.minorColor;
        case WishKind::Density:
            return state.density;
        case WishKind::Ambiguity:
            return state.ambiguity;
        case WishKind::Tension:
            return state.tension;
        case WishKind::Closeness:
        case WishKind::Mobility:
        default:
            return std::nullopt;
    }
}

/// @brief Shifts every note in state's voicing by homeOffsetSemitones; name/region/tags are
/// unaffected - transposition only moves where the voicing actually sits in pitch.
[[nodiscard]] constexpr HarmonicState transposeState(const HarmonicState& state, const int homeOffsetSemitones) noexcept
{
    HarmonicState transposed = state;
    Voicing voicing{};
    for (const auto note : state.voicing.notes())
    {
        voicing.add(note + homeOffsetSemitones);
    }
    transposed.voicing = voicing;
    return transposed;
}

}
