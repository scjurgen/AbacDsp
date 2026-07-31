#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <juce_gui_basics/juce_gui_basics.h>
#include <numeric>

#include "../inc/GuiConstants.h"
#include "Analysis/EnvelopeFollower.h"
#include "Helpers/ConstructArray.h"
#include "Numbers/Convert.h"

// Depicts the diffuser chain's real element sizes: each active element gets a bar whose
// width is proportional to its size in meters relative to the total, so e.g. sizes 10/20/30
// render as bars of relative width 1:2:3 regardless of the widget's pixel width. Each bar is
// filled with the same 3-band (low/mid/high) real-time levels as ShowProcessingBinsBands, but
// as flat stacked rectangles (no smoothed curve), like a per-element stacked bar chart.
class ShowDiffuserSizes : public juce::Component
{
  public:
    static constexpr size_t kMaxElements{50};
    static constexpr size_t kMeterDbRange{100};
    static constexpr size_t kNumBands{3};
    static constexpr size_t kLow{0};
    static constexpr size_t kMid{1};
    static constexpr size_t kHigh{2};

    ShowDiffuserSizes()
        : m_envLow{AbacDsp::constructArray<AbacDsp::PeakEnvelopeFollower<kMeterDbRange>, kMaxElements>(
              static_cast<float>(GuiConstants::instance().init.TimerHertz))}
        , m_envMid{AbacDsp::constructArray<AbacDsp::PeakEnvelopeFollower<kMeterDbRange>, kMaxElements>(
              static_cast<float>(GuiConstants::instance().init.TimerHertz))}
        , m_envHigh{AbacDsp::constructArray<AbacDsp::PeakEnvelopeFollower<kMeterDbRange>, kMaxElements>(
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

    // bandPeaks[bin][kLow|kMid|kHigh] is the same per-bin band data ShowProcessingBinsBands
    // consumes; bin 0 is the raw input, so element i's own level is bandPeaks[i + 1].
    void update(const std::array<float, kMaxElements>& sizesInMeters,
                const std::array<std::array<float, kNumBands>, kMaxElements + 1>& bandPeaks, const size_t activeCount)
    {
        m_sizes = sizesInMeters;
        m_activeCount = std::min(activeCount, kMaxElements);
        for (size_t i = 0; i < m_activeCount; ++i)
        {
            const auto low = m_envLow[i].step(bandPeaks[i + 1][kLow]);
            const auto mid = m_envMid[i].step(bandPeaks[i + 1][kMid]);
            const auto high = m_envHigh[i].step(bandPeaks[i + 1][kHigh]);
            m_cumulative[i][0] = low;
            m_cumulative[i][1] = low + mid;
            m_cumulative[i][2] = low + mid + high;
        }
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
        if (bounds.getWidth() <= 0.f || bounds.getHeight() <= 0.f || m_activeCount == 0)
        {
            return;
        }

        const auto total = std::accumulate(m_sizes.begin(), m_sizes.begin() + static_cast<long>(m_activeCount), 0.f);
        if (total <= 0.f)
        {
            return;
        }

        const auto gradient = GuiConstants::instance().getSpectrogramGradient();
        const std::array<juce::Colour, kNumBands> bandColours{
            gradient.getColourAtPosition(0.0), gradient.getColourAtPosition(0.5), gradient.getColourAtPosition(1.0)};

        auto x = bounds.getX();
        for (size_t i = 0; i < m_activeCount; ++i)
        {
            const auto width = bounds.getWidth() * m_sizes[i] / total;

            auto barTop = bounds.getBottom();
            for (size_t band = 0; band < kNumBands; ++band)
            {
                const auto segmentTop = binY(bounds, m_cumulative[i][band]);
                g.setColour(bandColours[band]);
                g.fillRect(juce::Rectangle<float>{x, segmentTop, width, barTop - segmentTop});
                barTop = segmentTop;
            }

            x += width;

            // A single divider line at each boundary, so the size distribution stays visible
            // even with no signal, without doubling up lines between adjacent bars.
            g.setColour(juce::Colour(colors.statusOutline));
            g.drawVerticalLine(static_cast<int>(std::round(x)), bounds.getY(), bounds.getBottom());
        }
        g.drawRect(juce::Rectangle<float>{bounds.getX(), bounds.getY(), x - bounds.getX(), bounds.getHeight()}, 1.f);
    }

  private:
    [[nodiscard]] static float binY(const juce::Rectangle<float>& bounds, const float cumulativeLevel) noexcept
    {
        constexpr float span = GuiConstants::kMeterMaxDb - GuiConstants::kMeterMinDb;
        const auto db = Convert::gainToDb(std::max(cumulativeLevel, 1E-5f));
        const float clamped =
            std::clamp(db, GuiConstants::kMeterMinDb, GuiConstants::kMeterMaxDb) - GuiConstants::kMeterMinDb;
        const float visibleHeight = juce::jmap(clamped, 0.f, span, 0.f, bounds.getHeight());
        return bounds.getBottom() - visibleHeight;
    }

    std::array<AbacDsp::PeakEnvelopeFollower<kMeterDbRange>, kMaxElements> m_envLow;
    std::array<AbacDsp::PeakEnvelopeFollower<kMeterDbRange>, kMaxElements> m_envMid;
    std::array<AbacDsp::PeakEnvelopeFollower<kMeterDbRange>, kMaxElements> m_envHigh;
    std::array<std::array<float, kNumBands>, kMaxElements> m_cumulative{};
    std::array<float, kMaxElements> m_sizes{};
    size_t m_activeCount{0};
};
