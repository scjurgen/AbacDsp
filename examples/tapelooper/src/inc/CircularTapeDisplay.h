#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <juce_graphics/juce_graphics.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <numbers>
#include <vector>

#include "../impl/ClockDisplayShared.h"
#include "Analysis/Spectrogram.h"
#include "GuiConstants.h"

using TapeLooperDetail::kIrisAngularBuckets;

// Tapelooper's clock: like CircularLoopDisplay, but the ring never grows - BARS
// fixes its span up front - and up to four transports (tracks A/B/C plus the
// groove) can be live at once, so per-track state is four small dots rather than
// extra hands. Inner disc = the bar in progress, outer ring = the full BARS-bar
// loop, both sharing a downbeat at 12 o'clock, time advancing clockwise. The iris
// is fed continuously from the plugin's live output, not a captured take.
class CircularTapeDisplay : public juce::Component
{
  public:
    enum class TrackState : int
    {
        Stopped = 0,
        Playing = 1,
        Recording = 2
    };

    CircularTapeDisplay()
    {
        rebuildLut();
        m_iris = juce::Image(juce::Image::ARGB, kIrisSize, kIrisSize, true);
        buildAnnulus();
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

    // Bar-in-progress audio (fed via the default signal gauge).
    void update(const std::vector<float>& data)
    {
        if (!data.empty())
        {
            m_data = data;
        }
        repaint();
    }

    // Rolling peak envelope of the current loop pass, one entry per angular bucket.
    void setLoopWaveform(const std::vector<float>& peaks)
    {
        m_loopPeaks = peaks;
    }
    void setPlayheadNormalized(float normalized) noexcept
    {
        m_playhead = normalized;
    }
    void setOuterRingBars(int bars) noexcept
    {
        m_outerRingBars = std::max(1, bars);
    }
    // "bar.beat" position (e.g. "2.3").
    void setBarBeatLabel(const juce::String& label)
    {
        m_barBeatLabel = label;
    }

    // Per-slice angular bucket, stamped by the impl at the moment each slice's audio
    // was actually fed in - must arrive before setSpectrogram() each tick.
    void setSpectrogramSliceBuckets(const std::vector<size_t>& buckets)
    {
        m_sliceBuckets = buckets;
    }

    // Position-indexed, not time-indexed: every new slice is written into its own
    // stamped bucket, overwriting what was there - a persistence display, not a
    // scrolling one. See consumeNewSlices().
    void setSpectrogram(const AbacDsp::SpectrumImageSet& spectro)
    {
        consumeNewSlices(spectro);
        m_spectro = spectro;
    }

    void setTrackStateA(int state) noexcept
    {
        m_trackStates[0] = state;
    }
    void setTrackStateB(int state) noexcept
    {
        m_trackStates[1] = state;
    }
    void setTrackStateC(int state) noexcept
    {
        m_trackStates[2] = state;
    }
    void setGrooveState(int state) noexcept
    {
        m_trackStates[3] = state;
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
        drawCenterLabel(g, geo, c);
        drawTitleRow(g, c);
    }

  private:
    // Reuses the already-live gradient from GuiConstants (rebuilt whenever the theme
    // changes) instead of independently re-deriving it via a disk read, so this never
    // drifts out of sync with what SpectrogramDisplay itself is showing.
    void rebuildLut()
    {
        GuiConstants::instance().getSpectrogramGradient().createLookupTable(m_lut, GuiConstants::kLutSize);
    }
    static constexpr float innerRingFactor{.6f};
    static constexpr float outerRingFactor{.98f};
    static constexpr float kPad = 8.f;
    static constexpr float kTitleH = 26.f;
    static constexpr float kHalfPi = std::numbers::pi_v<float> / 2.f;
    static constexpr float k2Pi = std::numbers::pi_v<float> * 2.f;
    static constexpr float kBeatAngle = -kHalfPi; // downbeat at 12 o'clock
    static constexpr float kSpectrogramEdgeMaskWidth = 7.f;

    struct Geometry
    {
        explicit Geometry(juce::Rectangle<float> b) noexcept
            : cx(b.getCentreX())
            , cy(b.getCentreY())
            , maxR(std::min(b.getWidth(), b.getHeight()) * 0.5f - 4.f)
            , ringOuterR(maxR * outerRingFactor)
            , ringInnerR(maxR * innerRingFactor)
            , baseR(maxR * 0.28f)
            , amplScale(maxR * 0.06f)
            , innerR(baseR - amplScale)
            , outerR(baseR + amplScale)
            , volOuterR(ringInnerR)
            , volInnerR(ringInnerR - maxR * 0.24f)
        {
        }
        float cx, cy, maxR, ringOuterR, ringInnerR, baseR, amplScale, innerR, outerR, volOuterR, volInnerR;
    };

    static juce::Point<float> polar(const Geometry& geo, const float angle, const float r) noexcept
    {
        return {geo.cx + r * std::cos(angle), geo.cy + r * std::sin(angle)};
    }

    // Precompute, per ring-annulus pixel of the iris image, its angle (0..1
    // clockwise from 12 o'clock) and its radial fraction across the band
    // (same 0.60..0.98 radii as Geometry's outer ring).
    void buildAnnulus()
    {
        constexpr float kHalf = static_cast<float>(kIrisSize) / 2.f;
        constexpr float innerR = kHalf * innerRingFactor;
        constexpr float outerR = kHalf * outerRingFactor;
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
                m_annulus.push_back(
                    {.px = px, .py = py, .angleNorm = phase / k2Pi, .normR = (r - innerR) / (outerR - innerR)});
            }
        }
    }

    // Copies every slice that arrived since the last call into the bucket it was
    // stamped with (m_sliceBuckets, set by the impl at feed time) - so a slice
    // lands where its audio actually was, not where the playhead is by now.
    void consumeNewSlices(const AbacDsp::SpectrumImageSet& s)
    {
        if (s.data == nullptr || s.width < 2 || s.height == 0 || m_sliceBuckets.size() != s.width)
        {
            return;
        }
        if (m_irisMagnitudes.size() != kIrisAngularBuckets * s.height)
        {
            m_irisMagnitudes.assign(kIrisAngularBuckets * s.height, 0.f);
            m_lastConsumedSlice = s.activeSlice;
            return;
        }
        size_t idx = m_lastConsumedSlice;
        size_t guard = 0;
        while (idx != s.activeSlice && guard < s.width)
        {
            const size_t bucket = std::min(kIrisAngularBuckets - 1, m_sliceBuckets[idx]);
            std::copy_n(&s.data[idx * s.height], s.height, &m_irisMagnitudes[bucket * s.height]);
            idx = (idx + 1) % s.width;
            ++guard;
        }
        m_lastConsumedSlice = s.activeSlice;
    }

    // Every annulus pixel maps to a fixed angular bucket and reads whatever was
    // last written there - no time lookback, so a bucket only changes when the
    // playhead sweeps past it again.
    void renderSpectrogram()
    {
        m_iris.clear(m_iris.getBounds(), juce::Colour(0u));
        const AbacDsp::SpectrumImageSet& s = m_spectro;
        const size_t fftHalf = s.height;
        if (fftHalf == 0 || s.sampleRate <= 0.f || m_irisMagnitudes.size() != kIrisAngularBuckets * fftHalf)
        {
            return;
        }
        const float logMin = std::log2(20.f);
        const float logMax = std::log2(s.sampleRate / 2.f);
        const float binHz = (s.sampleRate / 2.f) / static_cast<float>(fftHalf);
        const int maxBin = static_cast<int>(fftHalf) - 1;

        juce::Image::BitmapData bd(m_iris, juce::Image::BitmapData::writeOnly);
        for (const AnnulusPixel& p : m_annulus)
        {
            const auto bucket = std::min(kIrisAngularBuckets - 1,
                                         static_cast<size_t>(p.angleNorm * static_cast<float>(kIrisAngularBuckets)));
            const float hz = std::exp2(logMin + p.normR * (logMax - logMin));
            const float fbin = hz / binHz;
            const int bin0 = juce::jlimit(0, maxBin, static_cast<int>(fbin));
            const int bin1 = std::min(bin0 + 1, maxBin);
            const float bt = fbin - static_cast<float>(bin0);
            const float* row = &m_irisMagnitudes[bucket * fftHalf];
            float value = row[bin0] + bt * (row[bin1] - row[bin0]);
            value = std::pow(std::max(value, 0.f), 0.1f); // adjusted by taste (the smaller the more prominent)
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
        // Masks the jagged raster edge renderSpectrogram() leaves at the annulus
        // boundaries (m_iris is a fixed 256x256 image, stretched up to the ring).
        g.setColour(juce::Colour(c.backgroundDark));
        g.drawEllipse(geo.cx - geo.ringInnerR, geo.cy - geo.ringInnerR, 2.f * geo.ringInnerR, 2.f * geo.ringInnerR,
                      kSpectrogramEdgeMaskWidth);
        g.drawEllipse(geo.cx - geo.ringOuterR, geo.cy - geo.ringOuterR, 2.f * geo.ringOuterR, 2.f * geo.ringOuterR,
                      kSpectrogramEdgeMaskWidth);

        g.setColour(juce::Colour(c.labelColour).withAlpha(0.12f));
        g.drawEllipse(geo.cx - geo.ringInnerR, geo.cy - geo.ringInnerR, 2.f * geo.ringInnerR, 2.f * geo.ringInnerR,
                      1.f);
        g.drawEllipse(geo.cx - geo.ringOuterR, geo.cy - geo.ringOuterR, 2.f * geo.ringOuterR, 2.f * geo.ringOuterR,
                      1.f);

        drawLoopWaveformBand(g, geo, c);
        drawBarSpokes(g, geo, c);
    }

    // Actual data trace - statusOutline, matching WaveformShow's existing convention. A
    // separate band just inside the spectrogram ring (volOuterR..volInnerR), not overlapping
    // it: anchored at volOuterR, dipping inward toward volInnerR as loudness grows.
    void drawLoopWaveformBand(juce::Graphics& g, const Geometry& geo, const GuiConstants::Colors& c) const
    {
        if (m_loopPeaks.size() < 2)
        {
            return;
        }
        const float band = geo.volOuterR - geo.volInnerR;
        const size_t count = m_loopPeaks.size();
        const auto n = static_cast<float>(count);
        juce::Path wave;
        for (size_t i = 0; i < count; ++i)
        {
            const float angle = kBeatAngle + static_cast<float>(i) / n * k2Pi;
            const auto pt = polar(geo, angle, geo.volOuterR);
            if (i == 0)
            {
                wave.startNewSubPath(pt);
            }
            else
            {
                wave.lineTo(pt);
            }
        }
        for (size_t k = 0; k < count; ++k)
        {
            const size_t i = count - 1 - k;
            const float angle = kBeatAngle + static_cast<float>(i) / n * k2Pi;
            const float r = geo.volOuterR - juce::jlimit(0.f, 1.f, m_loopPeaks[i]) * band;
            wave.lineTo(polar(geo, angle, r));
        }
        wave.closeSubPath();
        g.setColour(juce::Colour(c.statusOutline).withAlpha(0.35f));
        g.fillPath(wave);
    }

    // Every bar is the same length (uniform BPM/tape speed), so spokes split the
    // ring evenly - unlike looper's mixed-meter takes, no per-bar length table needed.
    void drawBarSpokes(juce::Graphics& g, const Geometry& geo, const GuiConstants::Colors& c) const
    {
        g.setColour(juce::Colour(c.labelColour).withAlpha(0.20f));
        for (int bar = 0; bar < m_outerRingBars; ++bar)
        {
            const float frac = static_cast<float>(bar) / static_cast<float>(m_outerRingBars);
            const float angle = kBeatAngle + frac * k2Pi;
            g.drawLine(
                juce::Line<float>(polar(geo, angle, geo.ringInnerR * 0.98f), polar(geo, angle, geo.ringOuterR * 1.02f)),
                1.2f);
        }
    }

    void drawInnerDisc(juce::Graphics& g, const Geometry& geo, const GuiConstants::Colors& c) const
    {
        const auto barBeats = static_cast<size_t>(m_barBeats);

        g.setColour(juce::Colour(c.labelColour).withAlpha(0.10f));
        g.drawEllipse(geo.cx - geo.baseR, geo.cy - geo.baseR, 2.f * geo.baseR, 2.f * geo.baseR, 0.5f);

        g.setColour(juce::Colour(c.labelColour).withAlpha(0.25f));
        for (size_t k = 0; k < barBeats; ++k)
        {
            const float angle = kBeatAngle + static_cast<float>(k) / static_cast<float>(barBeats) * k2Pi;
            g.drawLine(juce::Line<float>(polar(geo, angle, geo.innerR * 0.5f), polar(geo, angle, geo.outerR * 1.1f)),
                       1.5f);
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
        // reaches the centre (that space belongs to the fast bar hand).
        const float loopAngle = kBeatAngle + juce::jlimit(0.f, 1.f, m_playhead) * k2Pi;
        g.setColour(juce::Colour(c.labelColour).withAlpha(0.85f));
        g.drawLine(juce::Line<float>(polar(geo, loopAngle, geo.ringInnerR), polar(geo, loopAngle, geo.ringOuterR)),
                   2.5f);

        // Short fast hand: bar phase, confined to the disc's band.
        const float barAngle = kBeatAngle + std::clamp(m_barPhase, 0.f, 1.f) * k2Pi;
        g.setColour(juce::Colour(c.labelColour).withAlpha(0.90f));
        g.drawLine(juce::Line<float>(polar(geo, barAngle, geo.innerR), polar(geo, barAngle, geo.outerR)), 2.0f);
    }

    // Bar.beat, centred on the hub - the space the fast hand used to sweep through
    // before it moved to the disc's band.
    void drawCenterLabel(juce::Graphics& g, const Geometry& geo, const GuiConstants::Colors& c) const
    {
        if (m_barBeatLabel.isEmpty())
        {
            return;
        }
        const juce::Rectangle<float> line(geo.cx - geo.innerR, geo.cy - geo.innerR * 0.3f, 2.f * geo.innerR,
                                          geo.innerR * 0.6f);
        g.setFont(juce::Font(juce::FontOptions(21.f)));
        g.setColour(juce::Colour(c.labelColour).withAlpha(0.85f));
        g.drawText(m_barBeatLabel, line, juce::Justification::centred);
    }

    // Left: one small dot per transport (A, B, C, Groove) - filled when recording,
    // outlined when playing, faint when stopped. Right: the title, unchanged.
    void drawTitleRow(juce::Graphics& g, const GuiConstants::Colors& c) const
    {
        const auto titleBounds = getLocalBounds().toFloat().reduced(kPad).removeFromTop(kTitleH);

        static constexpr std::array<const char*, 4> kDotLabels{"A", "B", "C", "G"};
        constexpr float kDotR = 6.f;
        constexpr float kDotGap = 22.f;
        float dotX = titleBounds.getX() + kDotR;
        const float dotY = titleBounds.getCentreY();
        g.setFont(juce::Font(juce::FontOptions(10.f)));
        for (size_t i = 0; i < kDotLabels.size(); ++i)
        {
            const auto state = static_cast<TrackState>(m_trackStates[i]);
            const juce::Rectangle<float> dot(dotX - kDotR, dotY - kDotR, 2.f * kDotR, 2.f * kDotR);
            if (state == TrackState::Recording)
            {
                g.setColour(juce::Colour(c.statusOutline));
                g.fillEllipse(dot);
            }
            else if (state == TrackState::Playing)
            {
                g.setColour(juce::Colour(c.statusOutline));
                g.drawEllipse(dot, 1.5f);
            }
            else
            {
                g.setColour(juce::Colour(c.labelColour).withAlpha(0.25f));
                g.drawEllipse(dot, 1.f);
            }
            g.setColour(juce::Colour(c.labelColour).withAlpha(0.70f));
            g.drawText(kDotLabels[i], dot.expanded(8.f, 2.f), juce::Justification::centredLeft);
            dotX += kDotGap;
        }

        if (m_label.isNotEmpty())
        {
            g.setFont(juce::Font(juce::FontOptions(21.f)));
            g.setColour(juce::Colour(c.labelColour).withAlpha(0.55f));
            g.drawText(m_label, titleBounds, juce::Justification::centredRight);
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
    size_t m_samplesPerBar{0};
    int m_barBeats{4};
    float m_barPhase{0.f};
    float m_playhead{0.f};
    int m_outerRingBars{1};
    juce::String m_label;
    juce::String m_barBeatLabel;
    std::array<int, 4> m_trackStates{};

    AbacDsp::SpectrumImageSet m_spectro{};
    std::vector<float> m_irisMagnitudes; // kIrisAngularBuckets * fftHalf, position-indexed
    std::vector<size_t> m_sliceBuckets;  // parallel to the spectrogram's own ring
    size_t m_lastConsumedSlice{0};
    juce::Image m_iris;
    juce::PixelARGB m_lut[GuiConstants::kLutSize]{};
    std::vector<AnnulusPixel> m_annulus;
};
