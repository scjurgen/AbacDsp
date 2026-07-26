#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <vector>

#include "GuiConstants.h"

// Loop overview for a slicing looper: a magnitude waveform of the recorded loop
// with vertical markers at slice boundaries and a moving playhead, plus a state
// label. Data is pushed from the processor each UI tick (normalised 0..1 for
// boundaries and playhead; the waveform is a peak-per-column array).
class SliceWaveDisplay : public juce::Component
{
  public:
    SliceWaveDisplay() = default;

    void setSampleRate(float sampleRate) noexcept
    {
        m_sampleRate = sampleRate;
    }

    void setLoopWaveform(const std::vector<float>& peaks)
    {
        m_peaks = peaks;
    }

    // Satisfies the default "signal" gauge callback; real data arrives via the
    // setters above, so the pushed argument is intentionally ignored.
    void update(const std::vector<float>&)
    {
        repaint();
    }

    void setSliceBoundaries(const std::vector<float>& normalized)
    {
        m_boundaries = normalized;
    }

    void setPlayheadNormalized(float normalized)
    {
        m_playhead = normalized;
        repaint();
    }

    void setStateLabel(const juce::String& label)
    {
        m_stateLabel = label;
    }

    // The gauge's display name from the blueprint, shown right-aligned.
    void setLabelText(const juce::String& label)
    {
        m_title = label;
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

        constexpr float kPad = 6.f;
        constexpr float kLabelH = 16.f;
        auto area = getLocalBounds().toFloat().reduced(kPad);
        const auto labelStrip = area.removeFromTop(kLabelH);
        const auto waveArea = area;
        const float cy = waveArea.getCentreY();
        const float halfH = waveArea.getHeight() * 0.5f;

        // Centre line
        g.setColour(juce::Colour(c.cols[4]).withAlpha(0.25f));
        g.drawHorizontalLine(static_cast<int>(cy), waveArea.getX(), waveArea.getRight());

        drawWaveform(g, waveArea, cy, halfH, c);
        drawSliceBoundaries(g, waveArea, c);
        drawPlayhead(g, waveArea, c);

        g.setFont(juce::Font(juce::FontOptions(12.f)));
        if (m_stateLabel.isNotEmpty())
        {
            g.setColour(juce::Colour(c.cols[7]).withAlpha(0.85f));
            g.drawText(m_stateLabel, labelStrip, juce::Justification::centredLeft);
        }
        if (m_title.isNotEmpty())
        {
            g.setColour(juce::Colour(c.cols[7]).withAlpha(0.55f));
            g.drawText(m_title, labelStrip, juce::Justification::centredRight);
        }
    }

  private:
    void drawWaveform(juce::Graphics& g, juce::Rectangle<float> waveArea, float cy, float halfH,
                      const GuiConstants::Colors& c) const
    {
        if (m_peaks.size() < 2)
        {
            return;
        }
        g.setColour(juce::Colour(c.cols[8]));
        const float n = static_cast<float>(m_peaks.size());
        for (size_t i = 0; i < m_peaks.size(); ++i)
        {
            const float x = waveArea.getX() + waveArea.getWidth() * static_cast<float>(i) / n;
            const float h = juce::jlimit(0.f, 1.f, m_peaks[i]) * halfH;
            g.drawVerticalLine(static_cast<int>(x), cy - h, cy + h);
        }
    }

    void drawSliceBoundaries(juce::Graphics& g, juce::Rectangle<float> waveArea, const GuiConstants::Colors& c) const
    {
        g.setColour(juce::Colour(c.cols[6]).withAlpha(0.6f));
        for (const float b : m_boundaries)
        {
            const float x = waveArea.getX() + waveArea.getWidth() * juce::jlimit(0.f, 1.f, b);
            g.drawVerticalLine(static_cast<int>(x), waveArea.getY(), waveArea.getBottom());
        }
    }

    void drawPlayhead(juce::Graphics& g, juce::Rectangle<float> waveArea, const GuiConstants::Colors& c) const
    {
        const float x = waveArea.getX() + waveArea.getWidth() * juce::jlimit(0.f, 1.f, m_playhead);
        g.setColour(juce::Colour(c.cols[9]).withAlpha(0.9f));
        g.drawVerticalLine(static_cast<int>(x), waveArea.getY(), waveArea.getBottom());
    }

    std::vector<float> m_peaks;
    std::vector<float> m_boundaries;
    float m_playhead{0.f};
    [[maybe_unused]] float m_sampleRate{48000.f};
    juce::String m_stateLabel;
    juce::String m_title;
};
