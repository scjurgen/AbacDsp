#pragma once

#include <algorithm>
#include <cmath>
#include <juce_graphics/juce_graphics.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <numbers>
#include <vector>

#include "Analysis/Spectrogram.h"
#include "GuiConstants.h"

// Merged looper "clock": an inner disc showing the current bar in detail and an
// outer ring showing the whole loop. Both share a centre and downbeat at 12
// o'clock, time advancing clockwise. Two hands sweep from the hub like a clock:
// the short fast hand tracks the bar phase (one turn per bar), the long slow hand
// tracks the loop playhead (one turn per loop). The outer ring's angular span is
// the loop length in bars, growing while recording.
class CircularLoopDisplay : public juce::Component
{
  public:
    CircularLoopDisplay()
    {
        rebuildLut();
        m_iris = juce::Image(juce::Image::ARGB, kIrisSize, kIrisSize, true);
        buildAnnulus();
    }

    // --- Inner bar disc ---
    void setSampleRate(float sampleRate) noexcept
    {
        m_sampleRate = sampleRate;
    }
    void setSamplesPerBar(size_t samplesPerBar) noexcept
    {
        m_samplesPerBar = samplesPerBar;
    }
    void setBarBeats(int barBeats) noexcept
    {
        m_barBeats = barBeats;
    }
    void setBarPhase(float phase) noexcept
    {
        m_barPhase = phase;
    }
    void setSubdivisionPositions(const std::vector<size_t>& positions)
    {
        m_subdivisionPositions = positions;
    }

    // Inner-disc audio for the bar in progress (fed via the default signal gauge).
    void update(const std::vector<float>& data)
    {
        if (!data.empty())
        {
            m_data = data;
        }
        repaint();
    }

    // --- Outer loop ring ---
    void setLoopWaveform(const std::vector<float>& peaks)
    {
        m_loopPeaks = peaks;
    }
    // No slice table while this looper is plain (pivot away from auto-slicing); kept
    // as a stable no-op so the generated wiring (blueprint extra_timer_callbacks)
    // doesn't need to change.
    void setSliceBoundaries(const std::vector<float>&) noexcept {}
    void setPlayheadNormalized(float normalized) noexcept
    {
        m_playhead = normalized;
    }
    void setOuterRingBars(int bars) noexcept
    {
        m_outerRingBars = std::max(1, bars);
    }
    // One frame-length entry per bar (see LooperImpl::getBarFrameLengths()), so
    // a mixed-meter take's bars get proportionally-sized angular spans instead
    // of a uniform 1/N split.
    void setBarFrameLengths(const std::vector<float>& lengths)
    {
        m_barFrameLengths = lengths;
    }
    void setStateLabel(const juce::String& label)
    {
        m_stateLabel = label;
    }
    // "bar.beat" position (e.g. "2.3"), generic over whatever beatsPerBar the
    // processor is currently using.
    void setBarBeatLabel(const juce::String& label)
    {
        m_barBeatLabel = label;
    }

    // --- Outer ring spectrogram (live while recording) ---
    void setSpectrogram(const AbacDsp::SpectrumImageSet& spectro) noexcept
    {
        m_spectro = spectro;
    }
    // Write-head position within the ring: live record frames while capturing,
    // the finalized loop length once stopped.
    void setRecordHeadFrames(size_t frames) noexcept
    {
        m_recordHeadFrames = frames;
    }

    void setLabelText(const juce::String& label)
    {
        m_label = label;
        repaint();
    }

    void updateColors()
    {
        rebuildLut();
        repaint();
    }

    void paint(juce::Graphics& g) override
    {
        const auto& c = GuiConstants::instance().colors;

        g.setColour(juce::Colour(c.backgroundDark));
        g.fillRoundedRectangle(getLocalBounds().toFloat(), 3.f);

        if (m_samplesPerBar == 0 || m_barBeats <= 0)
        {
            return;
        }

        auto bounds = getLocalBounds().toFloat().reduced(kPad);
        bounds.removeFromTop(kTitleH);

        const Geometry geo{bounds};
        drawSpectrogram(g, geo);
        drawOuterRing(g, geo, c);
        drawInnerDisc(g, geo, c);
        drawHands(g, geo, c);
        drawHub(g, geo, c);
        drawLabels(g, c);
    }

