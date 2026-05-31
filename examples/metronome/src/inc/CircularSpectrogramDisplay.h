#pragma once

#include <cmath>
#include <juce_graphics/juce_graphics.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <numbers>
#include <vector>

#include "Analysis/Spectrogram.h"
#include "AppSettings.h"
#include "GuiConstants.h"

// Circular spectrogram iris with a "writing-head" model.
// The display is a persistent polar image: one revolution = one bar, downbeat at
// 12 o'clock, time advancing clockwise.  Each timer tick paints a narrow wedge at
// the current bar-phase position using the latest FFT slice, exactly like a
// circular chart recorder.  Old data persists until the head sweeps over it again.
// Radial axis = log-scale frequency (low at centre, high at outer edge).
class CircularSpectrogramDisplay : public juce::Component
{
  public:
    CircularSpectrogramDisplay()
    {
        GuiConstants::buildLut(AppSettings::loadTheme(), m_lut);
        m_irisImage = juce::Image(juce::Image::ARGB, kImageSize, kImageSize, true);
    }

    void update(AbacDsp::SpectrumImageSet imageSet)
    {
        if (imageSet.data == nullptr)
        {
            return;
        }
        m_imageSet = imageSet;
        repaint();
    }

    void setGradientPreset(GuiConstants::GradientPreset preset)
    {
        GuiConstants::buildLut(preset, m_lut);
        m_irisImage.clear(m_irisImage.getBounds(), juce::Colour(0u));
        m_lastBarPhase = -1.f;
        repaint();
    }

    void setLabelText(const juce::String& label)
    {
        m_label = label;
    }
    void setSampleRate(float sr) noexcept
    {
        m_sampleRate = sr;
    }
    void setSamplesPerBeat(size_t spb) noexcept
    {
        m_samplesPerBeat = spb;
    }
    void setBarBeats(int beats) noexcept
    {
        m_barBeats = beats;
    }
    void setBarPhase(float phase) noexcept
    {
        m_barPhase = phase;
    }
    void setSubdivisionPositions(const std::vector<size_t>& pos)
    {
        m_subdivPositions = pos;
    }

    void updateColors()
    {
        GuiConstants::buildLut(AppSettings::loadTheme(), m_lut);
        m_irisImage.clear(m_irisImage.getBounds(), juce::Colour(0u));
        m_lastBarPhase = -1.f;
        repaint();
    }

    void paint(juce::Graphics& g) override
    {
        const auto& c = GuiConstants::instance().colors;
        g.setColour(juce::Colour(c.cols[0]));
        g.fillRoundedRectangle(getLocalBounds().toFloat(), 3.f);

        if (m_imageSet.data == nullptr || m_samplesPerBeat == 0 || m_barBeats == 0 || m_imageSet.fftLength == 0 ||
            m_sampleRate <= 0.f)
        {
            return;
        }

        paintWedge();

        constexpr float kPad = 8.f;
        constexpr float kTitleH = 16.f;
        auto drawBounds = getLocalBounds().toFloat().reduced(kPad);
        if (m_label.isNotEmpty())
        {
            drawBounds.removeFromTop(kTitleH);
        }

        const float side = std::min(drawBounds.getWidth(), drawBounds.getHeight());
        const auto irisArea = juce::Rectangle<float>(drawBounds.getCentreX() - side / 2.f,
                                                     drawBounds.getCentreY() - side / 2.f, side, side);

        g.drawImage(m_irisImage, irisArea, juce::RectanglePlacement::stretchToFit);
        drawOverlays(g, irisArea, c);

        if (m_label.isNotEmpty())
        {
            g.setColour(juce::Colour(c.cols[7]).withAlpha(0.75f));
            g.setFont(juce::Font(juce::FontOptions(11.f)));
            g.drawText(m_label, getLocalBounds().toFloat().reduced(kPad).removeFromTop(kTitleH).toNearestInt(),
                       juce::Justification::centred);
        }
    }

  private:
    static constexpr int kImageSize = 256;
    static constexpr float kCentre = static_cast<float>(kImageSize) / 2.f;
    static constexpr float kOuterR = kCentre * 0.92f;
    static constexpr float kInnerR = kCentre * 0.12f;
    static constexpr float kBeatAngle = -std::numbers::pi_v<float> / 2.f;
    static constexpr float k2Pi = std::numbers::pi_v<float> * 2.f;

    // Paint the wedge between the last bar phase and the current bar phase.
    void paintWedge()
    {
        const float currentPhase = m_barPhase;

        // First call or bar wrap: just record the position, paint nothing yet.
        if (m_lastBarPhase < 0.f)
        {
            m_lastBarPhase = currentPhase;
            return;
        }

        float fromAngle = kBeatAngle + m_lastBarPhase * k2Pi;
        float toAngle = kBeatAngle + currentPhase * k2Pi;

        // Bar wrapped around: draw from lastAngle to end-of-circle then reset.
        if (currentPhase < m_lastBarPhase - 0.05f)
        {
            // Paint remaining arc to complete the revolution, then start fresh.
            paintArc(fromAngle, kBeatAngle + k2Pi);
            m_lastBarPhase = 0.f;
            fromAngle = kBeatAngle;
        }

        if (toAngle > fromAngle)
        {
            paintArc(fromAngle, toAngle);
        }

        m_lastBarPhase = currentPhase;
    }

