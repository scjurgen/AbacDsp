#pragma once

#include <algorithm>
#include <cmath>
#include <juce_gui_basics/juce_gui_basics.h>
#include <numbers>
#include <vector>

#include "GuiConstants.h"

// Merged looper "clock": an inner disc showing the current bar in detail and an
// outer ring showing the whole loop. Both share a centre and downbeat at 12
// o'clock, time advancing clockwise. Two hands sweep from the hub like a clock:
// the short fast hand tracks the bar phase (one turn per bar), the long slow hand
// tracks the loop playhead (one turn per loop). The outer ring's angular span is
// the loop length in bars, growing while recording.
class CircularLoopDisplay : public juce::Component
{
  public:
    CircularLoopDisplay() = default;

    // --- Inner bar disc ---
    void setSampleRate(float sampleRate) noexcept
    {
        m_sampleRate = sampleRate;
    }
    void setSamplesPerBar(size_t samplesPerBar) noexcept
    {
        m_samplesPerBar = samplesPerBar;
    }
    void setBarBeats(int barBeats) noexcept
    {
        m_barBeats = barBeats;
    }
    void setBarPhase(float phase) noexcept
    {
        m_barPhase = phase;
    }
    void setSubdivisionPositions(const std::vector<size_t>& positions)
    {
        m_subdivisionPositions = positions;
    }

    // Inner-disc audio for the bar in progress (fed via the default signal gauge).
    void update(const std::vector<float>& data)
    {
        if (!data.empty())
        {
            m_data = data;
        }
        repaint();
    }

    // --- Outer loop ring ---
    void setLoopWaveform(const std::vector<float>& peaks)
    {
        m_loopPeaks = peaks;
    }
    void setSliceBoundaries(const std::vector<float>& normalized)
    {
        m_boundaries = normalized;
    }
    void setPlayheadNormalized(float normalized) noexcept
    {
        m_playhead = normalized;
    }
    void setOuterRingBars(int bars) noexcept
    {
        m_outerRingBars = std::max(1, bars);
    }
    void setStateLabel(const juce::String& label)
    {
        m_stateLabel = label;
    }

    void setLabelText(const juce::String& label)
    {
        m_label = label;
        repaint();
    }

    void updateColors()
    {
        repaint();
    }

    void paint(juce::Graphics& g) override
    {
        const auto& c = GuiConstants::instance().colors;

        g.setColour(juce::Colour(c.cols[0]));
        g.fillRoundedRectangle(getLocalBounds().toFloat(), 3.f);

        if (m_samplesPerBar == 0 || m_barBeats <= 0)
        {
            return;
        }

        auto bounds = getLocalBounds().toFloat().reduced(kPad);
        bounds.removeFromTop(kTitleH);

        const Geometry geo{bounds};
        drawOuterRing(g, geo, c);
        drawInnerDisc(g, geo, c);
        drawHands(g, geo, c);
        drawHub(g, geo, c);
        drawLabels(g, c);
    }

  private:
    static constexpr float kPad = 8.f;
    static constexpr float kTitleH = 16.f;
    static constexpr float kHalfPi = std::numbers::pi_v<float> / 2.f;
    static constexpr float k2Pi = std::numbers::pi_v<float> * 2.f;
    static constexpr float kBeatAngle = -kHalfPi; // downbeat at 12 o'clock

    struct Geometry
    {
        explicit Geometry(juce::Rectangle<float> b) noexcept
            : cx(b.getCentreX())
            , cy(b.getCentreY())
            , maxR(std::min(b.getWidth(), b.getHeight()) * 0.5f - 4.f)
            , ringOuterR(maxR * 0.98f)
            , ringInnerR(maxR * 0.70f)
            , baseR(maxR * 0.42f)
            , amplScale(maxR * 0.10f)
            , innerR(baseR - amplScale)
            , outerR(baseR + amplScale)
        {
        }
        float cx, cy, maxR, ringOuterR, ringInnerR, baseR, amplScale, innerR, outerR;
    };

    [[nodiscard]] juce::Point<float> polar(const Geometry& geo, float angle, float r) const noexcept
    {
        return {geo.cx + r * std::cos(angle), geo.cy + r * std::sin(angle)};
    }

