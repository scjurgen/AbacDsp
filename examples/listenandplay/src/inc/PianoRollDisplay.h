#pragma once

#include <algorithm>
#include <cmath>
#include <juce_graphics/juce_graphics.h>

#include "../impl/DisplaySnapshots.h"
#include "Harmony/ChordName.h"
#include "ListenPlayUi.h"

/**
 * Before a take of a phrase has been scored the display tells nothing about the phrase: only what is going on
 * (listen, count-in, play it back, analysing) with a time bar or the count-in beats. Once the result is in it
 * is a linear piano roll: time in beats with the downbeat as zero (a pickup lies to its left), pitch upwards.
 * The phrase is drawn as grey bars, brighter for louder notes, with the played notes over it coloured by how
 * right and how on time they were; missed notes are outlined in the danger colour.
 */
class PianoRollDisplay final : public ListenPlayUi::DisplayFrame
{
  public:
    void setSnapshot(const ListenPlayDisplay::PianoRollSnapshot& snapshot)
    {
        m_roll = snapshot;
        repaint();
    }

    void paint(juce::Graphics& g) override
    {
        const auto area = paintFrame(g);
        if (!m_roll.revealed)
        {
            drawNeutral(g, area);
            return;
        }
        const Mapping map = makeMapping(area);
        drawGrid(g, map);
        drawTarget(g, map);
        drawPlayed(g, map);
    }

  private:
    static constexpr float kGutterWidth{34.f};
    static constexpr float kAxisHeight{16.f};
    static constexpr float kPitchMargin{1.5f};
    static constexpr float kMinPitchSpan{11.f};
    static constexpr float kTimeMarginBeats{0.25f};
    static constexpr float kTargetHeight{0.8f};
    static constexpr float kPlayedHeight{0.5f};
    static constexpr float kMinNameWidth{26.f};
    static constexpr float kPanelHeight{110.f};
    static constexpr float kHeadlineHeight{52.f};
    static constexpr float kIndicatorHeight{30.f};
    static constexpr float kProgressHeight{10.f};

    struct Mapping
    {
        juce::Rectangle<float> plot;
        float tMin{0.f};
        float tMax{1.f};
        float pitchLow{48.f};
        float pitchHigh{72.f};

        [[nodiscard]] float x(const float beats) const noexcept
        {
            return plot.getX() + (beats - tMin) / (tMax - tMin) * plot.getWidth();
        }

        [[nodiscard]] float rowHeight() const noexcept
        {
            return plot.getHeight() / (pitchHigh - pitchLow + 1.f);
        }

        // The centre of the row of a (fractional) pitch.
        [[nodiscard]] float y(const float midi) const noexcept
        {
            return plot.getBottom() - (midi - pitchLow + 0.5f) * rowHeight();
        }
    };

    [[nodiscard]] Mapping makeMapping(const juce::Rectangle<float>& area) const
    {
        Mapping map;
        auto plot = area;
        plot.removeFromLeft(kGutterWidth);
        plot.removeFromBottom(kAxisHeight);
        map.plot = plot;
        map.tMin = -std::max(m_roll.pickupBeats, 0.f) - kTimeMarginBeats;
        map.tMax = m_roll.patternBeats + kTimeMarginBeats;
        float low = 127.f;
        float high = 0.f;
        for (size_t i = 0; i < m_roll.targetCount; ++i)
        {
            low = std::min(low, static_cast<float>(m_roll.target[i].pitch));
            high = std::max(high, static_cast<float>(m_roll.target[i].pitch));
        }
        for (size_t i = 0; i < m_roll.playedCount; ++i)
        {
            const auto& note = m_roll.played[i];
            low = std::min(low, note.midi);
            high = std::max(high, note.midi);
            map.tMin = std::min(map.tMin, note.startBeats);
            map.tMax = std::max(map.tMax, note.startBeats + note.lengthBeats);
        }
        map.pitchLow = std::floor(low - kPitchMargin);
        map.pitchHigh = std::ceil(high + kPitchMargin);
        const float missing = kMinPitchSpan - (map.pitchHigh - map.pitchLow);
        if (missing > 0.f)
        {
            map.pitchLow -= std::floor(missing * 0.5f);
            map.pitchHigh += std::ceil(missing * 0.5f);
        }
        return map;
    }

