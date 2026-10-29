#pragma once

#include <algorithm>
#include <array>
#include <juce_graphics/juce_graphics.h>

#include "../impl/SonalysisSnapshots.h"
#include "SonalysisDisplayFrame.h"

/**
 * Stereo bar meter in 1 dB segments: solid up to RMS, translucent up to the falling peak,
 * a line at the hold level and a clip LED on top. Segment colours follow the theme's
 * level zones. Numeric peak / RMS / hold readouts sit under each bar.
 */
class LevelMeterDisplay final : public SonalysisUi::DisplayFrame
{
  public:
    void setLevels(const Sonalysis::LevelSnapshot& levels)
    {
        m_levels = levels;
        repaint();
    }

    void paint(juce::Graphics& g) override
    {
        auto area = paintFrame(g);
        const float width = std::min(area.getWidth(), kMaxMeterWidth);
        area = area.withSizeKeepingCentre(width, area.getHeight());
        auto readout = area.removeFromBottom(kReadoutHeight);
        area.removeFromTop(kClipLedHeight + 4.f);
        drawScale(g, area.removeFromLeft(kScaleWidth));
        readout.removeFromLeft(kScaleWidth);

        static constexpr std::array<const char*, 2> kNames{"L", "R"};
        const float columnWidth = area.getWidth() / 2.f;
        for (size_t c = 0; c < 2; ++c)
        {
            const auto column = area.removeFromLeft(columnWidth).reduced(8.f, 0.f);
            drawChannel(g, column, m_levels.channels[c]);
            drawReadout(g, readout.removeFromLeft(columnWidth), m_levels.channels[c], kNames[c]);
        }
    }

  private:
    static constexpr float kMinDb{-60.f};
    static constexpr float kMaxDb{6.f};
    static constexpr float kMaxMeterWidth{320.f};
    static constexpr float kScaleWidth{34.f};
    static constexpr float kReadoutHeight{64.f};
    static constexpr float kClipLedHeight{12.f};

    [[nodiscard]] static float yFor(const float db, const juce::Rectangle<float>& area) noexcept
    {
        const float t = (std::clamp(db, kMinDb, kMaxDb) - kMinDb) / (kMaxDb - kMinDb);
        return area.getBottom() - t * area.getHeight();
    }

    static void drawScale(juce::Graphics& g, const juce::Rectangle<float>& area)
    {
        for (float db = kMaxDb; db >= kMinDb; db -= 6.f)
        {
            const float y = yFor(db, area);
            SonalysisUi::drawText(g, juce::String(static_cast<int>(db)),
                                  {area.getX(), y - 7.f, area.getWidth() - 4.f, 14.f}, 11.f,
                                  juce::Justification::centredRight, SonalysisUi::textColour(0.6f));
        }
    }

    static void drawChannel(juce::Graphics& g, const juce::Rectangle<float>& column,
                            const Sonalysis::ChannelLevels& level)
    {
        const auto led = juce::Rectangle<float>(column.getX(), column.getY() - kClipLedHeight - 4.f, column.getWidth(),
                                                kClipLedHeight);
        g.setColour(level.clipped ? SonalysisUi::zoneColour(SonalysisUi::Zone::Danger)
                                  : juce::Colour(GuiConstants::instance().colors.backgroundDark));
        g.fillRoundedRectangle(led, 2.f);

        const float segmentHeight = column.getHeight() / (kMaxDb - kMinDb);
        for (float db = kMinDb; db < kMaxDb; db += 1.f)
        {
            const float top = yFor(db + 1.f, column);
            const auto segment =
                juce::Rectangle<float>(column.getX(), top, column.getWidth(), segmentHeight).reduced(0.f, 0.5f);
            g.setColour(segmentColour(db, level));
            g.fillRect(segment);
        }
        if (level.holdDb > kMinDb)
        {
            g.setColour(SonalysisUi::textColour(0.9f));
            g.fillRect(column.getX(), yFor(level.holdDb, column) - 1.f, column.getWidth(), 2.f);
        }
    }

    [[nodiscard]] static juce::Colour segmentColour(const float db, const Sonalysis::ChannelLevels& level)
    {
        if (db < level.rmsDb)
        {
            return SonalysisUi::levelColour(db);
        }
        if (db < level.peakDb)
        {
            return SonalysisUi::levelColour(db).withAlpha(0.4f);
        }
        return juce::Colour(GuiConstants::instance().colors.backgroundDark);
    }

    static void drawReadout(juce::Graphics& g, juce::Rectangle<float> area, const Sonalysis::ChannelLevels& level,
                            const char* name)
    {
        constexpr float floorDb = AbacDsp::LevelMeter::kFloorDb;
        const float lineHeight = area.getHeight() / 4.f;
        const auto line = [&](const juce::String& text, const float alpha)
        {
            SonalysisUi::drawText(g, text, area.removeFromTop(lineHeight), 12.f, juce::Justification::centred,
                                  SonalysisUi::textColour(alpha));
        };
        line(name, 0.9f);
        line("peak " + SonalysisUi::formatDb(level.peakDb, floorDb), 0.8f);
        line("rms " + SonalysisUi::formatDb(level.rmsDb, floorDb), 0.8f);
        line("hold " + SonalysisUi::formatDb(level.holdDb, floorDb), 0.6f);
    }

    Sonalysis::LevelSnapshot m_levels{};
};