  private:
    // Reuses the already-live gradient from GuiConstants (rebuilt whenever the theme
    // changes) instead of independently re-deriving it via a disk read, so this never
    // drifts out of sync with what SpectrogramDisplay itself is showing.
    void rebuildLut()
    {
        GuiConstants::instance().getSpectrogramGradient().createLookupTable(m_lut, GuiConstants::kLutSize);
    }

    static constexpr float kPad = 8.f;
    static constexpr float kTitleH = 26.f;
    static constexpr float kHalfPi = std::numbers::pi_v<float> / 2.f;
    static constexpr float k2Pi = std::numbers::pi_v<float> * 2.f;
    static constexpr float kBeatAngle = -kHalfPi; // downbeat at 12 o'clock

    struct Geometry
    {
        explicit Geometry(juce::Rectangle<float> b) noexcept
            : cx(b.getCentreX())
            , cy(b.getCentreY())
            , maxR(std::min(b.getWidth(), b.getHeight()) * 0.5f - 4.f)
            , ringOuterR(maxR * 0.98f)
            , ringInnerR(maxR * 0.70f)
            , baseR(maxR * 0.42f)
            , amplScale(maxR * 0.10f)
            , innerR(baseR - amplScale)
            , outerR(baseR + amplScale)
        {
        }
        float cx, cy, maxR, ringOuterR, ringInnerR, baseR, amplScale, innerR, outerR;
    };

    [[nodiscard]] juce::Point<float> polar(const Geometry& geo, float angle, float r) const noexcept
    {
        return {geo.cx + r * std::cos(angle), geo.cy + r * std::sin(angle)};
    }

    // Precompute, per ring-annulus pixel of the iris image, its angle (0..1
    // clockwise from 12 o'clock) and its radial fraction across the band. The
    // image maps to a square whose half-side is maxR, so radii use the same
    // 0.70..0.98 fractions as Geometry's outer ring.
    void buildAnnulus()
    {
        constexpr float kHalf = static_cast<float>(kIrisSize) / 2.f;
        constexpr float innerR = kHalf * 0.70f;
        constexpr float outerR = kHalf * 0.98f;
        m_annulus.clear();
        for (int py = 0; py < kIrisSize; ++py)
        {
            for (int px = 0; px < kIrisSize; ++px)
            {
                const float dx = static_cast<float>(px) - kHalf;
                const float dy = static_cast<float>(py) - kHalf;
                const float r = std::hypot(dx, dy);
                if (r < innerR || r > outerR)
                {
                    continue;
                }
                float phase = std::atan2(dy, dx) - kBeatAngle;
                phase -= k2Pi * std::floor(phase / k2Pi);
                m_annulus.push_back({px, py, phase / k2Pi, (r - innerR) / (outerR - innerR)});
            }
        }
    }

    // Sum of setBarFrameLengths()'s per-bar entries, valid only when that vector
    // matches the current bar count (e.g. before the first poll tick it won't).
    [[nodiscard]] float totalBarFrames() const noexcept
    {
        float total = 0.f;
        for (const float len : m_barFrameLengths)
        {
            total += len;
        }
        return total;
    }

