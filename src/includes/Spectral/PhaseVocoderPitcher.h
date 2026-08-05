#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numbers>
#include <vector>

#include "pffft.h"

namespace AbacDsp
{

/**
 * @ingroup spectral
 * @brief Realtime streaming phase-vocoder pitch shifter (mono).
 *
 * Continuously analyses incoming audio in fixed-size 87.5%-overlapped (8x)
 * frames and reconstructs it with every bin's instantaneous frequency scaled
 * by the pitch ratio. Analysis and synthesis hop are identical (no
 * time-stretch stage), so no resampling is needed and the algorithmic
 * latency is fixed at WindowSize samples (the minimum needed for the OLA to
 * fully settle before a position is read back out). 8x overlap (vs. the more
 * common 4x) measurably reduces residual phase-lock artifacts (see
 * lockPhases()) at 2x the FFT rate.
 * @see https://ccrma.stanford.edu/~jos/sasp/Phase_Vocoder.html
 */
class PhaseVocoderPitcher
{
  public:
    explicit PhaseVocoderPitcher(const float sampleRate)
        : m_sampleRate(sampleRate)
    {
        setupFFT();
    }

    PhaseVocoderPitcher(const PhaseVocoderPitcher&) = delete;
    PhaseVocoderPitcher& operator=(const PhaseVocoderPitcher&) = delete;
    PhaseVocoderPitcher(PhaseVocoderPitcher&&) = delete;
    PhaseVocoderPitcher& operator=(PhaseVocoderPitcher&&) = delete;

    ~PhaseVocoderPitcher()
    {
        pffft_aligned_free(m_fftWork);
        pffft_aligned_free(m_fftBuffer);
        pffft_aligned_free(m_spectrum);
        if (m_fftSetup)
        {
            pffft_destroy_setup(m_fftSetup);
        }
    }

    void setPitchRatio(const float ratio) noexcept
    {
        m_pitchRatio = ratio;
    }

    [[nodiscard]] static constexpr size_t latencySamples() noexcept
    {
        return WindowSize;
    }

    void processBlock(const float* input, float* output, const size_t numSamples) noexcept
    {
        for (size_t i = 0; i < numSamples; ++i)
        {
            output[i] = step(input[i]);
        }
    }

  private:
    static constexpr size_t WindowSize = 4096;
    static constexpr size_t HopSize = WindowSize / 8;
    static constexpr size_t NumBins = WindowSize / 2 + 1;
    static constexpr size_t RingSize = WindowSize * 4;
    static constexpr float twoPi = 2.0f * std::numbers::pi_v<float>;

    void setupFFT()
    {
        m_fftSetup = pffft_new_setup(static_cast<int>(WindowSize), PFFFT_REAL);
        m_fftWork = static_cast<float*>(pffft_aligned_malloc(WindowSize * sizeof(float)));
        m_fftBuffer = static_cast<float*>(pffft_aligned_malloc(WindowSize * sizeof(float)));
        m_spectrum = static_cast<float*>(pffft_aligned_malloc(WindowSize * sizeof(float)));

        generateHannWindow();

        m_prevPhase.resize(NumBins, 0.0f);
        m_synthPhase.resize(NumBins, 0.0f);
        m_finalPhase.resize(NumBins, 0.0f);
        m_anaMagnitude.resize(NumBins, 0.0f);
        m_anaFreq.resize(NumBins, 0.0f);
        m_anaPhase.resize(NumBins, 0.0f);
        m_peakOfBin.resize(NumBins, 0);
        m_synMagnitude.resize(NumBins, 0.0f);
        m_synDonorMagnitude.resize(NumBins, 0.0f);
        m_synDonorBin.resize(NumBins, 0);
        m_synOffsetReal.resize(NumBins, 0.0f);
        m_synOffsetImag.resize(NumBins, 0.0f);
        m_isPeakHost.resize(NumBins, false);
        m_peakList.reserve(NumBins);
        m_inputRing.resize(WindowSize, 0.0f);
        m_outputRing.resize(RingSize, 0.0f);
        m_outputWritePos = HopSize;
    }

