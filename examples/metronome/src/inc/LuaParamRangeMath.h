#pragma once

#include <algorithm>
#include <cmath>

// Fixed, always-copied shared component (CPP_SOURCE_FILES_FIXED) - must stay JUCE-free so
// the audio-thread engine can use it too. Mirrors juce::NormalisableRange's own
// non-symmetric-skew formulas (see juce_NormalisableRange.h's convertFrom0to1/convertTo0to1)
// so the engine and the UI widget agree on what a given raw value displays as.

// Raw normalized [0,1] parameter value -> display value (script/knob-facing).
[[nodiscard]] inline float luaParamNormalizedToDisplay(const float rangeMin, const float rangeMax,
                                                       const float rangeStep, const float rangeSkew,
                                                       float normalized) noexcept
{
    normalized = std::clamp(normalized, 0.f, 1.f);
    if (rangeSkew > 0.f && rangeSkew != 1.f && normalized > 0.f)
    {
        normalized = std::exp(std::log(normalized) / rangeSkew);
    }
    float display = rangeMin + (rangeMax - rangeMin) * normalized;
    if (rangeStep > 0.f)
    {
        display = rangeMin + std::round((display - rangeMin) / rangeStep) * rangeStep;
    }
    return display;
}

// Display value -> raw normalized [0,1] parameter value (the inverse conversion).
[[nodiscard]] inline float luaParamDisplayToNormalized(const float rangeMin, const float rangeMax,
                                                       const float rangeSkew, const float display) noexcept
{
    float proportion = std::clamp((display - rangeMin) / (rangeMax - rangeMin), 0.f, 1.f);
    if (rangeSkew > 0.f && rangeSkew != 1.f)
    {
        proportion = std::pow(proportion, rangeSkew);
    }
    return proportion;
}
