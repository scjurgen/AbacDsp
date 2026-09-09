#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <fstream>
#include <juce_gui_basics/juce_gui_basics.h>

#include "../inc/GuiConstants.h"
#include "Synthesizer/AmbientPadVoice.h"

// One thin row per voice slot (always all 16), each a 1-minute Material/Volume timeline rather
// than a single instantaneous value: a fixed-size ring buffer per row swept left to right, the
// write cursor wrapping back to the start once it reaches the end (an oscilloscope-style sweep,
// not a shifting scroll) - cheap to update and free of any "spike falls off the edge" cliff.
class ShowVoiceRows : public juce::Component
{
  public:
    static constexpr size_t kMaxVoices{16};
    static constexpr size_t kHistoryPoints{600};
    static constexpr double kHistorySeconds{60.0};
    static constexpr const char* kDiagnosisLogPath{"/tmp/ambientpad_voice_debug.csv"};

    ShowVoiceRows()
    {
        std::ofstream log(kDiagnosisLogPath, std::ios::out | std::ios::trunc);
        log << "timestamp,material,materialRange,light,motion,breath,stability,bloom,hold,"
               "materialMorph,ouBreath,ouMaterial,ouLens,ouDrift,filterCutoffHz,filterCharacterPos,"
               "filterResonance,osc0Hz,osc1Hz,envelope,gain,breathRippleGain,velocityGain,pitchSemitones,"
               "isPlaying\n";
    }

    void update(const std::array<AbacDsp::AmbientPadVoice::ModulationSnapshot, kMaxVoices>& snapshots)
    {
        logForDiagnosis(snapshots);

        constexpr double pushIntervalMs = kHistorySeconds * 1000.0 / static_cast<double>(kHistoryPoints);
        const auto now = juce::Time::getMillisecondCounterHiRes();
        if (now - m_lastPushMs < pushIntervalMs)
        {
            return;
        }
        m_lastPushMs = now;
        for (size_t v = 0; v < kMaxVoices; ++v)
        {
            m_material[v][m_writeIndex] = std::clamp(snapshots[v].materialMorph, 0.f, 1.f);
            const auto& s = snapshots[v];
            m_volume[v][m_writeIndex] = std::clamp(s.envelope * s.velocityGain * s.breathRippleGain * s.gain, 0.f, 1.f);
        }
        m_writeIndex = (m_writeIndex + 1) % kHistoryPoints;
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

        const auto rowHeight = bounds.getHeight() / static_cast<float>(kMaxVoices);
        constexpr float channelLabelWidth = 22.f;
        const auto traceWidth = bounds.getWidth() - channelLabelWidth;

        // The gradient's low end ("coolest") sits close to backgroundDark in every theme, so
        // both traces are drawn from its bright half instead - never close to invisible.
        const auto gradient = GuiConstants::instance().getSpectrogramGradient();
        const auto materialColour = gradient.getColourAtPosition(1.0);
        const auto volumeColour = gradient.getColourAtPosition(0.55);

        g.setFont(juce::FontOptions(std::min(rowHeight * 0.6f, 11.f)));
        for (size_t v = 0; v < kMaxVoices; ++v)
        {
            const juce::Rectangle<float> row{bounds.getX() + channelLabelWidth,
                                             bounds.getY() + static_cast<float>(v) * rowHeight, traceWidth,
                                             rowHeight - 1.f};

            g.setColour(juce::Colour(colors.labelColour));
            g.drawText(juce::String(v + 1),
                       juce::Rectangle<float>{bounds.getX(), row.getY(), channelLabelWidth, row.getHeight()},
                       juce::Justification::centredLeft);

            drawSweep(g, row, m_material[v], materialColour);
            drawSweep(g, row, m_volume[v], volumeColour);
        }
    }

  private:
    // TEMPORARY: diagnosing modulation depth - remove once resolved. One CSV row per second
    // for voice 1 only, every ModulationSnapshot field, for offline analysis.
    void logForDiagnosis(const std::array<AbacDsp::AmbientPadVoice::ModulationSnapshot, kMaxVoices>& snapshots)
    {
        const auto now = juce::Time::getMillisecondCounterHiRes();
        if (now - m_lastLogMs < 1000.0)
        {
            return;
        }
        m_lastLogMs = now;
        const auto& s = snapshots[0];
        std::ofstream log(kDiagnosisLogPath, std::ios::app);
        log << juce::Time::getCurrentTime().toString(false, true, true, true) << ',' << s.material << ','
            << s.materialRange << ',' << s.light << ',' << s.motion << ',' << s.breath << ',' << s.stability << ','
            << s.bloom << ',' << (s.hold ? 1 : 0) << ',' << s.materialMorph << ',' << s.ouBreath << ',' << s.ouMaterial
            << ',' << s.ouLens << ',' << s.ouDrift << ',' << s.filterCutoffHz << ',' << s.filterCharacterPos << ','
            << s.filterResonance << ',' << s.osc0Hz << ',' << s.osc1Hz << ',' << s.envelope << ',' << s.gain << ','
            << s.breathRippleGain << ',' << s.velocityGain << ',' << s.pitchSemitones << ',' << (s.isPlaying ? 1 : 0)
            << '\n';
    }

    void drawSweep(juce::Graphics& g, const juce::Rectangle<float>& row, const std::array<float, kHistoryPoints>& data,
                   const juce::Colour colour) const
    {
        const auto pointAt = [&](const size_t index) noexcept
        {
            const auto x =
                row.getX() + row.getWidth() * static_cast<float>(index) / static_cast<float>(kHistoryPoints - 1);
            const auto y = row.getBottom() - row.getHeight() * data[index];
            return juce::Point<float>{x, y};
        };

        // Two subpaths, split at the write cursor: the points either side of it are 1 minute
        // apart in time, not adjacent, so joining them with a line would draw a false jump.
        g.setColour(colour);
        if (m_writeIndex > 1)
        {
            juce::Path fresh;
            fresh.startNewSubPath(pointAt(0));
            for (size_t i = 1; i < m_writeIndex; ++i)
            {
                fresh.lineTo(pointAt(i));
            }
            g.strokePath(fresh, juce::PathStrokeType(1.5f));
        }
        if (m_writeIndex < kHistoryPoints - 1)
        {
            juce::Path stale;
            stale.startNewSubPath(pointAt(m_writeIndex));
            for (size_t i = m_writeIndex + 1; i < kHistoryPoints; ++i)
            {
                stale.lineTo(pointAt(i));
            }
            g.strokePath(stale, juce::PathStrokeType(1.5f));
        }
    }

    std::array<std::array<float, kHistoryPoints>, kMaxVoices> m_material{};
    std::array<std::array<float, kHistoryPoints>, kMaxVoices> m_volume{};
    size_t m_writeIndex{0};
    double m_lastPushMs{0.0};
    double m_lastLogMs{0.0};
};