    void generateHannWindow()
    {
        m_window.resize(WindowSize);
        for (size_t i = 0; i < WindowSize; ++i)
        {
            m_window[i] = 0.5f * (1.0f - std::cos(twoPi * static_cast<float>(i) / static_cast<float>(WindowSize)));
        }
    }

    float step(const float sample) noexcept
    {
        m_inputRing[m_inputWritePos] = sample;
        m_inputWritePos = (m_inputWritePos + 1) % WindowSize;

        const float outSample = m_outputRing[m_outputReadPos];
        m_outputRing[m_outputReadPos] = 0.0f;
        m_outputReadPos = (m_outputReadPos + 1) % RingSize;

        ++m_samplesSinceHop;
        if (m_samplesSinceHop >= HopSize)
        {
            m_samplesSinceHop = 0;
            processFrame();
            m_outputWritePos = (m_outputWritePos + HopSize) % RingSize;
        }
        return outSample;
    }

    void processFrame() noexcept
    {
        for (size_t i = 0; i < WindowSize; ++i)
        {
            const size_t idx = (m_inputWritePos + i) % WindowSize;
            m_fftBuffer[i] = m_inputRing[idx] * m_window[i];
        }

        pffft_transform_ordered(m_fftSetup, m_fftBuffer, m_spectrum, m_fftWork, PFFFT_FORWARD);
        phaseVocode();
        pffft_transform_ordered(m_fftSetup, m_spectrum, m_fftBuffer, m_fftWork, PFFFT_BACKWARD);
        overlapAdd();
    }

    [[nodiscard]] static constexpr float freqPerBin(const float sampleRate) noexcept
    {
        return sampleRate / static_cast<float>(WindowSize);
    }

    [[nodiscard]] static constexpr float expectedPhaseAdvance() noexcept
    {
        return twoPi * static_cast<float>(HopSize) / static_cast<float>(WindowSize);
    }

    [[nodiscard]] static size_t shiftedBin(const size_t bin, const float ratio) noexcept
    {
        return static_cast<size_t>(std::lround(static_cast<float>(bin) * ratio));
    }

    // Classic FFT-based pitch shifter (Bernsee): analyse each bin's true
    // instantaneous frequency, then move magnitude and frequency to the bin
    // index scaled by the pitch ratio before resynthesising. On top of that,
    // lock every non-peak bin's phase to its region's spectral peak (Laroche
    // & Dolson) - without this, each bin's phase would drift independently
    // and a sustained tone comes out with a metallic, warbling vibrato as
    // neighbouring bins slip in and out of phase with each other.
    void phaseVocode() noexcept
    {
        analyseSpectrum();
        detectPeaksAndRegions();
        redistributeMagnitude();
        lockPhases();
        writeSpectrum();
    }

    void analyseSpectrum() noexcept
    {
        for (size_t k = 0; k < NumBins; ++k)
        {
            const size_t idx = (k < NumBins - 1) ? k * 2 : 1;
            const auto real = m_spectrum[idx];
            const auto imag = (k < NumBins - 1) ? m_spectrum[idx + 1] : 0.0f;
            m_anaMagnitude[k] = std::sqrt(real * real + imag * imag);
            const auto phase = std::atan2(imag, real);
            m_anaPhase[k] = phase;

            auto phaseDiff = phase - m_prevPhase[k];
            m_prevPhase[k] = phase;
            phaseDiff -= static_cast<float>(k) * expectedPhaseAdvance();
            const auto qpd = static_cast<int>(phaseDiff / twoPi);
            phaseDiff -=
                twoPi * static_cast<float>(qpd + static_cast<int>(std::copysign(1.0f, static_cast<float>(qpd)) *
                                                                  static_cast<float>(qpd & 1)));
            const auto binFreq = static_cast<float>(k) * freqPerBin(m_sampleRate);
            m_anaFreq[k] = binFreq + phaseDiff * m_sampleRate / (twoPi * static_cast<float>(HopSize));
        }
    }

