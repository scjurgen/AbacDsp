#pragma once

#include <algorithm>
#include <vector>

#include "Helpers/ConstructArray.h"
#include "Modulation/Flutter.h"
#include "Modulation/Wow.h"
#include "Numbers/MultichannelInterpolation.h"
#include "Numbers/TimeDistanceSmoother.h"
#include "Parameters/SmoothingParameter.h"
#include "SamplerateConverter/SrPushConverter.h"

namespace AbacDsp
{
template <size_t BufferSize, size_t NumChannels, size_t NumReadHeads, size_t TileSize>
class VariSpeedTapeDelay
{
  public:
    static constexpr auto accelPerSec = 6.f;
    static constexpr auto brakePerSec = 3.f;
    static constexpr auto NoisePeakQ = 0.1f;
    using TapeInterpolation = MultichannelInterpolation<NumChannels>;
    using DefectsInterpolation = MultichannelInterpolation<NumChannels>;

    VariSpeedTapeDelay(const float sampleRate, const std::shared_ptr<SincFilter>& filterSet)
        : m_sampleRate(sampleRate)
        , m_buffer((BufferSize + 6) * NumChannels, 0)
        , m_rdhd{constructArray<TimeDistanceSmoother<double>, NumReadHeads>(static_cast<double>(sampleRate))}
        , m_tmpOutput(static_cast<size_t>(sampleRate), 0)
        , m_srConverter(filterSet)
        , m_flutter(sampleRate / TileSize)
        , m_wow(sampleRate / TileSize)
    {
        for (size_t i = 0; i < NumReadHeads; ++i)
        {
            m_rdhd[i].setWrapPosition(BufferSize);
            m_rdhd[i].setCorrectionTime(.005f);
            setReadHead(i, (i + 1) * 4800, true);
        }
        m_flutter.setRate(.4f);
        m_flutter.setDepth(0.1f);
        m_wow.setRate(0.4f);
        m_wow.setDepth(0.1f);
        m_wow.setVariance(0.1f);
        m_wow.setDrift(0.05f);
    }

    void readBlock(const size_t hdIdx, std::array<float, NumChannels * TileSize>& out) noexcept
    {
        m_rdhd[hdIdx].setCurrentWritePosition(m_writeHead, m_ratio.getLastValue());
        for (size_t i = 0; i < TileSize; ++i)
        {
            m_rdhd[hdIdx].advancePosition();

            const auto indexBuffer = static_cast<size_t>(std::floor(m_rdhd[hdIdx].getPosition()));
            const float fraction = m_rdhd[hdIdx].getPosition() - static_cast<float>(indexBuffer);
            std::array<float, NumChannels> tmp{};
            TapeInterpolation::catmullRom(&m_buffer[indexBuffer * NumChannels], tmp.data(), fraction);
            for (size_t c = 0; c < NumChannels; ++c)
            {
                out[i * NumChannels + c] = tmp[c];
            }
        }
    }

    void feed(const std::array<float, NumChannels * TileSize>& in) noexcept
    {
        const auto w = m_wow.step();
        const auto f = m_flutter.step();
        const auto ratio = m_ratio.getValue(TileSize) * w * f;

        m_input = in.data();
        m_inputSize = TileSize;
        const auto producedFrames =
            m_srConverter.fetchBlock(ratio, m_input, TileSize, m_tmpOutput.data(), m_tmpOutput.size());
        if (producedFrames > 0)
        {
            writeToRingBuffer(m_tmpOutput.data(), producedFrames);
        }
    }

    void setReadHead(const size_t hdIdx, const float delta, const bool force = false) noexcept
    {
        constexpr float MaxModulationSafety{1000.f};
        const auto clampedDelta =
            std::clamp(delta, MaxModulationSafety, static_cast<float>(BufferSize) - 1 - MaxModulationSafety);
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
        const auto last = std::clamp(m_ratio.getLastValue(), 0.001f, 8.f);
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

    [[nodiscard]] const std::vector<float>& getBuffer() const noexcept
    {
        return m_buffer;
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

    const float m_sampleRate;

  protected:
    std::vector<float> m_buffer;

  private:
    LinearSmoothing m_ratio{1.f};

    std::array<TimeDistanceSmoother<double>, NumReadHeads> m_rdhd;

    size_t m_writeHead{};

    const float* m_input{};
    size_t m_inputSize{0};
    std::vector<float> m_tmpOutput;
    SrPushConverter<NumChannels> m_srConverter;
    Flutter m_flutter;
    Wow m_wow;
    float m_ratioTarget{1.f};
    float m_flutterRate{1.f};
    float m_wowRate{1.f};
};

}
