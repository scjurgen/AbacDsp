#pragma once

#include <algorithm>
#include <atomic>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include "Analysis/Spectrogram.h"

namespace AbacDsp
{

/**
 * @ingroup analysis
 * @brief Running mel spectrogram as columns of 0..1 values in dB, produced on the worker thread of SpectrogramBase.
 *
 * Each STFT frame (Hann window) becomes one column of `bands` values, lowest band first. A band is a triangle on the
 * mel scale (peak 1, centres equally spaced in mel between minHz and maxHz) and takes the power sum of the bins under
 * it, so a tone has the same level wherever it lies, and a band narrower than a bin still takes the nearest one. The
 * level is dB re a full-scale sine centred on a bin (0 dB; the Hann window's noise bandwidth of 1.5 bins is divided
 * out) and maps floorDb..ceilDb linearly onto 0..1, clamped.
 *
 * The columns wait in a ring of `columns` slots. pull() hands out the ones that are new since the last pull, oldest
 * first, and is meant for one reader at a time: the producer is the worker, the reader is any other thread. A reader
 * that falls behind loses the oldest columns; pull() never returns a column that the worker may be overwriting.
 * The audio side is processBlock(), which does not allocate or wait. The sample rate is fixed at construction, do not
 * call setSampleRate() or setFftLength() afterwards. Construction allocates.
 * @see https://en.wikipedia.org/wiki/Mel_scale
 * @see https://en.wikipedia.org/wiki/Window_function
 */
class MelColumns final : public SpectrogramBase
{
  public:
    struct Settings
    {
        unsigned bands{64};
        unsigned columns{512};
        unsigned fftLength{2048};
        float hopRatio{0.25f}; ///< fraction of fftLength between two frames
        float minHz{30.f};
        float maxHz{20000.f};
        float floorDb{-90.f};
        float ceilDb{0.f};
    };

    explicit MelColumns(const float sampleRate)
        : MelColumns{sampleRate, Settings{}}
    {
    }

    MelColumns(const float sampleRate, const Settings& settings)
        : m_settings{settings}
        , m_ring(static_cast<size_t>(settings.bands) * settings.columns, 0.f)
    {
        assert(settings.bands >= 2 && settings.columns >= 4 && settings.maxHz > settings.minHz);
        setSampleRate(sampleRate);
        setFftLength(settings.fftLength);
        setWindowForward(settings.hopRatio);
        buildFilterbank();
    }

    ~MelColumns() override
    {
        stopWorker();
    }

    [[nodiscard]] unsigned bands() const noexcept
    {
        return m_settings.bands;
    }

    [[nodiscard]] unsigned columns() const noexcept
    {
        return m_settings.columns;
    }

    /// Time between two columns.
    [[nodiscard]] float columnSeconds() const noexcept
    {
        return static_cast<float>(m_forwardLength) / m_sampleRate;
    }

    /// The centre of a band in Hz.
    [[nodiscard]] float bandCentreHz(const unsigned band) const noexcept
    {
        return melToHz(m_melPoints[band + 1]);
    }

    /// Writes the columns that are new since the last call into `out`, oldest first, `bands()` values each; returns how
    /// many. When `out` is too small or the reader fell behind by more than the ring holds, the newest ones are kept.
    [[nodiscard]] size_t pull(const std::span<float> out) noexcept
    {
        const size_t bandCount = m_settings.bands;
        const size_t capacity = m_settings.columns;
        const uint64_t written = m_written.load(std::memory_order_acquire);
        const size_t safe = capacity - guardColumns;
        const size_t wanted = std::min<uint64_t>(written - m_read, std::min(safe, out.size() / bandCount));
        const uint64_t first = written - wanted;
        for (size_t i = 0; i < wanted; ++i)
        {
            const auto slot = static_cast<size_t>((first + i) % capacity);
            std::copy_n(&m_ring[slot * bandCount], bandCount, &out[i * bandCount]);
        }
        const uint64_t after = m_written.load(std::memory_order_acquire);
        const size_t torn = std::min<uint64_t>(wanted, after >= first + capacity ? after - capacity + 1 - first : 0);
        if (torn > 0)
        {
            std::copy(out.begin() + static_cast<std::ptrdiff_t>(torn * bandCount),
                      out.begin() + static_cast<std::ptrdiff_t>(wanted * bandCount), out.begin());
        }
        m_read = written;
        return wanted - torn;
    }

