#pragma once

#include <cmath>
#include <juce_gui_basics/juce_gui_basics.h>
#include <numbers>
#include <vector>

#include "GuiConstants.h"

// Circular beat visualizer.
// The full circle represents one beat period; strong beat is at 12 o'clock,
// time advances clockwise. The audio waveform is plotted as radial amplitude
// deviation from the base ring; subdivision ticks appear at their angular positions.
class CircularBeatDisplay : public juce::Component
{
  public:
    CircularBeatDisplay() = default;

    void setSampleRate(float sampleRate) noexcept
    {
        m_sampleRate = sampleRate;
    }
    void setSamplesPerBeat(size_t spb) noexcept
    {
        m_samplesPerBeat = spb;
    }
    void setSubdivisionPositions(const std::vector<size_t>& positions)
    {
        m_subdivisionPositions = positions;
    }
    void setBeatIndex(size_t beatIndex) noexcept
    {
        m_beatIndex = beatIndex;
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

        if (m_data.size() < 2 || m_samplesPerBeat == 0)
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

        const size_t n = m_data.size();
        const size_t spb = m_samplesPerBeat;
        const size_t beatIdx = (m_beatIndex > 0 && m_beatIndex < n) ? m_beatIndex : n / 4;
        const float samplesPerMs = m_sampleRate / 1000.f;

        constexpr float kHalfPi = std::numbers::pi_v<float> / 2.f;
        constexpr float k2Pi = std::numbers::pi_v<float> * 2.f;
        constexpr float kBeatAngle = -kHalfPi; // 12 o'clock

        const auto polarPt = [&](float angle, float r) noexcept
        { return juce::Point<float>(cx + r * std::cos(angle), cy + r * std::sin(angle)); };

        // Returns the angular half-width for ±ms around the beat
        const auto halfAngleForMs = [&](float ms) noexcept
        { return ms * samplesPerMs / static_cast<float>(spb) * k2Pi; };

        // Filled arc sector between two radii
        const auto fillSector =
            [&](float fromAngle, float toAngle, float r0, float r1, juce::Colour colour, int steps = 80)
        {
            const float dAngle = (toAngle - fromAngle) / static_cast<float>(steps);
            juce::Path p;
            p.startNewSubPath(polarPt(fromAngle, r0));
            for (int i = 0; i <= steps; ++i)
            {
                p.lineTo(polarPt(fromAngle + static_cast<float>(i) * dAngle, r0));
            }
            for (int i = steps; i >= 0; --i)
            {
                p.lineTo(polarPt(fromAngle + static_cast<float>(i) * dAngle, r1));
            }
            p.closeSubPath();
            g.setColour(colour);
            g.fillPath(p);
        };

        // --- Timing zone halos (±50 ms, ±20 ms) ---
        fillSector(kBeatAngle - halfAngleForMs(50.f), kBeatAngle + halfAngleForMs(50.f), innerR, outerR,
                   juce::Colour(c.cols[2]).withAlpha(0.20f));
        fillSector(kBeatAngle - halfAngleForMs(20.f), kBeatAngle + halfAngleForMs(20.f), innerR, outerR,
                   juce::Colour(c.cols[3]).withAlpha(0.30f));

        // --- Base rings ---
        g.setColour(juce::Colour(c.cols[4]).withAlpha(0.20f));
        g.drawEllipse(cx - innerR, cy - innerR, 2.f * innerR, 2.f * innerR, 1.f);
        g.drawEllipse(cx - outerR, cy - outerR, 2.f * outerR, 2.f * outerR, 1.f);
        g.setColour(juce::Colour(c.cols[4]).withAlpha(0.12f));
        g.drawEllipse(cx - baseR, cy - baseR, 2.f * baseR, 2.f * baseR, 0.5f);

        // --- Subdivision markers ---
        if (!m_subdivisionPositions.empty())
        {
            constexpr float kSubHalfMs = 12.f;
            constexpr float kTickInner = 0.84f;
            constexpr float kTickOuter = 1.16f;

            for (const size_t offset : m_subdivisionPositions)
            {
                const float subAngle = kBeatAngle + static_cast<float>(offset) / static_cast<float>(spb) * k2Pi;
                fillSector(subAngle - halfAngleForMs(kSubHalfMs), subAngle + halfAngleForMs(kSubHalfMs),
                           baseR * kTickInner, baseR * kTickOuter, juce::Colour(c.cols[4]).withAlpha(0.28f), 24);
                g.setColour(juce::Colour(c.cols[6]).withAlpha(0.70f));
                g.drawLine(
                    juce::Line<float>(polarPt(subAngle, baseR * kTickInner), polarPt(subAngle, baseR * kTickOuter)),
                    1.5f);
            }
        }

        // --- Beat marker (12 o'clock) ---
        g.setColour(juce::Colour(c.cols[9]).withAlpha(0.90f));
        g.drawLine(juce::Line<float>(polarPt(kBeatAngle, innerR * 0.55f), polarPt(kBeatAngle, outerR * 1.18f)), 2.5f);

        // --- Circular waveform ---
        {
            juce::Path wave;
            bool started = false;
            for (size_t i = 0; i < n; ++i)
            {
                const float angle =
                    kBeatAngle + (static_cast<float>(i) - static_cast<float>(beatIdx)) / static_cast<float>(spb) * k2Pi;
                const float r = baseR + m_data[i] * amplScale;
                const auto pt = polarPt(angle, r);
                if (!started)
                {
                    wave.startNewSubPath(pt);
                    started = true;
                }
                else
                {
                    wave.lineTo(pt);
                }
            }
            g.setColour(juce::Colour(c.cols[8]));
            g.strokePath(wave, juce::PathStrokeType(1.5f));
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
    size_t m_samplesPerBeat{0};
    size_t m_beatIndex{0};
    std::vector<size_t> m_subdivisionPositions;
    juce::String m_label;
};