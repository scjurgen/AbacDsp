#pragma once

#include <algorithm>
#include <cmath>
#include <juce_graphics/juce_graphics.h>

#include "../impl/DisplaySnapshots.h"
#include "ListenPlayUi.h"

/**
 * The result of the last take: the overall score against the pass mark, the three scores as bars, and the
 * numbers behind them (what was found, how early or late, how far off in cents, the tempo, the octave,
 * the suggested latency).
 */
class ScoreDisplay final : public ListenPlayUi::DisplayFrame
{
  public:
    void setScore(const ListenPlayDisplay::ScoreSnapshot& score)
    {
        m_score = score;
        repaint();
    }

    void paint(juce::Graphics& g) override
    {
        auto area = paintFrame(g);
        if (!m_score.hasResult)
        {
            ListenPlayUi::drawText(g, "Play a phrase to see your score", area, 16.f, juce::Justification::centred,
                                   ListenPlayUi::textColour(0.6f));
            return;
        }
        drawHeadline(g, area.removeFromTop(kHeadlineHeight));
        drawBar(g, area.removeFromTop(kBarHeight), "Score", m_score.overall, true);
        drawBar(g, area.removeFromTop(kBarHeight), "Notes", m_score.notes, false);
        drawBar(g, area.removeFromTop(kBarHeight), "Rhythm", m_score.rhythm, false);
        drawBar(g, area.removeFromTop(kBarHeight), "Dynamics", m_score.dynamics, false);
        drawDetails(g, area.reduced(0.f, 6.f));
    }

  private:
    static constexpr float kHeadlineHeight{56.f};
    static constexpr float kBarHeight{26.f};
    static constexpr float kLabelWidth{80.f};
    static constexpr float kValueWidth{52.f};
    static constexpr float kSubBarZeroShare{0.6f};

    [[nodiscard]] ListenPlayUi::Zone zoneOf(const float value) const noexcept
    {
        if (value >= m_score.passScore)
        {
            return ListenPlayUi::Zone::Ok;
        }
        return value >= m_score.passScore * kSubBarZeroShare ? ListenPlayUi::Zone::Warn : ListenPlayUi::Zone::Danger;
    }

    void drawHeadline(juce::Graphics& g, juce::Rectangle<float> area) const
    {
        const auto zone = m_score.passed ? ListenPlayUi::Zone::Ok : ListenPlayUi::Zone::Warn;
        ListenPlayUi::drawText(g, juce::String(std::lround(static_cast<double>(m_score.overall))) + " %",
                               area.removeFromLeft(area.getWidth() * 0.4f), 44.f, juce::Justification::centredLeft,
                               ListenPlayUi::zoneColour(zone));
        const juce::String verdict =
            m_score.passed
                ? "Passed"
                : "Not yet, from " + juce::String(std::lround(static_cast<double>(m_score.passScore))) + " % it counts";
        ListenPlayUi::drawText(g, verdict, area, 20.f, juce::Justification::centredLeft,
                               ListenPlayUi::textColour(0.9f));
    }

    void drawBar(juce::Graphics& g, juce::Rectangle<float> row, const juce::String& label, const float value,
                 const bool showPassMark) const
    {
        ListenPlayUi::drawText(g, label, row.removeFromLeft(kLabelWidth), 14.f, juce::Justification::centredLeft,
                               ListenPlayUi::textColour(0.8f));
        const auto valueArea = row.removeFromRight(kValueWidth);
        ListenPlayUi::drawText(g, juce::String(std::lround(static_cast<double>(value))) + " %", valueArea, 14.f,
                               juce::Justification::centredRight, ListenPlayUi::textColour(0.9f));
        const auto track = row.reduced(4.f, 6.f);
        g.setColour(juce::Colour(GuiConstants::instance().colors.backgroundDark));
        g.fillRoundedRectangle(track, 3.f);
        g.setColour(ListenPlayUi::zoneColour(zoneOf(value)));
        g.fillRoundedRectangle(track.withWidth(track.getWidth() * std::clamp(value, 0.f, 100.f) / 100.f), 3.f);
        if (showPassMark)
        {
            const float x = track.getX() + track.getWidth() * m_score.passScore / 100.f;
            g.setColour(ListenPlayUi::textColour(0.9f));
            g.drawVerticalLine(static_cast<int>(x), track.getY() - 3.f, track.getBottom() + 3.f);
        }
    }

    [[nodiscard]] juce::StringArray detailLines() const
    {
        juce::StringArray lines;
        lines.add("Found " + juce::String(m_score.matched) + " of " + juce::String(m_score.expected) + ", missed " +
                  juce::String(m_score.missed) + ", extra " + juce::String(m_score.extras));
        lines.add("Timing " + juce::String(m_score.meanTimingMs, 0) + " +/- " +
                  juce::String(m_score.timingSpreadMs, 0) + " ms (positive is late)");
        lines.add("Intonation " + juce::String(m_score.meanIntonationCents, 0) + " cents (positive is sharp)");
        if (!m_score.clickOn)
        {
            lines.add("Tempo " + juce::String(static_cast<double>(m_score.tempoRatio) * 100.0, 0) +
                      " % of the set tempo");
        }
        if (m_score.octaveShift != 0)
        {
            const int octaves = std::abs(m_score.octaveShift) / 12;
            lines.add("Played " + juce::String(octaves) + (octaves == 1 ? " octave " : " octaves ") +
                      (m_score.octaveShift > 0 ? "higher" : "lower"));
        }
        if (m_score.latencyKnown)
        {
            lines.add("Suggested latency " + juce::String(m_score.suggestedLatencyMs, 0) + " ms");
        }
        return lines;
    }

    void drawDetails(juce::Graphics& g, juce::Rectangle<float> area) const
    {
        for (const auto& line : detailLines())
        {
            ListenPlayUi::drawText(g, line, area.removeFromTop(20.f), 13.f, juce::Justification::centredLeft,
                                   ListenPlayUi::textColour(0.7f));
        }
    }

    ListenPlayDisplay::ScoreSnapshot m_score{};
};
