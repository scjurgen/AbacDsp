#pragma once

#include <juce_graphics/juce_graphics.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "Analysis/Spectrogram.h"
#include "GuiConstants.h"

/*
 * TODO:
 * - Mono/Stereo
 * - In/Out
 * - Noisefloor/Balanced/Peaks
 */

class SpectrogramBackground : public juce::Component
{
  public:
    SpectrogramBackground()
    {
        backgroundApp = juce::Colour(GuiConstants::instance().colors.background);
        setBufferedToImage(true);
        setInterceptsMouseClicks(false, false);
    }

    void paint(juce::Graphics& g) override
    {
        g.setColour(backgroundApp);
        g.fillRoundedRectangle(getLocalBounds().toFloat(), 1);
    }

    void resized() override
    {
        repaint();
    }

    void updateColors()
    {
        backgroundApp = juce::Colour(GuiConstants::instance().colors.background);
        repaint();
    }

  private:
    juce::Colour backgroundApp;
};

class SpectrogramValue : public juce::Component
{
  public:
    SpectrogramValue(const GuiConstants::GradientPreset lutPreset)
    {
        GuiConstants::buildLut(lutPreset, m_lut);
        setInterceptsMouseClicks(false, false);
    }

    void paint(juce::Graphics& g) override
    {
        if (m_imageSet.data == nullptr)
        {
            return;
        }

        const auto bounds = getLocalBounds().toFloat();
        const float nyquist = m_imageSet.sampleRate / 2.f;
        const float logMin = std::log2(20.f);
        const float logMax = std::log2(nyquist);
        const float binHz = nyquist / static_cast<float>(m_imageSet.height);

        juce::Image spectrogramImage(juce::Image::PixelFormat::ARGB, static_cast<int>(m_imageSet.width),
                                     static_cast<int>(m_imageSet.height), true);

        juce::Image::BitmapData bitmapData(spectrogramImage, juce::Image::BitmapData::writeOnly);

        for (int x = 0; x < bitmapData.width; ++x)
        {
            const size_t base = static_cast<size_t>(x) * m_imageSet.height;

            for (int y = 0; y < bitmapData.height; ++y)
            {
                const float norm = 1.f - static_cast<float>(y) / static_cast<float>(bitmapData.height - 1);
                const float hz = std::pow(2.f, logMin + norm * (logMax - logMin));
                const float fBin = hz / binHz;
                const int bin0 = juce::jlimit(0, static_cast<int>(m_imageSet.height) - 1, static_cast<int>(fBin));
                const int bin1 = std::min(bin0 + 1, static_cast<int>(m_imageSet.height) - 1);
                const float t = fBin - static_cast<float>(bin0);

                const float v0 = m_imageSet.data[base + static_cast<size_t>(bin0)];
                const float v1 = m_imageSet.data[base + static_cast<size_t>(bin1)];
                float value = v0 + t * (v1 - v0);
                value = std::pow(value, 0.15f);
                const int idx =
                    juce::jlimit(0, GuiConstants::kLutSize - 1, static_cast<int>(value * (GuiConstants::kLutSize - 1)));
                bitmapData.setPixelColour(x, y, juce::Colour(m_lut[idx]));
            }
        }

        g.drawImage(spectrogramImage, bounds);
    }

    void resized() override
    {
        repaint();
    }

    void update(AbacDsp::SpectrumImageSet imageSet)
    {
        if (m_imageSet.activeSlice != imageSet.activeSlice)
        {
            m_imageSet = imageSet;
            repaint();
        }
    }

    void setGradientPreset(GuiConstants::GradientPreset preset)
    {
        GuiConstants::buildLut(preset, m_lut);
        repaint();
    }

  private:
    AbacDsp::SpectrumImageSet m_imageSet{};
    juce::PixelARGB m_lut[GuiConstants::kLutSize]{};
};

class SpectrogramOverlay : public juce::Component
{
  public:
    SpectrogramOverlay()
    {
        setInterceptsMouseClicks(false, false);
        labelColour = juce::Colour(GuiConstants::instance().colors.labelColour);
        labelBgColour = juce::Colour(GuiConstants::instance().colors.background).withAlpha(0.55f);
    }

    void update(const AbacDsp::SpectrumImageSet& imageSet)
    {
        m_sampleRate = imageSet.sampleRate;
        m_fftLength = imageSet.fftLength;
        m_slices = imageSet.width;
        m_windowForwardRatio = imageSet.windowForwardRatio;
        repaint();
    }