    // A bin is a peak if it locally out-magnitudes both neighbours and is not just
    // sidelobe/noise ripple (must clear a floor relative to the frame's loudest bin) -
    // otherwise tiny spurious local maxima create unstable regions whose donor can flip
    // between frames, which introduces phase jumps rather than removing them. Every bin
    // is then assigned to whichever real peak is closest (region of influence, split at
    // the midpoint between adjacent peaks), so its phase can later be locked to it.
    void detectPeaksAndRegions() noexcept
    {
        constexpr float relativeFloor = 0.01f; // -40 dB relative to the frame's loudest bin
        const auto maxMagnitude = *std::ranges::max_element(m_anaMagnitude);
        const auto magnitudeFloor = maxMagnitude * relativeFloor;

        m_peakList.clear();
        for (size_t k = 1; k + 1 < NumBins; ++k)
        {
            if (m_anaMagnitude[k] > magnitudeFloor && m_anaMagnitude[k] > m_anaMagnitude[k - 1] &&
                m_anaMagnitude[k] > m_anaMagnitude[k + 1])
            {
                m_peakList.push_back(k);
            }
        }
        if (m_peakList.empty())
        {
            m_peakList.push_back(0);
        }

        size_t cursor = 0;
        for (size_t k = 0; k < NumBins; ++k)
        {
            while (cursor + 1 < m_peakList.size() && k > (m_peakList[cursor] + m_peakList[cursor + 1]) / 2)
            {
                ++cursor;
            }
            m_peakOfBin[k] = m_peakList[cursor];
        }
    }

    void redistributeMagnitude() noexcept
    {
        std::ranges::fill(m_synMagnitude, 0.0f);
        std::ranges::fill(m_synDonorMagnitude, 0.0f);
        std::ranges::fill(m_synOffsetReal, 0.0f);
        std::ranges::fill(m_synOffsetImag, 0.0f);
        for (size_t k = 0; k < NumBins; ++k)
        {
            const auto shiftedIndex = shiftedBin(k, m_pitchRatio);
            if (shiftedIndex >= NumBins)
            {
                continue;
            }
            m_synMagnitude[shiftedIndex] += m_anaMagnitude[k];
            // Several analysis bins collapsing onto one synthesis bin (shifting down) each
            // carry their own offset from their region's peak; a hard "strongest wins"
            // pick flips discretely between frames whenever the ranking is close, which
            // reintroduces the very phase jumps locking is meant to remove. A
            // magnitude-weighted circular mean of all donors' offsets instead changes
            // smoothly as their relative magnitudes drift.
            const auto offset = m_anaPhase[k] - m_anaPhase[m_peakOfBin[k]];
            m_synOffsetReal[shiftedIndex] += m_anaMagnitude[k] * std::cos(offset);
            m_synOffsetImag[shiftedIndex] += m_anaMagnitude[k] * std::sin(offset);
            // Still track the single strongest donor, used only to pick which peak
            // "hosts" this output bin's phase reference.
            if (m_anaMagnitude[k] >= m_synDonorMagnitude[shiftedIndex])
            {
                m_synDonorMagnitude[shiftedIndex] = m_anaMagnitude[k];
                m_synDonorBin[shiftedIndex] = k;
            }
        }
    }

