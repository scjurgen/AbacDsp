#pragma once

#include <algorithm>
#include <juce_graphics/juce_graphics.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "GuiConstants.h"

namespace SonalysisUi
{

enum class Zone
{
    Ok,
    Warn,
    Danger
};

// Zone colours come from the theme's level gradient, so they match every other meter.
[[nodiscard]] inline juce::Colour levelColour(const float db)
{
    const float position = std::clamp(GuiConstants::levelPosition(db), 0.f, 1.f);
    return GuiConstants::instance().getLevelGradient().getColourAtPosition(static_cast<double>(position));
}

[[nodiscard]] inline juce::Colour zoneColour(const Zone zone)
{
    if (zone == Zone::Ok)
    {
        return levelColour(GuiConstants::kLevelWarnDb - 12.f);
    }
    if (zone == Zone::Warn)
    {
        return levelColour(0.5f * (GuiConstants::kLevelWarnDb + GuiConstants::kLevelDangerDb));
    }
    return levelColour(GuiConstants::kLevelDangerDb + 3.f);
}

[[nodiscard]] inline juce::Colour textColour(const float alpha = 1.f)
{
    return juce::Colour(GuiConstants::instance().colors.labelColour).withAlpha(alpha);
}

[[nodiscard]] inline juce::Colour accentColour(const size_t index)
{
    const auto& cols = GuiConstants::instance().colors.cols;
    return juce::Colour(cols[std::min(index, cols.size() - 1)]);
}

inline void drawText(juce::Graphics& g, const juce::String& text, const juce::Rectangle<float> area, const float height,
                     const juce::Justification justification, const juce::Colour colour)
{
    g.setColour(colour);
    g.setFont(juce::Font(juce::FontOptions(height)));
    g.drawText(text, area, justification, false);
}

[[nodiscard]] inline juce::String formatDb(const float db, const float floorDb)
{
    return db <= floorDb + 0.5f ? juce::String("-inf") : juce::String(db, 1);
}

/**
 * Base of the Sonalysis displays: themed rounded background with the gauge label on top.
 * Colours are read from GuiConstants on every paint, so a theme change needs no callback.
 */
class DisplayFrame : public juce::Component
{
  public:
    void setLabelText(const juce::String& label)
    {
        m_label = label;
        repaint();
    }

  protected:
    [[nodiscard]] juce::Rectangle<float> paintFrame(juce::Graphics& g) const
    {
        g.setColour(juce::Colour(GuiConstants::instance().colors.backgroundComponent));
        g.fillRoundedRectangle(getLocalBounds().toFloat(), 3.f);
        auto area = getLocalBounds().toFloat().reduced(8.f);
        if (m_label.isNotEmpty())
        {
            drawText(g, m_label, area.removeFromTop(18.f), 13.f, juce::Justification::centred, textColour(0.75f));
        }
        return area;
    }

  private:
    juce::String m_label;
};

}