    void paint(juce::Graphics& g) override
    {
        if (m_sampleRate <= 0.f || m_fftLength == 0)
        {
            return;
        }

        const float w = static_cast<float>(getWidth());
        const float h = static_cast<float>(getHeight());
        const float nyquist = m_sampleRate / 2.f;
        const float logMin = std::log2(20.f);
        const float logMax = std::log2(nyquist);

        static constexpr float kGridHz[] = {20.f, 50.f, 100.f, 200.f, 500.f, 1000.f, 2000.f, 5000.f, 10000.f, 20000.f};
        g.setFont(juce::FontOptions(12.f));

        for (const float hz : kGridHz)
        {
            if (hz < 20.f || hz > nyquist)
            {
                continue;
            }

            const float norm = (std::log2(hz) - logMin) / (logMax - logMin);
            const float y = h * (1.f - norm);
            const bool is1k = (static_cast<int>(hz) == 1000 || static_cast<int>(hz) == 10000);

            g.setColour(labelColour.withAlpha(is1k ? 0.55f : 0.25f));
            g.drawLine(0.f, y, w, y, is1k ? 1.f : 0.7f);

            const juce::String label =
                hz >= 1000.f ? juce::String(static_cast<int>(hz / 1000)) + "k" : juce::String(static_cast<int>(hz));

            constexpr int lx = 2;
            const int ly = static_cast<int>(y) - 11;

            g.setColour(labelBgColour);
            g.fillRect(lx, ly, 26, 11);

            g.setColour(labelColour);
            g.drawText(label, lx, ly, 26, 11, juce::Justification::left, false);
        }

        const float hopSamples = static_cast<float>(m_fftLength) * m_windowForwardRatio;
        const float totalDuration = static_cast<float>(m_slices) * hopSamples / m_sampleRate;
        // const float xPerSecond = w / totalDuration;

        g.setColour(labelColour.withAlpha(0.30f));
        for (float t = 1.f; t < totalDuration; t += 1.f)
        {
            const float x = w - (t / totalDuration) * w;
            g.drawLine(x, 0.f, x, h, 0.7f);
        }
    }

    void updateColors()
    {
        labelColour = juce::Colour(GuiConstants::instance().colors.labelColour);
        labelBgColour = juce::Colour(GuiConstants::instance().colors.background).withAlpha(0.55f);
        repaint();
    }

  private:
    float m_sampleRate{0.f};
    unsigned m_fftLength{0};
    size_t m_slices{0};
    float m_windowForwardRatio{1.f / 3.f};
    juce::Colour labelColour;
    juce::Colour labelBgColour;
};

class SpectrogramDisplay : public juce::Component, public juce::SettableTooltipClient
{
  public:
    SpectrogramDisplay(GuiConstants::GradientPreset lutPreset)
        : spectrogramImage(lutPreset)
    {
        addAndMakeVisible(spectrogramBg);
        addAndMakeVisible(spectrogramImage);
        addAndMakeVisible(spectrogramOverlay);
        backgroundDarkGrey = juce::Colour(GuiConstants::instance().colors.backgroundDark);
    }

    void paint(juce::Graphics& g) override
    {
        g.setColour(backgroundDarkGrey);
        g.fillRoundedRectangle(getLocalBounds().toFloat(), 3);
        g.setColour(juce::Colour(GuiConstants::instance().colors.labelColour));
        g.drawText(m_label, getLocalBounds().removeFromTop(20), juce::Justification::centred);
    }

    void resized() override
    {
        auto bounds = getLocalBounds();
        bounds.removeFromTop(20);
        spectrogramBg.setBounds(bounds);
        spectrogramImage.setBounds(bounds);
        spectrogramOverlay.setBounds(bounds);
    }

    void update(AbacDsp::SpectrumImageSet imageSet)
    {
        spectrogramImage.update(imageSet);
        spectrogramOverlay.update(imageSet);
    }

    void setLabelText(const juce::String& label)
    {
        m_label = label;
        repaint();
    }

    void setGradientPreset(GuiConstants::GradientPreset preset)
    {
        spectrogramImage.setGradientPreset(preset);
        spectrogramBg.updateColors();
        spectrogramOverlay.updateColors();
        backgroundDarkGrey = juce::Colour(GuiConstants::instance().colors.backgroundDark);
        repaint();
    }

  private:
    SpectrogramBackground spectrogramBg;
    SpectrogramValue spectrogramImage;
    SpectrogramOverlay spectrogramOverlay;
    juce::Colour backgroundDarkGrey;
    juce::String m_label;
};