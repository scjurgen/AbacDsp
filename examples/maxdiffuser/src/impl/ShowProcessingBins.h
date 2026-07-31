#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <juce_gui_basics/juce_gui_basics.h>

#include "../inc/GuiConstants.h"
#include "Analysis/EnvelopeFollower.h"
#include "Helpers/ConstructArray.h"
#include "Numbers/Convert.h"
#include "Numbers/Interpolation.h"

enum class BinsDisplayMode
{
    ShowBins,
    ShowContinuousLine
};

enum class LevelUnit
{
    Linear,
    Decibel
};

// Per-element level meter for the diffuser chain: bin 0 is the raw input level, bin N is the
// level after the N-th active element has processed. Bins beyond the currently active element
// count sit at the DSP-side floor, so they naturally read as "off" in either display mode.
template <BinsDisplayMode Mode, LevelUnit Unit>
class ShowProcessingBins : public juce::Component
{
  public:
    static constexpr size_t kNumBins{51};
    static constexpr size_t kMeterDbRange{100};

    ShowProcessingBins()
        : m_envelopes{AbacDsp::constructArray<AbacDsp::PeakEnvelopeFollower<kMeterDbRange>, kNumBins>(
              static_cast<float>(GuiConstants::instance().init.TimerHertz))}
    {
        for (auto& env : m_envelopes)
        {
            // Fast attack, slow release: matches how most volume meters ballistically decay.
            env.setAttackInMsecs(200.f);
            env.setReleaseInMsecs(1800.f);
        }
    }

    // peaks are raw, unfiltered per-block peaks (linear gain); this is where the ballistic
    // smoothing and dB/linear conversion for display happen.
    void update(const std::array<float, kNumBins>& peaks, const size_t activeCount)
    {
        for (size_t i = 0; i < kNumBins; ++i)
        {
            const auto smoothed = m_envelopes[i].step(peaks[i]);
            if constexpr (Unit == LevelUnit::Decibel)
            {
                m_levels[i] = Convert::gainToDb(std::max(smoothed, 1E-5f));
            }
            else
            {
                m_levels[i] = smoothed * 10.f;
            }
        }
        m_activeCount = activeCount;
        repaint();
    }

    void setLabelText(const juce::String& /*label*/) noexcept {}

    void updateColors()
    {
        repaint();
    }

    void paint(juce::Graphics& g) override
    {
        const auto& colors = GuiConstants::instance().colors;
        g.setColour(juce::Colour(colors.backgroundDark));
        g.fillRoundedRectangle(getLocalBounds().toFloat(), 3.f);

        constexpr float pad = 3.f;
        const auto bounds = getLocalBounds().toFloat().reduced(pad);
        if (bounds.getWidth() <= 0.f || bounds.getHeight() <= 0.f)
        {
            return;
        }

        if constexpr (Mode == BinsDisplayMode::ShowBins)
        {
            paintBars(g, bounds);
        }
        else
        {
            paintContinuousLine(g, bounds);
        }
    }

  private:
    // Maps a bin's level to its y coordinate within bounds. Decibel and Linear modes clamp to
    // the same underlying dB window (GuiConstants::kMeterMinDb..kMeterMaxDb), just mapped
    // logarithmically vs. linearly, so the two stay visually comparable.
    [[nodiscard]] float binY(const juce::Rectangle<float>& bounds, const size_t bin) const noexcept
    {
        if constexpr (Unit == LevelUnit::Decibel)
        {
            constexpr float span = GuiConstants::kMeterMaxDb - GuiConstants::kMeterMinDb;
            const float clamped = std::clamp(m_levels[bin], GuiConstants::kMeterMinDb, GuiConstants::kMeterMaxDb) -
                                  GuiConstants::kMeterMinDb;
            const float visibleHeight = juce::jmap(clamped, 0.f, span, 0.f, bounds.getHeight());
            return bounds.getBottom() - visibleHeight;
        }
        else
        {
            const float minGain = Convert::dbToGain(GuiConstants::kMeterMinDb);
            const float maxGain = Convert::dbToGain(GuiConstants::kMeterMaxDb);
            const float clamped = std::clamp(m_levels[bin], minGain, maxGain) - minGain;
            const float visibleHeight = juce::jmap(clamped, 0.f, maxGain - minGain, 0.f, bounds.getHeight());
            return bounds.getBottom() - visibleHeight;
        }
    }

