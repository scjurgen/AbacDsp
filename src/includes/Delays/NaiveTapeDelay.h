#pragma once

#include "Numbers/Interpolation.h"

#include <vector>

namespace AbacDsp
{
template <size_t NumChannels, size_t MAXSIZE>
    requires(NumChannels > 0 && MAXSIZE > 10)
class NaiveTapeDelay
{
  public:
    explicit NaiveTapeDelay(const float sampleRate)
        : m_sampleRate(sampleRate)
        , m_buffer((MAXSIZE + 6) * NumChannels, 0.f)
    {
    }

    void setFeedFactor(const float value)
    {
        m_feedFactor = value;
    }

    void setHoldFactor(const float value)
    {
        m_holdFactor = value;
    }

    void setFeedbackFactor(const float value)
    {
        m_feedbackFactor = value;
    }

    void step(const float* in, float* out)
    {
        write(in);
        read(out);
        advanceHeads();
    }

  private:
    void moveHeadTo(const float newDistance)
    {
        // depending on modulation and current requests and easy functions we move the read head
    }

    void advanceHeads()
    {
        if (++m_headWrite >= MAXSIZE)
        {
            m_headWrite = 0;
        }

        if (++m_headReadMain >= MAXSIZE)
        {
            m_headReadMain = 0;
        }

        m_headRead += m_readAdvance;
        if (m_headRead >= MAXSIZE)
        {
            m_headRead -= MAXSIZE;
        }
    }

    void write(const float* in)
    {
        for (size_t c = 0; c < NumChannels; ++c)
        {
            // we add to the buffer depending on feed factor
            const size_t idx = m_headWrite * NumChannels + c;
            m_buffer[idx] = m_buffer[m_headReadMain * NumChannels + c] * m_holdFactor + m_feedFactor * in[c] +
                            m_feedbackFactor * m_lastRead[c];

            if (m_headWrite < 6) // wrap to end of buffer for interpolation.
            {
                const size_t padIndex = m_headWrite + MAXSIZE;
                m_buffer[padIndex * NumChannels + c] = m_buffer[idx];
            }
        }
    }

    void read(float* out)
    {
        const size_t idx = floor(m_headRead);
        const float fraction = m_headRead - static_cast<float>(idx);
        MultichannelInterpolation<NumChannels>::bspline_43x(&m_buffer[idx * NumChannels], m_lastRead.data(), fraction);
        std::copy_n(m_lastRead.data(), NumChannels, out);
    }

    float m_sampleRate;
    size_t m_headWrite{MAXSIZE / 8};
    float m_headRead{};
    size_t m_headReadMain{};
    float m_readAdvance{1.f};
    float m_feedFactor{1.f};
    float m_holdFactor{1.f};
    float m_feedbackFactor{0.f};
    std::array<float, NumChannels> m_lastRead;
    std::vector<float> m_buffer;
};
}
