#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

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
        const float height = bounds.getHeight() - 2 * pad;
        const float width = bounds.getWidth() - 2 * pad;
        const float channelWidth = width;

        juce::Rectangle<float> meterBounds(pad, pad, channelWidth, height);

        auto gradient = GuiConstants::instance().getCpuGradient();
        gradient.point1 = meterBounds.getBottomLeft();
        gradient.point2 = meterBounds.getTopLeft();
        g.setGradientFill(gradient);
        g.fillRect(meterBounds);

        // Calculate the height of the visible portion
        const float visibleHeight = juce::jmap(std::clamp(value, 0.f, 100.f), 0.f, 100.f, 0.0f, height);

        // Paint over the unused portion with the background color
        g.setColour(juce::Colour(GuiConstants::instance().colors.background));
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
        backgroundDarkGrey = juce::Colour(GuiConstants::instance().colors.backgroundDark);
    }

    void paint(juce::Graphics& g) override
    {
        g.setColour(backgroundDarkGrey);
        g.fillRoundedRectangle(getLocalBounds().toFloat(), 3);
        g.setColour(juce::Colour(GuiConstants::instance().colors.labelColour));
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

    void updateColors()
    {
        backgroundDarkGrey = juce::Colour(GuiConstants::instance().colors.backgroundDark);
        gaugeBg.updateColors();
        repaint();
    }

  private:
    GaugeBackground gaugeBg;
    CpuValue gaugeValue;
    juce::Colour backgroundDarkGrey;
    juce::String m_label;
};
