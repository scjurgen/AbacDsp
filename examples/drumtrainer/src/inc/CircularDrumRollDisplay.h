#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <juce_graphics/juce_graphics.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <limits>
#include <numbers>
#include <optional>
#include <vector>

#include "../impl/DrumAnalysisReport.h"
#include "../impl/DrumDisplayData.h"
#include "GuiConstants.h"

/**
 * Circular piano roll of the target pattern with the drummer's hits on top. One revolution is
 * the whole pattern, downbeat at 12 o'clock, clockwise; one ring per instrument, kick innermost.
 * Target hits are arcs in their ring, played hits dots coloured by the report's timing zones
 * against the nearest target of the same limb (grey when no target is in reach, hollow for the
 * wrong hihat articulation). A target the playhead has passed without a hit is outlined red.
 * Hits of the previous revolution stay until the playhead sweeps over them.
 */
class CircularDrumRollDisplay : public juce::Component
{
  public:
    void setLabelText(const juce::String& label)
    {
        m_label = label;
    }

    void setBpm(const float bpm) noexcept
    {
        m_bpm = std::max(bpm, 1.f);
    }

    void setPattern(const DrumDisplay::DisplayPattern& pattern)
    {
        if (pattern.version == m_patternVersion)
        {
            return;
        }
        m_patternVersion = pattern.version;
        m_beatsPerBar = std::max<size_t>(pattern.beatsPerBar, 1);
        m_patternBars = pattern.pattern.active ? std::max<size_t>(pattern.pattern.patternBars, 1) : 1;
        rebuildSlots(pattern.pattern);
        m_hits.clear();
    }

    void addHits(const std::vector<DrumDisplay::DisplayHit>& hits)
    {
        for (const auto& hit : hits)
        {
            m_hits.push_back(placeHit(hit));
        }
    }

    void setPatternPosition(const DrumDisplay::PatternPosition position)
    {
        if (position.running && !m_position.running)
        {
            m_runStartRevolution = position.revolution;
            m_runStartBeats = position.phase * totalBeats();
        }
        m_position = position;
        pruneHits();
        repaint();
    }

    void paint(juce::Graphics& g) override
    {
        const auto& c = GuiConstants::instance().colors;
        g.setColour(juce::Colour(c.backgroundComponent));
        g.fillRoundedRectangle(getLocalBounds().toFloat(), 3.f);

        auto area = getLocalBounds().toFloat().reduced(8.f);
        if (m_label.isNotEmpty())
        {
            g.setColour(juce::Colour(c.labelColour).withAlpha(0.75f));
            g.setFont(juce::Font(juce::FontOptions(11.f)));
            g.drawText(m_label, area.removeFromTop(16.f).toNearestInt(), juce::Justification::centred);
        }
        const float side = std::min(area.getWidth(), area.getHeight());
        m_geometry = Geometry{area.getCentre(), side * 0.5f};

        paintRings(g, c);
        paintGrid(g, c);
        paintSlots(g, c);
        paintHits(g, c);
        paintPlayhead(g, c);
    }

  private:
    using Instrument = DrumPattern::Instrument;
    using PlayedVoice = DrumInput::PlayedVoice;

    static constexpr uint32_t kNoRevolution{std::numeric_limits<uint32_t>::max()};
    static constexpr float k2Pi{2.f * std::numbers::pi_v<float>};
    // Narrower than a 16th note's spacing, so neighbouring 16ths stay apart.
    static constexpr float kSlotHalfWidthBeats{0.08f};

    // Indexed by Instrument; ring 0 is the innermost.
    static constexpr auto kRingOf = std::to_array<size_t>({0, 1, 5, 6, 7, 4, 3, 2});
    static constexpr auto kRingLabels = std::to_array<const char*>({"BD", "SD", "FT", "LT", "HT", "HH", "OH", "CR"});
    static_assert(kRingOf.size() == DrumPattern::kInstrumentNames.size());

    struct Slot
    {
        float beats{0.f};
        Instrument instrument{Instrument::Kick};
        uint8_t velocity{0};
        float halfGapBeats{0.f};
        uint32_t matchedRevolution{kNoRevolution};
    };

