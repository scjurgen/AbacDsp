#pragma once

#include <cassert>
#include <cmath>
#include <cstdint>
#include <functional>

#include "Analysis/Spectrogram.h"
#include "Audio/AudioBuffer.h"
#include "EffectBase.h"

template <size_t BlockSize>
class GenericImpl final : public EffectBase
{
  public:
    explicit GenericImpl(const float sampleRate)
        : EffectBase(sampleRate)
    {
        m_visualWavedata.resize(6000);
    }
    void setRecord(const bool value)
    {
        m_record = value;
    }
    void setPlay(const bool value)
    {
        m_play = value;
    }
    void setOverdub(const bool value)
    {
        m_overdub = value;
    }
    void setClear(const bool value)
    {
        m_clear = value;
    }
    void setThreshRec(const bool value)
    {
        m_threshRec = value;
    }
    void setHostSync(const bool value)
    {
        m_hostSync = value;
    }
    void setFreeRecord(const bool value)
    {
        m_freeRecord = value;
    }
    void setCountInBars(const size_t value)
    {
        m_countInBars = value;
    }
    void setRecordBars(const size_t value)
    {
        m_recordBars = value;
    }
    void setSliceDivision(const size_t value)
    {
        m_sliceDivision = value;
    }
    void setBpm(const float value)
    {
        m_bpm = value;
    }
    void setClickVolume(const float value)
    {
        m_clickVolume = std::pow(10.f, value / 20.f);
    }
    void setClickRecordVolume(const float value)
    {
        m_clickRecordVolume = std::pow(10.f, value / 20.f);
    }
    void setLoopVolume(const float value)
    {
        m_loopVolume = std::pow(10.f, value / 20.f);
    }
    void setRecThreshold(const float value)
    {
        m_recThreshold = std::pow(10.f, value / 20.f);
    }
    void setFreeze(const bool value)
    {
        m_freeze = value;
    }
    void setSeqPlay(const bool value)
    {
        m_seqPlay = value;
    }
    void setClearSeq(const bool value)
    {
        m_clearSeq = value;
    }

    void processBlock(const AbacDsp::AudioBuffer<2, BlockSize>& in, AbacDsp::AudioBuffer<2, BlockSize>& out)
    {
        for (size_t i = 0; i < BlockSize; ++i)
        {
            out(i, 0) = in(i, 0);
            out(i, 1) = in(i, 1);
        }

        for (size_t i = 0; i < BlockSize; ++i)
        {
            m_visualWavedata[m_currentSample] = out(i, 0) + out(i, 1);
            m_currentSample++;
            if (m_currentSample >= m_visualWavedata.size())
            {
                m_currentSample = 0;
            }
        }
    }
    const std::vector<float>& visualizeWaveData()
    {
        m_preparedWavedata.resize(m_visualWavedata.size());
        m_preparedWavedata = m_visualWavedata;
        return m_preparedWavedata;
    }

  private:
    bool m_record{};
    bool m_play{};
    bool m_overdub{};
    bool m_clear{};
    bool m_threshRec{};
    bool m_hostSync{};
    bool m_freeRecord{};
    size_t m_countInBars{};
    size_t m_recordBars{};
    size_t m_sliceDivision{};
    float m_bpm{};
    float m_clickVolume{};
    float m_clickRecordVolume{};
    float m_loopVolume{};
    float m_recThreshold{};
    bool m_freeze{};
    bool m_seqPlay{};
    bool m_clearSeq{};


    std::vector<float> m_visualWavedata;
    std::vector<float> m_preparedWavedata;
    size_t m_currentSample = 0;
};