    // Repaint the whole iris each tick (no persistent state), so the growing ring
    // rescales seamlessly: every annulus pixel maps back to the spectrogram slice
    // whose record-time falls at that angle. Pixels ahead of the head or older than
    // the ring buffer holds stay transparent.
    void renderSpectrogram()
    {
        m_iris.clear(m_iris.getBounds(), juce::Colour(0u));
        const AbacDsp::SpectrumImageSet& s = m_spectro;
        const bool haveLengths = m_barFrameLengths.size() == static_cast<size_t>(m_outerRingBars);
        const float ringFrames = (haveLengths && totalBarFrames() > 0.f)
                                     ? totalBarFrames()
                                     : static_cast<float>(m_samplesPerBar * static_cast<size_t>(m_outerRingBars));
        const size_t fftHalf = s.height;
        if (s.data == nullptr || s.width < 2 || fftHalf == 0 || s.sampleRate <= 0.f || ringFrames <= 0.f)
        {
            return;
        }
        // s.sampleRate is the (possibly decimated) rate the spectrogram FFT ran at;
        // m_recordHeadFrames is always in full-rate frames, so hop must be rescaled
        // back to that domain to stay in step with the write head.
        const float decimation = m_sampleRate / s.sampleRate;
        const float hop = static_cast<float>(s.fftLength) * s.windowForwardRatio * decimation;
        const float headF = static_cast<float>(m_recordHeadFrames);
        if (hop <= 0.f || headF <= 0.f)
        {
            return;
        }
        const size_t validSlices = std::min(s.width - 1, static_cast<size_t>(headF / hop));
        if (validSlices == 0)
        {
            return;
        }
        const size_t lastSlice = (s.activeSlice + s.width - 1) % s.width;
        const float logMin = std::log2(20.f);
        const float logMax = std::log2(s.sampleRate / 2.f);
        const float binHz = (s.sampleRate / 2.f) / static_cast<float>(fftHalf);
        const int maxBin = static_cast<int>(fftHalf) - 1;

        juce::Image::BitmapData bd(m_iris, juce::Image::BitmapData::writeOnly);
        for (const AnnulusPixel& p : m_annulus)
        {
            const float jf = (headF - p.angleNorm * ringFrames) / hop;
            if (jf < 0.f)
            {
                continue;
            }
            const size_t j = static_cast<size_t>(jf);
            if (j >= validSlices)
            {
                continue;
            }
            const size_t sliceIdx = (lastSlice + s.width - j) % s.width;
            const float hz = std::exp2(logMin + p.normR * (logMax - logMin));
            const float fbin = hz / binHz;
            const int bin0 = juce::jlimit(0, maxBin, static_cast<int>(fbin));
            const int bin1 = std::min(bin0 + 1, maxBin);
            const float bt = fbin - static_cast<float>(bin0);
            const float* row = &s.data[sliceIdx * fftHalf];
            float value = row[bin0] + bt * (row[bin1] - row[bin0]);
            value = std::pow(std::max(value, 0.f), 0.15f);
            const int lutIdx =
                juce::jlimit(0, GuiConstants::kLutSize - 1, static_cast<int>(value * (GuiConstants::kLutSize - 1)));
            const juce::Colour base = juce::Colour(m_lut[static_cast<size_t>(lutIdx)]);
            bd.setPixelColour(p.px, p.py, base.withAlpha(juce::jlimit(0.f, 1.f, value)));
        }
    }

    void drawSpectrogram(juce::Graphics& g, const Geometry& geo)
    {
        renderSpectrogram();
        const float side = 2.f * geo.maxR;
        g.drawImage(m_iris, juce::Rectangle<float>(geo.cx - geo.maxR, geo.cy - geo.maxR, side, side),
                    juce::RectanglePlacement::stretchToFit);
    }

    void drawOuterRing(juce::Graphics& g, const Geometry& geo, const GuiConstants::Colors& c) const
    {
        g.setColour(juce::Colour(c.labelColour).withAlpha(0.12f));
        g.drawEllipse(geo.cx - geo.ringInnerR, geo.cy - geo.ringInnerR, 2.f * geo.ringInnerR, 2.f * geo.ringInnerR,
                      1.f);
        g.drawEllipse(geo.cx - geo.ringOuterR, geo.cy - geo.ringOuterR, 2.f * geo.ringOuterR, 2.f * geo.ringOuterR,
                      1.f);

        drawLoopWaveformBand(g, geo, c);
        drawBarSpokes(g, geo, c);
    }

