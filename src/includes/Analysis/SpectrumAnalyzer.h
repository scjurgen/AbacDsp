#pragma once

#include <algorithm>
#include <bit>
#include <cmath>
#include <complex>
#include <cstddef>
#include <numbers>
#include <numeric>
#include <optional>
#include <span>
#include <vector>

#include "Analysis/FftMisc.h"

namespace AbacDsp
{

/**
 * @ingroup analysis
 * @brief Hann-windowed FFT magnitudes mapped onto log-spaced display bands, in dBFS.
 *
 * A new frame is computed every hop (sampleRate / frameRate samples), independent of the
 * FFT size, so the display rate stays constant while the size trades time for frequency
 * resolution. A band shows the highest of: its own bins, the dB-parabola vertex of any spectral
 * peak lying inside it (correcting the Hann scalloping loss of a sine between bins), and the
 * dB-linear interpolation between the bins around its centre. Levels fall at a fixed dB per second
 * (infinity = no smoothing) and a peak-hold curve keeps the maximum until resetPeakHold().
 * Allocates only in the constructor; meant for an analysis thread, not the audio callback.
 */
class SpectrumAnalyzer
{
  public:
    static constexpr float kFloorDb{-120.f};
    static constexpr size_t kMinFftSize{256};

    SpectrumAnalyzer(const float sampleRate, const size_t numBands, const size_t maxFftSize = 16384,
                     const float frameRateHz = 60.f, const float minFrequency = 20.f,
                     const float maxFrequency = 20000.f)
        : m_sampleRate(sampleRate)
        , m_maxFftSize(std::bit_ceil(std::max(maxFftSize, kMinFftSize)))
        , m_hopSize(std::max<size_t>(1, static_cast<size_t>(std::lround(sampleRate / frameRateHz))))
        , m_history(m_maxFftSize, 0.f)
        , m_window(m_maxFftSize, 0.f)
        , m_fftIn(m_maxFftSize)
        , m_fftOut(m_maxFftSize)
        , m_binDb(m_maxFftSize / 2 + 1, kFloorDb)
        , m_fft(m_maxFftSize, false)
        , m_bandEdges(numBands + 1)
        , m_bandCenters(numBands)
        , m_bandsDb(numBands, kFloorDb)
        , m_peakHoldDb(numBands, kFloorDb)
    {
        computeBandEdges(minFrequency, std::min(maxFrequency, 0.49f * sampleRate));
        setFftSize(m_maxFftSize);
    }

    // Rounded up to a power of two and clamped to [kMinFftSize, maxFftSize].
    void setFftSize(const size_t fftSize)
    {
        m_fftSize = std::clamp(std::bit_ceil(fftSize), kMinFftSize, m_maxFftSize);
        m_fft.resize(m_fftSize);
        for (size_t n = 0; n < m_fftSize; ++n)
        {
            const float phase = 2.f * std::numbers::pi_v<float> * static_cast<float>(n) / static_cast<float>(m_fftSize);
            m_window[n] = 0.5f * (1.f - std::cos(phase));
        }
        const float windowSum = std::accumulate(m_window.begin(), m_window.begin() + toDiff(m_fftSize), 0.f);
        m_magnitudeScale = 2.f / windowSum;
    }

    void setFallDbPerSecond(const float dbPerSecond) noexcept
    {
        m_fallDbPerSecond = std::max(0.f, dbPerSecond);
    }

    void resetPeakHold() noexcept
    {
        std::ranges::copy(m_bandsDb, m_peakHoldDb.begin());
    }

    // Returns the number of frames computed while consuming the samples.
    [[nodiscard]] size_t process(const std::span<const float> samples)
    {
        size_t frames{0};
        for (const auto sample : samples)
        {
            m_history[m_writePos] = sample;
            m_writePos = (m_writePos + 1) & (m_maxFftSize - 1);
            if (++m_hopCounter >= m_hopSize)
            {
                m_hopCounter = 0;
                computeFrame();
                ++frames;
            }
        }
        return frames;
    }

    [[nodiscard]] size_t fftSize() const noexcept
    {
        return m_fftSize;
    }

    [[nodiscard]] std::span<const float> bandsDb() const noexcept
    {
        return m_bandsDb;
    }

    [[nodiscard]] std::span<const float> peakHoldDb() const noexcept
    {
        return m_peakHoldDb;
    }

    [[nodiscard]] std::span<const float> bandCenters() const noexcept
    {
        return m_bandCenters;
    }

  private:
    [[nodiscard]] static std::ptrdiff_t toDiff(const size_t value) noexcept
    {
        return static_cast<std::ptrdiff_t>(value);
    }

