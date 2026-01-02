#pragma once

#include <algorithm>
#include <cmath>
#include <memory>
#include <numbers>
#include <vector>

#include "pffft.h"

namespace AbacDsp
{

/**
 * @brief Phase vocoder-based time stretcher with transient preservation.
 *
 * Performs independent time stretching on stereo samples using STFT with
 * phase-locked vocoder. Supports transient-aware repositioning with
 * configurable lookahead zero-padding to preserve attack transients.
 * Uses 75% overlap (hopSize = windowSize/4) with Hann windowing.
 */
class StretchedSampleProducer
{
  public:
    explicit StretchedSampleProducer(const auto sampleRate)
        : m_sampleRate(sampleRate)
    {
        setupFFT();
    }

    ~StretchedSampleProducer()
    {
        pffft_aligned_free(m_fftWork);
        pffft_aligned_free(m_fftBuffer);
        pffft_aligned_free(m_spectrum);
        if (m_fftSetup)
        {
            pffft_destroy_setup(m_fftSetup);
        }
    }

    void setRawStereoSample(const std::shared_ptr<std::vector<float>>& data)
    {
        m_data = data;
    }

    void setFeedRate(const float ratio)
    {
        m_feedRatio = ratio;
    }

    void setTransientLookAhead(const float ratio)
    {
        m_transientLookAhead = std::clamp(ratio, 0.0f, 0.5f);
    }

    void setPosition(const size_t samplePos, const bool isTransient = false)
    {
        m_isDone = false;
        m_intPlayPos = samplePos;
        m_fracPlayPos = 0.0f;
        resetPhases();
        std::fill(m_outputRingL.begin(), m_outputRingL.end(), 0.0f);
        std::fill(m_outputRingR.begin(), m_outputRingR.end(), 0.0f);
        m_ringWritePos = 0;
        m_ringReadPos = 0;

        const auto lookAheadSamples = static_cast<size_t>(m_fftSize * m_transientLookAhead);

        if (isTransient)
        {
            if (samplePos >= lookAheadSamples)
            {
                m_transientOffset = lookAheadSamples;
                m_intPlayPos = samplePos - lookAheadSamples;
                m_samplesToSkip = lookAheadSamples;
            }
            else
            {
                m_transientOffset = samplePos;
                m_intPlayPos = 0;
                m_samplesToSkip = lookAheadSamples;
            }
        }
        else
        {
            m_transientOffset = 0;
            m_samplesToSkip = 0;
        }
    }

    bool produceSamples(float* outLeft, float* outRight, const size_t numSamples)
    {
        if (m_isDone || !m_data)
        {
            return false;
        }

        const size_t dataSize = m_data->size() / 2;
        const float* samples = m_data->data();
        for (size_t i = 0; i < numSamples; ++i)
        {
            if (getAvailableSamples() == 0)
            {
                if (!processFrame(samples, dataSize))
                    return false;
            }

            if (m_samplesToSkip > 0)
            {
                m_ringReadPos = (m_ringReadPos + 1) % m_outputRingL.size();
                --m_samplesToSkip;
                --i;
                continue;
            }

            outLeft[i] = m_outputRingL[m_ringReadPos];
            outRight[i] = m_outputRingR[m_ringReadPos];
            m_outputRingL[m_ringReadPos] = 0.0f;
            m_outputRingR[m_ringReadPos] = 0.0f;
            m_ringReadPos = (m_ringReadPos + 1) % m_outputRingL.size();
        }
        return true;
    }

    [[nodiscard]] bool isDone() const
    {
        return m_isDone;
    }

    [[nodiscard]] size_t getRemainingSourceSamples() const
    {
        if (!m_data || m_isDone)
        {
            return 0;
        }

        const size_t dataSize = m_data->size() / 2;
        return (m_intPlayPos < dataSize) ? (dataSize - m_intPlayPos) : 0;
    }

  private:
    static constexpr size_t WindowSize = 1024;
    static constexpr float twoPi = 2.0f * std::numbers::pi_v<float>;
    static constexpr size_t RingBufferSize = WindowSize * 2;

