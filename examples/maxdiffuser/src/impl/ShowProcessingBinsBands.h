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

// Experimental sibling of ShowProcessingBins: instead of a single per-bin volume
// curve, stacks 3 filtered bands (low/mid/high) on top of each other. The low
// band's curve is the lower edge for the mid band; low+mid is the lower edge for
// the high band, whose top is low+mid+high.
class ShowProcessingBinsBands : public juce::Component
{
  public:
    static constexpr size_t kNumBins{51};
    static constexpr size_t kMeterDbRange{60};
    static constexpr size_t kNumBands{3};
    static constexpr size_t kLow{0};
    static constexpr size_t kMid{1};
    static constexpr size_t kHigh{2};

    ShowProcessingBinsBands()
        : m_envLow{AbacDsp::constructArray<AbacDsp::PeakEnvelopeFollower<kMeterDbRange>, kNumBins>(
              static_cast<float>(GuiConstants::instance().init.TimerHertz))}
        , m_envMid{AbacDsp::constructArray<AbacDsp::PeakEnvelopeFollower<kMeterDbRange>, kNumBins>(
              static_cast<float>(GuiConstants::instance().init.TimerHertz))}
        , m_envHigh{AbacDsp::constructArray<AbacDsp::PeakEnvelopeFollower<kMeterDbRange>, kNumBins>(
              static_cast<float>(GuiConstants::instance().init.TimerHertz))}
    {
        for (auto* envelopes : {&m_envLow, &m_envMid, &m_envHigh})
        {
            for (auto& env : *envelopes)
            {
                env.setAttackInMsecs(200.f);
                env.setReleaseInMsecs(2000.f);
            }
        }
    }

    // peaks[bin][kLow|kMid|kHigh] are raw, unfiltered-except-for-band-filter per-block peaks
    // (linear gain); smoothing and cumulative low/low+mid/low+mid+high sums happen here.
    void update(const std::array<std::array<float, kNumBands>, kNumBins>& peaks, const size_t activeCount)
    {
        for (size_t i = 0; i < kNumBins; ++i)
        {
            const auto low = m_envLow[i].step(peaks[i][kLow]);
            const auto mid = m_envMid[i].step(peaks[i][kMid]);
            const auto high = m_envHigh[i].step(peaks[i][kHigh]);
            m_cumulative[i][0] = low;
            m_cumulative[i][1] = low + mid;
            m_cumulative[i][2] = low + mid + high;
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

        const auto gradient = GuiConstants::instance().getSpectrogramGradient();
        const std::array<juce::Colour, kNumBands> bandColours{
            gradient.getColourAtPosition(0.0), gradient.getColourAtPosition(0.5), gradient.getColourAtPosition(1.0)};

        std::array<juce::Path, kNumBands> boundaryPaths;
        for (size_t band = 0; band < kNumBands; ++band)
        {
            buildSmoothPath(boundaryPaths[band], bounds, band);
        }

        for (size_t band = 0; band < kNumBands; ++band)
        {
            juce::Path fillPath(boundaryPaths[band]);
            if (band == 0)
            {
                fillPath.lineTo(bounds.getRight(), bounds.getBottom());
                fillPath.lineTo(bounds.getX(), bounds.getBottom());
            }
            else
            {
                appendReversed(fillPath, boundaryPaths[band - 1]);
            }
            fillPath.closeSubPath();
            g.setColour(bandColours[band].withAlpha(0.85f));
            g.fillPath(fillPath);
        }

        g.setColour(juce::Colour(colors.statusOutline));
        g.strokePath(boundaryPaths[kNumBands - 1], juce::PathStrokeType(1.5f));
    }

  private:
    static constexpr int kSubSteps = 8;

    [[nodiscard]] float binY(const juce::Rectangle<float>& bounds, const size_t bin, const size_t band) const noexcept
    {
        constexpr float span = GuiConstants::kMeterMaxDb - GuiConstants::kMeterMinDb;
        const auto db = Convert::gainToDb(std::max(m_cumulative[bin][band], 1E-5f));
        const float clamped =
            std::clamp(db, GuiConstants::kMeterMinDb, GuiConstants::kMeterMaxDb) - GuiConstants::kMeterMinDb;
        const float visibleHeight = juce::jmap(clamped, 0.f, span, 0.f, bounds.getHeight());
        return bounds.getBottom() - visibleHeight;
    }

    [[nodiscard]] juce::Point<float> binPoint(const juce::Rectangle<float>& bounds, const size_t bin,
                                              const size_t band) const noexcept
    {
        const float stepX = bounds.getWidth() / static_cast<float>(kNumBins - 1);
        return {bounds.getX() + static_cast<float>(bin) * stepX, binY(bounds, bin, band)};
    }

    // bspline43x is a uniform cubic B-spline: it approximates the bin values rather than
    // passing through them exactly, matching the smoothing used by the single-band meter.
    void buildSmoothPath(juce::Path& path, const juce::Rectangle<float>& bounds, const size_t band) const
    {
        const auto pointAt = [this, &bounds, band](const int index) noexcept
        {
            const auto clamped = static_cast<size_t>(std::clamp(index, 0, static_cast<int>(kNumBins) - 1));
            return binPoint(bounds, clamped, band);
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

    // Appends source's points onto target in reverse, closing target's area between the two
    // curves. Both paths were built by buildSmoothPath, so they share the same point count.
    static void appendReversed(juce::Path& target, const juce::Path& source)
    {
        juce::Path::Iterator it(source);
        std::vector<juce::Point<float>> points;
        while (it.next())
        {
            if (it.elementType == juce::Path::Iterator::startNewSubPath ||
                it.elementType == juce::Path::Iterator::lineTo)
            {
                points.emplace_back(it.x1, it.y1);
            }
        }
        for (auto pointIt = points.rbegin(); pointIt != points.rend(); ++pointIt)
        {
            target.lineTo(*pointIt);
        }
    }

    std::array<AbacDsp::PeakEnvelopeFollower<kMeterDbRange>, kNumBins> m_envLow;
    std::array<AbacDsp::PeakEnvelopeFollower<kMeterDbRange>, kNumBins> m_envMid;
    std::array<AbacDsp::PeakEnvelopeFollower<kMeterDbRange>, kNumBins> m_envHigh;
    std::array<std::array<float, kNumBands>, kNumBins> m_cumulative{};
    size_t m_activeCount{0};
};