    // Actual data trace - statusOutline, matching WaveformShow's existing convention.
    void drawLoopWaveformBand(juce::Graphics& g, const Geometry& geo, const GuiConstants::Colors& c) const
    {
        if (m_loopPeaks.size() < 2)
        {
            return;
        }
        const float band = geo.ringOuterR - geo.ringInnerR;
        const float n = static_cast<float>(m_loopPeaks.size());
        g.setColour(juce::Colour(c.statusOutline));
        for (size_t i = 0; i < m_loopPeaks.size(); ++i)
        {
            const float angle = kBeatAngle + static_cast<float>(i) / n * k2Pi;
            const float r = geo.ringInnerR + juce::jlimit(0.f, 1.f, m_loopPeaks[i]) * band;
            g.drawLine(juce::Line<float>(polar(geo, angle, geo.ringInnerR), polar(geo, angle, r)), 1.0f);
        }
    }

    // Many spokes stack up visually - kept faint (except the first-bar marker) so they
    // don't read as a dark ring, matching the density lesson from CircularBarDisplay.
    void drawBarSpokes(juce::Graphics& g, const Geometry& geo, const GuiConstants::Colors& c) const
    {
        const bool haveLengths = m_barFrameLengths.size() == static_cast<size_t>(m_outerRingBars);
        const float total = haveLengths ? totalBarFrames() : 0.f;
        float cumulative = 0.f;
        for (int bar = 0; bar < m_outerRingBars; ++bar)
        {
            const float frac = (haveLengths && total > 0.f)
                                   ? cumulative / total
                                   : static_cast<float>(bar) / static_cast<float>(m_outerRingBars);
            if (haveLengths)
            {
                cumulative += m_barFrameLengths[static_cast<size_t>(bar)];
            }
            const float angle = kBeatAngle + frac * k2Pi;
            const bool first = (bar == 0);
            g.setColour(juce::Colour(c.labelColour).withAlpha(first ? 0.85f : 0.20f));
            g.drawLine(
                juce::Line<float>(polar(geo, angle, geo.ringInnerR * 0.98f), polar(geo, angle, geo.ringOuterR * 1.02f)),
                first ? 2.5f : 1.2f);
        }
    }

    void drawInnerDisc(juce::Graphics& g, const Geometry& geo, const GuiConstants::Colors& c) const
    {
        const size_t barBeats = static_cast<size_t>(m_barBeats);
        const size_t spb = m_samplesPerBar / barBeats;

        g.setColour(juce::Colour(c.labelColour).withAlpha(0.10f));
        g.drawEllipse(geo.cx - geo.baseR, geo.cy - geo.baseR, 2.f * geo.baseR, 2.f * geo.baseR, 0.5f);

        if (spb > 0 && !m_subdivisionPositions.empty())
        {
            g.setColour(juce::Colour(c.labelColour).withAlpha(0.25f));
            for (size_t b = 0; b < barBeats; ++b)
            {
                for (const size_t offset : m_subdivisionPositions)
                {
                    const float frac = static_cast<float>(b * spb + offset) / static_cast<float>(m_samplesPerBar);
                    const float angle = kBeatAngle + frac * k2Pi;
                    g.drawLine(
                        juce::Line<float>(polar(geo, angle, geo.baseR * 0.9f), polar(geo, angle, geo.baseR * 1.1f)),
                        1.0f);
                }
            }
        }

        for (size_t k = 0; k < barBeats; ++k)
        {
            const float angle = kBeatAngle + static_cast<float>(k) / static_cast<float>(barBeats) * k2Pi;
            const bool downbeat = (k == 0);
            g.setColour(juce::Colour(c.labelColour).withAlpha(downbeat ? 0.85f : 0.25f));
            g.drawLine(juce::Line<float>(polar(geo, angle, geo.innerR * 0.5f),
                                         polar(geo, angle, geo.outerR * (downbeat ? 1.2f : 1.1f))),
                       downbeat ? 2.5f : 1.5f);
        }

        drawBarWaveform(g, geo, c);
    }

