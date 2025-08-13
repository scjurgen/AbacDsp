#pragma once

#include "Filters/Biquad.h"
#include "Filters/OnePoleFilter.h"
#include "Numbers/Interpolation.h"
#include "Samplerate/sinc_3_128.h"
#include "Samplerate/VariReader.h"

#include <cmath>
#include <vector>

namespace AbacDsp
{
template <size_t BufferSize>
class VarispeedTapeDelay
{
  public:
    explicit VarispeedTapeDelay(const float sampleRate)
        : m_sampleRate(sampleRate)
        , m_vr(sampleRate, std::make_shared<SincFilter>(init_3_128))
    {
    }

    void setTapeLengthInMsecs(const float tapeLoopTime)
    {
        m_tapeLoopTime = tapeLoopTime;
    }

    void setDelayTimeMsecs(const float delayTime)
    {
        m_delayTime = delayTime;
        m_vr.setReadHead(delayTime * m_sampleRate * 0.001f);
    }

    void setTapeSpeed(const float value)
    {
        m_tapeSpeed = value;
        m_vr.setRatio(value);
    }

    template <size_t BlockSize>
    void process(const float* in, float* out)
    {
        m_vr.process(in, BlockSize);
        m_vr.readBlock(out, BlockSize);
    }

    void reset()
    {
        m_vr.reset();
    }

  private:
    float m_sampleRate;
    float m_tapeSpeed{1.f};
    float m_tapeLoopTime{500.f};
    float m_delayTime{50.f};
    VariReader<BufferSize, 2> m_vr;
};
}