    void paintBars(juce::Graphics& g, const juce::Rectangle<float>& bounds) const
    {
        const auto& colors = GuiConstants::instance().colors;
        const float columnWidth = bounds.getWidth() / static_cast<float>(kNumBins);
        auto gradient = GuiConstants::instance().getLevelGradient();

        for (size_t i = 0; i < kNumBins; ++i)
        {
            juce::Rectangle<float> columnBounds{bounds.getX() + static_cast<float>(i) * columnWidth, bounds.getY(),
                                                columnWidth, bounds.getHeight()};
            columnBounds.reduce(0.4f, 0.f);

            const bool active = i < m_activeCount;
            if (active)
            {
                gradient.point1 = columnBounds.getBottomLeft();
                gradient.point2 = columnBounds.getTopLeft();
                g.setGradientFill(gradient);
            }
            else
            {
                g.setColour(juce::Colour(colors.backgroundMid));
            }
            g.fillRect(columnBounds);

            g.setColour(juce::Colour(colors.backgroundComponent));
            g.fillRect(columnBounds.withBottom(binY(bounds, i)));
        }
    }

    [[nodiscard]] juce::Point<float> binPoint(const juce::Rectangle<float>& bounds, const size_t bin) const noexcept
    {
        const float stepX = bounds.getWidth() / static_cast<float>(kNumBins - 1);
        return {bounds.getX() + static_cast<float>(bin) * stepX, binY(bounds, bin)};
    }

    // bspline43x is a uniform cubic B-spline: it approximates the bin values rather than
    // passing through them exactly, giving a smoother curve with less overshoot than Hermite.
    void buildSmoothPath(juce::Path& path, const juce::Rectangle<float>& bounds) const
    {
        constexpr int kSubSteps = 8;
        const auto pointAt = [this, &bounds](const int index) noexcept
        {
            const auto clamped = static_cast<size_t>(std::clamp(index, 0, static_cast<int>(kNumBins) - 1));
            return binPoint(bounds, clamped);
        };

        path.startNewSubPath(pointAt(0));
        for (int bin = 0; bin + 1 < static_cast<int>(kNumBins); ++bin)
        {
            const auto p0 = pointAt(bin - 1);
            const auto p1 = pointAt(bin);
            const auto p2 = pointAt(bin + 1);
            const auto p3 = pointAt(bin + 2);

            const std::array<float, 4> xs{p0.x, p1.x, p2.x, p3.x};
            const std::array<float, 4> ys{p0.y, p1.y, p2.y, p3.y};

            for (int step = 1; step <= kSubSteps; ++step)
            {
                const float t = static_cast<float>(step) / static_cast<float>(kSubSteps);
                path.lineTo(AbacDsp::Interpolation::bspline43x(xs.data(), t),
                            AbacDsp::Interpolation::bspline43x(ys.data(), t));
            }
        }
    }

    void paintContinuousLine(juce::Graphics& g, const juce::Rectangle<float>& bounds) const
    {
        juce::Path linePath;
        buildSmoothPath(linePath, bounds);

        juce::Path fillPath(linePath);
        fillPath.lineTo(bounds.getRight(), bounds.getBottom());
        fillPath.lineTo(bounds.getX(), bounds.getBottom());
        fillPath.closeSubPath();

        // Gradient spans the full dB range top-to-bottom, same as the bar mode, so a given
        // height always maps to the same colour regardless of how tall the curve happens to be.
        auto gradient = GuiConstants::instance().getLevelGradient();
        gradient.point1 = bounds.getBottomLeft();
        gradient.point2 = bounds.getTopLeft();
        g.setGradientFill(gradient);
        g.fillPath(fillPath);

        g.setColour(juce::Colour(GuiConstants::instance().colors.statusOutline));
        g.strokePath(linePath, juce::PathStrokeType(1.5f));
    }

    std::array<AbacDsp::PeakEnvelopeFollower<kMeterDbRange>, kNumBins> m_envelopes;
    std::array<float, kNumBins> m_levels{};
    size_t m_activeCount{0};
};
