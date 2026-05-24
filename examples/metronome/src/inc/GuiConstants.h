#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <numeric>
#include <juce_graphics/juce_graphics.h>

class GuiConstants
{
  public:
    struct Colors
    {
        std::array<uint32_t, 10> cols{};
        uint32_t bg_App{};
        uint32_t bg_Component{};
        uint32_t bg_DarkGrey{};
        uint32_t bg_MidGrey{};
        uint32_t bg_LightGrey{};
        uint32_t gd_LightGreyStart{};
        uint32_t gd_LightGreyEnd{};
        uint32_t gd_DarkGreyStart{};
        uint32_t knobGradStart{};
        uint32_t knobGradCenter{};
        uint32_t knobGradEnd{};
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
        instance_ = std::make_unique<GuiConstants>(preset);
    }

    explicit GuiConstants(GradientPreset preset = GradientPreset::Ink)
    {
        m_gradient = makeGradient(preset);

        std::generate(
            colors.cols.begin(), colors.cols.end(), [&, i = size_t{0}]() mutable
            { return m_gradient.getColourAtPosition(static_cast<double>(i++) / colors.cols.size()).getARGB(); });

        colors.bg_App = colors.cols[2];
        colors.bg_Component = colors.cols[1];
        colors.bg_DarkGrey = colors.cols[5];      // box borders, menu
        colors.bg_MidGrey = colors.cols[3];       // gradient knob top
        colors.bg_LightGrey = colors.cols[9];     // ?
        colors.gd_DarkGreyStart = colors.cols[2]; // gradien knob bottom
        colors.gd_LightGreyStart = colors.cols[0];
        colors.gd_LightGreyEnd = colors.cols[0];

        colors.knobGradStart = colors.cols[0];
        colors.knobGradCenter = colors.cols[3];
        colors.knobGradEnd = colors.cols[8];

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
    static inline std::unique_ptr<GuiConstants> instance_;

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