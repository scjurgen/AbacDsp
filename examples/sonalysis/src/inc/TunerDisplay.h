#pragma once

#include <algorithm>
#include <cmath>
#include <juce_graphics/juce_graphics.h>
#include <numbers>

#include "../impl/SonalysisSnapshots.h"
#include "SonalysisDisplayFrame.h"

/**
 * Tuner: a +-50 cent needle arc above the nearest note, its deviation in cents and the
 * measured frequency. The needle eases towards new readings while the note stays the same,
 * and jumps when the note changes. Without a confident pitch it shows "--".
 */
class TunerDisplay final : public SonalysisUi::DisplayFrame
{
  public:
    void setPitch(const Sonalysis::PitchSnapshot& pitch)
    {
        const bool sameNote = pitch.valid && m_pitch.valid && pitch.reading.midiNote == m_pitch.reading.midiNote;
        m_displayCents =
            sameNote ? m_displayCents + kEasing * (pitch.reading.cents - m_displayCents) : pitch.reading.cents;
        m_pitch = pitch;
        repaint();
    }

    void paint(juce::Graphics& g) override
    {
        auto area = paintFrame(g);
        auto gauge = area.removeFromTop(area.getHeight() * 0.55f);
        drawScale(g, gauge);
        if (m_pitch.valid)
        {
            drawNeedle(g, gauge);
        }
        drawReadout(g, area);
    }

  private:
    static constexpr float kMaxCents{50.f};
    static constexpr float kMaxAngle{std::numbers::pi_v<float> / 3.f};
    static constexpr float kInTuneCents{5.f};
    static constexpr float kCloseCents{15.f};
    static constexpr float kEasing{0.3f};

    struct Geometry
    {
        juce::Point<float> centre;
        float radius{0.f};

        [[nodiscard]] juce::Point<float> pointAt(const float cents, const float radiusScale) const noexcept
        {
            const float angle = std::clamp(cents, -kMaxCents, kMaxCents) / kMaxCents * kMaxAngle;
            return centre + juce::Point<float>{std::sin(angle), -std::cos(angle)} * radius * radiusScale;
        }
    };

    [[nodiscard]] static Geometry geometryFor(const juce::Rectangle<float>& gauge) noexcept
    {
        const float radius = std::min(gauge.getWidth() * 0.5f / std::sin(kMaxAngle), gauge.getHeight()) * 0.9f;
        return {{gauge.getCentreX(), gauge.getBottom() - 4.f}, radius};
    }

    [[nodiscard]] static SonalysisUi::Zone zoneFor(const float cents) noexcept
    {
        const float deviation = std::abs(cents);
        if (deviation <= kInTuneCents)
        {
            return SonalysisUi::Zone::Ok;
        }
        return deviation <= kCloseCents ? SonalysisUi::Zone::Warn : SonalysisUi::Zone::Danger;
    }

    static void drawScale(juce::Graphics& g, const juce::Rectangle<float>& gauge)
    {
        const auto geo = geometryFor(gauge);
        const float inTuneAngle = kInTuneCents / kMaxCents * kMaxAngle;
        juce::Path inTune;
        inTune.addCentredArc(geo.centre.x, geo.centre.y, geo.radius, geo.radius, 0.f, -inTuneAngle, inTuneAngle, true);
        g.setColour(SonalysisUi::zoneColour(SonalysisUi::Zone::Ok).withAlpha(0.6f));
        g.strokePath(inTune, juce::PathStrokeType(6.f));

        for (int cents = -50; cents <= 50; cents += 10)
        {
            const bool major = cents % 25 == 0;
            g.setColour(SonalysisUi::textColour(major ? 0.7f : 0.35f));
            g.drawLine({geo.pointAt(static_cast<float>(cents), major ? 0.86f : 0.92f),
                        geo.pointAt(static_cast<float>(cents), 1.f)},
                       1.5f);
        }
        for (const int cents : {-50, -25, 0, 25, 50})
        {
            const auto at = geo.pointAt(static_cast<float>(cents), 0.76f);
            const auto text = cents > 0 ? "+" + juce::String(cents) : juce::String(cents);
            SonalysisUi::drawText(g, text, {at.x - 20.f, at.y - 8.f, 40.f, 16.f}, 11.f, juce::Justification::centred,
                                  SonalysisUi::textColour(0.6f));
        }
    }

    void drawNeedle(juce::Graphics& g, const juce::Rectangle<float>& gauge) const
    {
        const auto geo = geometryFor(gauge);
        g.setColour(SonalysisUi::zoneColour(zoneFor(m_displayCents)));
        g.drawLine({geo.centre, geo.pointAt(m_displayCents, 0.98f)}, 3.f);
        g.fillEllipse(juce::Rectangle<float>(10.f, 10.f).withCentre(geo.centre));
    }

    void drawReadout(juce::Graphics& g, juce::Rectangle<float> area) const
    {
        const auto& reading = m_pitch.reading;
        const auto note =
            m_pitch.valid ? juce::String(reading.name().data(), reading.name().size()) + juce::String(reading.octave())
                          : juce::String("--");
        SonalysisUi::drawText(g, note, area.removeFromTop(area.getHeight() * 0.5f), 56.f, juce::Justification::centred,
                              m_pitch.valid ? SonalysisUi::zoneColour(zoneFor(m_displayCents))
                                            : SonalysisUi::textColour(0.4f));
        const float lineHeight = area.getHeight() / 3.f;
        if (m_pitch.valid)
        {
            const auto cents = (reading.cents >= 0.f ? "+" : "") + juce::String(reading.cents, 1) + " cents";
            SonalysisUi::drawText(g, cents, area.removeFromTop(lineHeight), 18.f, juce::Justification::centred,
                                  SonalysisUi::textColour(0.9f));
            SonalysisUi::drawText(g, juce::String(reading.frequency, 2) + " Hz", area.removeFromTop(lineHeight), 16.f,
                                  juce::Justification::centred, SonalysisUi::textColour(0.75f));
        }
        else
        {
            area.removeFromTop(2.f * lineHeight);
        }
        SonalysisUi::drawText(g, "A4 = " + juce::String(m_pitch.referenceA4, 1) + " Hz", area, 12.f,
                              juce::Justification::centred, SonalysisUi::textColour(0.5f));
    }

    Sonalysis::PitchSnapshot m_pitch{};
    float m_displayCents{0.f};
};
