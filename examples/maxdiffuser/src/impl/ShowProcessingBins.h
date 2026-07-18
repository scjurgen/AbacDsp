#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <juce_gui_basics/juce_gui_basics.h>

#include "../inc/GuiConstants.h"

// Per-element level meter for the diffuser chain: bin 0 is the raw input level, bin N is the
// level after the N-th active element has processed. Bins beyond the currently active element
// count are drawn dimmed, so it's visually obvious how many elements are actually in the chain.
class ShowProcessingBins : public juce::Component
{
  public:
    static constexpr size_t kNumBins{51};

    ShowProcessingBins() = default;

    void update(const std::array<float, kNumBins>& levels, size_t activeCount)
    {
        m_levels = levels;
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
        const float columnWidth = bounds.getWidth() / static_cast<float>(kNumBins);
        auto gradient = GuiConstants::instance().getLevelGradient();

        constexpr float span = GuiConstants::kMeterMaxDb - GuiConstants::kMeterMinDb;

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

            const float clampedDb = std::clamp(m_levels[i], GuiConstants::kMeterMinDb, GuiConstants::kMeterMaxDb) -
                                    GuiConstants::kMeterMinDb;
            const float visibleHeight = juce::jmap(clampedDb, 0.f, span, 0.f, bounds.getHeight());
            g.setColour(juce::Colour(colors.backgroundComponent));
            g.fillRect(columnBounds.withBottom(bounds.getBottom() - visibleHeight));
        }
    }

  private:
    std::array<float, kNumBins> m_levels{};
    size_t m_activeCount{0};
};