    void drawOuterRing(juce::Graphics& g, const Geometry& geo, const GuiConstants::Colors& c) const
    {
        g.setColour(juce::Colour(c.cols[4]).withAlpha(0.20f));
        g.drawEllipse(geo.cx - geo.ringInnerR, geo.cy - geo.ringInnerR, 2.f * geo.ringInnerR, 2.f * geo.ringInnerR,
                      1.f);
        g.drawEllipse(geo.cx - geo.ringOuterR, geo.cy - geo.ringOuterR, 2.f * geo.ringOuterR, 2.f * geo.ringOuterR,
                      1.f);

        drawLoopWaveformBand(g, geo, c);
        drawBarSpokes(g, geo, c);
        drawSliceSpokes(g, geo, c);
    }

    void drawLoopWaveformBand(juce::Graphics& g, const Geometry& geo, const GuiConstants::Colors& c) const
    {
        if (m_loopPeaks.size() < 2)
        {
            return;
        }
        const float band = geo.ringOuterR - geo.ringInnerR;
        const float n = static_cast<float>(m_loopPeaks.size());
        g.setColour(juce::Colour(c.cols[8]));
        for (size_t i = 0; i < m_loopPeaks.size(); ++i)
        {
            const float angle = kBeatAngle + static_cast<float>(i) / n * k2Pi;
            const float r = geo.ringInnerR + juce::jlimit(0.f, 1.f, m_loopPeaks[i]) * band;
            g.drawLine(juce::Line<float>(polar(geo, angle, geo.ringInnerR), polar(geo, angle, r)), 1.0f);
        }
    }

    void drawBarSpokes(juce::Graphics& g, const Geometry& geo, const GuiConstants::Colors& c) const
    {
        for (int bar = 0; bar < m_outerRingBars; ++bar)
        {
            const float angle = kBeatAngle + static_cast<float>(bar) / static_cast<float>(m_outerRingBars) * k2Pi;
            const bool first = (bar == 0);
            g.setColour(juce::Colour(c.cols[first ? 9 : 4]).withAlpha(first ? 0.90f : 0.45f));
            g.drawLine(
                juce::Line<float>(polar(geo, angle, geo.ringInnerR * 0.98f), polar(geo, angle, geo.ringOuterR * 1.02f)),
                first ? 2.5f : 1.2f);
        }
    }

    void drawSliceSpokes(juce::Graphics& g, const Geometry& geo, const GuiConstants::Colors& c) const
    {
        g.setColour(juce::Colour(c.cols[6]).withAlpha(0.60f));
        for (const float b : m_boundaries)
        {
            const float angle = kBeatAngle + juce::jlimit(0.f, 1.f, b) * k2Pi;
            g.drawLine(juce::Line<float>(polar(geo, angle, geo.ringInnerR), polar(geo, angle, geo.ringOuterR)), 1.0f);
        }
    }

    void drawInnerDisc(juce::Graphics& g, const Geometry& geo, const GuiConstants::Colors& c) const
    {
        const size_t barBeats = static_cast<size_t>(m_barBeats);
        const size_t spb = m_samplesPerBar / barBeats;

        g.setColour(juce::Colour(c.cols[4]).withAlpha(0.18f));
        g.drawEllipse(geo.cx - geo.baseR, geo.cy - geo.baseR, 2.f * geo.baseR, 2.f * geo.baseR, 0.5f);

        if (spb > 0 && !m_subdivisionPositions.empty())
        {
            g.setColour(juce::Colour(c.cols[6]).withAlpha(0.50f));
            for (size_t b = 0; b < barBeats; ++b)
            {
                for (const size_t offset : m_subdivisionPositions)
                {
                    const float frac = static_cast<float>(b * spb + offset) / static_cast<float>(m_samplesPerBar);
                    const float angle = kBeatAngle + frac * k2Pi;
                    g.drawLine(
                        juce::Line<float>(polar(geo, angle, geo.baseR * 0.9f), polar(geo, angle, geo.baseR * 1.1f)),
                        1.0f);
                }
            }
        }

        for (size_t k = 0; k < barBeats; ++k)
        {
            const float angle = kBeatAngle + static_cast<float>(k) / static_cast<float>(barBeats) * k2Pi;
            const bool downbeat = (k == 0);
            g.setColour(juce::Colour(c.cols[downbeat ? 9 : 4]).withAlpha(downbeat ? 0.90f : 0.55f));
            g.drawLine(juce::Line<float>(polar(geo, angle, geo.innerR * 0.5f),
                                         polar(geo, angle, geo.outerR * (downbeat ? 1.2f : 1.1f))),
                       downbeat ? 2.5f : 1.5f);
        }

        drawBarWaveform(g, geo, c);
    }

