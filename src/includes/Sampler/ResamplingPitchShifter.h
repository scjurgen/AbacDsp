#pragma once

#include <algorithm>
#include <cmath>
#include <memory>
#include <numbers>
#include <vector>

namespace AbacDsp
{

/// @ingroup sampler
/// @brief Anything that can fill a stereo pair of buffers on demand and report whether it succeeded.
template <typename Producer>
concept SampleProducerConcept = requires(Producer p, float* l, float* r, size_t n) {
    { p.produceSamples(l, r, n) } -> std::convertible_to<bool>;
};

/**
 * @ingroup sampler
 * @brief Shifts pitch by resampling, pulling from an upstream producer through a ring buffer.
 *
 * Resampling changes pitch and duration together. Chained after a producer that
 * already stretches time, the two effects cancel and the result is pitch shift
 * at the original tempo, which is why this is a wrapper rather than a
 * standalone shifter.
 *
 * The ring buffer decouples the two rates: a shifted read consumes a different
 * number of source frames than it produces, so the producer is pulled only when
 * the buffer runs low rather than once per output block.
 */
template <SampleProducerConcept Producer>
class ResamplingPitchShifter
{
  public:
    explicit ResamplingPitchShifter(Producer* producer, const size_t ringBufferSize = 4096)
        : m_producer(producer)
        , m_ringBufferSize(ringBufferSize)
    {
        setupBuffers();
    }

    ~ResamplingPitchShifter() = default;

    void setPitchRatio(const float ratio)
    {
        m_pitchRatio = ratio;
    }

    [[nodiscard]] bool produceSamples(float* outLeft, float* outRight, const size_t numSamples)
    {
        if (!m_producer)
        {
            return false;
        }

        for (size_t i = 0; i < numSamples; ++i)
        {
            if (getAvailableSamples() < m_ringBufferSize / 4)
            {
                if (!fillIntermediateBuffer())
                {
                    return false;
                }
            }

            outLeft[i] = hermiteInterp(m_ringBufferL, m_fractionalReadPos);
            outRight[i] = hermiteInterp(m_ringBufferR, m_fractionalReadPos);

            m_fractionalReadPos += m_pitchRatio;
            while (m_fractionalReadPos >= static_cast<float>(m_ringBufferSize))
            {
                m_fractionalReadPos -= static_cast<float>(m_ringBufferSize);
            }
        }

        return true;
    }

    [[nodiscard]] bool isDone() const noexcept
    {
        return m_producerDone;
    }

  private:
    static constexpr size_t IntermediateBufferSize = 1024;

    void setupBuffers()
    {
        m_ringBufferL.resize(m_ringBufferSize, 0.0f);
        m_ringBufferR.resize(m_ringBufferSize, 0.0f);
        m_intermediateBufferL.resize(IntermediateBufferSize, 0.0f);
        m_intermediateBufferR.resize(IntermediateBufferSize, 0.0f);
    }

    [[nodiscard]] size_t getAvailableSamples() const
    {
        const float fracReadPosWrapped = std::fmod(m_fractionalReadPos, static_cast<float>(m_ringBufferSize));
        const size_t intReadPos = static_cast<size_t>(fracReadPosWrapped);

        if (m_ringWritePos >= intReadPos)
        {
            return m_ringWritePos - intReadPos;
        }
        return m_ringBufferSize - intReadPos + m_ringWritePos;
    }

    [[nodiscard]] bool fillIntermediateBuffer()
    {
        if (!m_producer->produceSamples(m_intermediateBufferL.data(), m_intermediateBufferR.data(),
                                        IntermediateBufferSize))
        {
            m_producerDone = true;
            return false;
        }

        for (size_t i = 0; i < IntermediateBufferSize; ++i)
        {
            m_ringBufferL[m_ringWritePos] = m_intermediateBufferL[i];
            m_ringBufferR[m_ringWritePos] = m_intermediateBufferR[i];
            m_ringWritePos = (m_ringWritePos + 1) % m_ringBufferSize;
        }

        return true;
    }

    [[nodiscard]] static float hermiteInterp(const std::vector<float>& buffer, const float pos)
    {
        const size_t size = buffer.size();
        const size_t p1 = static_cast<size_t>(std::floor(pos)) % size;
        const size_t p0 = (p1 + size - 1) % size;
        const size_t p2 = (p1 + 1) % size;
        const size_t p3 = (p1 + 2) % size;

        const float frac = pos - std::floor(pos);
        const float xm1 = buffer[p0];
        const float x0 = buffer[p1];
        const float x1 = buffer[p2];
        const float x2 = buffer[p3];

        return x0 +
               0.5f * frac *
                   (x1 - xm1 + frac * (2.0f * xm1 - 5.0f * x0 + 4.0f * x1 - x2 + frac * (3.0f * (x0 - x1) + x2 - xm1)));
    }

    Producer* m_producer{nullptr};
    size_t m_ringBufferSize;
    float m_pitchRatio{1.0f};
    float m_fractionalReadPos{0.0f};
    size_t m_ringWritePos{0};
    bool m_producerDone{false};

    std::vector<float> m_ringBufferL;
    std::vector<float> m_ringBufferR;
    std::vector<float> m_intermediateBufferL;
    std::vector<float> m_intermediateBufferR;
};

}