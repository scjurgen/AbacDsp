#pragma once

#include <array>
#include <cmath>
#include <algorithm>
#include "Filters/Biquad.h"

namespace AbacDsp
{

template <size_t BlockSize, size_t BinsPerOctave>
    requires(BinsPerOctave >= 1 && BinsPerOctave <= 6)
class OctaveBandAnalyzer
{
  public:
    static constexpr size_t NumOctaves = 10;
    static constexpr size_t NumBins = NumOctaves * BinsPerOctave;
    static constexpr size_t MaxBufferSize = 1024;
    static constexpr size_t MaxDecimationStages = NumOctaves;

    explicit OctaveBandAnalyzer(const float sampleRate)
        : m_sampleRate(sampleRate)
    {
        computeQ();
        initializeFilters();
        computeCenterFrequencies();
    }

    void process(const std::array<std::array<float, BlockSize>, 2>& input)
    {
        for (size_t i = 0; i < BlockSize; ++i)
        {
            m_inputBuffer[0][m_writePos] = input[0][i];
            m_inputBuffer[1][m_writePos] = input[1][i];
            m_writePos++;

            if (m_writePos >= MaxBufferSize)
            {
                analyzeAllOctaves();
                m_writePos = 0;
            }
        }
    }

    const std::array<float, NumBins>& getLevels() const
    {
        for (size_t i = 0; i < NumBins; ++i)
        {
            const size_t internalIdx = mapUserIndexToInternal(i);
            m_levels[i] = m_internalLevels[internalIdx];
        }
        return m_levels;
    }

    const std::array<float, NumBins>& getCenterFrequencies() const
    {
        for (size_t i = 0; i < NumBins; ++i)
        {
            const size_t internalIdx = mapUserIndexToInternal(i);
            m_centerFrequencies[i] = m_internalCenterFreqs[internalIdx];
        }
        return m_centerFrequencies;
    }

    void setSampleRate(const float sampleRate)
    {
        m_sampleRate = sampleRate;
        computeQ();
        initializeFilters();
        computeCenterFrequencies();
        reset();
    }

    void reset()
    {
        m_writePos = 0;
        m_inputBuffer[0].fill(0.0f);
        m_inputBuffer[1].fill(0.0f);
        m_workBuffer[0].fill(0.0f);
        m_workBuffer[1].fill(0.0f);
        for (auto& bp : m_bp)
        {
            bp.reset();
        }
        for (auto& channelFilters : m_aaFilters)
        {
            for (auto& filter : channelFilters)
            {
                filter.reset();
            }
        }
    }

  private:
    float m_sampleRate;
    float m_Q;
    std::array<float, NumBins> m_internalLevels{};
    std::array<float, NumBins> m_internalCenterFreqs{};
    mutable std::array<float, NumBins> m_levels{};
    mutable std::array<float, NumBins> m_centerFrequencies{};

    std::array<Biquad<BiquadFilterType::BandPass>, NumBins> m_bp;
    std::array<std::array<Biquad<BiquadFilterType::LowPass>, 2>, MaxDecimationStages> m_aaFilters;

    alignas(16) std::array<std::array<float, MaxBufferSize>, 2> m_inputBuffer{};
    alignas(16) std::array<std::array<float, MaxBufferSize>, 2> m_workBuffer{};
    alignas(16) std::array<std::array<float, MaxBufferSize>, 2> m_filteredBuffer{};

    size_t m_writePos{0};