    struct ShownHit
    {
        DrumDisplay::DisplayHit hit;
        std::optional<float> deviationMs;
        bool wrongArticulation{false};
    };

    struct Geometry
    {
        juce::Point<float> centre;
        float outer{0.f};

        [[nodiscard]] float inner() const noexcept
        {
            return outer * 0.22f;
        }
        [[nodiscard]] float ringWidth() const noexcept
        {
            return (outer - inner()) / static_cast<float>(kRingOf.size());
        }
        [[nodiscard]] float ringRadius(const size_t ring) const noexcept
        {
            return inner() + (static_cast<float>(ring) + 0.5f) * ringWidth();
        }
    };

    [[nodiscard]] float totalBeats() const noexcept
    {
        return static_cast<float>(m_patternBars * m_beatsPerBar);
    }

    [[nodiscard]] float angleOf(const float beats) const noexcept
    {
        return k2Pi * beats / totalBeats();
    }

    [[nodiscard]] juce::Point<float> pointAt(const float beats, const float radius) const noexcept
    {
        return m_geometry.centre.getPointOnCircumference(radius, angleOf(beats));
    }

    [[nodiscard]] float ringRadiusOf(const Instrument instrument) const noexcept
    {
        return m_geometry.ringRadius(kRingOf[static_cast<size_t>(instrument)]);
    }

    [[nodiscard]] float msPerBeat() const noexcept
    {
        return 60000.f / m_bpm;
    }

    [[nodiscard]] float headBeats() const noexcept
    {
        return m_position.phase * totalBeats();
    }

    // Signed distance in beats, the short way round the pattern.
    [[nodiscard]] float circularDelta(const float from, const float to) const noexcept
    {
        const float total = totalBeats();
        float delta = std::fmod(to - from, total);
        if (delta > 0.5f * total)
        {
            delta -= total;
        }
        else if (delta < -0.5f * total)
        {
            delta += total;
        }
        return delta;
    }

    void rebuildSlots(const DrumPattern::HitPattern& pattern)
    {
        m_slots.clear();
        if (!pattern.active)
        {
            return;
        }
        for (size_t i = 0; i < pattern.hitCount; ++i)
        {
            const auto& hit = pattern.hits[i];
            m_slots.push_back({hit.positionBeats, hit.instrument, hit.velocity, 0.5f * totalBeats(), kNoRevolution});
        }
        for (auto& slot : m_slots)
        {
            for (const auto& other : m_slots)
            {
                const float gap = std::abs(circularDelta(slot.beats, other.beats));
                if (&other != &slot && gap > 0.f &&
                    DrumAnalysis::limbOf(other.instrument) == DrumAnalysis::limbOf(slot.instrument))
                {
                    slot.halfGapBeats = std::min(slot.halfGapBeats, 0.5f * gap);
                }
            }
        }
    }

    [[nodiscard]] ShownHit placeHit(const DrumDisplay::DisplayHit& hit)
    {
        ShownHit shown{hit, std::nullopt, false};
        const auto instrument = DrumAnalysis::scoredInstrument(hit.voice);
        if (!instrument)
        {
            return shown;
        }
        const float maxWindowBeats = DrumAnalysis::kMaxMatchWindowMs / msPerBeat();
        Slot* best = nullptr;
        float bestDelta = 0.f;
        for (auto& slot : m_slots)
        {
            const float delta = circularDelta(hit.patternBeats, slot.beats);
            const bool sameLimb = DrumAnalysis::limbOf(slot.instrument) == DrumAnalysis::limbOf(*instrument);
            if (sameLimb && std::abs(delta) <= std::min(maxWindowBeats, slot.halfGapBeats) &&
                (best == nullptr || std::abs(delta) < std::abs(bestDelta)))
            {
                best = &slot;
                bestDelta = delta;
            }
        }
        if (best != nullptr)
        {
            const float unwrapped = best->beats - hit.patternBeats;
            const bool slotInNextRevolution = unwrapped - bestDelta < -0.5f * totalBeats();
            const bool slotInPreviousRevolution = unwrapped - bestDelta > 0.5f * totalBeats();
            best->matchedRevolution =
                hit.revolution + (slotInNextRevolution ? 1u : 0u) - (slotInPreviousRevolution ? 1u : 0u);
            shown.deviationMs = bestDelta * msPerBeat();
            shown.wrongArticulation = best->instrument != *instrument;
        }
        return shown;
    }

