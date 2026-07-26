#pragma once

#include <cmath>
#include <juce_gui_basics/juce_gui_basics.h>
#include <numbers>
#include <vector>

#include "GuiConstants.h"

// Circular bar visualizer: the full circle is one bar. The downbeat is at 12
// o'clock and time advances clockwise. Beat markers ring the circle (downbeat
// emphasized), subdivision ticks sit within each beat, a hand sweeps to the
// current bar phase, and the bar's audio is drawn as radial amplitude.
class CircularBarDisplay : public juce::Component
{
  public:
    CircularBarDisplay() = default;

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
    // Subdivision offsets within a single beat (0..samplesPerBeat).
    void setSubdivisionPositions(const std::vector<size_t>& positions)
    {
        m_subdivisionPositions = positions;
    }

    void update(const std::vector<float>& data)
    {
        if (!data.empty())
        {
            m_data = data;
        }
        repaint();
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

        if (m_data.size() < 2 || m_samplesPerBar == 0 || m_barBeats <= 0)
        {
            return;
        }

        constexpr float kPad = 8.f;
        constexpr float kTitleH = 16.f;
        auto bounds = getLocalBounds().toFloat().reduced(kPad);
        if (m_label.isNotEmpty())
        {
            bounds.removeFromTop(kTitleH);
        }

        const float cx = bounds.getCentreX();
        const float cy = bounds.getCentreY();
        const float minDim = std::min(bounds.getWidth(), bounds.getHeight());
        const float baseR = minDim * 0.36f;
        const float amplScale = baseR * 0.22f;
        const float innerR = baseR - amplScale;
        const float outerR = baseR + amplScale;

        constexpr float kHalfPi = std::numbers::pi_v<float> / 2.f;
        constexpr float k2Pi = std::numbers::pi_v<float> * 2.f;
        constexpr float kBeatAngle = -kHalfPi; // downbeat at 12 o'clock

        const auto polarPt = [&](float angle, float r) noexcept
        { return juce::Point<float>(cx + r * std::cos(angle), cy + r * std::sin(angle)); };

        const size_t n = m_data.size();
        const size_t barBeats = static_cast<size_t>(m_barBeats);
        const size_t spb = m_samplesPerBar / barBeats;

        // --- Base rings ---
        g.setColour(juce::Colour(c.cols[4]).withAlpha(0.20f));
        g.drawEllipse(cx - innerR, cy - innerR, 2.f * innerR, 2.f * innerR, 1.f);
        g.drawEllipse(cx - outerR, cy - outerR, 2.f * outerR, 2.f * outerR, 1.f);
        g.setColour(juce::Colour(c.cols[4]).withAlpha(0.12f));
        g.drawEllipse(cx - baseR, cy - baseR, 2.f * baseR, 2.f * baseR, 0.5f);

        // --- Subdivision ticks (within each beat) ---
        if (spb > 0 && !m_subdivisionPositions.empty())
        {
            g.setColour(juce::Colour(c.cols[6]).withAlpha(0.55f));
            for (size_t b = 0; b < barBeats; ++b)
            {
                for (const size_t offset : m_subdivisionPositions)
                {
                    const float frac = static_cast<float>(b * spb + offset) / static_cast<float>(m_samplesPerBar);
                    const float angle = kBeatAngle + frac * k2Pi;
                    g.drawLine(juce::Line<float>(polarPt(angle, baseR * 0.9f), polarPt(angle, baseR * 1.1f)), 1.0f);
                }
            }
        }

        // --- Beat markers (downbeat emphasized) ---
        for (size_t k = 0; k < barBeats; ++k)
        {
            const float angle = kBeatAngle + static_cast<float>(k) / static_cast<float>(barBeats) * k2Pi;
            const bool downbeat = (k == 0);
            g.setColour(juce::Colour(c.cols[downbeat ? 9 : 4]).withAlpha(downbeat ? 0.90f : 0.55f));
            g.drawLine(
                juce::Line<float>(polarPt(angle, innerR * 0.5f), polarPt(angle, outerR * (downbeat ? 1.2f : 1.1f))),
                downbeat ? 2.5f : 1.5f);
        }

        // --- Bar waveform ---
        {
            juce::Path wave;
            for (size_t i = 0; i < n; ++i)
            {
                const float angle = kBeatAngle + static_cast<float>(i) / static_cast<float>(n) * k2Pi;
                const auto pt = polarPt(angle, baseR + m_data[i] * amplScale);
                if (i == 0)
                {
                    wave.startNewSubPath(pt);
                }
                else
                {
                    wave.lineTo(pt);
                }
            }
            g.setColour(juce::Colour(c.cols[8]));
            g.strokePath(wave, juce::PathStrokeType(1.5f));
        }

        // --- Sweeping hand at the current bar phase ---
        {
            const float handAngle = kBeatAngle + std::clamp(m_barPhase, 0.f, 1.f) * k2Pi;
            g.setColour(juce::Colour(c.cols[7]).withAlpha(0.90f));
            g.drawLine(juce::Line<float>(polarPt(handAngle, 0.f), polarPt(handAngle, outerR)), 2.0f);
        }

        // --- Centre hub ---
        constexpr float kHubR = 4.f;
        g.setColour(juce::Colour(c.cols[9]).withAlpha(0.85f));
        g.fillEllipse(cx - kHubR, cy - kHubR, 2.f * kHubR, 2.f * kHubR);

        // --- Title ---
        if (m_label.isNotEmpty())
        {
            const auto titleBounds = getLocalBounds().toFloat().reduced(kPad).removeFromTop(kTitleH);
            g.setColour(juce::Colour(c.cols[7]).withAlpha(0.75f));
            g.setFont(juce::Font(juce::FontOptions(11.f)));
            g.drawText(m_label, titleBounds.toNearestInt(), juce::Justification::centred);
        }
    }

  private:
    std::vector<float> m_data;
    float m_sampleRate{48000.f};
    size_t m_samplesPerBar{0};
    int m_barBeats{4};
    float m_barPhase{0.f};
    std::vector<size_t> m_subdivisionPositions;
    juce::String m_label;
};