    void analyzeAllOctaves()
    {
        std::copy(m_inputBuffer[0].begin(), m_inputBuffer[0].end(), m_workBuffer[0].begin());
        std::copy(m_inputBuffer[1].begin(), m_inputBuffer[1].end(), m_workBuffer[1].begin());

        size_t currentSize = MaxBufferSize;

        for (size_t octave = 0; octave < NumOctaves; ++octave)
        {
            const size_t baseIdx = octave * BinsPerOctave;

            for (size_t bin = 0; bin < BinsPerOctave; ++bin)
            {
                const size_t bandIdx = baseIdx + bin;
                m_bp[bandIdx].processBlock(m_workBuffer[0].data(), m_filteredBuffer[0].data(), currentSize);
                m_bp[bandIdx].processBlock(m_workBuffer[1].data(), m_filteredBuffer[1].data(), currentSize);
                m_internalLevels[bandIdx] = computeRMSLevel(currentSize);
            }

            if (octave < NumOctaves - 1)
            {
                m_aaFilters[octave][0].processBlock(m_workBuffer[0].data(), m_filteredBuffer[0].data(), currentSize);
                m_aaFilters[octave][1].processBlock(m_workBuffer[1].data(), m_filteredBuffer[1].data(), currentSize);

                currentSize /= 2;
                for (size_t i = 0; i < currentSize; ++i)
                {
                    m_workBuffer[0][i] = m_filteredBuffer[0][i * 2];
                    m_workBuffer[1][i] = m_filteredBuffer[1][i * 2];
                }
            }
        }
    }

    static constexpr size_t mapUserIndexToInternal(const size_t userIdx)
    {
        const size_t octaveNum = userIdx / BinsPerOctave;
        const size_t binInOctave = userIdx % BinsPerOctave;
        const size_t internalOctave = (NumOctaves - 1) - octaveNum;
        return internalOctave * BinsPerOctave + binInOctave;
    }

    void computeQ()
    {
        const float n = 1.0f / static_cast<float>(BinsPerOctave);
        const float numerator = std::pow(2.0f, n * 0.5f);
        const float denominator = std::pow(2.0f, n) - 1.0f;
        m_Q = numerator / denominator;
    }

    void initializeFilters()
    {
        for (size_t octave = 0; octave < NumOctaves; ++octave)
        {
            const float octaveSampleRate = m_sampleRate / static_cast<float>(1 << octave);
            const size_t baseIdx = octave * BinsPerOctave;

            for (size_t bin = 0; bin < BinsPerOctave; ++bin)
            {
                const size_t bandIdx = baseIdx + bin;
                const float centerFreq = computeBandCenterFreq(octave, bin, octaveSampleRate);
                m_bp[bandIdx].computeCoefficients(octaveSampleRate, centerFreq, m_Q, 0.0f);
            }

            if (octave < NumOctaves - 1)
            {
                const float cutoff = octaveSampleRate * 0.25f;
                constexpr float aaQ = 0.707f;
                m_aaFilters[octave][0].computeCoefficients(octaveSampleRate, cutoff, aaQ, 0.0f);
                m_aaFilters[octave][1].computeCoefficients(octaveSampleRate, cutoff, aaQ, 0.0f);
            }
        }
    }

    void computeCenterFrequencies()
    {
        for (size_t octave = 0; octave < NumOctaves; ++octave)
        {
            const float octaveSampleRate = m_sampleRate / static_cast<float>(1 << octave);
            const size_t baseIdx = octave * BinsPerOctave;

            for (size_t bin = 0; bin < BinsPerOctave; ++bin)
            {
                const size_t bandIdx = baseIdx + bin;
                m_internalCenterFreqs[bandIdx] = computeBandCenterFreq(octave, bin, octaveSampleRate);
            }
        }
    }

    float computeBandCenterFreq(const size_t octave, const size_t bin, const float octaveSampleRate) const
    {
        const float baseCenter = octaveSampleRate * 0.25f;
        const float binOffset = (static_cast<float>(bin) - static_cast<float>(BinsPerOctave - 1) * 0.5f) /
                                static_cast<float>(BinsPerOctave);
        return baseCenter * std::pow(2.0f, binOffset);
    }

    float computeRMSLevel(const size_t numSamples)
    {
        float sumL = 0.0f;
        float sumR = 0.0f;

        for (size_t i = 0; i < numSamples; ++i)
        {
            const float lSample = m_filteredBuffer[0][i];
            const float rSample = m_filteredBuffer[1][i];
            sumL += lSample * lSample;
            sumR += rSample * rSample;
        }

        const float rmsL = std::sqrt(sumL / static_cast<float>(numSamples));
        const float rmsR = std::sqrt(sumR / static_cast<float>(numSamples));
        return (rmsL + rmsR) * 0.5f;
    }
};

}