    void paintArc(float fromAngle, float toAngle)
    {
        if (toAngle <= fromAngle)
        {
            return;
        }

        const size_t fftHalf = m_imageSet.height;
        if (fftHalf == 0 || m_imageSet.width == 0)
        {
            return;
        }

        // Use the most recently completed FFT slice.
        const size_t sliceIdx = m_imageSet.activeSlice == 0 ? m_imageSet.width - 1 : m_imageSet.activeSlice - 1;

        const float logMinHz = std::log2(20.f);
        const float logMaxHz = std::log2(m_sampleRate / 2.f);
        const float binHz = (m_sampleRate / 2.f) / static_cast<float>(fftHalf);

        // Angular step: one pixel arc-length at the outer edge.
        const float dAngle = 1.f / kOuterR;

        juce::Image::BitmapData bd(m_irisImage, juce::Image::BitmapData::readWrite);

        for (float angle = fromAngle; angle <= toAngle + dAngle * 0.5f; angle += dAngle)
        {
            const float cosA = std::cos(angle);
            const float sinA = std::sin(angle);

            for (float r = kInnerR; r <= kOuterR; r += 1.f)
            {
                const float normR = (r - kInnerR) / (kOuterR - kInnerR);
                const float hz = std::pow(2.f, logMinHz + normR * (logMaxHz - logMinHz));
                const float fBin = hz / binHz;
                const int bin0 = juce::jlimit(0, static_cast<int>(fftHalf) - 1, static_cast<int>(fBin));
                const int bin1 = std::min(bin0 + 1, static_cast<int>(fftHalf) - 1);
                const float bt = fBin - static_cast<float>(bin0);

                const float v0 = m_imageSet.data[sliceIdx * fftHalf + static_cast<size_t>(bin0)];
                const float v1 = m_imageSet.data[sliceIdx * fftHalf + static_cast<size_t>(bin1)];
                float value = v0 + bt * (v1 - v0);
                value = std::pow(std::max(value, 0.f), 0.15f);
                const int lutIdx =
                    juce::jlimit(0, GuiConstants::kLutSize - 1,
                                 static_cast<int>(value * static_cast<float>(GuiConstants::kLutSize - 1)));

                const int px = static_cast<int>(kCentre + r * cosA);
                const int py = static_cast<int>(kCentre + r * sinA);
                if (static_cast<unsigned>(px) < static_cast<unsigned>(kImageSize) &&
                    static_cast<unsigned>(py) < static_cast<unsigned>(kImageSize))
                {
                    bd.setPixelColour(px, py, juce::Colour(m_lut[static_cast<size_t>(lutIdx)]));
                }
            }
        }
    }

    void drawOverlays(juce::Graphics& g, juce::Rectangle<float> irisArea, const GuiConstants::Colors& c) const
    {
        const float scale = irisArea.getWidth() / static_cast<float>(kImageSize);
        const float scx = irisArea.getCentreX();
        const float scy = irisArea.getCentreY();
        const float outerR = kOuterR * scale;
        const float innerR = kInnerR * scale;

        const auto pt = [&](float angle, float r) noexcept
        { return juce::Point<float>(scx + r * std::cos(angle), scy + r * std::sin(angle)); };

        // Beat markers
        for (int b = 0; b < m_barBeats; ++b)
        {
            const float angle = kBeatAngle + static_cast<float>(b) / static_cast<float>(m_barBeats) * k2Pi;
            const bool isDown = (b == 0);
            g.setColour(juce::Colour(isDown ? c.cols[9] : c.cols[6]).withAlpha(0.85f));
            g.drawLine(juce::Line<float>(pt(angle, isDown ? innerR * 0.45f : innerR * 0.9f),
                                         pt(angle, isDown ? outerR * 1.10f : outerR * 1.04f)),
                       isDown ? 2.5f : 1.5f);
        }

        // Subdivision markers (per beat)
        if (!m_subdivPositions.empty() && m_samplesPerBeat > 0)
        {
            const float spbF = static_cast<float>(m_samplesPerBeat);
            const float barF = spbF * static_cast<float>(m_barBeats);
            for (int b = 0; b < m_barBeats; ++b)
            {
                for (const size_t offset : m_subdivPositions)
                {
                    const float norm = (static_cast<float>(b) * spbF + static_cast<float>(offset)) / barF;
                    const float angle = kBeatAngle + norm * k2Pi;
                    g.setColour(juce::Colour(c.cols[4]).withAlpha(0.60f));
                    g.drawLine(juce::Line<float>(pt(angle, innerR * 0.85f), pt(angle, outerR * 1.02f)), 1.f);
                }
            }
        }

        // Current-position sweep line (write head)
        if (m_barPhase >= 0.f)
        {
            const float sweepAngle = kBeatAngle + m_barPhase * k2Pi;
            g.setColour(juce::Colour(c.cols[9]).withAlpha(0.60f));
            g.drawLine(juce::Line<float>(pt(sweepAngle, innerR * 0.45f), pt(sweepAngle, outerR * 1.06f)), 1.5f);
        }

        constexpr float kHubR = 4.f;
        g.setColour(juce::Colour(c.cols[9]).withAlpha(0.85f));
        g.fillEllipse(scx - kHubR, scy - kHubR, 2.f * kHubR, 2.f * kHubR);
    }

    AbacDsp::SpectrumImageSet m_imageSet{};
    juce::PixelARGB m_lut[GuiConstants::kLutSize]{};
    float m_sampleRate{48000.f};
    size_t m_samplesPerBeat{0};
    int m_barBeats{4};
    float m_barPhase{0.f};
    float m_lastBarPhase{-1.f}; // negative = uninitialized
    std::vector<size_t> m_subdivPositions;
    juce::String m_label;
    juce::Image m_irisImage;
};
