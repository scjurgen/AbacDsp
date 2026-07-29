#include <array>
#include <cmath>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_events/juce_events.h>
#include <juce_graphics/juce_graphics.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <memory>
#include <vector>

#include "Analysis/Spectrogram.h"

// clang-format off
// StubConstants.h must precede StatusBar.h: it defines the Constants:: namespace
// StatusBar.h reads from (normally supplied by a generated per-blueprint Constants.h).
#include "StubConstants.h"
#include "GuiConstants.h"
#include "themes/Themes.h"

#include "CircularBarDisplay.h"
#include "CpuMeter.h"
#include "CustomRotaryDial.h"
#include "GenericMeter.h"
#include "LookAndFeel.h"
#include "SpectrogramDisplay.h"
#include "StatusBar.h"
#include "WaveformMeter.h"
// clang-format on

namespace
{

constexpr int kPanelWidth = 900;
constexpr int kPanelHeight = 360;
constexpr int kThumbWidth = 450;
constexpr int kThumbHeight = 180;

struct RoleSwatch
{
    const char* name;
    std::uint32_t argb;
};

[[nodiscard]] std::array<RoleSwatch, 11> currentRoleSwatches()
{
    const auto& colors = GuiConstants::instance().colors;
    return {{
        {"background", colors.background},
        {"backgroundComponent", colors.backgroundComponent},
        {"backgroundDark", colors.backgroundDark},
        {"backgroundMid", colors.backgroundMid},
        {"backgroundLight", colors.backgroundLight},
        {"gradientDark", colors.gradientDark},
        {"knobGradientStart", colors.knobGradientStart},
        {"knobGradientCenter", colors.knobGradientCenter},
        {"knobGradientEnd", colors.knobGradientEnd},
        {"statusOutline", colors.statusOutline},
        {"labelColour", colors.labelColour},
    }};
}

class RoleSwatchStrip : public juce::Component
{
  public:
    void paint(juce::Graphics& g) override
    {
        const auto rows = currentRoleSwatches();
        const int rowHeight = getHeight() / static_cast<int>(rows.size());

        for (size_t i = 0; i < rows.size(); ++i)
        {
            const juce::Rectangle<int> bounds{0, static_cast<int>(i) * rowHeight, getWidth(), rowHeight};
            const juce::Colour colour{rows[i].argb};

            g.setColour(colour);
            g.fillRect(bounds);
            g.setColour(colour.contrasting());
            g.drawText(juce::String(rows[i].name) + " " + colour.toDisplayString(false), bounds.reduced(4),
                       juce::Justification::centredLeft);
        }
    }
};

// Plain, undistorted view of the theme's spectrogram gradient stops, independent of the
// fake data's log-frequency remapping in SpectrogramDisplay - makes stop colours/positions
// directly inspectable.
class SpectrogramGradientBar : public juce::Component
{
  public:
    void paint(juce::Graphics& g) override
    {
        auto gradient = GuiConstants::instance().getSpectrogramGradient();
        const auto bounds = getLocalBounds().toFloat();
        gradient.point1 = bounds.getBottomLeft();
        gradient.point2 = bounds.getBottomRight();
        g.setGradientFill(gradient);
        g.fillRect(bounds);
    }
};

[[nodiscard]] std::vector<float> makeFakeSpectrumData(const size_t width, const size_t height)
{
    std::vector<float> data(width * height);
    for (size_t x = 0; x < width; ++x)
    {
        for (size_t y = 0; y < height; ++y)
        {
            const float t = static_cast<float>(x) / static_cast<float>(width);
            const float band = static_cast<float>(y) / static_cast<float>(height);
            const float sweep = std::exp(-std::pow((band - t) * 4.0f, 2.0f));
            const float floor = 0.05f + 0.05f * std::sin(band * 30.0f + t * 10.0f);
            data[x * height + y] = std::clamp(sweep + floor, 0.0f, 1.0f);
        }
    }
    return data;
}

[[nodiscard]] std::vector<float> makeFakeWaveform()
{
    std::vector<float> values(128);
    for (size_t i = 0; i < values.size(); ++i)
    {
        values[i] = std::sin(static_cast<float>(i) * 0.2f) * 0.8f;
    }
    return values;
}

class ThemePanel : public juce::Component
{
  public:
    explicit ThemePanel(const Themes::Theme theme)
        : m_dial(nullptr)
        , m_spectrogram(theme)
        , m_spectrumData(makeFakeSpectrumData(kSpectrumWidth, kSpectrumHeight))
    {
        addAndMakeVisible(m_dial);
        m_dial.setLabelText("Mix");
        m_dial.setValue(0.65);

        addAndMakeVisible(m_toggleOn);
        m_toggleOn.setButtonText("Power (on)");
        m_toggleOn.setToggleState(true, juce::dontSendNotification);

        addAndMakeVisible(m_toggleOff);
        m_toggleOff.setButtonText("Power (off)");
        m_toggleOff.setToggleState(false, juce::dontSendNotification);

        addAndMakeVisible(m_label);
        m_label.setText("Label text", juce::dontSendNotification);

        addAndMakeVisible(m_cpu);
        m_cpu.setLabelText("CPU");
        m_cpu.update(62.f);

        addAndMakeVisible(m_levels);
        m_levels.setLabelText("Level");
        m_levels.update(std::vector<float>{-3.f, -18.f, 2.f, -50.f});

        addAndMakeVisible(m_spectrogram);
        m_spectrogram.setLabelText("Spectrogram");
        m_spectrogram.update(AbacDsp::SpectrumImageSet{
            .activeSlice = 1,
            .width = kSpectrumWidth,
            .height = kSpectrumHeight,
            .data = m_spectrumData.data(),
            .sampleRate = 48000.f,
            .fftLength = 1024,
            .windowForwardRatio = 1.0f / 3.0f,
        });

        addAndMakeVisible(m_signal);
        m_signal.setLabelText("Signal");
        m_signal.update(makeFakeWaveform());

        addAndMakeVisible(m_circularBar);
        m_circularBar.setLabelText("Beat");
        m_circularBar.setSamplesPerBar(4 * 12000);
        m_circularBar.setBarBeats(4);
        m_circularBar.setBarPhase(0.4f);
        m_circularBar.setSubdivisionPositions({3000, 6000, 9000});
        m_circularBar.update(makeFakeWaveform());

        addAndMakeVisible(m_spectrogramGradientBar);

        addAndMakeVisible(m_statusBar);
        m_statusBar.showMessage("Status message");

        addAndMakeVisible(m_swatches);
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colour(GuiConstants::instance().colors.background));
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced(6);
        auto swatchArea = area.removeFromRight(220);
        m_swatches.setBounds(swatchArea);

