#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <juce_graphics/juce_graphics.h>

#include "themes/Themes.h"

class GuiConstants
{
  public:
    using Theme = Themes::Theme;
    using GradientPreset = Themes::Theme; // legacy name, kept for hand-written widgets

    struct Colors
    {
        // Accent ramp sampled from the spectrogram gradient, not the curated UI palette.
        std::array<uint32_t, 10> cols{};
        uint32_t background{};
        uint32_t backgroundComponent{};
        uint32_t backgroundDark{};
        uint32_t backgroundMid{};
        uint32_t backgroundLight{};
        uint32_t gradientDark{};
        uint32_t knobGradientStart{};
        uint32_t knobGradientCenter{};
        uint32_t knobGradientEnd{};
        uint32_t statusOutline{};
        uint32_t labelColour{};
    };

    struct Text
    {
        float labelHeight{30.f};
        float labelWidth{90.f};
        float fontHeight{16.f};
    };

    struct Margins
    {
        float small{2.0f};
        float medium{4.0f};
        float big{8.0f};
    };

    struct InitJuce
    {
        int WindowWidth{1200};
        int WindowHeight{800};
        int TimerHertz{60};
    };

    static constexpr int kLutSize = 256;

    static constexpr float kMeterMinDb = -84.f;
    static constexpr float kMeterMaxDb = 12.f;
    static constexpr float kLevelWarnDb = -12.f;
    static constexpr float kLevelDangerDb = 0.f;
    static constexpr float kCpuWarnFraction = 0.60f;
    static constexpr float kCpuDangerFraction = 0.75f;

    // Function-local static: thread-safe lazy init guaranteed by C++11, and
    // instance() is always valid even if setPreset() is never called (defaults
    // to Theme::Ink). The previous heap-allocated-singleton-with-raw-pointer
    // pattern required setPreset() to run before the first instance() call or
    // it dereferenced a null/dangling pointer (UB in Release, since the guard
    // was only a jassert) - a real risk in a plugin host that may not call
    // createEditor() (which called setPreset()) on the thread/order expected.
    static GuiConstants& instance()
    {
        static GuiConstants inst;
        return inst;
    }

    static void setPreset(Theme theme)
    {
        instance().applyTheme(theme);
    }

    GuiConstants(const GuiConstants&) = delete;
    GuiConstants& operator=(const GuiConstants&) = delete;

    [[nodiscard]] juce::ColourGradient getSpectrogramGradient() const
    {
        return m_spectrogramGradient;
    }

    [[nodiscard]] juce::ColourGradient getCpuGradient() const
    {
        return m_cpuGradient;
    }

    [[nodiscard]] juce::ColourGradient getLevelGradient() const
    {
        return m_levelGradient;
    }

    static void buildLut(Theme theme, juce::PixelARGB (&lut)[kLutSize])
    {
        makeSpectrogramGradient(Themes::definition(theme)).createLookupTable(lut, kLutSize);
    }

    [[nodiscard]] static constexpr float levelPosition(const float db)
    {
        return (db - kMeterMinDb) / (kMeterMaxDb - kMeterMinDb);
    }

    Colors colors;
    Text text;
    Margins margins;
    InitJuce init;

  private:
    GuiConstants()
    {
        applyTheme(Theme::Ink);
    }

    void applyTheme(Theme theme)
    {
        const auto& def = Themes::definition(theme);

        m_spectrogramGradient = makeSpectrogramGradient(def);
        m_cpuGradient = makeZoneGradient(def.cpuZones, kCpuWarnFraction, kCpuDangerFraction);
        m_levelGradient = makeZoneGradient(def.levelZones, levelPosition(kLevelWarnDb), levelPosition(kLevelDangerDb));

        std::generate(colors.cols.begin(), colors.cols.end(),
                      [&, i = size_t{0}]() mutable
                      {
                          return m_spectrogramGradient
                              .getColourAtPosition(static_cast<double>(i++) / colors.cols.size())
                              .getARGB();
                      });

        colors.background = def.background;
        colors.backgroundComponent = def.backgroundComponent;
        colors.backgroundDark = def.backgroundDark;
        colors.backgroundMid = def.backgroundMid;
        colors.backgroundLight = def.backgroundLight;
        colors.gradientDark = def.gradientDark;
        colors.knobGradientStart = def.knobGradientStart;
        colors.knobGradientCenter = def.knobGradientCenter;
        colors.knobGradientEnd = def.knobGradientEnd;
        colors.statusOutline = def.statusOutline;
        colors.labelColour = def.labelColour;
    }

    juce::ColourGradient m_spectrogramGradient;
    juce::ColourGradient m_cpuGradient;
    juce::ColourGradient m_levelGradient;

    static juce::ColourGradient makeSpectrogramGradient(const Themes::ThemeDefinition& def)
    {
        const auto& stops = def.spectrogramStops;
        const auto count = def.spectrogramStopCount;

        juce::ColourGradient gradient(juce::Colour(stops[0].argb), 0.0f, 0.0f, juce::Colour(stops[count - 1].argb),
                                      1.0f, 0.0f, false);
        for (size_t i = 1; i + 1 < count; ++i)
        {
            gradient.addColour(stops[i].position, juce::Colour(stops[i].argb));
        }
        return gradient;
    }

    // Smooth blend inside each zone, sharp edge at the warn/danger thresholds.
    static juce::ColourGradient makeZoneGradient(const Themes::MeterZoneColors& zones, const float warnPosition,
                                                 const float dangerPosition)
    {
        constexpr float kEdge = 0.002f;
        constexpr float kZoneBrighten = 0.25f;

        const juce::Colour safe{zones.safe};
        const juce::Colour warn{zones.warn};
        const juce::Colour danger{zones.danger};

        juce::ColourGradient gradient(safe, 0.0f, 0.0f, danger.brighter(kZoneBrighten), 1.0f, 0.0f, false);
        gradient.addColour(warnPosition - kEdge, safe.brighter(kZoneBrighten));
        gradient.addColour(warnPosition + kEdge, warn);
        gradient.addColour(dangerPosition - kEdge, warn.brighter(kZoneBrighten));
        gradient.addColour(dangerPosition + kEdge, danger);
        return gradient;
    }
};
