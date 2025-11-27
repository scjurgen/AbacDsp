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
        backgroundLightGrey = juce::Colour(Constants::Colors::bg_LightGrey);
        backgroundApp = juce::Colour(Constants::Colors::bg_App);
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
    SpectrogramValue() {}

    void paint(juce::Graphics& g) override
    {
        if (m_imageSet.data == nullptr)
        {
            return;
        }
        const auto bounds = getLocalBounds().toFloat();
        juce::Image spectrogramImage(juce::Image::PixelFormat::RGB, static_cast<int>(m_imageSet.width),
                                     static_cast<int>(m_imageSet.height), true);

        juce::Image::BitmapData bitmapData(spectrogramImage, juce::Image::BitmapData::writeOnly);
        for (int y = 0; y < bitmapData.height; ++y)
        {
            for (int x = 0; x < bitmapData.width; ++x)
            {
                float value = m_imageSet.data[static_cast<size_t>(x) * m_imageSet.height +
                                              (m_imageSet.height - static_cast<size_t>(y) - 1)];
                value = std::pow(value, 0.15f);
                auto color = juce::Colour::fromHSV(0.67f - (value * 0.67f), 1.0f, value, 1.0f);
                bitmapData.setPixelColour(x, y, color);
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

  private:
    AbacDsp::SpectrumImageSet m_imageSet{};
};


class SpectrogramDisplay : public juce::Component
{
  public:
    SpectrogramDisplay()
    {
        addAndMakeVisible(spectrogramBg);
        addAndMakeVisible(spectrogramImage);
        backgroundDarkGrey = juce::Colour(Constants::Colors::bg_DarkGrey);
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
