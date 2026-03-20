#pragma once

#include <algorithm>
#include <random>
#include <vector>

#include "DSP/Samplerate/SrPushConverter.h"
#include "DSP/Numbers/MultichannelInterpolation.h"
#include "DSP/SmoothingParameter.h"

#include "DSP/Filter/Biquad.h"
#include "DSP/Filter/PinkFilter.h"

#include "utility/makearray.h"

#include "Flutter.h"
#include "Wow.h"

#include "TimeDistanceSmoother.h"

namespace OT::DSP
{
template <size_t BufferSize, size_t NumChannels, size_t InternalBlockSize>
class VariSpeedTapeDelay
{
  public:
    static constexpr size_t NumReadHeads{4};
    // future parameters, these could be parametrized if needed
    static constexpr auto accelPerSec = 6.f; // max ratio units per second (acceleration)
    static constexpr auto brakePerSec = 3.f; // max ratio units per second (braking)
    static constexpr auto NoisePeakQ = 0.1f;
    using TapeInterpolation = MultichannelInterpolation<NumChannels>;
    using DefectsInterpolation = MultichannelInterpolation<NumChannels>;

    VariSpeedTapeDelay(const float sampleRate, const std::shared_ptr<SincFilter>& filterSet)
        : m_sampleRate(sampleRate)
        , m_buffer((BufferSize + 6) * NumChannels, 0)
        , m_tapeDefectsBuffer((BufferSize + 6) * NumChannels, 0)
        , m_rdhd{Utility::makeArray<TimeDistanceSmoother, NumReadHeads>(sampleRate)}

        , m_tmpOutput(static_cast<size_t>(sampleRate), 0)
        , m_srConverter(filterSet)
        , m_biquad{Utility::makeArray<PeakBiquad, NumChannels>(sampleRate)}
        , m_flutter(sampleRate / InternalBlockSize)
        , m_wow(sampleRate / InternalBlockSize)
    {
        setTapeDefects(.02f);
        for (size_t i = 0; i < NumReadHeads; ++i)
        {
            m_rdhd[i].setWrapPosition(BufferSize);
            m_rdhd[i].setCorrectionTime(.005f);
            setReadHead(i, (i + 1) * 4800, true);
        }
        m_biquad[0].computeCoefficients(5000, 0.f, NoisePeakQ);
        m_biquad[1].computeCoefficients(5000, 0.f, NoisePeakQ);
        m_flutter.setRate(.4f);
        m_flutter.setDepth(0.1f);
        m_wow.setRate(0.4f);
        m_wow.setDepth(0.1f);
        m_wow.setVariance(0.1f);
        m_wow.setDrift(0.05f);
    }

    void setTapeDefects(const float pinkNoiseLevel) noexcept
    {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<float> distribution(-1.0f, 1.0f);

        std::generate(m_tapeDefectsBuffer.begin(), m_tapeDefectsBuffer.end(),
                      [&]() mutable
                      {
                          const float white = distribution(gen);
                          return pinkNoiseLevel * m_pink.step(white);
                      });
        std::copy_n(m_tapeDefectsBuffer.begin(), 6 * NumChannels,
                    m_tapeDefectsBuffer.begin() + NumChannels * BufferSize);
    }

    void readBlock(const size_t hdIdx, std::array<float, NumChannels * InternalBlockSize>& out) noexcept
    {
        m_rdhd[hdIdx].setCurrentWritePosition(m_writeHead, m_ratio.getLastValue());
        for (size_t i = 0; i < InternalBlockSize; ++i)
        {
            m_rdhd[hdIdx].advancePosition();

            const size_t indexBuffer = std::floor(m_rdhd[hdIdx].getPosition());
            const float fraction = m_rdhd[hdIdx].getPosition() - static_cast<float>(indexBuffer);
            std::array<float, NumChannels> tmp[2]{};
            TapeInterpolation::bspline_65x(&m_buffer[indexBuffer * NumChannels], tmp[0].data(), fraction);
            // clamp for the loop time tape
            const size_t sosHeadIndex =
                std::max<size_t>(1000, static_cast<size_t>(m_rdhd[NumReadHeads - 1].getCurrentDelta()));
            const size_t wrappedIndex = indexBuffer % (sosHeadIndex > 0 ? sosHeadIndex : indexBuffer);
            DefectsInterpolation::linearPt2(&m_tapeDefectsBuffer[wrappedIndex * NumChannels], tmp[1].data(), fraction);
            for (size_t c = 0; c < NumChannels; ++c)
            {
                out[i * NumChannels + c] = tmp[0][c] + m_tapeNoiseFactor * m_biquad[c].step(tmp[1][c]);
            }
        }
    }