    // Every output bin keeps its own continuously-accumulating phase alive every
    // frame (same formula as the original, un-locked algorithm) so it never goes
    // stale. This matters because which bin counts as "the peak" can occasionally
    // flip between two closely-tied candidates from one frame to the next; a bin
    // that stops being the peak must still be ready to resume immediately with a
    // continuous value, not restart from a frozen, now-discontinuous one.
    //
    // Coherence with the region's peak (Laroche & Dolson) is then applied by
    // locking each bin's phase to the peak's phase plus the two bins' magnitude-
    // weighted offset in the raw analysis spectrum. A damped (partial) pull toward
    // that target was tried and measured against the hard lock below across
    // several ratios - it only ever traded one ratio's improvement for a worse
    // regression elsewhere, never a net win, so the lock is applied directly.
    void lockPhases() noexcept
    {
        for (size_t j = 0; j < NumBins; ++j)
        {
            if (m_synMagnitude[j] > 0.0f)
            {
                accumulatePhase(j, m_anaFreq[m_synDonorBin[j]] * m_pitchRatio);
            }
        }

        std::ranges::fill(m_isPeakHost, false);
        for (const auto peak : m_peakList)
        {
            const auto shiftedPeak = shiftedBin(peak, m_pitchRatio);
            if (shiftedPeak < NumBins)
            {
                m_isPeakHost[shiftedPeak] = true;
            }
        }

        for (size_t j = 0; j < NumBins; ++j)
        {
            if (m_synMagnitude[j] <= 0.0f || m_isPeakHost[j])
            {
                continue;
            }
            const auto donor = m_synDonorBin[j];
            const auto peak = m_peakOfBin[donor];
            const auto shiftedPeak = shiftedBin(peak, m_pitchRatio);
            if (shiftedPeak < NumBins && m_isPeakHost[shiftedPeak])
            {
                const auto offset = std::atan2(m_synOffsetImag[j], m_synOffsetReal[j]);
                const auto target = m_synthPhase[shiftedPeak] + offset;
                m_synthPhase[j] += std::remainder(target - m_synthPhase[j], twoPi);
            }
        }

        m_finalPhase = m_synthPhase;
    }

    void accumulatePhase(const size_t outputBin, const float targetFreq) noexcept
    {
        const auto binFreq = static_cast<float>(outputBin) * freqPerBin(m_sampleRate);
        const auto phaseIncrement = (targetFreq - binFreq) * twoPi * static_cast<float>(HopSize) / m_sampleRate;
        m_synthPhase[outputBin] += static_cast<float>(outputBin) * expectedPhaseAdvance() + phaseIncrement;
    }

    void writeSpectrum() const noexcept
    {
        for (size_t k = 0; k < NumBins; ++k)
        {
            const size_t idx = (k < NumBins - 1) ? k * 2 : 1;
            m_spectrum[idx] = m_synMagnitude[k] * std::cos(m_finalPhase[k]);
            if (k < NumBins - 1)
            {
                m_spectrum[idx + 1] = m_synMagnitude[k] * std::sin(m_finalPhase[k]);
            }
        }
    }

    void overlapAdd() noexcept
    {
        constexpr float colaFactor = 3.0f;
        constexpr float normFactor = 1.0f / (static_cast<float>(WindowSize) * colaFactor);
        size_t writePos = m_outputWritePos;
        for (size_t i = 0; i < WindowSize; ++i)
        {
            m_outputRing[writePos] += m_fftBuffer[i] * m_window[i] * normFactor;
            writePos = (writePos + 1) % RingSize;
        }
    }

    float m_sampleRate;
    float m_pitchRatio{1.0f};
    PFFFT_Setup* m_fftSetup{nullptr};
    float* m_fftWork{nullptr};
    float* m_fftBuffer{nullptr};
    float* m_spectrum{nullptr};
    std::vector<float> m_window;
    std::vector<float> m_prevPhase;
    std::vector<float> m_synthPhase;
    std::vector<float> m_finalPhase;
    std::vector<float> m_anaMagnitude;
    std::vector<float> m_anaFreq;
    std::vector<float> m_anaPhase;
    std::vector<size_t> m_peakOfBin;
    std::vector<size_t> m_peakList;
    std::vector<float> m_synMagnitude;
    std::vector<float> m_synDonorMagnitude;
    std::vector<size_t> m_synDonorBin;
    std::vector<float> m_synOffsetReal;
    std::vector<float> m_synOffsetImag;
    std::vector<uint8_t> m_isPeakHost;
    std::vector<float> m_inputRing;
    std::vector<float> m_outputRing;
    size_t m_inputWritePos{0};
    size_t m_outputWritePos{0};
    size_t m_outputReadPos{0};
    size_t m_samplesSinceHop{0};
};

} // namespace AbacDsp