  protected:
    void onNewFFTData(const std::vector<float>& magnitudes) override
    {
        const size_t bandCount = m_settings.bands;
        const uint64_t index = m_written.load(std::memory_order_relaxed);
        float* const column = &m_ring[static_cast<size_t>(index % m_settings.columns) * bandCount];
        for (size_t band = 0; band < bandCount; ++band)
        {
            column[band] = levelOf(band, magnitudes);
        }
        m_written.store(index + 1, std::memory_order_release);
    }

  private:
    /// Columns kept clear of the worker's write position when reading.
    static constexpr size_t guardColumns{2};
    /// The power of a bin-centred sine's three Hann bins relative to its peak bin (equivalent noise bandwidth).
    static constexpr float hannNoiseBandwidth{1.5f};
    /// The magnitude of the Hann-windowed FFT of a full-scale sine is a quarter, the transform divides by N.
    static constexpr float fullScaleSineMagnitude{0.25f};

    struct BandBins
    {
        size_t first{0};
        size_t count{0};
        size_t weightOffset{0};
    };

    [[nodiscard]] static float hzToMel(const float hz) noexcept
    {
        return 2595.f * std::log10(1.f + hz / 700.f);
    }

    [[nodiscard]] static float melToHz(const float mel) noexcept
    {
        return 700.f * (std::pow(10.f, mel / 2595.f) - 1.f);
    }

    void buildFilterbank()
    {
        const size_t bandCount = m_settings.bands;
        const float melMin = hzToMel(m_settings.minHz);
        const float melMax = hzToMel(std::min(m_settings.maxHz, m_sampleRate * 0.5f));
        m_melPoints.resize(bandCount + 2);
        for (size_t i = 0; i < m_melPoints.size(); ++i)
        {
            m_melPoints[i] = melMin + (melMax - melMin) * static_cast<float>(i) / static_cast<float>(bandCount + 1);
        }
        const size_t bins = m_fftLength / 2;
        const float binHz = m_sampleRate / static_cast<float>(m_fftLength);
        m_bands.resize(bandCount);
        for (size_t band = 0; band < bandCount; ++band)
        {
            m_bands[band] = bandBins(melToHz(m_melPoints[band]), melToHz(m_melPoints[band + 1]),
                                     melToHz(m_melPoints[band + 2]), binHz, bins);
        }
    }

    /// The bins under one triangle and their weights; a triangle that no bin sits near gets the nearest bin.
    [[nodiscard]] BandBins bandBins(const float lowHz, const float centreHz, const float highHz, const float binHz,
                                    const size_t bins)
    {
        const auto firstBin = static_cast<size_t>(std::max(0.f, std::floor(lowHz / binHz)));
        const auto lastBin = std::min(bins - 1, static_cast<size_t>(std::ceil(highHz / binHz)));
        const auto nearest = std::min(bins - 1, static_cast<size_t>(std::lround(centreHz / binHz)));
        const size_t start = std::min(firstBin, nearest);
        const size_t end = std::max(lastBin, nearest);
        BandBins result{start, end - start + 1, m_weights.size()};
        float peak{0.f};
        for (size_t bin = start; bin <= end; ++bin)
        {
            const float hz = static_cast<float>(bin) * binHz;
            const float weight =
                hz < centreHz ? (hz - lowHz) / (centreHz - lowHz) : (highHz - hz) / (highHz - centreHz);
            m_weights.push_back(std::max(0.f, weight));
            peak = std::max(peak, m_weights.back());
        }
        if (peak < 0.5f)
        {
            m_weights[result.weightOffset + (nearest - start)] = 1.f;
        }
        return result;
    }

    [[nodiscard]] float levelOf(const size_t band, const std::vector<float>& magnitudes) const noexcept
    {
        const BandBins& bins = m_bands[band];
        float power{0.f};
        for (size_t i = 0; i < bins.count; ++i)
        {
            const float magnitude = magnitudes[bins.first + i];
            power += m_weights[bins.weightOffset + i] * magnitude * magnitude;
        }
        const float amplitude = std::sqrt(power / hannNoiseBandwidth) / fullScaleSineMagnitude;
        const float db = 20.f * std::log10(std::max(amplitude, 1e-9f));
        return std::clamp((db - m_settings.floorDb) / (m_settings.ceilDb - m_settings.floorDb), 0.f, 1.f);
    }

    Settings m_settings;
    std::vector<float> m_melPoints;
    std::vector<BandBins> m_bands;
    std::vector<float> m_weights;
    std::vector<float> m_ring;
    std::atomic<uint64_t> m_written{0};
    uint64_t m_read{0};
};

} // namespace AbacDsp