    void drawBarWaveform(juce::Graphics& g, const Geometry& geo, const GuiConstants::Colors& c) const
    {
        if (m_data.size() < 2)
        {
            return;
        }
        const size_t n = m_data.size();
        juce::Path wave;
        for (size_t i = 0; i < n; ++i)
        {
            const float angle = kBeatAngle + static_cast<float>(i) / static_cast<float>(n) * k2Pi;
            const auto pt = polar(geo, angle, geo.baseR + m_data[i] * geo.amplScale);
            if (i == 0)
            {
                wave.startNewSubPath(pt);
            }
            else
            {
                wave.lineTo(pt);
            }
        }
        g.setColour(juce::Colour(c.cols[8]).withAlpha(0.85f));
        g.strokePath(wave, juce::PathStrokeType(1.5f));
    }

    void drawHands(juce::Graphics& g, const Geometry& geo, const GuiConstants::Colors& c) const
    {
        // Slow hand: loop playhead, drawn only across the outer ring so it never
        // reaches the centre (that space belongs to the fast bar hand).
        const float loopAngle = kBeatAngle + juce::jlimit(0.f, 1.f, m_playhead) * k2Pi;
        g.setColour(juce::Colour(c.cols[9]).withAlpha(0.85f));
        g.drawLine(juce::Line<float>(polar(geo, loopAngle, geo.ringInnerR), polar(geo, loopAngle, geo.ringOuterR)),
                   2.5f);

        // Short fast hand: bar phase, within the inner disc.
        const float barAngle = kBeatAngle + std::clamp(m_barPhase, 0.f, 1.f) * k2Pi;
        g.setColour(juce::Colour(c.cols[7]).withAlpha(0.90f));
        g.drawLine(juce::Line<float>(polar(geo, barAngle, 0.f), polar(geo, barAngle, geo.outerR)), 2.0f);
    }

    void drawHub(juce::Graphics& g, const Geometry& geo, const GuiConstants::Colors& c) const
    {
        constexpr float kHubR = 4.f;
        g.setColour(juce::Colour(c.cols[9]).withAlpha(0.85f));
        g.fillEllipse(geo.cx - kHubR, geo.cy - kHubR, 2.f * kHubR, 2.f * kHubR);
    }

    void drawLabels(juce::Graphics& g, const GuiConstants::Colors& c) const
    {
        const auto titleBounds = getLocalBounds().toFloat().reduced(kPad).removeFromTop(kTitleH);
        g.setFont(juce::Font(juce::FontOptions(12.f)));
        if (m_stateLabel.isNotEmpty())
        {
            g.setColour(juce::Colour(c.cols[7]).withAlpha(0.85f));
            g.drawText(m_stateLabel, titleBounds, juce::Justification::centredLeft);
        }
        if (m_label.isNotEmpty())
        {
            g.setColour(juce::Colour(c.cols[7]).withAlpha(0.55f));
            g.drawText(m_label, titleBounds, juce::Justification::centredRight);
        }
    }

    std::vector<float> m_data;
    std::vector<float> m_loopPeaks;
    std::vector<float> m_boundaries;
    std::vector<size_t> m_subdivisionPositions;
    float m_sampleRate{48000.f};
    size_t m_samplesPerBar{0};
    int m_barBeats{4};
    float m_barPhase{0.f};
    float m_playhead{0.f};
    int m_outerRingBars{1};
    juce::String m_stateLabel;
    juce::String m_label;
};
