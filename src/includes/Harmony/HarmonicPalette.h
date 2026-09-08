#pragma once

#include <array>
#include <cstddef>
#include <span>

#include "Harmony/HarmonicState.h"

namespace AbacDsp
{

/// @brief Builds one palette entry - a small helper so defaultPalette()'s hand-authored table
/// reads as one line per state rather than a full aggregate-init block each.
[[nodiscard]] constexpr HarmonicState makeHarmonicState(const std::string_view name, const PaletteRegion region,
                                                        const std::span<const int> semitones, const float luminosity,
                                                        const float minorColor, const float density,
                                                        const float ambiguity, const float tension) noexcept
{
    return HarmonicState{.name = name,
                         .region = region,
                         .voicing = Voicing::fromSemitones(semitones),
                         .luminosity = luminosity,
                         .minorColor = minorColor,
                         .density = density,
                         .ambiguity = ambiguity,
                         .tension = tension};
}

inline constexpr size_t kDefaultPaletteSize{15};

/// @ingroup harmony
/// @brief The hand-curated palette of harmonic states around an E-centred home, authored
/// home-relative (home = 0); grouped as the spec groups them. See transposedPalette().
[[nodiscard]] constexpr std::array<HarmonicState, kDefaultPaletteSize> defaultPalette() noexcept
{
    return {{
        // -- Home: minor tonic (1) --
        makeHarmonicState("1m(add9)", PaletteRegion::Home, std::to_array<int>({0, 2, 3, 7}), 0.35f, 0.85f, 0.35f, 0.2f,
                          0.15f),
        makeHarmonicState("1m9", PaletteRegion::Home, std::to_array<int>({-2, 0, 2, 3, 7}), 0.35f, 0.85f, 0.5f, 0.3f,
                          0.2f),
        makeHarmonicState("1m11", PaletteRegion::Home, std::to_array<int>({-2, 0, 2, 3, 5, 7}), 0.3f, 0.8f, 0.6f, 0.4f,
                          0.25f),
        // -- Major-light: major tonic (1) --
        makeHarmonicState("1maj7", PaletteRegion::MajorLight, std::to_array<int>({-1, 0, 4, 7}), 0.75f, 0.1f, 0.35f,
                          0.2f, 0.2f),
        makeHarmonicState("16/9", PaletteRegion::MajorLight, std::to_array<int>({0, 2, 4, 7, 9}), 0.8f, 0.15f, 0.5f,
                          0.3f, 0.15f),
        makeHarmonicState("1maj9", PaletteRegion::MajorLight, std::to_array<int>({0, 2, 4, 7, 11}), 0.8f, 0.1f, 0.55f,
                          0.25f, 0.2f),
        // -- Modal warmth and shared-tone --
        makeHarmonicState("b6maj7", PaletteRegion::ModalWarmth, std::to_array<int>({-4, 0, 3, 7}), 0.55f, 0.4f, 0.3f,
                          0.5f, 0.3f),
        makeHarmonicState("b36", PaletteRegion::ModalWarmth, std::to_array<int>({-2, 0, 3, 7}), 0.6f, 0.45f, 0.3f, 0.5f,
                          0.25f),
        makeHarmonicState("4m9", PaletteRegion::ModalWarmth, std::to_array<int>({-7, -4, 0, 3, 7}), 0.45f, 0.6f, 0.5f,
                          0.55f, 0.3f),
        // -- Open and suspended --
        makeHarmonicState("b7maj9", PaletteRegion::OpenSuspended, std::to_array<int>({-2, 0, 2, 5, 9}), 0.65f, 0.3f,
                          0.5f, 0.55f, 0.3f),
        makeHarmonicState("4(add9)", PaletteRegion::OpenSuspended, std::to_array<int>({-7, -3, 0, 7}), 0.6f, 0.35f,
                          0.35f, 0.5f, 0.25f),
        makeHarmonicState("5sus4", PaletteRegion::OpenSuspended, std::to_array<int>({-5, 0, 14}), 0.55f, 0.5f, 0.15f,
                          0.65f, 0.3f),
        // -- Brief chromatic weather --
        makeHarmonicState("b2maj", PaletteRegion::ChromaticWeather, std::to_array<int>({-4, 1, 5}), 0.45f, 0.4f, 0.2f,
                          0.75f, 0.7f),
        makeHarmonicState("6maj", PaletteRegion::ChromaticWeather, std::to_array<int>({-3, 1, 4}), 0.5f, 0.3f, 0.2f,
                          0.8f, 0.75f),
        makeHarmonicState("b5maj7", PaletteRegion::ChromaticWeather, std::to_array<int>({-2, 1, 5, 6}), 0.55f, 0.35f,
                          0.35f, 0.85f, 0.8f),
    }};
}

/// @brief The default palette transposed so home sits homeOffsetSemitones away from where it
/// was authored (0). Any int works, not just 0..11 - Voicing offsets are register-aware.
[[nodiscard]] constexpr std::array<HarmonicState, kDefaultPaletteSize> transposedPalette(
    const int homeOffsetSemitones) noexcept
{
    auto palette = defaultPalette();
    for (auto& state : palette)
    {
        state = transposeState(state, homeOffsetSemitones);
    }
    return palette;
}

}
