#pragma once

#include <atomic>
#include <cmath>
#include <span>
#include <thread>
#include <vector>

#include "Analysis/FftMisc.h"

namespace AbacDsp
{
class MelSpectroGram
{
  public:
    struct ImageSet
    {
        size_t currentSlice;
        size_t width;
        size_t fftHalfLength;
        const float* data;

        [[nodiscard]] size_t size() const noexcept
        {
            return width * fftHalfLength;
        }
    };

    MelSpectroGram()
        : m_magnitudes(512, 0)
        , m_fft{1024}
        , m_workerThread(&MelSpectroGram::workerFunction, this)
    {
        setMelBands(40);
        setFftLength(1024);
        setSlices(1920);
    }

    ~MelSpectroGram()
    {
        m_shouldExit.store(true, std::memory_order_release);
        m_workerThread.join();
    }

    void setMelBands(const size_t melBands)
    {
        m_melHeight = melBands;
        m_melSpectrogram.resize(m_melHeight * m_slices);
        createMelFilterbank();
    }

    void setSlices(const size_t cnt)
    {
        m_slices = cnt;
        m_spectrogram.resize(m_fftLength / 2 * m_slices);
        m_currentSlice = std::clamp(m_currentSlice, size_t{0}, cnt);
    }

    void setFftLength(const size_t N)
    {
        m_fftLength = N;
        m_fft.resize(m_fftLength);
        m_spectrogram.resize(m_fftLength / 2 * m_slices);
        m_buffer.resize(m_fftLength);
        m_fftBuffer.resize(m_fftLength);
        m_magnitudes.resize(m_fftLength / 2);
    }

    void processBlock(std::span<const float> input)
    {
        for (const float sample : input)
        {
            m_buffer[m_bufferIndex++] = sample;
            if (m_bufferIndex >= m_fftLength)
            {
                enqueueFFT();
                advanceWindow();
            }
        }
    }

    [[nodiscard]] ImageSet getImageSet() const
    {
        return {m_currentSlice, m_slices, m_fftLength / 2, m_spectrogram.data()};
    }

  private:
    void enqueueFFT()
    {
        const size_t head = m_queueHead.load(std::memory_order_relaxed);
        const size_t nextHead = (head + 1) % QUEUE_SIZE;
        if (nextHead != m_queueTail.load(std::memory_order_acquire))
        {
            m_fftQueue[head % QUEUE_SIZE] = m_buffer;
            m_queueHead.store(nextHead, std::memory_order_release);
        }
    }

    void advanceWindow()
    {
        const auto samplesToKeep = static_cast<size_t>(m_fftLength * (1.0f - m_windowForward));
        const auto index = m_fftLength - samplesToKeep;
        std::copy_n(&m_buffer[index], m_buffer.size() - index, m_buffer.data());
        m_bufferIndex = samplesToKeep;
    }

    void workerFunction()
    {
        while (!m_shouldExit.load(std::memory_order_acquire))
        {
            const size_t tail = m_queueTail.load(std::memory_order_relaxed);
            const size_t head = m_queueHead.load(std::memory_order_acquire);
            if (tail != head)
            {
                processFFT(m_fftQueue[tail % QUEUE_SIZE]);
                m_queueTail.store((tail + 1) % QUEUE_SIZE, std::memory_order_release);
            }
            else
            {
                std::this_thread::yield();
            }
        }
    }

    void processFFT(const std::vector<float>& buffer)
    {
        m_fft.compute(buffer, m_magnitudes);
        std::copy_n(m_magnitudes.data(), m_magnitudes.size(), &m_spectrogram[m_currentSlice * (m_fftLength / 2)]);
        m_currentSlice++;
        if (m_currentSlice >= m_slices)
        {
            m_currentSlice = 0;
        }
        std::fill_n(&m_spectrogram[m_currentSlice * (m_fftLength / 2)], m_magnitudes.size(), 1.f);
    }

    std::vector<float> m_melSpectrogram;
    size_t m_melHeight{40};
    std::vector<std::vector<float>> m_melFilterbank;
    float m_sampleRate{48000.f};

