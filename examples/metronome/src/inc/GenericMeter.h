#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <vector>

#include "GuiConstants.h"

class GaugeBackground : public juce::Component
{
  public:
    GaugeBackground()
    {
        backgroundApp = juce::Colour(GuiConstants::instance().colors.backgroundComponent);
        setBufferedToImage(true);
    }

    void paint(juce::Graphics& g) override
    {
        const auto bounds = GaugeArea.toFloat();
        g.setColour(backgroundApp);
        g.fillRoundedRectangle(bounds, 1);
    }

    void resized() override
    {
        GaugeArea = getLocalBounds().reduced(3);
        repaint();
    }

    void updateColors()
    {
        backgroundApp = juce::Colour(GuiConstants::instance().colors.backgroundComponent);
        repaint();
    }

  private:
    juce::Rectangle<int> GaugeArea;
    juce::Colour backgroundApp;
};

class GaugeValue : public juce::Component
{
  public:
    GaugeValue() {}

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
        const float height = bounds.getHeight() - pad * 2;
        const float width = bounds.getWidth() - pad * 2;
        const float channelWidth = width / static_cast<float>(values.size());
        const juce::Rectangle<float> meterBounds(pad, pad, channelWidth, height);

        for (size_t i = 0; i < values.size(); ++i)
        {
            constexpr float padC = 2;
            juce::Rectangle<float> columnBounds{meterBounds.getX() + i * channelWidth + padC, meterBounds.getY(),
                                                channelWidth - padC * 2, height};

            auto gradient = GuiConstants::instance().getLevelGradient();
            gradient.point1 = columnBounds.getBottomLeft();
            gradient.point2 = columnBounds.getTopLeft();

            g.setGradientFill(gradient);
            g.fillRect(columnBounds);

            constexpr float span = GuiConstants::kMeterMaxDb - GuiConstants::kMeterMinDb;
            const float linValue =
                std::clamp(values[i], GuiConstants::kMeterMinDb, GuiConstants::kMeterMaxDb) - GuiConstants::kMeterMinDb;
            const float visibleHeight = juce::jmap(linValue, 0.f, span, 0.0f, height);

            g.setColour(juce::Colour(GuiConstants::instance().colors.backgroundComponent));
            columnBounds.expand(1, 0);
            g.fillRect(columnBounds.withBottom(height - visibleHeight));
        }
    }

  private:
    std::vector<float> values;
};

class GaugeIndicators : public juce::Component
{
  public:
    GaugeIndicators()
    {
        lineIndicatorColor = juce::Colour(GuiConstants::instance().colors.statusOutline);
        setInterceptsMouseClicks(false, false);
    }

    void paint(juce::Graphics& g) override
    {
        constexpr size_t pad = 4;
        const auto height = static_cast<float>(getHeight());
        const auto width = static_cast<float>(getWidth());

        for (float db = minValue_; db <= maxValue_; db += 6)
        {
            const float y = juce::jmap(db, minValue_, maxValue_, height, 0.0f);
            const bool isZero = (std::abs(db) < 0.01f);
            g.setColour(isZero ? lineIndicatorColor.withAlpha(0.75f) : lineIndicatorColor.withAlpha(0.30f));
            g.drawLine(pad, y, width - pad * 2, y, isZero ? 1.5f : 1.0f);
        }
    }

  private:
    // Match the bar's dB mapping so the 0 dB line sits exactly on the gradient's danger edge.
    float minValue_{GuiConstants::kMeterMinDb}, maxValue_{GuiConstants::kMeterMaxDb};
    juce::Colour lineIndicatorColor;
};

class Gauge : public juce::Component
{
  public:
    Gauge()
    {
        addAndMakeVisible(gaugeBg);
        addAndMakeVisible(gaugeValue);
        addAndMakeVisible(gaugeIndicators);
        backgroundDarkGrey = juce::Colour(GuiConstants::instance().colors.backgroundDark);
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
        bounds.removeFromTop(20);
        gaugeBg.setBounds(bounds);
        gaugeValue.setBounds(bounds);
        gaugeIndicators.setBounds(bounds);
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
        std::vector<float> values = {left, right};
        gaugeValue.update(values);
    }

    void update(std::pair<float, float> inLevel, std::pair<float, float> outLevel)
    {
        std::vector<float> values = {inLevel.first, inLevel.second, outLevel.first, outLevel.second};
        gaugeValue.update(values);
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
    GaugeValue gaugeValue;
    GaugeIndicators gaugeIndicators;
    juce::Colour backgroundDarkGrey;
    juce::String m_label;
};