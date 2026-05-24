#pragma once

#include <cmath>
#include <juce_gui_basics/juce_gui_basics.h>
#include <vector>

#include "GuiConstants.h"

// Metronome-specific waveform display.
// The incoming data vector always has the beat marker at its centre sample.
// Timing zones: ±20ms (good), ±50ms (acceptable) are shaded.
class MetronomeWaveDisplay : public juce::Component
{
  public:
    MetronomeWaveDisplay() = default;

    void setSampleRate(float sampleRate) noexcept
    {
        m_sampleRate = sampleRate;
    }
    void setSamplesPerBeat(size_t spb) noexcept
    {
        m_samplesPerBeat = spb;
    }
    void setSubdivisionType(int type) noexcept
    {
        m_subdivisionType = type;
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

    void paint(juce::Graphics& g) override
    {
        const auto& c = GuiConstants::instance().colors;

        g.setColour(juce::Colour(c.cols[0]));
        g.fillRoundedRectangle(getLocalBounds().toFloat(), 3.f);

        if (m_data.size() < 2)
        {
            return;
        }

        constexpr float kPad = 4.f;
        constexpr float kTimeLabelH = 14.f;
        constexpr float kTitleH = 14.f;

        auto area = getLocalBounds().toFloat().reduced(kPad);
        area.removeFromTop(kTitleH);
        const auto timeLabelStrip = area.removeFromBottom(kTimeLabelH);
        const auto waveArea = area;

        const size_t n = m_data.size();
        const float cx = waveArea.getCentreX();
        const float samplesPerMs = m_sampleRate / 1000.f;
        const float pxPerSample = waveArea.getWidth() / static_cast<float>(n);
        const float halfWindowMs = static_cast<float>(n / 2) / samplesPerMs;

        // Timing zones (±50ms, ±20ms)
        auto fillZone = [&](float halfMs, juce::Colour colour)
        {
            const float hw = halfMs * samplesPerMs * pxPerSample;
            g.setColour(colour);
            g.fillRect(juce::Rectangle<float>(cx - hw, waveArea.getY(), 2.f * hw, waveArea.getHeight()));
        };
        fillZone(50.f, juce::Colour(c.cols[2]).withAlpha(0.20f));
        fillZone(20.f, juce::Colour(c.cols[3]).withAlpha(0.30f));

        // Horizontal amplitude grid
        g.setColour(juce::Colour(c.cols[4]).withAlpha(0.18f));
        for (int i = 1; i < 4; ++i)
        {
            const float y = waveArea.getY() + waveArea.getHeight() * static_cast<float>(i) / 4.f;
            g.drawHorizontalLine(static_cast<int>(y), waveArea.getX(), waveArea.getRight());
        }

        // Time grid lines and ms labels
        const float gridMs = halfWindowMs > 150.f ? 100.f : halfWindowMs > 60.f ? 50.f : 25.f;
        g.setFont(juce::Font(10.f));
        for (float ms = -halfWindowMs; ms <= halfWindowMs + 0.1f; ms += gridMs)
        {
            const float x = cx + ms * samplesPerMs * pxPerSample;
            if (x < waveArea.getX() || x > waveArea.getRight())
            {
                continue;
            }
            g.setColour(juce::Colour(c.cols[5]).withAlpha(0.30f));
            g.drawVerticalLine(static_cast<int>(x), waveArea.getY(), waveArea.getBottom());
            g.setColour(juce::Colour(c.cols[7]).withAlpha(0.75f));
            g.drawText(juce::String(static_cast<int>(std::round(ms))) + "ms", static_cast<int>(x) - 20,
                       static_cast<int>(timeLabelStrip.getY()), 40, static_cast<int>(kTimeLabelH),
                       juce::Justification::centred);
        }

        // Subdivision zones — shaded area + center line, rendered before the beat marker
        if (m_samplesPerBeat > 0 && m_subdivisionType > 0)
        {
            const size_t halfWindow = n / 2;
            const size_t spb = m_samplesPerBeat;
            std::vector<size_t> offsets;
            switch (m_subdivisionType)
            {
                case 1:
                    offsets = {spb / 2};
                    break;
                case 2:
                    offsets = {spb * 2 / 3};
                    break;
                case 3:
                    offsets = {spb / 3, spb * 2 / 3};
                    break;
                case 4:
                    offsets = {spb / 4, spb / 2, spb * 3 / 4};
                    break;
                default:
                    break;
            }

            constexpr float kSubZoneHalfMs = 15.f;
            const float subZoneHalfPx = kSubZoneHalfMs * samplesPerMs * pxPerSample;

            auto drawSubdivision = [&](float subX)
            {
                g.setColour(juce::Colour(c.cols[4]).withAlpha(0.35f));
                g.fillRect(juce::Rectangle<float>(subX - subZoneHalfPx, waveArea.getY(), 2.f * subZoneHalfPx,
                                                  waveArea.getHeight()));
                g.setColour(juce::Colour(c.cols[6]).withAlpha(0.70f));
                g.drawVerticalLine(static_cast<int>(subX), waveArea.getY(), waveArea.getBottom());
            };

            for (const size_t offset : offsets)
            {
                if (offset < halfWindow)
                {
                    drawSubdivision(cx + static_cast<float>(offset) * pxPerSample);
                }
                if (spb > offset && (spb - offset) <= halfWindow)
                {
                    drawSubdivision(cx - static_cast<float>(spb - offset) * pxPerSample);
                }
            }
        }

        // Beat marker (centre line) — drawn on top of subdivision markers
        g.setColour(juce::Colour(c.cols[9]).withAlpha(0.85f));
        g.drawVerticalLine(static_cast<int>(cx), waveArea.getY(), waveArea.getBottom());

        // Waveform
        g.setColour(juce::Colour(c.cols[8]));
        juce::Path path;
        const float xScale = waveArea.getWidth() / static_cast<float>(n - 1);
        const float yScale = waveArea.getHeight() / 2.f;
        const float waveCy = waveArea.getCentreY();
        path.startNewSubPath(waveArea.getX(), waveCy - m_data[0] * yScale);
        for (size_t i = 1; i < n; ++i)
        {
            path.lineTo(waveArea.getX() + static_cast<float>(i) * xScale, waveCy - m_data[i] * yScale);
        }
        g.strokePath(path, juce::PathStrokeType(1.5f));

        // Title / label
        if (m_label.isNotEmpty())
        {
            g.setColour(juce::Colour(c.cols[7]).withAlpha(0.75f));
            g.setFont(juce::Font(11.f));
            g.drawText(m_label, getLocalBounds().removeFromTop(static_cast<int>(kTitleH + kPad)),
                       juce::Justification::centred);
        }
    }

  private:
    std::vector<float> m_data;
    float m_sampleRate{48000.f};
    size_t m_samplesPerBeat{0};
    int m_subdivisionType{0};
    juce::String m_label;
};
