#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <vector>

class GaugeBackground : public juce::Component
{
  public:
    GaugeBackground()
    {
        backgroundLightGrey = juce::Colour(Constants::Colors::bg_LightGrey);
        backgroundApp = juce::Colour(Constants::Colors::bg_App);
        setBufferedToImage(true);
    }

    void paint(juce::Graphics& g) override
    {
        const auto bounds = GaugeArea.toFloat();

        g.setColour(backgroundApp);
        g.fillRoundedRectangle(bounds, 1);

        g.setColour(backgroundLightGrey);
        drawIndicators(g, minValue, maxValue);
    }

    void resized() override
    {
        GaugeArea = getLocalBounds().reduced(3);
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
    juce::Rectangle<int> GaugeArea;
    juce::Colour backgroundLightGrey, backgroundApp;
    float minValue{-60.f}, maxValue{12.f};
};

class GaugeValue : public juce::Component
{
  public:
    GaugeValue()
    {
    }

    void paint(juce::Graphics& g) override
    {
        const auto bounds = getLocalBounds().toFloat();
        redrawValue(g, bounds);
    }

    void resized() override
    {
        repaint();
    }

    void update(const std::vector<float>& newValues)
    {
        if (values != newValues)
        {
            values = newValues;
            repaint();
        }
    }

    void update(const float newValues)
    {
        values.resize(1);
        if (std::abs(values[0] - newValues) < 1E-7f)
        {
            values[0] = newValues;
            repaint();
        }
    }

    void redrawValue(juce::Graphics& g, const juce::Rectangle<float>& bounds) const
    {
        constexpr size_t pad = 4;
        constexpr float s = 4.0f;
        constexpr uint32_t alpha = 0x88000000;
        const float height = bounds.getHeight() - pad * 2;
        const float width = bounds.getWidth() - pad * 2;
        const float channelWidth = width / static_cast<float>(values.size());
        juce::Rectangle<float> meterBounds(pad, pad, channelWidth, height);
        juce::ColourGradient gradient(juce::Colour(alpha | 0x00FF00), meterBounds.getBottomLeft(),
                                      juce::Colour(alpha | 0xFF0088), meterBounds.getTopLeft(), false);
        gradient.addColour(juce::jmap(70.0f - s, 0.0f, 100.0f, 0.0f, 1.0f), juce::Colour(alpha | 0x00FF00));
        gradient.addColour(juce::jmap(70.0f + s, 0.0f, 100.0f, 0.0f, 1.0f), juce::Colour(alpha | 0xFFFF00));
        gradient.addColour(juce::jmap(85.0f - s, 0.0f, 100.0f, 0.0f, 1.0f), juce::Colour(alpha | 0xFFFF00));
        gradient.addColour(juce::jmap(85.0f + s, 0.0f, 100.0f, 0.0f, 1.0f), juce::Colour(alpha | 0xFF0000));

        for (size_t i = 0; i < values.size(); ++i)
        {
            constexpr float padC = 2;
            juce::Rectangle<float> columnBounds{meterBounds.getX() + i * channelWidth + padC, meterBounds.getY(),
                                                channelWidth - padC * 2, height};
            float linValue = std::clamp(values[i], -84.f, 12.f) + 84;
            g.setGradientFill(gradient);
            g.fillRect(columnBounds);
            float visibleHeight = juce::jmap(std::clamp(linValue, 0.f, 100.f), 0.f, 100.f, 0.0f, height);

            g.setColour(juce::Colour(Constants::Colors::bg_App));
            columnBounds.expand(1, 0);
            g.fillRect(columnBounds.withBottom(height - visibleHeight));
        }
    }

  private:
    std::vector<float> values;
};

class Gauge : public juce::Component
{
  public:
    Gauge()
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

    void update(const float value)
    {
        gaugeValue.update(value);
    }

    void update(const float left, const float right)
    {
        std::vector values = {left, right};
        gaugeValue.update(values);
    }

    void update(std::pair<float, float> inLevel, std::pair<float, float> outLevel)
    {
        std::vector values = {inLevel.first, inLevel.second, outLevel.first, outLevel.second};
        gaugeValue.update(values);
    }

    void setLabelText(const juce::String& label)
    {
        m_label = label;
        repaint();
    }

  private:
    GaugeBackground gaugeBg;
    GaugeValue gaugeValue;
    juce::Colour backgroundDarkGrey;
    juce::String m_label;
};