    void createMelFilterbank()
    {
        const float maxFreq = m_sampleRate / 2.0f;
        constexpr float minMel = 0.f;
        const float maxMel = 2595.0f * std::log10(1.0f + maxFreq / 700.0f);

        m_melFilterbank.resize(m_melHeight);

        for (size_t i = 0; i < m_melHeight; ++i)
        {
            const float mel = minMel + (maxMel - minMel) * static_cast<float>(i) / static_cast<float>(m_melHeight - 1);
            const float hz = 700.0f * (std::pow(10.0f, mel / 2595.0f) - 1.0f);
            const size_t fftBin = static_cast<size_t>(std::round(hz * static_cast<float>(m_fftLength) / m_sampleRate));

            m_melFilterbank[i].assign(m_fftLength / 2, 0.0f);

            if (i > 0 && i < m_melHeight - 1)
            {
                const float prevMel =
                    minMel + (maxMel - minMel) * static_cast<float>(i - 1) / static_cast<float>(m_melHeight - 1);
                const float nextMel =
                    minMel + (maxMel - minMel) * static_cast<float>(i + 1) / static_cast<float>(m_melHeight - 1);
                const float prevHz = 700.0f * (std::pow(10.0f, prevMel / 2595.0f) - 1.0f);
                const float nextHz = 700.0f * (std::pow(10.0f, nextMel / 2595.0f) - 1.0f);
                const size_t prevBin =
                    static_cast<size_t>(std::round(prevHz * static_cast<float>(m_fftLength) / m_sampleRate));
                const size_t nextBin =
                    static_cast<size_t>(std::round(nextHz * static_cast<float>(m_fftLength) / m_sampleRate));

                for (size_t j = prevBin; j < fftBin; ++j)
                {
                    m_melFilterbank[i][j] = static_cast<float>(j - prevBin) / static_cast<float>(fftBin - prevBin);
                }
                for (size_t j = fftBin; j < nextBin; ++j)
                {
                    m_melFilterbank[i][j] =
                        1.0f - static_cast<float>(j - fftBin) / static_cast<float>(nextBin - fftBin);
                }
            }
        }
    }

    void processFFTToMel(const std::vector<float>& buffer)
    {
        m_fft.compute(buffer, m_magnitudes);
        std::copy_n(m_magnitudes.data(), m_magnitudes.size(), &m_spectrogram[m_currentSlice * (m_fftLength / 2)]);

        for (size_t i = 0; i < m_melHeight; ++i)
        {
            float melEnergy = 0.0f;
            for (size_t j = 0; j < m_fftLength / 2; ++j)
            {
                melEnergy += m_magnitudes[j] * m_melFilterbank[i][j];
            }
            m_melSpectrogram[m_currentSlice * m_melHeight + i] = std::log(melEnergy + 1e-6f);
        }

        m_currentSlice++;
        if (m_currentSlice >= m_slices)
        {
            m_currentSlice = 0;
        }
    }

    std::vector<float> m_spectrogram;
    std::vector<float> m_buffer;
    std::vector<float> m_fftBuffer;
    std::vector<float> m_magnitudes;
    HannWindowMagnitudesFft m_fft;
    size_t m_fftLength{2048};
    size_t m_slices{1920};
    float m_windowForward{0.75f};
    size_t m_currentSlice{0};
    size_t m_bufferIndex{0};
    static constexpr size_t QUEUE_SIZE = 4;
    std::array<std::vector<float>, QUEUE_SIZE> m_fftQueue;
    std::atomic<size_t> m_queueHead{0};
    std::atomic<size_t> m_queueTail{0};
    std::atomic<bool> m_shouldExit{false};
    std::thread m_workerThread;
};

struct SpectrumImageSet
{
    size_t activeSlice;
    size_t width;
    size_t height;
    const float* data;
    float sampleRate;
    unsigned fftLength;
    float windowForwardRatio; // hop / fftLength — for time axis labelling

    [[nodiscard]] size_t size() const noexcept
    {
        return width * height;
    }
};

class SpectrogramBase
{
  public:
    SpectrogramBase()
        : m_magnitudes(512, 0)
        , m_fft{1024}
        , m_workerThread(&SpectrogramBase::workerFunction, this)
    {
        setFftLength(1024);
    }

    virtual ~SpectrogramBase()
    {
        // Safety net only: by the time this runs, the derived class's vtable
        // slot is already gone. Every concrete subclass must call stopWorker()
        // as the first line of its own destructor, before that happens, or a
        // still-running worker thread calls the now-pure onNewFFTData().
        stopWorker();
    }

    void setSampleRate(const float sr)
    {
        m_sampleRate = sr;
    }

    void setFftLength(const unsigned N)
    {
        m_fftLength = N;
        m_forwardLength = static_cast<unsigned>(N * m_windowForwardRatio);
        m_fft.resize(m_fftLength);
        m_buffer.resize(m_fftLength, 0.f);
        m_fftBuffer.resize(m_fftLength, 0.f);
        m_magnitudes.resize(m_fftLength / 2, 0.f);
        m_bufferIndex = 0;
        onFftLengthChanged();
    }

    // ratio in (0, 1) — fraction of fftLength advanced per frame
    void setWindowForward(const float ratio)
    {
        m_windowForwardRatio = std::clamp(ratio, 0.01f, 0.99f);
        m_forwardLength = static_cast<unsigned>(m_fftLength * m_windowForwardRatio);
    }

