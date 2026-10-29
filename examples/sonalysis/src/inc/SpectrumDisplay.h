#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <juce_graphics/juce_graphics.h>

#include "../impl/SonalysisSnapshots.h"
#include "SonalysisDisplayFrame.h"

/**
 * Log-frequency spectrum, 20 Hz to 20 kHz, in dBFS. Decade grid with labelled frequencies,
 * filled level curve and, when enabled, the peak-hold curve on top. The header line shows
 * the FFT size and its bin width.
 */
class SpectrumDisplay final : public SonalysisUi::DisplayFrame
{
  public:
    void setSpectrum(const Sonalysis::SpectrumSnapshot& spectrum)
    {
        m_spectrum = spectrum;
        repaint();
    }

    void paint(juce::Graphics& g) override
    {
        auto area = paintFrame(g);
        if (m_spectrum.fftSize > 0)
        {
            const auto info = juce::String("FFT ") + juce::String(m_spectrum.fftSize) + ", bin " +
                              juce::String(m_spectrum.resolutionHz, 1) + " Hz";
            SonalysisUi::drawText(g, info, area.removeFromTop(16.f), 11.f, juce::Justification::centredRight,
                                  SonalysisUi::textColour(0.6f));
        }
        auto frequencyLabels = area.removeFromBottom(16.f);
        const auto dbLabels = area.removeFromLeft(kDbLabelWidth);
        frequencyLabels.removeFromLeft(kDbLabelWidth);
        drawGrid(g, area, dbLabels, frequencyLabels);
        if (m_spectrum.bandCenters.front() <= 0.f)
        {
            return;
        }
        drawCurve(g, area);
    }

  private:
    static constexpr float kMinDb{-96.f};
    static constexpr float kMaxDb{6.f};
    static constexpr float kMinHz{20.f};
    static constexpr float kMaxHz{20000.f};
    static constexpr float kDbLabelWidth{34.f};
    static constexpr auto kLabelledHz =
        std::to_array<float>({20.f, 50.f, 100.f, 200.f, 500.f, 1000.f, 2000.f, 5000.f, 10000.f, 20000.f});

    [[nodiscard]] static float xFor(const float hz, const juce::Rectangle<float>& area) noexcept
    {
        const float t = std::log(std::clamp(hz, kMinHz, kMaxHz) / kMinHz) / std::log(kMaxHz / kMinHz);
        return area.getX() + t * area.getWidth();
    }

    [[nodiscard]] static float yFor(const float db, const juce::Rectangle<float>& area) noexcept
    {
        const float t = (kMaxDb - std::clamp(db, kMinDb, kMaxDb)) / (kMaxDb - kMinDb);
        return area.getY() + t * area.getHeight();
    }

    [[nodiscard]] static juce::String hzLabel(const float hz)
    {
        return hz >= 1000.f ? juce::String(static_cast<int>(hz / 1000.f)) + "k" : juce::String(static_cast<int>(hz));
    }

    static void drawGrid(juce::Graphics& g, const juce::Rectangle<float>& plot, const juce::Rectangle<float>& dbLabels,
                         const juce::Rectangle<float>& frequencyLabels)
    {
        for (float decade = 10.f; decade < kMaxHz; decade *= 10.f)
        {
            for (int multiple = 1; multiple < 10; ++multiple)
            {
                const float hz = decade * static_cast<float>(multiple);
                if (hz >= kMinHz && hz <= kMaxHz)
                {
                    g.setColour(SonalysisUi::textColour(multiple == 1 ? 0.25f : 0.08f));
                    g.drawVerticalLine(static_cast<int>(xFor(hz, plot)), plot.getY(), plot.getBottom());
                }
            }
        }
        for (const float hz : kLabelledHz)
        {
            const float x = xFor(hz, plot);
            SonalysisUi::drawText(g, hzLabel(hz), {x - 20.f, frequencyLabels.getY(), 40.f, frequencyLabels.getHeight()},
                                  11.f, juce::Justification::centred, SonalysisUi::textColour(0.6f));
        }
        for (int db = 0; db >= static_cast<int>(kMinDb); db -= 12)
        {
            const float y = yFor(static_cast<float>(db), plot);
            g.setColour(SonalysisUi::textColour(db == 0 ? 0.25f : 0.1f));
            g.drawHorizontalLine(static_cast<int>(y), plot.getX(), plot.getRight());
            SonalysisUi::drawText(g, juce::String(db),
                                  {dbLabels.getX(), y - 7.f, dbLabels.getWidth() - 4.f, 14.f}, 11.f,
                                  juce::Justification::centredRight, SonalysisUi::textColour(0.6f));
        }
    }

    [[nodiscard]] juce::Path curve(const std::array<float, Sonalysis::kSpectrumBands>& levels,
                                   const juce::Rectangle<float>& plot) const
    {
        juce::Path path;
        for (size_t i = 0; i < levels.size(); ++i)
        {
            const juce::Point<float> point{xFor(m_spectrum.bandCenters[i], plot), yFor(levels[i], plot)};
            if (i == 0)
            {
                path.startNewSubPath(point);
            }
            else
            {
                path.lineTo(point);
            }
        }
        return path;
    }

    void drawCurve(juce::Graphics& g, const juce::Rectangle<float>& plot) const
    {
        const juce::Graphics::ScopedSaveState clip(g);
        g.reduceClipRegion(plot.toNearestInt());

        const auto line = curve(m_spectrum.bandsDb, plot);
        auto filled = line;
        filled.lineTo(xFor(m_spectrum.bandCenters.back(), plot), plot.getBottom());
        filled.lineTo(xFor(m_spectrum.bandCenters.front(), plot), plot.getBottom());
        filled.closeSubPath();
        g.setColour(SonalysisUi::accentColour(6).withAlpha(0.35f));
        g.fillPath(filled);
        g.setColour(SonalysisUi::accentColour(7));
        g.strokePath(line, juce::PathStrokeType(1.5f));

        if (m_spectrum.showPeakHold)
        {
            g.setColour(SonalysisUi::textColour(0.7f));
            g.strokePath(curve(m_spectrum.peakHoldDb, plot), juce::PathStrokeType(1.f));
        }
    }

    Sonalysis::SpectrumSnapshot m_spectrum{};
};
