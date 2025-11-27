#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <vector>
#include "GenericMeter.h"


class WaveformShow : public juce::Component
{
  public:
    WaveformShow() = default;

    void paint(juce::Graphics& g) override
    {
        const auto bounds = getLocalBounds().toFloat();
        redrawValue(g, bounds);
    }

    void resized() override
    {
        repaint();
    }

    void update(const std::vector<float>& values)
    {
        m_lastValues = values;
        repaint();
    }

    void redrawValue(juce::Graphics& g, const juce::Rectangle<float>& bounds) const
    {
        if (m_lastValues.size() < 2)
        {
            return;
        }
        constexpr size_t pad = 8;
        // constexpr uint32_t alpha = 0x88000000;
        const float height = bounds.getHeight() - 2 * pad;
        const float width = bounds.getWidth() - 2 * pad;
        const float channelWidth = width;

        juce::Rectangle<float> meterBounds(pad, pad, channelWidth, height);

        // Draw grid
        g.setColour(juce::Colours::grey.withAlpha(0.2f));
        for (int i = 0; i < 5; ++i)
        {
            float y = meterBounds.getY() + i * meterBounds.getHeight() / 4.0f;
            g.drawHorizontalLine(static_cast<int>(y), meterBounds.getX(), meterBounds.getRight());
        }
        for (int i = 0; i < 10; ++i)
        {
            float x = meterBounds.getX() + i * meterBounds.getWidth() / 9.0f;
            g.drawVerticalLine(static_cast<int>(x), meterBounds.getY(), meterBounds.getBottom());
        }

        // Draw waveform
        g.setColour(juce::Colours::white.withAlpha(0.8f));
        juce::Path waveformPath;
        const float xScale = meterBounds.getWidth() / static_cast<float>(m_lastValues.size() - 1);
        const float yScale = meterBounds.getHeight() / 2.0f;
        const float centerY = meterBounds.getCentreY();

        waveformPath.startNewSubPath(meterBounds.getX(), centerY - m_lastValues[0] * yScale);

        for (size_t i = 1; i < m_lastValues.size(); ++i)
        {
            float x = meterBounds.getX() + i * xScale;
            float y = centerY - m_lastValues[i] * yScale;
            waveformPath.lineTo(x, y);
        }

        g.strokePath(waveformPath, juce::PathStrokeType(2.0f));
    }


  private:
    std::vector<float> m_lastValues{};
};


class WaveformGauge : public juce::Component
{
  public:
    WaveformGauge()
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

    void update(const std::vector<float>& values)
    {
        gaugeValue.update(values);
    }

    void setLabelText(const juce::String& label)
    {
        m_label = label;
        repaint();
    }

  private:
    GaugeBackground gaugeBg;
    WaveformShow gaugeValue;
    juce::Colour backgroundDarkGrey;
    juce::String m_label;
};