    // Actual data trace - statusOutline, matching WaveformShow's existing convention.
    void drawBarWaveform(juce::Graphics& g, const Geometry& geo, const GuiConstants::Colors& c) const
    {
        if (m_data.size() < 2)
        {
            return;
        }
        const size_t n = m_data.size();
        juce::Path wave;
        for (size_t i = 0; i < n; ++i)
        {
            const float angle = kBeatAngle + static_cast<float>(i) / static_cast<float>(n) * k2Pi;
            const auto pt = polar(geo, angle, geo.baseR + m_data[i] * geo.amplScale);
            if (i == 0)
            {
                wave.startNewSubPath(pt);
            }
            else
            {
                wave.lineTo(pt);
            }
        }
        g.setColour(juce::Colour(c.statusOutline).withAlpha(0.85f));
        g.strokePath(wave, juce::PathStrokeType(1.5f));
    }

    void drawHands(juce::Graphics& g, const Geometry& geo, const GuiConstants::Colors& c) const
    {
        // Slow hand: loop playhead, drawn only across the outer ring so it never
        // reaches the centre (that space belongs to the fast bar hand). labelColour,
        // not statusOutline, so it stays distinct from the data traces it sweeps over.
        const float loopAngle = kBeatAngle + juce::jlimit(0.f, 1.f, m_playhead) * k2Pi;
        g.setColour(juce::Colour(c.labelColour).withAlpha(0.85f));
        g.drawLine(juce::Line<float>(polar(geo, loopAngle, geo.ringInnerR), polar(geo, loopAngle, geo.ringOuterR)),
                   2.5f);

        // Short fast hand: bar phase, within the inner disc.
        const float barAngle = kBeatAngle + std::clamp(m_barPhase, 0.f, 1.f) * k2Pi;
        g.setColour(juce::Colour(c.labelColour).withAlpha(0.90f));
        g.drawLine(juce::Line<float>(polar(geo, barAngle, 0.f), polar(geo, barAngle, geo.outerR)), 2.0f);
    }

    void drawHub(juce::Graphics& g, const Geometry& geo, const GuiConstants::Colors& c) const
    {
        constexpr float kHubR = 4.f;
        g.setColour(juce::Colour(c.labelColour).withAlpha(0.85f));
        g.fillEllipse(geo.cx - kHubR, geo.cy - kHubR, 2.f * kHubR, 2.f * kHubR);
    }

    void drawLabels(juce::Graphics& g, const GuiConstants::Colors& c) const
    {
        const auto titleBounds = getLocalBounds().toFloat().reduced(kPad).removeFromTop(kTitleH);
        g.setFont(juce::Font(juce::FontOptions(21.f)));
        if (m_stateLabel.isNotEmpty())
        {
            g.setColour(juce::Colour(c.labelColour).withAlpha(0.85f));
            g.drawText(m_stateLabel, titleBounds, juce::Justification::centredLeft);
        }
        if (m_label.isNotEmpty())
        {
            g.setColour(juce::Colour(c.labelColour).withAlpha(0.55f));
            g.drawText(m_label, titleBounds, juce::Justification::centredRight);
        }
        if (m_barBeatLabel.isNotEmpty())
        {
            g.setColour(juce::Colour(c.labelColour).withAlpha(0.85f));
            g.drawText(m_barBeatLabel, titleBounds, juce::Justification::centred);
        }
    }

    static constexpr int kIrisSize = 256;

    struct AnnulusPixel
    {
        int px;
        int py;
        float angleNorm; // 0..1 clockwise from 12 o'clock
        float normR;     // 0..1 across the ring band (inner->outer)
    };

    std::vector<float> m_data;
    std::vector<float> m_loopPeaks;
    std::vector<size_t> m_subdivisionPositions;
    float m_sampleRate{48000.f};
    size_t m_samplesPerBar{0};
    int m_barBeats{4};
    float m_barPhase{0.f};
    float m_playhead{0.f};
    int m_outerRingBars{1};
    std::vector<float> m_barFrameLengths;
    juce::String m_stateLabel;
    juce::String m_label;
    juce::String m_barBeatLabel;

    AbacDsp::SpectrumImageSet m_spectro{};
    size_t m_recordHeadFrames{0};
    juce::Image m_iris;
    juce::PixelARGB m_lut[GuiConstants::kLutSize]{};
    std::vector<AnnulusPixel> m_annulus;
};
