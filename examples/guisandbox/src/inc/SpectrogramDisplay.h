#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <vector>

#include "Analysis/Spectrogram.h"
/*
 * TODO:
 *    - Windowsize (128,256,512,1024,...16384)
 *    - Mono/Stereo
 *    - In/Out
 *    - Linear/Mel
 *    - Noisefloor/Balanced/Peaks
 */
class SpectrogramBackground : public juce::Component
{
  public:
    SpectrogramBackground()
    {
        backgroundLightGrey = juce::Colour(GuiConstants::instance().colors.bg_LightGrey);
        backgroundApp = juce::Colour(GuiConstants::instance().colors.bg_App);
        setBufferedToImage(true);
    }

    void paint(juce::Graphics& g) override
    {
        const auto bounds = SpectrogramArea.toFloat();

        g.setColour(backgroundApp);
        g.fillRoundedRectangle(bounds, 1);

        g.setColour(backgroundLightGrey);
        drawIndicators(g, minValue, maxValue);
    }

    void resized() override
    {
        SpectrogramArea = getLocalBounds().reduced(3);
        repaint();
    }

    void drawIndicators(juce::Graphics& /*g*/, const float /*minValue*/, const float /*maxValue*/) const
    {
        // g.setColour(juce::Colour(0xff202020));
        // const auto height = static_cast<float>(getHeight());
        // const auto width = static_cast<float>(getWidth());
        //
        // for (float db = minValue; db <= maxValue; db += 6)
        // {
        //     float y = juce::jmap(db, minValue, maxValue, height, 0.0f);
        //     g.drawLine(0, y, width, y, 1.0f);
        // }
    }

  private:
    juce::Rectangle<int> SpectrogramArea;
    juce::Colour backgroundLightGrey, backgroundApp;
    float minValue{-60.f}, maxValue{12.f};
};
class SpectrogramValue : public juce::Component
{
  public:
    enum class GradientPreset
    {
        Classic, // original HSV blue→red
        Viridis, // dark purple → teal → yellow
        Inferno, // black → red → yellow → white
        Grayscale,
        Heat, // white → orange → dark red
        Ink,  // white → indigo, clean and neutral
        Teal, // white → dark teal, low visual fatigue
    };

    SpectrogramValue()
    {
        buildLut(GradientPreset::Heat);
    }

    void paint(juce::Graphics& g) override
    {
        if (m_imageSet.data == nullptr)
        {
            return;
        }

        const auto bounds = getLocalBounds().toFloat();
        juce::Image spectrogramImage(juce::Image::PixelFormat::ARGB, static_cast<int>(m_imageSet.width),
                                     static_cast<int>(m_imageSet.height), true);

        juce::Image::BitmapData bitmapData(spectrogramImage, juce::Image::BitmapData::writeOnly);
        for (int y = 0; y < bitmapData.height; ++y)
        {
            for (int x = 0; x < bitmapData.width; ++x)
            {
                float value = m_imageSet.data[static_cast<size_t>(x) * m_imageSet.height +
                                              (m_imageSet.height - static_cast<size_t>(y) - 1)];
                value = std::pow(value, 0.15f);
                const int idx = juce::jlimit(0, kLutSize - 1, static_cast<int>(value * (kLutSize - 1)));
                bitmapData.setPixelColour(x, y, juce::Colour(m_lut[idx]));
            }
        }

        g.drawImage(spectrogramImage, bounds);
    }

    void resized() override
    {
        repaint();
    }

    void update(AbacDsp::SpectrumImageSet imageSet)
    {
        if (m_imageSet.activeSlice != imageSet.activeSlice)
        {
            m_imageSet = imageSet;
            repaint();
        }
    }

    void setGradientPreset(GradientPreset preset)
    {
        buildLut(preset);
        repaint();
    }

  private:
    static constexpr int kLutSize = 256;

    void buildLut(GradientPreset preset)
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
            case GradientPreset::Heat: // FBEAD7
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

        gradient.createLookupTable(m_lut, kLutSize);
    }

    AbacDsp::SpectrumImageSet m_imageSet{};
    juce::PixelARGB m_lut[kLutSize]{};
};

class SpectrogramDisplay : public juce::Component
{
  public:
    SpectrogramDisplay()
    {
        addAndMakeVisible(spectrogramBg);
        addAndMakeVisible(spectrogramImage);
        backgroundDarkGrey = juce::Colour(GuiConstants::instance().colors.bg_DarkGrey);
    }

    void paint(juce::Graphics& g) override
    {
        g.setColour(backgroundDarkGrey);
        g.fillRoundedRectangle(getLocalBounds().toFloat(), 3);
        g.setColour(juce::Colours::white);
        g.drawText(m_label, getLocalBounds().removeFromTop(20), juce::Justification::centred);
    }

    void resized() override
    {
        auto bounds = getLocalBounds();
        bounds.removeFromTop(20); // Space for label
        spectrogramBg.setBounds(bounds);
        spectrogramImage.setBounds(bounds);
    }

    void update(AbacDsp::SpectrumImageSet imageSet)
    {
        spectrogramImage.update(imageSet);
    }

    void setLabelText(const juce::String& label)
    {
        m_label = label;
        repaint();
    }

  private:
    juce::ComboBox modeBox;
    SpectrogramBackground spectrogramBg;
    SpectrogramValue spectrogramImage;
    juce::Colour backgroundDarkGrey;
    juce::String m_label;
};