        auto top = area.removeFromTop(140);
        m_dial.setBounds(top.removeFromLeft(110));

        auto toggleColumn = top.removeFromLeft(150);
        m_toggleOn.setBounds(toggleColumn.removeFromTop(26));
        m_toggleOff.setBounds(toggleColumn.removeFromTop(26));
        m_label.setBounds(toggleColumn.removeFromTop(26));

        m_cpu.setBounds(top.removeFromLeft(70));
        m_levels.setBounds(top.removeFromLeft(110));
        m_circularBar.setBounds(top.removeFromLeft(120).reduced(4, 0));
        m_signal.setBounds(top.reduced(4, 0));

        auto bottom = area.reduced(0, 4);
        m_statusBar.setBounds(bottom.removeFromBottom(22));
        m_spectrogramGradientBar.setBounds(bottom.removeFromBottom(18));
        bottom.removeFromBottom(2);
        m_spectrogram.setBounds(bottom);
    }

  private:
    static constexpr size_t kSpectrumWidth = 64;
    static constexpr size_t kSpectrumHeight = 128;

    CustomRotaryDial m_dial;
    juce::ToggleButton m_toggleOn;
    juce::ToggleButton m_toggleOff;
    juce::Label m_label;
    CpuGauge m_cpu;
    Gauge m_levels;
    SpectrogramDisplay m_spectrogram;
    SpectrogramGradientBar m_spectrogramGradientBar;
    WaveformGauge m_signal;
    CircularBarDisplay m_circularBar;
    StatusBar m_statusBar;
    RoleSwatchStrip m_swatches;
    std::vector<float> m_spectrumData;
};