    [[nodiscard]] static juce::String pitchName(const int midi)
    {
        return juce::String(std::string(AbacDsp::noteName(((midi % 12) + 12) % 12))) + juce::String(midi / 12 - 1);
    }

    void drawGrid(juce::Graphics& g, const Mapping& map) const
    {
        g.setColour(juce::Colour(GuiConstants::instance().colors.backgroundDark));
        g.fillRect(map.plot);
        const float downbeatX = map.x(0.f);
        if (downbeatX > map.plot.getX())
        {
            g.setColour(ListenPlayUi::textColour(0.06f));
            g.fillRect(map.plot.withRight(downbeatX));
        }
        const auto perBar = static_cast<int>(m_roll.beatsPerBar);
        for (int beat = static_cast<int>(std::ceil(map.tMin)); beat <= static_cast<int>(std::floor(map.tMax)); ++beat)
        {
            const bool barLine = ((beat % perBar) + perBar) % perBar == 0;
            g.setColour(ListenPlayUi::textColour(barLine ? 0.35f : 0.12f));
            g.drawVerticalLine(static_cast<int>(map.x(static_cast<float>(beat))), map.plot.getY(),
                               map.plot.getBottom());
            if (barLine && beat >= 0)
            {
                ListenPlayUi::drawText(g, juce::String(beat / perBar + 1),
                                       {map.x(static_cast<float>(beat)) + 3.f, map.plot.getBottom(), 24.f, kAxisHeight},
                                       11.f, juce::Justification::centredLeft, ListenPlayUi::textColour(0.6f));
            }
        }
        for (int midi = static_cast<int>(map.pitchLow); midi <= static_cast<int>(map.pitchHigh); ++midi)
        {
            if (midi % 12 == 0)
            {
                g.setColour(ListenPlayUi::textColour(0.15f));
                g.drawHorizontalLine(static_cast<int>(map.y(static_cast<float>(midi))), map.plot.getX(),
                                     map.plot.getRight());
                ListenPlayUi::drawText(
                    g, pitchName(midi),
                    {map.plot.getX() - kGutterWidth, map.y(static_cast<float>(midi)) - 8.f, kGutterWidth - 4.f, 16.f},
                    11.f, juce::Justification::centredRight, ListenPlayUi::textColour(0.6f));
            }
        }
    }

    [[nodiscard]] juce::Rectangle<float> barOf(const Mapping& map, const float start, const float length,
                                               const float midi, const float heightShare) const
    {
        const float height = std::max(3.f, map.rowHeight() * heightShare);
        const float x = map.x(start);
        return {x, map.y(midi) - 0.5f * height, std::max(3.f, map.x(start + length) - x - 1.f), height};
    }

    void drawTarget(juce::Graphics& g, const Mapping& map) const
    {
        for (size_t i = 0; i < m_roll.targetCount; ++i)
        {
            const auto& note = m_roll.target[i];
            const auto bar =
                barOf(map, note.startBeats, note.lengthBeats, static_cast<float>(note.pitch), kTargetHeight);
            const float loudness = std::clamp(static_cast<float>(note.velocity) / 127.f, 0.f, 1.f);
            g.setColour(ListenPlayUi::textColour(0.18f + 0.45f * loudness));
            g.fillRoundedRectangle(bar, 2.f);
            if (note.missed)
            {
                g.setColour(ListenPlayUi::zoneColour(ListenPlayUi::Zone::Danger));
                g.drawRoundedRectangle(bar, 2.f, 2.f);
            }
            if (bar.getWidth() >= kMinNameWidth)
            {
                ListenPlayUi::drawText(g, pitchName(note.pitch), bar.reduced(3.f, 0.f), 11.f,
                                       juce::Justification::centredLeft,
                                       juce::Colour(GuiConstants::instance().colors.backgroundDark));
            }
        }
    }

