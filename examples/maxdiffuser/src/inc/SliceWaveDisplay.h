#pragma once

#include <algorithm>
#include <cmath>
#include <juce_gui_basics/juce_gui_basics.h>
#include <vector>

#include "GuiConstants.h"
#include "Sampler/SliceLibrary.h"

// Sequencer pattern overview: each slice's own time-frequency thumbnail,
// placed at its step position and stretched to its real duration, plus
// boundary markers and a playhead, all synced to the shared beat clock.
// Not the recorded loop audio; see CircularLoopDisplay.
class SliceWaveDisplay : public juce::Component
{
  public:
    SliceWaveDisplay()
    {
        rebuildLut();
    }

    void setSampleRate(float sampleRate) noexcept
    {
        m_sampleRate = sampleRate;
    }

    void setSliceThumbnails(const std::vector<AbacDsp::SequencerSliceThumbnail>& thumbnails)
    {
        m_thumbnails = thumbnails;
    }

    // Satisfies the default "signal" gauge callback; real data arrives via the
    // setters above, so the pushed argument is intentionally ignored.
    void update(const std::vector<float>&)
    {
        repaint();
    }

    void setSliceBoundaries(const std::vector<float>& normalized)
    {
        m_boundaries = normalized;
    }

    void setPlayheadNormalized(float normalized)
    {
        m_playhead = normalized;
        repaint();
    }

    void setStateLabel(const juce::String& label)
    {
        m_stateLabel = label;
    }

    // The gauge's display name from the blueprint, shown right-aligned.
    void setLabelText(const juce::String& label)
    {
        m_title = label;
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

        constexpr float kPad = 6.f;
        constexpr float kLabelH = 16.f;
        auto area = getLocalBounds().toFloat().reduced(kPad);
        const auto labelStrip = area.removeFromTop(kLabelH);
        const auto waveArea = area;

        drawSliceThumbnails(g, waveArea);
        drawSliceBoundaries(g, waveArea, c);
        drawPlayhead(g, waveArea, c);

        g.setFont(juce::Font(juce::FontOptions(12.f)));
        if (m_stateLabel.isNotEmpty())
        {
            g.setColour(juce::Colour(c.labelColour).withAlpha(0.85f));
            g.drawText(m_stateLabel, labelStrip, juce::Justification::centredLeft);
        }
        if (m_title.isNotEmpty())
        {
            g.setColour(juce::Colour(c.labelColour).withAlpha(0.55f));
            g.drawText(m_title, labelStrip, juce::Justification::centredRight);
        }
    }

  private:
    // Reuses the already-live gradient from GuiConstants (rebuilt whenever the theme
    // changes) instead of independently re-deriving it, so this never drifts out of sync
    // with what SpectrogramDisplay itself is showing for the same theme.
    void rebuildLut()
    {
        GuiConstants::instance().getSpectrogramGradient().createLookupTable(m_lut, GuiConstants::kLutSize);
    }

    void drawSliceThumbnails(juce::Graphics& g, juce::Rectangle<float> waveArea)
    {
        const int w = std::max(1, static_cast<int>(waveArea.getWidth()));
        const int h = std::max(1, static_cast<int>(waveArea.getHeight()));
        if (m_thumbImage.getWidth() != w || m_thumbImage.getHeight() != h)
        {
            m_thumbImage = juce::Image(juce::Image::ARGB, w, h, true);
        }
        m_thumbImage.clear(m_thumbImage.getBounds(), juce::Colour(0u));
        {
            juce::Image::BitmapData bd(m_thumbImage, juce::Image::BitmapData::writeOnly);
            for (const auto& thumb : m_thumbnails)
            {
                paintThumbnail(bd, w, h, thumb);
            }
        }
        g.drawImage(m_thumbImage, waveArea, juce::RectanglePlacement::stretchToFit);
    }

