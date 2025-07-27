#pragma once


#include <cmath>
#include <thread>
#include <vector>

#include "Analysis/FftSmall.h"

class MelSpectroGram
{
  public:
    struct ImageSet
    {
        size_t currentSlize;
        size_t width;
        size_t fftHalfLength;
        const float* data;

        [[nodiscard]] size_t size() const
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

    void setMelBands(size_t melBands)
    {
        m_melHeight = melBands;
        m_melSpectrogram.resize(m_melHeight * m_slices);
        createMelFilterbank();
    }

    void setSlices(size_t cnt)
    {
        m_slices = cnt;
        m_spectrogram.resize(m_fftLength / 2 * m_slices);
        m_currentSlice = std::clamp<size_t>(m_currentSlice, 0u, cnt);
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

    void processBlock(const float* in, size_t numSamples)
    {
        for (size_t i = 0; i < numSamples; ++i)
        {
            m_buffer[m_bufferIndex] = in[i];
            m_bufferIndex++;

            if (m_bufferIndex >= m_fftLength)
            {
                calculateFFT();
                advanceWindow();
            }
        }
    }

    [[nodiscard]] ImageSet getImageSet() const
    {
        return {m_currentSlice, m_slices, m_fftLength / 2, m_spectrogram.data()};
    }

  private:
    void calculateFFT()
    {
        size_t head = m_queueHead.load(std::memory_order_relaxed);
        size_t nextHead = (head + 1) % QUEUE_SIZE;

        if (nextHead != m_queueTail.load(std::memory_order_acquire))
        {
            m_fftQueue[head % QUEUE_SIZE] = m_buffer;
            m_queueHead.store(nextHead, std::memory_order_release);
        }
        // If the queue is full, we skip this FFT computation
    }

    void advanceWindow()
    {
        auto samplesToKeep = static_cast<size_t>(m_fftLength * (1.0f - m_windowForward));
        auto index = m_fftLength - samplesToKeep;
        std::copy_n(&m_buffer[index], m_buffer.size() - index, m_buffer.data());
        m_bufferIndex = samplesToKeep;
    }

    void workerFunction()
    {
        while (!m_shouldExit.load(std::memory_order_acquire))
        {
            size_t tail = m_queueTail.load(std::memory_order_relaxed);
            size_t head = m_queueHead.load(std::memory_order_acquire);

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
        float maxFreq = m_sampleRate / 2.0f;
        float minMel = 0;
        float maxMel = 2595.0f * std::log10(1.0f + maxFreq / 700.0f);

        m_melFilterbank.resize(m_melHeight);

        for (size_t i = 0; i < m_melHeight; ++i)
        {
            float mel = minMel + (maxMel - minMel) * i / (m_melHeight - 1);
            float hz = 700.0f * (std::pow(10.0f, mel / 2595.0f) - 1.0f);
            size_t fftBin = static_cast<size_t>(std::round(hz * m_fftLength / m_sampleRate));

            m_melFilterbank[i].resize(m_fftLength / 2);
            std::fill(m_melFilterbank[i].begin(), m_melFilterbank[i].end(), 0.0f);

            if (i > 0 && i < m_melHeight - 1)
            {
                float prevMel = minMel + (maxMel - minMel) * (i - 1) / (m_melHeight - 1);
                float nextMel = minMel + (maxMel - minMel) * (i + 1) / (m_melHeight - 1);
                float prevHz = 700.0f * (std::pow(10.0f, prevMel / 2595.0f) - 1.0f);
                float nextHz = 700.0f * (std::pow(10.0f, nextMel / 2595.0f) - 1.0f);
                size_t prevBin = static_cast<size_t>(std::round(prevHz * m_fftLength / m_sampleRate));
                size_t nextBin = static_cast<size_t>(std::round(nextHz * m_fftLength / m_sampleRate));

                for (size_t j = prevBin; j < fftBin; ++j)
                {
                    m_melFilterbank[i][j] = (j - prevBin) / static_cast<float>(fftBin - prevBin);
                }
                for (size_t j = fftBin; j < nextBin; ++j)
                {
                    m_melFilterbank[i][j] = 1.0f - (j - fftBin) / static_cast<float>(nextBin - fftBin);
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
    size_t m_fftLength{1024};
    size_t m_slices{1920};
    float m_windowForward{0.5f};
    size_t m_currentSlice{0};
    size_t m_bufferIndex{0};
    // fft async
    static constexpr size_t QUEUE_SIZE = 4;
    std::array<std::vector<float>, QUEUE_SIZE> m_fftQueue;
    std::atomic<size_t> m_queueHead{0};
    std::atomic<size_t> m_queueTail{0};
    std::atomic<bool> m_shouldExit{false};
    std::thread m_workerThread;
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
        m_shouldExit.store(true, std::memory_order_release);
        m_workerThread.join();
    }

    void setFftLength(const unsigned N)
    {
        m_fftLength = N;
        m_fft.resize(m_fftLength);
        m_buffer.resize(m_fftLength);
        m_fftBuffer.resize(m_fftLength);
        m_magnitudes.resize(m_fftLength / 2);
    }

    void processBlock(const float* in, const unsigned numSamples)
    {
        for (unsigned i = 0; i < numSamples; ++i)
        {
            m_buffer[m_bufferIndex] = in[i];
            m_bufferIndex++;

            if (m_bufferIndex >= m_fftLength)
            {
                calculateFFT();
                advanceWindow();
            }
        }
    }

  protected:
    virtual void onNewFFTData(const std::vector<float>& magnitudes) = 0;

    float m_sampleRate{48000.f};
    std::vector<float> m_buffer;
    std::vector<float> m_fftBuffer;
    std::vector<float> m_magnitudes;
    HannWindowMagnitudesFft m_fft;
    unsigned m_fftLength{1024};
    unsigned m_forwardLength{341}; // 33%
    unsigned m_bufferIndex{0};

  private:
    void calculateFFT()
    {
        size_t head = m_queueHead.load(std::memory_order_relaxed);
        size_t nextHead = (head + 1) % QUEUE_SIZE;

        if (nextHead != m_queueTail.load(std::memory_order_acquire))
        {
            m_fftQueue[head % QUEUE_SIZE] = m_buffer;
            m_queueHead.store(nextHead, std::memory_order_release);
        }
    }

    void advanceWindow()
    {
        const auto samplesToKeep = m_fftLength - m_forwardLength;
        const auto index = m_forwardLength;
        std::copy(m_buffer.begin() + index, m_buffer.end(), m_buffer.begin());
        m_bufferIndex = samplesToKeep;
    }

    void workerFunction()
    {
        while (!m_shouldExit.load(std::memory_order_acquire))
        {
            size_t tail = m_queueTail.load(std::memory_order_relaxed);
            size_t head = m_queueHead.load(std::memory_order_acquire);

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

struct SpectrumImageSet
{
    size_t activeSlice;
    size_t width;
    size_t height;
    const float* data;

    [[nodiscard]] size_t size() const
    {
        return width * height;
    }
};

class SimpleSpectrogram : public SpectrogramBase
{
  public:
    SimpleSpectrogram()
        : m_slices{1920}
        , m_currentSlice{0}
        , m_spectrogram(m_fftLength / 2 * m_slices)
    {
    }

    [[nodiscard]] SpectrumImageSet getImageSet() const
    {
        return {m_currentSlice, m_slices, m_fftLength / 2, m_spectrogram.data()};
    }

  protected:
    void onSlicesChanged()
    {
        m_spectrogram.resize(m_fftLength / 2 * m_slices);
    }

    void onFftLengthChanged()
    {
        m_spectrogram.resize(m_fftLength / 2 * m_slices);
    }

    void onNewFFTData(const std::vector<float>& magnitudes) override
    {
        if (m_spectrogram.size() == 0)
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

    void setSlices(size_t cnt)
    {
        m_slices = cnt;
        m_currentSlice = std::clamp<size_t>(m_currentSlice, 0u, cnt);
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

    [[nodiscard]] SpectrumImageSet getImageSet() const
    {
        return {m_currentSlice, m_width, m_height, m_image.data()};
    }

  protected:
    void onSlicesChanged()
    {
        m_image.resize(m_width * m_height, 0.0f);
        m_horizon.resize(m_width, 0.0f);
    }

    void onFftLengthChanged()
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
            const size_t imgIdx = static_cast<size_t>(yp + xpos * m_width);
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
            if (ypp < yp) // new point is greater, draw down up
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
                while (ypTmp < ypp) // new point lower, draw reverse
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
            std::fill(m_horizon.begin(), m_horizon.end(), 0.0f);
            std::fill(m_image.begin(), m_image.end(), 0.0f);
        }
        m_count = 0;
        std::fill(m_magnitudeCollector.begin(), m_magnitudeCollector.end(), 0.f);
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