    void processBlock(std::span<const float> input)
    {
        for (const float sample : input)
        {
            m_buffer[m_bufferIndex++] = sample;
            if (m_bufferIndex >= m_fftLength)
            {
                enqueueFFT();
                advanceWindow();
            }
        }
    }

    void processBlock(const float* data, const size_t numSamples)
    {
        processBlock(std::span<const float>{data, numSamples});
    }

    // Number of samples processBlock() must receive to trigger exactly one more
    // FFT enqueue (once the window is already full). Lets a non-realtime producer
    // (e.g. background regeneration) pace itself to one enqueue per call.
    [[nodiscard]] unsigned forwardLength() const noexcept
    {
        return m_forwardLength;
    }

    // Non-blocking peek at whether the next enqueueFFT() would succeed. For
    // producers that must not silently drop frames (unlike the realtime path,
    // which drops under backpressure), poll this before calling processBlock().
    [[nodiscard]] bool queueHasRoom() const noexcept
    {
        const size_t head = m_queueHead.load(std::memory_order_relaxed);
        const size_t tail = m_queueTail.load(std::memory_order_acquire);
        return (head + 1) % QUEUE_SIZE != tail;
    }

  protected:
    // Every concrete subclass must call this as the first line of its own
    // destructor. Idempotent: safe to also let ~SpectrogramBase() call it.
    void stopWorker()
    {
        if (m_workerThread.joinable())
        {
            m_shouldExit.store(true, std::memory_order_release);
            m_workerThread.join();
        }
    }

    // Only safe once the caller guarantees no further processBlock() calls will
    // arrive until this returns (e.g. the sole producer is about to rebuild its
    // image from scratch). Spins until the FFT worker has drained every
    // already-enqueued frame, then clears the sliding window.
    void resetWindow() noexcept
    {
        while (m_queueHead.load(std::memory_order_relaxed) != m_queueTail.load(std::memory_order_acquire))
        {
            std::this_thread::yield();
        }
        m_bufferIndex = 0;
        std::ranges::fill(m_buffer, 0.f);
    }

    virtual void onNewFFTData(const std::vector<float>& magnitudes) = 0;
    virtual void onFftLengthChanged() {}

    float m_sampleRate{48000.f};
    float m_windowForwardRatio{1.f / 3.f};
    std::vector<float> m_buffer;
    std::vector<float> m_fftBuffer;
    std::vector<float> m_magnitudes;
    HannWindowMagnitudesFft m_fft;
    unsigned m_fftLength{1024};
    unsigned m_forwardLength{341};
    unsigned m_bufferIndex{0};

  private:
    void enqueueFFT()
    {
        const size_t head = m_queueHead.load(std::memory_order_relaxed);
        const size_t nextHead = (head + 1) % QUEUE_SIZE;
        if (nextHead != m_queueTail.load(std::memory_order_acquire))
        {
            m_fftQueue[head % QUEUE_SIZE] = m_buffer;
            m_queueHead.store(nextHead, std::memory_order_release);
        }
    }

    void advanceWindow()
    {
        const auto samplesToKeep = m_fftLength - m_forwardLength;
        std::copy(m_buffer.begin() + m_forwardLength, m_buffer.end(), m_buffer.begin());
        m_bufferIndex = samplesToKeep;
    }

    void workerFunction()
    {
        while (!m_shouldExit.load(std::memory_order_acquire))
        {
            const size_t tail = m_queueTail.load(std::memory_order_relaxed);
            const size_t head = m_queueHead.load(std::memory_order_acquire);
            if (tail != head)
            {
                m_fft.compute(m_fftQueue[tail % QUEUE_SIZE], m_magnitudes);
                onNewFFTData(m_magnitudes);
                m_queueTail.store((tail + 1) % QUEUE_SIZE, std::memory_order_release);
            }
            else
            {
                std::this_thread::yield();
            }
        }
    }

    static constexpr size_t QUEUE_SIZE = 4;
    std::array<std::vector<float>, QUEUE_SIZE> m_fftQueue;
    std::atomic<size_t> m_queueHead{0};
    std::atomic<size_t> m_queueTail{0};
    std::atomic<bool> m_shouldExit{false};
    std::thread m_workerThread;
};

class SimpleSpectrogram : public SpectrogramBase
{
  public:
    SimpleSpectrogram()
        : m_slices{1920}
        , m_currentSlice{0}
        , m_spectrogram(m_fftLength / 2 * m_slices, 0.f)
    {
    }

    ~SimpleSpectrogram() override
    {
        stopWorker();
    }

    [[nodiscard]] SpectrumImageSet getImageSet() const
    {
        return {m_currentSlice, m_slices,    m_fftLength / 2,     m_spectrogram.data(),
                m_sampleRate,   m_fftLength, m_windowForwardRatio};
    }

