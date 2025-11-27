#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <vector>
#include "GenericMeter.h"


class CpuValue : public juce::Component
{
  public:
    CpuValue() = default;

    void paint(juce::Graphics& g) override
    {
        const auto bounds = getLocalBounds().toFloat();
        redrawValue(g, bounds);
    }

    void resized() override
    {
        repaint();
    }

    void update(const float newValue)
    {
        if (std::abs(value - newValue) > 1E-5f)
        {
            value = newValue;
            repaint();
        }
    }

    void redrawValue(juce::Graphics& g, const juce::Rectangle<float>& bounds) const
    {
        constexpr size_t pad = 8;
        constexpr float s = 4.0f;
        constexpr uint32_t alpha = 0x88000000;
        const float height = bounds.getHeight() - 2 * pad;
        const float width = bounds.getWidth() - 2 * pad;
        const float channelWidth = width;

        juce::Rectangle<float> meterBounds(pad, pad, channelWidth, height);

        // Create the gradient
        juce::ColourGradient gradient(juce::Colour(alpha | 0x00FF00), meterBounds.getBottomLeft(),
                                      juce::Colour(alpha | 0xFF0088), meterBounds.getTopLeft(), false);

        // Add intermediate color stops
        gradient.addColour(juce::jmap(70.0f - s, 0.0f, 100.0f, 0.0f, 1.0f), juce::Colour(alpha | 0x00FF00));
        gradient.addColour(juce::jmap(70.0f + s, 0.0f, 100.0f, 0.0f, 1.0f), juce::Colour(alpha | 0xFFFF00));
        gradient.addColour(juce::jmap(85.0f - s, 0.0f, 100.0f, 0.0f, 1.0f), juce::Colour(alpha | 0xFFFF00));
        gradient.addColour(juce::jmap(85.0f + s, 0.0f, 100.0f, 0.0f, 1.0f), juce::Colour(alpha | 0xFF0000));

        // Draw the full gradient rectangle
        g.setGradientFill(gradient);
        g.fillRect(meterBounds);

        // Calculate the height of the visible portion
        float visibleHeight = juce::jmap(std::clamp(value, 0.f, 100.f), 0.f, 100.f, 0.0f, height);

        // Paint over the unused portion with the background color
        g.setColour(juce::Colour(Constants::Colors::bg_App));
        g.fillRect(meterBounds.withBottom(height - visibleHeight));
    }

  private:
    float value;
};


class CpuGauge : public juce::Component
{
  public:
    CpuGauge()
    {
        addAndMakeVisible(gaugeBg);
        addAndMakeVisible(gaugeValue);
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
        gaugeBg.setBounds(bounds);
        gaugeValue.setBounds(bounds);
    }

    void update(const float valuePercentage)
    {
        gaugeValue.update(valuePercentage);
    }

    void setLabelText(const juce::String& label)
    {
        m_label = label;
        repaint();
    }

  private:
    GaugeBackground gaugeBg;
    CpuValue gaugeValue;
    juce::Colour backgroundDarkGrey;
    juce::String m_label;
};