    void pruneHits()
    {
        const uint32_t current = m_position.revolution;
        const float head = headBeats();
        std::erase_if(m_hits,
                      [&](const ShownHit& shown)
                      {
                          const uint32_t revolution = shown.hit.revolution;
                          const bool keep = revolution == current || revolution == current + 1 ||
                                            (revolution + 1 == current && shown.hit.patternBeats > head);
                          return !keep;
                      });
    }

    // A slot is judged in the current revolution once the playhead has left its window, else
    // still in the previous one; nothing before the moment the click started counts.
    [[nodiscard]] bool isMissed(const Slot& slot) const noexcept
    {
        if (!m_position.running)
        {
            return false;
        }
        const float window = std::min(DrumAnalysis::kMaxMatchWindowMs / msPerBeat(), slot.halfGapBeats);
        const bool judgedNow = headBeats() > slot.beats + window;
        const uint32_t revolution = judgedNow ? m_position.revolution : m_position.revolution - 1;
        const bool afterStart =
            revolution > m_runStartRevolution || (revolution == m_runStartRevolution && slot.beats > m_runStartBeats);
        return afterStart && revolution != kNoRevolution && slot.matchedRevolution != revolution;
    }

    [[nodiscard]] static juce::Colour zoneColour(const float deviationMs)
    {
        const auto hex = DrumAnalysis::bandForDeviation(std::abs(deviationMs)).zoneColor;
        return juce::Colour::fromString("ff" + juce::String(std::string{hex.substr(1)}));
    }

    void paintRings(juce::Graphics& g, const GuiConstants::Colors& c) const
    {
        const auto& geo = m_geometry;
        for (size_t ring = 0; ring < kRingOf.size(); ++ring)
        {
            const float radius = geo.ringRadius(ring);
            g.setColour(juce::Colour(ring % 2 == 0 ? c.backgroundMid : c.backgroundDark).withAlpha(0.6f));
            g.drawEllipse(juce::Rectangle<float>(2.f * radius, 2.f * radius).withCentre(geo.centre), geo.ringWidth());
        }
        g.setColour(juce::Colour(c.labelColour).withAlpha(0.6f));
        g.setFont(juce::Font(juce::FontOptions(std::clamp(geo.ringWidth() * 0.5f, 7.f, 11.f))));
        for (size_t ring = 0; ring < kRingLabels.size(); ++ring)
        {
            const float radius = geo.ringRadius(ring);
            const auto box =
                juce::Rectangle<float>(22.f, geo.ringWidth()).withCentre({geo.centre.x - 13.f, geo.centre.y - radius});
            g.drawText(kRingLabels[ring], box, juce::Justification::centredRight);
        }
    }

    void paintGrid(juce::Graphics& g, const GuiConstants::Colors& c) const
    {
        const auto& geo = m_geometry;
        const size_t beats = m_patternBars * m_beatsPerBar;
        for (size_t beat = 0; beat < beats; ++beat)
        {
            const bool barLine = beat % m_beatsPerBar == 0;
            g.setColour(juce::Colour(c.labelColour).withAlpha(barLine ? 0.55f : 0.18f));
            g.drawLine(juce::Line<float>(pointAt(static_cast<float>(beat), geo.inner()),
                                         pointAt(static_cast<float>(beat), geo.outer)),
                       barLine ? 1.6f : 0.8f);
        }
    }