    void setSlices(const size_t cnt)
    {
        m_slices = cnt;
        m_currentSlice = std::clamp(m_currentSlice, size_t{0}, cnt);
        m_spectrogram.resize(m_fftLength / 2 * m_slices, 0.f);
    }

    // Rebuilds the image from scratch: only safe when no other producer is
    // feeding processBlock() concurrently (see resetWindow()'s contract).
    void reset() noexcept
    {
        resetWindow();
        m_currentSlice = 0;
        std::ranges::fill(m_spectrogram, 0.f);
    }

  protected:
    void onFftLengthChanged() override
    {
        m_spectrogram.resize(m_fftLength / 2 * m_slices, 0.f);
    }

    void onNewFFTData(const std::vector<float>& magnitudes) override
    {
        if (m_spectrogram.empty())
        {
            return;
        }
        std::copy_n(magnitudes.data(), magnitudes.size(), &m_spectrogram[m_currentSlice * (m_fftLength / 2)]);
        m_currentSlice++;
        if (m_currentSlice >= m_slices)
        {
            m_currentSlice = 0;
        }
        std::fill_n(&m_spectrogram[m_currentSlice * (m_fftLength / 2)], magnitudes.size(), 1.f);
    }

  private:
    size_t m_slices;
    size_t m_currentSlice;
    std::vector<float> m_spectrogram;
};

class FloatingHorizonFFTImage : public SpectrogramBase
{
  public:
    FloatingHorizonFFTImage()
        : m_magnitudeCollector(m_fftLength / 2)
        , m_horizon(m_width)
        , m_image(m_width * m_height)
    {
    }

    ~FloatingHorizonFFTImage() override
    {
        stopWorker();
    }

    [[nodiscard]] SpectrumImageSet getImageSet() const
    {
        return {m_currentSlice, m_width, m_height, m_image.data(), m_sampleRate, m_fftLength, m_windowForwardRatio};
    }

  protected:
    void onSlicesChanged()
    {
        m_image.resize(m_width * m_height, 0.0f);
        m_horizon.resize(m_width, 0.0f);
    }

    void onFftLengthChanged() override
    {
        m_image.resize(m_fftLength / 2 * m_slices, 0.0f);
    }

    void plotOverHorizon(const unsigned xpos, const float yp, const float color)
    {
        if (xpos >= m_width)
        {
            return;
        }
        if (yp > m_horizon[xpos])
        {
            const size_t imgIdx = static_cast<size_t>(yp + static_cast<float>(xpos) * static_cast<float>(m_width));
            if (imgIdx >= m_image.size())
            {
                return;
            }
            m_image[imgIdx] = color;
            m_horizon[xpos] = yp;
        }
    }

    void onNewFFTData(const std::vector<float>& magnitudes) override
    {
        for (size_t i = 0; i < m_magnitudeCollector.size(); ++i)
        {
            m_magnitudeCollector[i] += magnitudes[i];
        }
        m_count++;
        if (m_count < 20)
        {
            return;
        }

        const size_t offsetX = m_currentSlice * m_gap / 2;
        auto ypp = 120.f + static_cast<float>(m_currentSlice * m_gap) +
                   20 * std::log10(m_magnitudeCollector[0] / static_cast<float>(m_count));
        for (size_t x = 1; x < magnitudes.size(); ++x)
        {
            const auto value = m_magnitudeCollector[x] / static_cast<float>(m_count);
            const auto yp = 120.f + static_cast<float>(m_currentSlice * m_gap) + 20 * std::log10(value);
            const auto xpos = offsetX + x;
            if (ypp < yp)
            {
                while (ypp < yp)
                {
                    plotOverHorizon(static_cast<unsigned>(xpos), ypp, value * 4 + 0.00001f);
                    ypp++;
                }
            }
            else
            {
                auto ypTmp = yp;
                while (ypTmp < ypp)
                {
                    plotOverHorizon(static_cast<unsigned>(xpos), ypTmp, value * 4 + 0.00001f);
                    ypTmp++;
                }
            }
            ypp = yp;
        }

        m_currentSlice = (m_currentSlice + 1) % m_slices;
        if (m_currentSlice == 0)
        {
            std::ranges::fill(m_horizon, 0.0f);
            std::ranges::fill(m_image, 0.0f);
        }
        m_count = 0;
        std::ranges::fill(m_magnitudeCollector, 0.f);
    }

  private:
    size_t m_count{0};
    std::vector<float> m_magnitudeCollector;
    size_t m_width{1920};
    size_t m_height{512};
    size_t m_slices{32};
    size_t m_currentSlice{0};
    size_t m_gap{16};
    std::vector<float> m_horizon;
    std::vector<float> m_image;
};

}