    [[nodiscard]] static float toDb(const float gain) noexcept
    {
        return gain > 0.f ? std::max(kFloorDb, 20.f * std::log10(gain)) : kFloorDb;
    }

    void computeBandEdges(const float minFrequency, const float maxFrequency)
    {
        const float ratio = maxFrequency / minFrequency;
        const auto numBands = static_cast<float>(m_bandCenters.size());
        for (size_t i = 0; i < m_bandEdges.size(); ++i)
        {
            m_bandEdges[i] = minFrequency * std::pow(ratio, static_cast<float>(i) / numBands);
        }
        for (size_t i = 0; i < m_bandCenters.size(); ++i)
        {
            m_bandCenters[i] = std::sqrt(m_bandEdges[i] * m_bandEdges[i + 1]);
        }
    }

    void computeFrame()
    {
        const size_t start = (m_writePos + m_maxFftSize - m_fftSize) & (m_maxFftSize - 1);
        for (size_t n = 0; n < m_fftSize; ++n)
        {
            m_fftIn[n] = {m_history[(start + n) & (m_maxFftSize - 1)] * m_window[n], 0.f};
        }
        m_fft.compute(m_fftIn.data(), m_fftOut.data());
        for (size_t k = 0; k <= m_fftSize / 2; ++k)
        {
            m_binDb[k] = toDb(std::abs(m_fftOut[k]) * m_magnitudeScale);
        }

        const float fall = m_fallDbPerSecond * static_cast<float>(m_hopSize) / m_sampleRate;
        for (size_t i = 0; i < m_bandsDb.size(); ++i)
        {
            const float level = bandLevel(i);
            m_bandsDb[i] = std::max({level, m_bandsDb[i] - fall, kFloorDb});
            m_peakHoldDb[i] = std::max(m_peakHoldDb[i], m_bandsDb[i]);
        }
    }

    struct SpectralPeak
    {
        float position{0.f};
        float level{0.f};
    };

    [[nodiscard]] float bandLevel(const size_t band) const noexcept
    {
        const float binsPerHz = static_cast<float>(m_fftSize) / m_sampleRate;
        const float lowBin = m_bandEdges[band] * binsPerHz;
        const float highBin = m_bandEdges[band + 1] * binsPerHz;
        const auto inBand = [lowBin, highBin](const float position)
        { return position >= lowBin && position < highBin; };

        float level = interpolatedAt(m_bandCenters[band] * binsPerHz);
        const auto first = std::max<size_t>(1, static_cast<size_t>(std::floor(lowBin)));
        const auto last = std::min(static_cast<size_t>(std::ceil(highBin)), m_fftSize / 2 - 1);
        for (size_t k = first; k <= last; ++k)
        {
            if (inBand(static_cast<float>(k)))
            {
                level = std::max(level, m_binDb[k]);
            }
            if (const auto peak = spectralPeakAt(k); peak && inBand(peak->position))
            {
                level = std::max(level, peak->level);
            }
        }
        return level;
    }

    [[nodiscard]] float interpolatedAt(const float bin) const noexcept
    {
        const auto lower = std::min(static_cast<size_t>(bin), m_fftSize / 2 - 1);
        const float fraction = std::clamp(bin - static_cast<float>(lower), 0.f, 1.f);
        return m_binDb[lower] + fraction * (m_binDb[lower + 1] - m_binDb[lower]);
    }

    // Vertex of the parabola through a local maximum and its neighbours, in bins and dB.
    [[nodiscard]] std::optional<SpectralPeak> spectralPeakAt(const size_t bin) const noexcept
    {
        const float a = m_binDb[bin - 1];
        const float b = m_binDb[bin];
        const float c = m_binDb[bin + 1];
        if (!(a < b && c < b))
        {
            return std::nullopt;
        }
        const float offset = 0.5f * (a - c) / (a - 2.f * b + c);
        return SpectralPeak{static_cast<float>(bin) + offset, b - 0.25f * (a - c) * offset};
    }

    float m_sampleRate;
    size_t m_maxFftSize;
    size_t m_hopSize;
    size_t m_fftSize{0};
    size_t m_writePos{0};
    size_t m_hopCounter{0};
    float m_fallDbPerSecond{24.f};
    float m_magnitudeScale{1.f};
    std::vector<float> m_history;
    std::vector<float> m_window;
    std::vector<std::complex<float>> m_fftIn;
    std::vector<std::complex<float>> m_fftOut;
    std::vector<float> m_binDb;
    KissFft<float> m_fft;
    std::vector<float> m_bandEdges;
    std::vector<float> m_bandCenters;
    std::vector<float> m_bandsDb;
    std::vector<float> m_peakHoldDb;
};

}