void writePng(const juce::Image& image, const juce::File& file)
{
    // FileOutputStream appends to an existing file rather than truncating it, so a stale
    // PNG from a previous run stays intact (and visible) underneath whatever gets written now.
    juce::ignoreUnused(file.deleteFile());
    const std::unique_ptr<juce::FileOutputStream> stream(file.createOutputStream());
    if (stream != nullptr)
    {
        juce::PNGImageFormat png;
        png.writeImageToStream(image, *stream);
    }
}

class ThemeReviewApplication : public juce::JUCEApplication
{
  public:
    const juce::String getApplicationName() override
    {
        return "ThemeReviewTool";
    }

    const juce::String getApplicationVersion() override
    {
        return "1.0";
    }

    void initialise(const juce::String&) override
    {
        runReview();
        quit();
    }

    void shutdown() override {}

  private:
    static void runReview()
    {
        const auto outputDir = juce::File(THEME_REVIEW_SOURCE_DIR).getChildFile("output");
        juce::ignoreUnused(outputDir.createDirectory());

        constexpr int kColumns = 2;
        constexpr int kRows = Themes::kHueCount;

        const auto families = {
            std::pair{ui::ThemeFamily::Bichromatic, juce::String("bichromatic")},
            std::pair{ui::ThemeFamily::Trichromatic, juce::String("trichromatic")},
        };

        for (const auto& [fam, famName] : families)
        {
            juce::Image contactSheet(juce::Image::PixelFormat::ARGB, kThumbWidth * kColumns, kThumbHeight * kRows,
                                     true);
            juce::Graphics contactSheetGraphics(contactSheet);

            for (int hueStep = 0; hueStep < Themes::kHueCount; ++hueStep)
            {
                for (const bool dark : {false, true})
                {
                    const auto theme = Themes::makeTheme(hueStep, dark, fam);
                    GuiConstants::setPreset(theme);

                    auto lookAndFeel = std::make_unique<GuiLookAndFeel>();
                    juce::LookAndFeel::setDefaultLookAndFeel(lookAndFeel.get());

                    ThemePanel panel(theme);
                    panel.setLookAndFeel(lookAndFeel.get());
                    panel.setSize(kPanelWidth, kPanelHeight);
                    panel.resized();

                    const auto snapshot = panel.createComponentSnapshot(panel.getLocalBounds());

                    const juce::String modeLabel = dark ? "dark" : "light";
                    const juce::String fileName = famName + "_hue" +
                                                  juce::String(hueStep * Themes::kHueStepDeg).paddedLeft('0', 3) + "_" +
                                                  modeLabel + ".png";
                    writePng(snapshot, outputDir.getChildFile(fileName));

                    const int column = dark ? 1 : 0;
                    const int row = hueStep;
                    contactSheetGraphics.drawImage(
                        snapshot, juce::Rectangle<float>(
                                      static_cast<float>(column * kThumbWidth), static_cast<float>(row * kThumbHeight),
                                      static_cast<float>(kThumbWidth), static_cast<float>(kThumbHeight)));

                    panel.setLookAndFeel(nullptr);
                    juce::LookAndFeel::setDefaultLookAndFeel(nullptr);
                }
            }

            writePng(contactSheet, outputDir.getChildFile("contact-sheet-" + famName + ".png"));
        }

        juce::Logger::writeToLog("Theme review output written to " + outputDir.getFullPathName());
    }
};

} // namespace

START_JUCE_APPLICATION(ThemeReviewApplication)