    void setupFFT()
    {
        m_fftSize = WindowSize;
        m_hopSize = WindowSize / 4;
        m_numBins = WindowSize / 2 + 1;

        if (m_fftSetup)
        {
            pffft_destroy_setup(m_fftSetup);
        }

        m_fftSetup = pffft_new_setup(static_cast<int>(m_fftSize), PFFFT_REAL);
        pffft_aligned_free(m_fftWork);
        pffft_aligned_free(m_fftBuffer);
        pffft_aligned_free(m_spectrum);
        m_fftWork = static_cast<float*>(pffft_aligned_malloc(m_fftSize * sizeof(float)));
        m_fftBuffer = static_cast<float*>(pffft_aligned_malloc(m_fftSize * sizeof(float)));
        m_spectrum = static_cast<float*>(pffft_aligned_malloc(m_fftSize * sizeof(float)));

        m_prevPhaseL.resize(m_numBins, 0.0f);
        m_prevPhaseR.resize(m_numBins, 0.0f);
        m_synthPhaseL.resize(m_numBins, 0.0f);
        m_synthPhaseR.resize(m_numBins, 0.0f);
        m_outputRingL.resize(RingBufferSize, 0.0f);
        m_outputRingR.resize(RingBufferSize, 0.0f);
        m_inputBufferL.resize(m_fftSize, 0.0f);
        m_inputBufferR.resize(m_fftSize, 0.0f);

        generateHannWindow();
    }

    void generateHannWindow()
    {
        m_window.resize(m_fftSize);
        for (size_t i = 0; i < m_fftSize; ++i)
        {
            m_window[i] = 0.5f * (1.0f - std::cos(twoPi * static_cast<float>(i) / static_cast<float>(m_fftSize - 1)));
        }
    }

    void resetPhases()
    {
        std::ranges::fill(m_prevPhaseL, 0.0f);
        std::ranges::fill(m_prevPhaseR, 0.0f);
        std::ranges::fill(m_synthPhaseL, 0.0f);
        std::ranges::fill(m_synthPhaseR, 0.0f);
        m_phaseReset = true;
    }

    size_t getAvailableSamples() const
    {
        if (m_ringWritePos >= m_ringReadPos)
        {
            return m_ringWritePos - m_ringReadPos;
        }
        return m_outputRingL.size() - m_ringReadPos + m_ringWritePos;
    }

    bool processFrame(const float* samples, const size_t dataSize)
    {
        if (!fillInputBuffer(samples, dataSize))
            return false;

        processChannel(m_inputBufferL.data(), m_outputRingL.data(), m_prevPhaseL, m_synthPhaseL);
        processChannel(m_inputBufferR.data(), m_outputRingR.data(), m_prevPhaseR, m_synthPhaseR);
        m_phaseReset = false;
        m_ringWritePos = (m_ringWritePos + m_hopSize) % m_outputRingL.size();
        return true;
    }

    bool fillInputBuffer(const float* samples, const size_t dataSize)
    {
        const auto zeroPadSamples = (m_transientOffset > 0) ? std::min(m_transientOffset, m_fftSize) : 0;

        for (size_t i = 0; i < zeroPadSamples; ++i)
        {
            m_inputBufferL[i] = 0.0f;
            m_inputBufferR[i] = 0.0f;
        }

        for (size_t i = zeroPadSamples; i < m_fftSize; ++i)
        {
            const size_t readPos = m_intPlayPos + (i - zeroPadSamples);
            if (readPos >= dataSize)
            {
                m_isDone = true;
                return false;
            }

            m_inputBufferL[i] = samples[readPos * 2];
            m_inputBufferR[i] = samples[readPos * 2 + 1];
        }

        m_transientOffset = (m_transientOffset > zeroPadSamples) ? m_transientOffset - zeroPadSamples : 0;

        const auto analysisHop = static_cast<float>(m_hopSize) * m_feedRatio;
        m_fracPlayPos += analysisHop;
        const auto intAdvance = static_cast<size_t>(m_fracPlayPos);
        m_fracPlayPos -= static_cast<float>(intAdvance);
        m_intPlayPos += intAdvance;

        return true;
    }

    void processChannel(const float* input, float* outputRing, std::vector<float>& prevPhase,
                        std::vector<float>& synthPhase)
    {
        for (size_t i = 0; i < m_fftSize; ++i)
        {
            m_fftBuffer[i] = input[i] * m_window[i];
        }

        pffft_transform_ordered(m_fftSetup, m_fftBuffer, m_spectrum, m_fftWork, PFFFT_FORWARD);
        if (m_phaseReset)
        {
            initializePhasesFromSpectrum(prevPhase, synthPhase);
        }
        else
        {
            phaseCorrection(prevPhase, synthPhase);
        }

        pffft_transform_ordered(m_fftSetup, m_spectrum, m_fftBuffer, m_fftWork, PFFFT_BACKWARD);
        synthesize(outputRing);
    }

