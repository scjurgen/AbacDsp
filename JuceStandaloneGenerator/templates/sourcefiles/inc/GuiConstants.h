#pragma once

#include <array>
#include <cstdint>
#include <juce_graphics/juce_graphics.h>
#include <memory>
#include <numeric>

class GuiConstants : public juce::DeletedAtShutdown
{
  public:
    struct Colors
    {
        std::array<uint32_t, 10> cols{};
        uint32_t background{};
        uint32_t backgroundComponent{};
        uint32_t backgroundDark{};
        uint32_t backgroundMid{};
        uint32_t backgroundLight{};
        uint32_t gradientStart{};
        uint32_t gradientEnd{};
        uint32_t gradientDark{};
        uint32_t knobGradientStart{};
        uint32_t knobGradientCenter{};
        uint32_t knobGradientEnd{};
        uint32_t statusOutline{};
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

    enum class GradientPreset
    {
        Classic,
        Viridis,
        Inferno,
        Grayscale,
        Heat,
        Ink,
        Teal,
    };

    static constexpr int kLutSize = 256;

    static GuiConstants& instance()
    {
        jassert(instance_ != nullptr); // must call setPreset() before first use
        return *instance_;
    }
    auto getGradient()
    {
        return m_gradient;
    }
    static void setPreset(GradientPreset preset)
    {
        delete instance_;
        instance_ = new GuiConstants(preset);
    }

    explicit GuiConstants(GradientPreset preset = GradientPreset::Ink)
    {
        m_gradient = makeGradient(preset);

        std::generate(
            colors.cols.begin(), colors.cols.end(), [&, i = size_t{0}]() mutable
            { return m_gradient.getColourAtPosition(static_cast<double>(i++) / colors.cols.size()).getARGB(); });

        colors.background = colors.cols[2];
        colors.backgroundComponent = colors.cols[1];
        colors.backgroundDark = colors.cols[5];
        colors.backgroundMid = colors.cols[3];
        colors.backgroundLight = colors.cols[9];
        colors.gradientDark = colors.cols[2];
        colors.gradientStart = colors.cols[0];
        colors.gradientEnd = colors.cols[0];

        colors.knobGradientStart = colors.cols[0];
        colors.knobGradientCenter = colors.cols[3];
        colors.knobGradientEnd = colors.cols[8];

        colors.statusOutline = colors.cols[8];
    }

    static void buildLut(GradientPreset preset, juce::PixelARGB (&lut)[kLutSize])
    {
        makeGradient(preset).createLookupTable(lut, kLutSize);
    }

    Colors colors;
    Text text;
    Margins margins;
    InitJuce init;

  private:
    juce::ColourGradient m_gradient;
    static inline GuiConstants* instance_ = nullptr;

    static juce::ColourGradient makeGradient(GradientPreset preset)
    {
        juce::ColourGradient gradient;

        switch (preset)
        {
            case GradientPreset::Classic:
                gradient = juce::ColourGradient(juce::Colour::fromHSV(0.67f, 1.0f, 0.0f, 1.0f), 0.0f, 0.0f,
                                                juce::Colour::fromHSV(0.0f, 1.0f, 1.0f, 1.0f), 1.0f, 0.0f, false);
                break;
            case GradientPreset::Viridis:
                gradient = juce::ColourGradient(juce::Colour(0xff440154), 0.0f, 0.0f, juce::Colour(0xfffde725), 1.0f,
                                                0.0f, false);
                gradient.addColour(0.33, juce::Colour(0xff31688e));
                gradient.addColour(0.66, juce::Colour(0xff35b779));
                break;
            case GradientPreset::Inferno:
                gradient = juce::ColourGradient(juce::Colour(0xff000004), 0.0f, 0.0f, juce::Colour(0xfffcffa4), 1.0f,
                                                0.0f, false);
                gradient.addColour(0.33, juce::Colour(0xff56106e));
                gradient.addColour(0.55, juce::Colour(0xffbc3754));
                gradient.addColour(0.75, juce::Colour(0xfff98e09));
                break;
            case GradientPreset::Grayscale:
                gradient =
                    juce::ColourGradient(juce::Colours::black, 0.0f, 0.0f, juce::Colours::white, 1.0f, 0.0f, false);
                break;
            case GradientPreset::Heat:
                gradient = juce::ColourGradient(juce::Colour(0xffffffff), 0.0f, 0.0f, juce::Colour(0xff1a0000), 1.0f,
                                                0.0f, false);
                gradient.addColour(0.30, juce::Colour(0xffffcc99));
                gradient.addColour(0.60, juce::Colour(0xffcc3300));
                gradient.addColour(0.85, juce::Colour(0xff660000));
                break;
            case GradientPreset::Ink:
                gradient = juce::ColourGradient(juce::Colour(0xffffffff), 0.0f, 0.0f, juce::Colour(0xff0d0221), 1.0f,
                                                0.0f, false);
                gradient.addColour(0.35, juce::Colour(0xffc8b8e8));
                gradient.addColour(0.65, juce::Colour(0xff4b1fa8));
                break;
            case GradientPreset::Teal:
                gradient = juce::ColourGradient(juce::Colour(0xffffffff), 0.0f, 0.0f, juce::Colour(0xff001a1a), 1.0f,
                                                0.0f, false);
                gradient.addColour(0.35, juce::Colour(0xffb2dfdb));
                gradient.addColour(0.65, juce::Colour(0xff00695c));
                break;
        }

        return gradient;
    }
};