#pragma once

// StatusBar.h (a generic widget template shared by every generated example) reads
// Constants::Text / Constants::Margins from the per-blueprint generated Constants.h.
// This tool has no blueprint, so it stubs the same fixed defaults every blueprint gets.
namespace Constants
{
namespace Text
{
constexpr float labelHeight = 30.f;
constexpr float labelWidth = 90.f;
constexpr float fontHeight = 16.f;
}

namespace Margins
{
constexpr float small = 2.0f;
constexpr float medium = 4.0f;
constexpr float big = 8.0f;
}

namespace Dial
{
constexpr float minSize = 80.0f;
constexpr float maxSize = 180.0f;
}
}