    void synthesize(float* outputRing)
    {
        constexpr float colaFactor = 1.5f;
        const float normFactor = 1.0f / (static_cast<float>(m_fftSize) * colaFactor);
        size_t writePos = m_ringWritePos;
        for (size_t i = 0; i < m_fftSize; ++i)
        {
            outputRing[writePos] += m_fftBuffer[i] * m_window[i] * normFactor;
            writePos = (writePos + 1) % m_outputRingL.size();
        }
    }

    void initializePhasesFromSpectrum(std::vector<float>& prevPhase, std::vector<float>& synthPhase)
    {
        for (size_t k = 0; k < m_numBins; ++k)
        {
            const size_t idx = (k < m_numBins - 1) ? k * 2 : 1;
            const auto real = m_spectrum[idx];
            const auto imag = (k < m_numBins - 1) ? m_spectrum[idx + 1] : 0.0f;
            const auto magnitude = std::sqrt(real * real + imag * imag);
            const auto phase = std::atan2(imag, real);
            prevPhase[k] = phase;
            synthPhase[k] = phase;
            m_spectrum[idx] = magnitude * std::cos(phase);
            if (k < m_numBins - 1)
            {
                m_spectrum[idx + 1] = magnitude * std::sin(phase);
            }
        }
    }

    void phaseCorrection(std::vector<float>& prevPhase, std::vector<float>& synthPhase)
    {
        const auto freqPerBin = m_sampleRate / static_cast<float>(m_fftSize);
        const auto analysisHop = static_cast<float>(m_hopSize) * m_feedRatio;
        const auto expectedPhaseAdvance = twoPi * analysisHop / static_cast<float>(m_fftSize);
        for (size_t k = 0; k < m_numBins; ++k)
        {
            const size_t idx = (k < m_numBins - 1) ? k * 2 : 1;
            const auto real = m_spectrum[idx];
            const auto imag = (k < m_numBins - 1) ? m_spectrum[idx + 1] : 0.0f;
            const auto magnitude = std::sqrt(real * real + imag * imag);
            const auto phase = std::atan2(imag, real);
            auto phaseDiff = phase - prevPhase[k];
            prevPhase[k] = phase;
            phaseDiff -= static_cast<float>(k) * expectedPhaseAdvance;
            const auto qpd = static_cast<int>(phaseDiff / twoPi);
            phaseDiff -= twoPi * static_cast<float>(
                                     qpd + static_cast<int>(std::copysign(1.0f, static_cast<float>(qpd)) * (qpd & 1)));
            const auto binFreq = static_cast<float>(k) * freqPerBin;
            const auto instantFreq = binFreq + phaseDiff * m_sampleRate / (twoPi * analysisHop);
            synthPhase[k] += instantFreq * twoPi * static_cast<float>(m_hopSize) / m_sampleRate;
            m_spectrum[idx] = magnitude * std::cos(synthPhase[k]);
            if (k < m_numBins - 1)
            {
                m_spectrum[idx + 1] = magnitude * std::sin(synthPhase[k]);
            }
        }
    }

    std::shared_ptr<std::vector<float>> m_data{};
    size_t m_intPlayPos{0};
    float m_fracPlayPos{0.0f};
    float m_feedRatio{1.0f};
    bool m_isDone{true};
    bool m_phaseReset{true};
    float m_sampleRate;
    PFFFT_Setup* m_fftSetup{nullptr};
    float* m_fftWork{nullptr};
    float* m_fftBuffer{nullptr};
    float* m_spectrum{nullptr};
    size_t m_fftSize{WindowSize};
    size_t m_hopSize{WindowSize / 4};
    size_t m_numBins{WindowSize / 2 + 1};
    size_t m_ringWritePos{0};
    size_t m_ringReadPos{0};
    std::vector<float> m_window;
    std::vector<float> m_prevPhaseL;
    std::vector<float> m_prevPhaseR;
    std::vector<float> m_synthPhaseL;
    std::vector<float> m_synthPhaseR;
    std::vector<float> m_outputRingL;
    std::vector<float> m_outputRingR;
    std::vector<float> m_inputBufferL;
    std::vector<float> m_inputBufferR;
    float m_transientLookAhead{0.25f};
    size_t m_transientOffset{0};
    size_t m_samplesToSkip{0};
};

}
