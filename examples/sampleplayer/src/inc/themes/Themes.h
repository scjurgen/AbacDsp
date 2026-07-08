#pragma once

#include <array>

#include "ThemeClassic.h"
#include "ThemeGrayscale.h"
#include "ThemeHeat.h"
#include "ThemeInferno.h"
#include "ThemeInk.h"
#include "ThemeTeal.h"
#include "ThemeViridis.h"

namespace Themes
{
// Order must stay stable: the selected theme is persisted as this index (AppSettings).
enum class Theme
{
    Classic,
    Viridis,
    Inferno,
    Grayscale,
    Heat,
    Ink,
    Teal,
};

inline constexpr auto kThemes = std::to_array<ThemeDefinition>({
    kClassic,
    kViridis,
    kInferno,
    kGrayscale,
    kHeat,
    kInk,
    kTeal,
});

[[nodiscard]] constexpr const ThemeDefinition& definition(const Theme theme)
{
    return kThemes[static_cast<size_t>(theme)];
}
}