    void process(const std::array<float, NumChannels * InternalBlockSize>& in) noexcept
    {
        const auto w = m_wow.step();
        const auto f = m_flutter.step();
        const auto ratio = m_ratio.getValue(InternalBlockSize) * w * f;

        m_input = in.data();
        m_inputSize = InternalBlockSize;
        const auto producedFrames =
            m_srConverter.fetchBlock(ratio, m_input, InternalBlockSize, m_tmpOutput.data(), m_tmpOutput.size());
        if (producedFrames > 0)
        {
            writeToRingBuffer(m_tmpOutput.data(), producedFrames);
        }
    }


    void setReadHead(const size_t hdIdx, const float delta, const bool force = false) noexcept
    {
        // savety areas of 1000 samples for modulation
        constexpr float MaxModulationSavety{1000.f};
        const auto clampedDelta =
            std::clamp(delta, MaxModulationSavety, static_cast<float>(BufferSize) - 1 - MaxModulationSavety);
        if (force)
        {
            m_rdhd[hdIdx].forceReadPositionDistance(clampedDelta);
        }
        else
        {
            m_rdhd[hdIdx].newTargetDistance(clampedDelta, 0.5f);
        }
    }

    void setRatio(const float targetRatio, const bool force = false) noexcept
    {
        // acceleration and braking model based on exponential model (doubling speed is always same time)
        const auto ctRatio = std::clamp(targetRatio, 0.001f, 8.f);
        const auto last = m_ratio.getLastValue();
        const auto delta = std::abs(std::log2(ctRatio / last));
        const auto rate = ctRatio > last ? accelPerSec : brakePerSec;
        const auto transitionTime = delta / rate;
        m_ratio.newTransition(ctRatio, transitionTime, m_sampleRate, force);
        m_ratioTarget = ctRatio;
        m_wow.setRate(m_ratioTarget * m_wowRate);
        m_flutter.setRate(m_ratioTarget * m_flutterRate);
    }

    void reset() noexcept
    {
        std::ranges::fill(m_buffer, 0);
        std::ranges::fill(m_tmpOutput, 0);
    }

    const std::vector<float>& getBuffer() const noexcept
    {
        return m_buffer;
    }

    void setTapeNoiseFactor(const float v) noexcept
    {
        m_tapeNoiseFactor = v;
    }

    void setTapeNoiseDistribution(const float v) noexcept
    {
        const float peak = std::sqrt(v) * 5.f;
        const float f = 5000.f - v * 3700.f;
        m_biquad[0].computeCoefficients(f, peak, NoisePeakQ);
        m_biquad[1].computeCoefficients(f, peak, NoisePeakQ);
    }

    void setFlutterDepth(const float value) noexcept
    {
        m_flutter.setDepth(value);
    }

    void setFlutterRate(const float value) noexcept
    {
        m_flutterRate = value;
        m_flutter.setRate(m_ratioTarget * value);
    }

    void setWowDepth(const float value) noexcept
    {
        m_wow.setDepth(value);
    }

    void setWowRate(const float value) noexcept
    {
        m_wowRate = value;
        m_wow.setRate(m_ratioTarget * value);
    }

    void setWowVariance(const float value) noexcept
    {
        m_wow.setVariance(value);
    }

    void setWowDrift(const float value) noexcept
    {
        m_wow.setDrift(value);
    }

  private:
    void writeToRingBuffer(const float* data, const size_t frames) noexcept
    {
        if (m_writeHead >= 6)
        {
            // no special treatment for interpolation wrap, just copy and advance
            if (const auto availableFrames = std::min(BufferSize - m_writeHead, frames); availableFrames >= frames)
            {
                std::copy_n(data, availableFrames * NumChannels, m_buffer.begin() + m_writeHead * NumChannels);
                m_writeHead += availableFrames;
                return;
            }
        }
        for (size_t i = 0; i < frames; ++i)
        {
            for (size_t c = 0; c < NumChannels; ++c)
            {
                m_buffer[m_writeHead * NumChannels + c] = data[i * NumChannels + c];
                if (m_writeHead < 6)
                {
                    m_buffer[(m_writeHead + BufferSize) * NumChannels + c] = data[i * NumChannels + c];
                }
            }
            m_writeHead++;
            if (m_writeHead >= BufferSize)
            {
                m_writeHead = 0;
            }
        }
    }

    float m_sampleRate;

  protected:
    std::vector<float> m_buffer;
    std::vector<float> m_tapeDefectsBuffer;

  private:
    LinearSmoothing m_ratio{1.f};

    std::array<TimeDistanceSmoother, 4> m_rdhd;

    size_t m_writeHead{};

    const float* m_input{};
    size_t m_inputSize{0};
    std::vector<float> m_tmpOutput;
    SrPushConverter<NumChannels> m_srConverter;
    PinkFilter m_pink;
    float m_tapeNoiseFactor{0.f};
    std::array<PeakBiquad, NumChannels> m_biquad;
    Flutter m_flutter;
    Wow m_wow;
    float m_ratioTarget{1.f};
    float m_flutterRate{1.f};
    float m_wowRate{1.f};
};
}