    void drawPlayed(juce::Graphics& g, const Mapping& map) const
    {
        for (size_t i = 0; i < m_roll.playedCount; ++i)
        {
            const auto& note = m_roll.played[i];
            const auto bar = barOf(map, note.startBeats, note.lengthBeats, note.midi, kPlayedHeight);
            if (note.extra)
            {
                g.setColour(ListenPlayUi::zoneColour(ListenPlayUi::Zone::Danger));
                g.drawRoundedRectangle(bar, 2.f, 1.5f);
                continue;
            }
            g.setColour(ListenPlayUi::zoneColour(ListenPlayUi::zoneOfCredit(note.credit)));
            g.fillRoundedRectangle(bar, 2.f);
            g.setColour(juce::Colour(GuiConstants::instance().colors.backgroundDark).withAlpha(0.6f));
            g.drawRoundedRectangle(bar, 2.f, 1.f);
        }
    }

    [[nodiscard]] juce::String headline() const
    {
        using Phase = ListenPlayDisplay::Phase;
        if (!m_roll.hasPhrase)
        {
            return "Load a lesson on the Lessons page";
        }
        if (m_roll.reference)
        {
            return "Reference tone";
        }
        switch (m_roll.phase)
        {
            case Phase::Listening:
                return "Listen";
            case Phase::CountIn:
                return "Count-in";
            case Phase::Capturing:
                return "Play it back";
            case Phase::Analysing:
                return "Analysing...";
            case Phase::Replaying:
                return "Replaying your take";
            case Phase::Idle:
            case Phase::Review:
                return "Press Play to hear the phrase";
        }
        return {};
    }

    void drawBeatDots(juce::Graphics& g, const juce::Rectangle<float>& area) const
    {
        const float diameter = std::min(22.f, area.getHeight());
        const float pitch = diameter * 1.6f;
        const float width = pitch * static_cast<float>(m_roll.countInBeats) - (pitch - diameter);
        float x = area.getCentreX() - 0.5f * width;
        for (int beat = 1; beat <= static_cast<int>(m_roll.countInBeats); ++beat)
        {
            const juce::Rectangle<float> dot{x, area.getCentreY() - 0.5f * diameter, diameter, diameter};
            g.setColour(beat <= m_roll.countInBeat ? ListenPlayUi::accentColour(7) : ListenPlayUi::textColour(0.2f));
            g.fillEllipse(dot);
            x += pitch;
        }
    }

    void drawProgress(juce::Graphics& g, const juce::Rectangle<float>& area) const
    {
        const auto track = area.withSizeKeepingCentre(area.getWidth() * 0.6f, kProgressHeight);
        g.setColour(juce::Colour(GuiConstants::instance().colors.backgroundDark));
        g.fillRoundedRectangle(track, 3.f);
        g.setColour(ListenPlayUi::accentColour(7));
        g.fillRoundedRectangle(track.withWidth(track.getWidth() * std::clamp(m_roll.progress, 0.f, 1.f)), 3.f);
    }

    void drawNeutral(juce::Graphics& g, const juce::Rectangle<float>& area) const
    {
        using Phase = ListenPlayDisplay::Phase;
        auto panel = area.withSizeKeepingCentre(area.getWidth() * 0.8f, kPanelHeight);
        ListenPlayUi::drawText(g, headline(), panel.removeFromTop(kHeadlineHeight), 30.f, juce::Justification::centred,
                               ListenPlayUi::textColour(0.9f));
        auto indicator = panel.removeFromTop(kIndicatorHeight);
        if (!m_roll.hasPhrase || m_roll.reference)
        {
            return;
        }
        if (m_roll.phase == Phase::CountIn)
        {
            drawBeatDots(g, indicator);
        }
        else if (m_roll.phase == Phase::Listening || m_roll.phase == Phase::Capturing ||
                 m_roll.phase == Phase::Analysing)
        {
            drawProgress(g, indicator);
        }
        else if (m_roll.hasReference)
        {
            ListenPlayUi::drawText(g, "The Reference button plays the key note", indicator, 14.f,
                                   juce::Justification::centred, ListenPlayUi::textColour(0.55f));
        }
    }

    ListenPlayDisplay::PianoRollSnapshot m_roll{};
};
