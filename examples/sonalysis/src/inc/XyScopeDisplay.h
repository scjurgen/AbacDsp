#pragma once

#include <algorithm>
#include <juce_graphics/juce_graphics.h>

#include "../impl/SonalysisSnapshots.h"
#include "SonalysisDisplayFrame.h"

/**
 * XY scope: left channel on the horizontal axis, right on the vertical, full scale at the
 * square's edge. The newest points are drawn brightest, older ones fade, so the trace shows
 * its direction. A mono signal lies on the rising diagonal, a polarity-inverted one on the
 * falling diagonal.
 */
class XyScopeDisplay final : public SonalysisUi::DisplayFrame
{
  public:
    void setPoints(const Sonalysis::XySnapshot& snapshot)
    {
        m_snapshot = snapshot;
        repaint();
    }

    void paint(juce::Graphics& g) override
    {
        const auto area = paintFrame(g);
        const float side = std::min(area.getWidth(), area.getHeight()) - 2.f * kAxisLabelSpace;
        const auto square = area.withSizeKeepingCentre(side, side);
        drawGrid(g, square);
        drawTrace(g, square);
    }

  private:
    static constexpr float kAxisLabelSpace{16.f};
    static constexpr size_t kFadeSegments{8};

    [[nodiscard]] static juce::Point<float> toScreen(const Sonalysis::XyPoint& point,
                                                     const juce::Rectangle<float>& square) noexcept
    {
        const float half = square.getWidth() * 0.5f;
        return {square.getCentreX() + std::clamp(point.x, -1.f, 1.f) * half,
                square.getCentreY() - std::clamp(point.y, -1.f, 1.f) * half};
    }

    static void drawGrid(juce::Graphics& g, const juce::Rectangle<float>& square)
    {
        g.setColour(SonalysisUi::textColour(0.25f));
        g.drawRect(square, 1.f);
        g.drawHorizontalLine(static_cast<int>(square.getCentreY()), square.getX(), square.getRight());
        g.drawVerticalLine(static_cast<int>(square.getCentreX()), square.getY(), square.getBottom());
        g.setColour(SonalysisUi::textColour(0.1f));
        g.drawLine({square.getBottomLeft(), square.getTopRight()}, 1.f);
        g.drawLine({square.getTopLeft(), square.getBottomRight()}, 1.f);

        const auto colour = SonalysisUi::textColour(0.6f);
        SonalysisUi::drawText(g, "L", {square.getRight() + 2.f, square.getCentreY() - 8.f, kAxisLabelSpace, 16.f}, 12.f,
                              juce::Justification::centredLeft, colour);
        SonalysisUi::drawText(g, "R",
                              {square.getCentreX() - 8.f, square.getY() - kAxisLabelSpace, 16.f, kAxisLabelSpace}, 12.f,
                              juce::Justification::centred, colour);
    }

    void drawTrace(juce::Graphics& g, const juce::Rectangle<float>& square) const
    {
        const auto& points = m_snapshot.points;
        const size_t perSegment = points.size() / kFadeSegments;
        for (size_t segment = 0; segment < kFadeSegments; ++segment)
        {
            const size_t first = segment * perSegment;
            juce::Path path;
            path.startNewSubPath(toScreen(points[first], square));
            for (size_t i = first + 1; i <= std::min(first + perSegment, points.size() - 1); ++i)
            {
                path.lineTo(toScreen(points[i], square));
            }
            const float alpha = 0.15f + 0.85f * static_cast<float>(segment + 1) / static_cast<float>(kFadeSegments);
            g.setColour(SonalysisUi::accentColour(7).withAlpha(alpha));
            g.strokePath(path, juce::PathStrokeType(1.2f));
        }
    }

    Sonalysis::XySnapshot m_snapshot{};
};