    // Log-frequency row mapping matches CircularLoopDisplay's own spectrogram;
    // each pixel is a bilinear lookup across the thumbnail's time/frequency grid.
    void paintThumbnail(juce::Image::BitmapData& bd, const int w, const int h,
                        const AbacDsp::SequencerSliceThumbnail& thumb) const
    {
        if (thumb.data == nullptr || thumb.width == 0 || thumb.height == 0 || thumb.sampleRate <= 0.f)
        {
            return;
        }
        const int x0 = juce::jlimit(0, w, static_cast<int>(thumb.normalizedStart * static_cast<float>(w)));
        const int x1 = juce::jlimit(
            0, w, static_cast<int>((thumb.normalizedStart + thumb.normalizedWidth) * static_cast<float>(w)));
        if (x1 <= x0)
        {
            return;
        }
        const float logMin = std::log2(20.f);
        const float logMax = std::log2(thumb.sampleRate / 2.f);
        const float binHz = (thumb.sampleRate / 2.f) / static_cast<float>(thumb.height);
        const int maxBin = static_cast<int>(thumb.height) - 1;
        const int maxFrame = static_cast<int>(thumb.width) - 1;

        for (int y = 0; y < h; ++y)
        {
            const float normR = 1.f - static_cast<float>(y) / static_cast<float>(std::max(1, h - 1));
            const float fbin = std::exp2(logMin + normR * (logMax - logMin)) / binHz;
            const int bin0 = juce::jlimit(0, maxBin, static_cast<int>(fbin));
            const int bin1 = std::min(bin0 + 1, maxBin);
            const float bt = fbin - static_cast<float>(bin0);

            for (int x = x0; x < x1; ++x)
            {
                const float tf =
                    static_cast<float>(x - x0) / static_cast<float>(x1 - x0) * static_cast<float>(maxFrame);
                const int t0 = juce::jlimit(0, maxFrame, static_cast<int>(tf));
                const int t1 = std::min(t0 + 1, maxFrame);
                const float tt = tf - static_cast<float>(t0);

                const auto sample = [&](const int t, const int bin)
                { return thumb.data[static_cast<size_t>(t) * thumb.height + static_cast<size_t>(bin)]; };
                const float v0 = sample(t0, bin0) + bt * (sample(t0, bin1) - sample(t0, bin0));
                const float v1 = sample(t1, bin0) + bt * (sample(t1, bin1) - sample(t1, bin0));
                const float value = std::pow(std::max(v0 + tt * (v1 - v0), 0.f), 0.15f);

                const int lutIdx =
                    juce::jlimit(0, GuiConstants::kLutSize - 1,
                                 static_cast<int>(value * static_cast<float>(GuiConstants::kLutSize - 1)));
                bd.setPixelColour(x, y, juce::Colour(m_lut[static_cast<size_t>(lutIdx)]));
            }
        }
    }

    void drawSliceBoundaries(juce::Graphics& g, juce::Rectangle<float> waveArea, const GuiConstants::Colors& c) const
    {
        g.setColour(juce::Colour(c.labelColour).withAlpha(0.4f));
        for (const float b : m_boundaries)
        {
            const float x = waveArea.getX() + waveArea.getWidth() * juce::jlimit(0.f, 1.f, b);
            g.drawVerticalLine(static_cast<int>(x), waveArea.getY(), waveArea.getBottom());
        }
    }

    void drawPlayhead(juce::Graphics& g, juce::Rectangle<float> waveArea, const GuiConstants::Colors& c) const
    {
        const float x = waveArea.getX() + waveArea.getWidth() * juce::jlimit(0.f, 1.f, m_playhead);
        g.setColour(juce::Colour(c.statusOutline).withAlpha(0.9f));
        g.drawVerticalLine(static_cast<int>(x), waveArea.getY(), waveArea.getBottom());
    }

    std::vector<AbacDsp::SequencerSliceThumbnail> m_thumbnails;
    std::vector<float> m_boundaries;
    float m_playhead{0.f};
    [[maybe_unused]] float m_sampleRate{48000.f};
    juce::String m_stateLabel;
    juce::String m_title;
    juce::Image m_thumbImage;
    juce::PixelARGB m_lut[GuiConstants::kLutSize]{};
};
