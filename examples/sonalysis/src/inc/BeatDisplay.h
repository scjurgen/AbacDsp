#pragma once

#include <algorithm>
#include <cstdint>
#include <juce_graphics/juce_graphics.h>

#include "../impl/SonalysisSnapshots.h"
#include "SonalysisDisplayFrame.h"

/**
 * Beat finder: tempo with one decimal, the timing jitter around the fitted grid and the
 * number of beats it is based on. A dot flashes on every detected onset and fades over a
 * few frames; it expects setBeat() once per UI frame.
 */
class BeatDisplay final : public SonalysisUi::DisplayFrame
{
  public:
    void setBeat(const Sonalysis::BeatSnapshot& beat)
    {
        if (beat.onsetCount != m_beat.onsetCount)
        {
            m_flash = 1.f;
        }
        else
        {
            m_flash = std::max(0.f, m_flash - kFlashDecayPerFrame);
        }
        m_beat = beat;
        repaint();
    }

    void paint(juce::Graphics& g) override
    {
        auto area = paintFrame(g);
        drawFlash(g, area.removeFromTop(area.getHeight() * 0.35f));
        if (m_beat.valid)
        {
            drawReading(g, area);
        }
        else
        {
            drawHint(g, area);
        }
    }

  private:
    static constexpr float kFlashDecayPerFrame{1.f / 9.f};

    void drawFlash(juce::Graphics& g, const juce::Rectangle<float>& area) const
    {
        const float diameter = std::min(area.getWidth(), area.getHeight()) * 0.7f;
        const auto dot = juce::Rectangle<float>(diameter, diameter).withCentre(area.getCentre());
        g.setColour(juce::Colour(GuiConstants::instance().colors.backgroundDark));
        g.fillEllipse(dot);
        g.setColour(SonalysisUi::accentColour(7).withAlpha(m_flash));
        g.fillEllipse(dot);
    }

    void drawReading(juce::Graphics& g, juce::Rectangle<float> area) const
    {
        SonalysisUi::drawText(g, juce::String(m_beat.bpm, 1), area.removeFromTop(area.getHeight() * 0.45f), 64.f,
                              juce::Justification::centredBottom, SonalysisUi::textColour(0.95f));
        const float lineHeight = area.getHeight() / 3.f;
        SonalysisUi::drawText(g, "BPM", area.removeFromTop(lineHeight), 16.f, juce::Justification::centredTop,
                              SonalysisUi::textColour(0.7f));
        SonalysisUi::drawText(g, "jitter " + juce::String(m_beat.jitterMs, 1) + " ms", area.removeFromTop(lineHeight),
                              16.f, juce::Justification::centred, SonalysisUi::textColour(0.8f));
        SonalysisUi::drawText(g, juce::String(static_cast<int64_t>(m_beat.beats)) + " beats", area, 12.f,
                              juce::Justification::centred, SonalysisUi::textColour(0.5f));
    }

    static void drawHint(juce::Graphics& g, juce::Rectangle<float> area)
    {
        SonalysisUi::drawText(g, "---", area.removeFromTop(area.getHeight() * 0.45f), 64.f,
                              juce::Justification::centredBottom, SonalysisUi::textColour(0.4f));
        SonalysisUi::drawText(g, "Play a steady pulse above -22 dBFS", area, 14.f, juce::Justification::centredTop,
                              SonalysisUi::textColour(0.6f));
    }

    Sonalysis::BeatSnapshot m_beat{};
    float m_flash{0.f};
};