    void paintSlots(juce::Graphics& g, const GuiConstants::Colors& c) const
    {
        const auto& geo = m_geometry;
        for (const auto& slot : m_slots)
        {
            const float radius = ringRadiusOf(slot.instrument);
            const float angle = angleOf(slot.beats);
            const float halfArc = std::max(angleOf(kSlotHalfWidthBeats), 0.3f * geo.ringWidth() / radius);
            juce::Path arc;
            arc.addCentredArc(geo.centre.x, geo.centre.y, radius, radius, 0.f, angle - halfArc, angle + halfArc, true);
            const float strength = 0.35f + 0.55f * static_cast<float>(slot.velocity) / 127.f;
            g.setColour(juce::Colour(c.cols[6]).withAlpha(strength));
            g.strokePath(arc, juce::PathStrokeType(geo.ringWidth() * 0.55f, juce::PathStrokeType::curved,
                                                   juce::PathStrokeType::rounded));
            if (isMissed(slot))
            {
                g.setColour(zoneColour(1000.f));
                g.strokePath(arc, juce::PathStrokeType(geo.ringWidth() * 0.75f, juce::PathStrokeType::curved,
                                                       juce::PathStrokeType::rounded));
                g.setColour(juce::Colour(c.backgroundComponent));
                g.strokePath(arc, juce::PathStrokeType(geo.ringWidth() * 0.45f, juce::PathStrokeType::curved,
                                                       juce::PathStrokeType::rounded));
            }
        }
    }

    void paintHits(juce::Graphics& g, const GuiConstants::Colors& c) const
    {
        const auto& geo = m_geometry;
        const float dot = geo.ringWidth() * 0.34f;
        for (const auto& shown : m_hits)
        {
            const auto& hit = shown.hit;
            const float alpha = hit.revolution + 1 == m_position.revolution ? 0.4f : 1.f;
            const auto colour =
                (shown.deviationMs ? zoneColour(*shown.deviationMs) : juce::Colour(c.labelColour)).withAlpha(alpha);
            g.setColour(colour);
            if (hit.voice == PlayedVoice::HihatPedal)
            {
                const auto tip = pointAt(hit.patternBeats, ringRadiusOf(Instrument::HihatClosed) - geo.ringWidth() * 0.5f);
                g.fillEllipse(juce::Rectangle<float>(dot, dot).withCentre(tip));
                continue;
            }
            const auto instrument = DrumAnalysis::scoredInstrument(hit.voice).value_or(Instrument::HihatClosed);
            const auto centre = pointAt(hit.patternBeats, ringRadiusOf(instrument));
            const auto box = juce::Rectangle<float>(2.f * dot, 2.f * dot).withCentre(centre);
            if (shown.wrongArticulation)
            {
                g.drawEllipse(box, dot * 0.45f);
                continue;
            }
            g.fillEllipse(box);
            if (hit.voice == PlayedVoice::HihatHalfOpen)
            {
                g.setColour(juce::Colour(c.backgroundComponent).withAlpha(alpha));
                g.fillEllipse(box.reduced(dot * 0.55f));
            }
        }
    }

    void paintPlayhead(juce::Graphics& g, const GuiConstants::Colors& c) const
    {
        const auto& geo = m_geometry;
        g.setColour(juce::Colour(c.labelColour).withAlpha(m_position.running ? 0.9f : 0.3f));
        g.drawLine(juce::Line<float>(pointAt(headBeats(), geo.inner() * 0.8f), pointAt(headBeats(), geo.outer)), 2.f);
        if (m_patternBars > 1)
        {
            const auto bar =
                std::min(static_cast<size_t>(m_position.phase * static_cast<float>(m_patternBars)), m_patternBars - 1) +
                1;
            g.setFont(juce::Font(juce::FontOptions(std::max(10.f, geo.inner() * 0.45f))));
            g.drawText(juce::String(bar) + "/" + juce::String(m_patternBars),
                       juce::Rectangle<float>(2.f * geo.inner(), 2.f * geo.inner()).withCentre(geo.centre),
                       juce::Justification::centred);
        }
    }

    juce::String m_label;
    float m_bpm{120.f};
    uint64_t m_patternVersion{std::numeric_limits<uint64_t>::max()};
    size_t m_beatsPerBar{4};
    size_t m_patternBars{1};
    std::vector<Slot> m_slots;
    std::vector<ShownHit> m_hits;
    DrumDisplay::PatternPosition m_position{};
    uint32_t m_runStartRevolution{0};
    float m_runStartBeats{0.f};
    Geometry m_geometry{};
};
