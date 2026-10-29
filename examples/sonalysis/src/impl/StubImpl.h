#pragma once

#include <cassert>
#include <cmath>
#include <cstdint>
#include <functional>

#include "Analysis/Spectrogram.h"
#include "Audio/AudioBuffer.h"
#include "EffectBase.h"

template <size_t BlockSize>
class StubImpl final : public EffectBase
{
  public:
    explicit StubImpl(const float sampleRate)
        : EffectBase(sampleRate)
    {
    }
    void setPeakFall(const size_t value)
    {
        m_peakFall = value;
    }
    void setRmsWindow(const size_t value)
    {
        m_rmsWindow = value;
    }
    void setPeakHold(const size_t value)
    {
        m_peakHold = value;
    }
    void setHoldReset(const bool value)
    {
        m_holdReset = value;
    }
    void setFftSize(const size_t value)
    {
        m_fftSize = value;
    }
    void setFftFall(const size_t value)
    {
        m_fftFall = value;
    }
    void setSpectrumPeakHold(const bool value)
    {
        m_spectrumPeakHold = value;
    }
    void setReferencePitch(const float value)
    {
        m_referencePitch = value;
    }
    void setPitchSource(const size_t value)
    {
        m_pitchSource = value;
    }
    void setBeatReset(const bool value)
    {
        m_beatReset = value;
    }
    void setThru(const bool value)
    {
        m_thru = value;
    }

    void processBlock(const AbacDsp::AudioBuffer<2, BlockSize>& in, AbacDsp::AudioBuffer<2, BlockSize>& out)
    {
        for (size_t i = 0; i < BlockSize; ++i)
        {
            out(i, 0) = in(i, 0);
            out(i, 1) = in(i, 1);
        }
    }

  private:
    size_t m_peakFall{};
    size_t m_rmsWindow{};
    size_t m_peakHold{};
    bool m_holdReset{};
    size_t m_fftSize{};
    size_t m_fftFall{};
    bool m_spectrumPeakHold{};
    float m_referencePitch{};
    size_t m_pitchSource{};
    bool m_beatReset{};
    bool m_thru{};
};