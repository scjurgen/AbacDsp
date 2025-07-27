#pragma once

#include <algorithm>
#include <array>
#include <cmath>

namespace AbacadDsp
{
// 1-m_fdbk lowpass
enum class OnePoleFilterCharacteristic
{
    LowPass,
    HighPass,
    AllPass
};

template <OnePoleFilterCharacteristic FilterCharacteristic, bool ClampValues>
class OnePoleFilter
{
public:
    explicit OnePoleFilter(const float sampleRate, const float frequency = 100.f) noexcept
        : m_sampleRate(sampleRate)
          , m_cutoff(frequency)
    {
        setCutoff(frequency);
    }

    void setSampleRate(const float sr) noexcept
    {
        m_sampleRate = sr;
    }

    void setCutoff(const float cutoff) noexcept
    {
        m_cutoff = cutoff;
        if (cutoff >= m_sampleRate / 2)
        {
            m_fdbk = 0;
        }
        else
        {
            const auto w0 = 2.0f * static_cast<float>(M_PI) * cutoff / m_sampleRate;
            if (FilterCharacteristic == OnePoleFilterCharacteristic::AllPass)
            {
                m_fdbk = (std::tan(w0 / 2.0f) - 1.0f) / (std::tan(w0 / 2.0f) + 1.0f);
            }
            else if (FilterCharacteristic == OnePoleFilterCharacteristic::LowPass)
            {
                // auto c2 = 2 - cos(w0);
                // m_fdbk = c2 - sqrt(c2 * c2 - 1);
                m_fdbk = std::exp(-w0);
            }
            else
            {
                m_fdbk = std::exp(-w0);
            }
        }
    }

    void setTimeConstant(float timeInSeconds, const float targetRatio = 0.1f) noexcept
    {
        // Calculate the filter coefficient for a 20dB (90%) change
        // targetRatio=0.1 -> -20dB is approximately 10% of the original gain
        m_fdbk = std::pow(targetRatio, 1.0f / (timeInSeconds * m_sampleRate));
    }

    [[nodiscard]] float magnitude(const float f) const noexcept
    {
        // float w = f / m_cutoff;
        if constexpr (FilterCharacteristic == OnePoleFilterCharacteristic::LowPass)
        {
            const double x = std::exp(-2.0 * M_PI * m_cutoff / m_sampleRate);
            // Calculate the magnitude response |H(f)|
            const double numerator = 1.0 - x;
            const double denominator = std::sqrt(1.0 + x * x - 2.0 * x * std::cos(2.0 * M_PI * f / m_sampleRate));
            return static_cast<float>(numerator / denominator);
            //            return 1.0f / std::sqrt(1.0f + w * w);
        }
        if constexpr (FilterCharacteristic == OnePoleFilterCharacteristic::HighPass)
        {
            const double x = std::exp(-2.0 * M_PI * m_cutoff / m_sampleRate);
            const double numerator = 1.0 - x;
            const double denominator = std::sqrt(1.0 + x * x - 2.0 * x * std::cos(2.0 * M_PI * f / m_sampleRate));
            const double lowpassMag = numerator / denominator;

            // Highpass magnitude computation
            return static_cast<float>(std::sqrt(1.0 - lowpassMag * lowpassMag));
            // return w / std::sqrt(1.0f + w * w);
        }
        if constexpr (FilterCharacteristic == OnePoleFilterCharacteristic::AllPass)
        {
            return 1.0f; // unity gain at all frequencies
        }
        return 0.0f; // This line should never be reached
    }

    [[nodiscard]] float feedback() const noexcept
    {
        return m_fdbk;
    }

    void setFeedback(const float value) noexcept
    {
        m_fdbk = value;
    }

    float step(const float in) noexcept
    {
        if (FilterCharacteristic == OnePoleFilterCharacteristic::AllPass)
        {
            float out = m_fdbk * in + m_v;
            m_v = in - m_fdbk * out;
            if (ClampValues)
            {
                m_v = std::clamp(m_v, -1.f, 1.f);
            }
            return out;
        }

        m_v = in + m_fdbk * (m_v - in);
        if (ClampValues)
        {
            m_v = std::clamp(m_v, -1.f, 1.f);
        }
        if (FilterCharacteristic == OnePoleFilterCharacteristic::LowPass)
        {
            return m_v;
        }
        if (FilterCharacteristic == OnePoleFilterCharacteristic::HighPass)
        {
            return in - m_v;
        }
        return 0.0f; // This line should never be reached
    }

    void processBlock(float* inPlace, size_t numSamples) noexcept
    {
        if (m_fdbk <= 1E-8f)
        {
            return;
        }
        for (size_t i = 0; i < numSamples; ++i)
        {
            inPlace[i] = step(inPlace[i]);
        }
    }

    void processBlock(const float* in, float* out, size_t numSamples) noexcept
    {
        if (m_fdbk == 0)
        {
            std::copy_n(in, numSamples, out);
            return;
        }
        for (size_t i = 0; i < numSamples; ++i)
        {
            out[i] = step(in[i]);
        }
    }

    void reset() noexcept
    {
        m_v = 0;
    }

private:
    float m_sampleRate{};
    float m_cutoff{0};
    float m_fdbk{0};
    float m_v{0};
};

template <OnePoleFilterCharacteristic FilterCharacteristic, bool ClampValues>
class OnePoleFilterStereo
{
public:
    explicit OnePoleFilterStereo(const float sampleRate)
        : OnePoleFilterStereo(sampleRate, FilterCharacteristic == OnePoleFilterCharacteristic::LowPass ? 8000 : 50)
    {
    }

    OnePoleFilterStereo(const float sampleRate, const float defaultCutoff)
        : m_sampleRate(sampleRate)
    {
        setCutoff(defaultCutoff);
    }

    void setSampleRate(const float sr)
    {
        m_sampleRate = sr;
    }

    void setCutoff(const float cutoff)
    {
        m_cutoff = cutoff;
        if (cutoff >= m_sampleRate / 2)
        {
            m_fdbk = 0;
        }
        else
        {
            m_fdbk = std::exp(-2.0f * static_cast<float>(M_PI) * cutoff / m_sampleRate);
        }
    }

    [[nodiscard]] float getMagnitude(const float hz) const
    {
        auto w = hz / m_cutoff;
        return 1 / (std::sqrt(w * w + 1));
    }

    void setFeedback(const float value)
    {
        m_fdbk = value;
    }

    void stepStereo(const float left, const float right, float& outLeft, float& outRight)
    {
        m_v[0] = left + m_fdbk * (m_v[0] - left);
        m_v[1] = right + m_fdbk * (m_v[1] - right);
        if (ClampValues)
        {
            m_v[0] = std::clamp(m_v[0], -1.f, 1.f);
            m_v[1] = std::clamp(m_v[1], -1.f, 1.f);
        }
        if (FilterCharacteristic == OnePoleFilterCharacteristic::LowPass)
        {
            outLeft = m_v[0];
            outRight = m_v[1];
        }
        if (FilterCharacteristic == OnePoleFilterCharacteristic::HighPass)
        {
            outLeft = left - m_v[0];
            outRight = right - m_v[1];
        }
    }

    void processBlock(float* inPlaceLeft, float* inPlaceRight, size_t numSamples)
    {
        for (size_t i = 0; i < numSamples; ++i)
        {
            stepStereo(inPlaceLeft[i], inPlaceRight[i], inPlaceLeft[i], inPlaceRight[i]);
        }
    }

    void processBlock(const float* inLeft, const float* inRight, float* outLeft, float* outRight, size_t numSamples)
    {
        for (size_t i = 0; i < numSamples; ++i)
        {
            stepStereo(inLeft[i], inRight[i], outLeft[i], outRight[i]);
        }
    }

    void reset()
    {
        m_v = {0, 0};
    }

private:
    float m_sampleRate;
    float m_cutoff{0};
    float m_fdbk{0};
    std::array<float, 2> m_v{};
};


template <OnePoleFilterCharacteristic FilterCharacteristic, bool ClampValues, size_t NumChannels>
class MultiChannelOnePoleFilter
{
public:
    explicit MultiChannelOnePoleFilter(const float sampleRate)
        : m_sampleRate(sampleRate)
    {
        m_v.fill(0.0f);
    }

    void setSampleRate(const float sr)
    {
        m_sampleRate = sr;
    }

    void setCutoff(const float cutoff)
    {
        m_cutoff = cutoff;
        if (cutoff >= m_sampleRate / 2)
        {
            m_fdbk = 0;
        }
        else
        {
            if (FilterCharacteristic == OnePoleFilterCharacteristic::AllPass)
            {
                float w0 = 2.0f * static_cast<float>(M_PI) * cutoff / m_sampleRate;
                m_fdbk = (std::tan(w0 / 2.0f) - 1.0f) / (std::tan(w0 / 2.0f) + 1.0f);
            }
            else
            {
                m_fdbk = std::exp(-2.0f * static_cast<float>(M_PI) * cutoff / m_sampleRate);
            }
        }
    }

    [[nodiscard]] float magnitude(const float hz) const
    {
        float w = hz / m_cutoff;
        if (FilterCharacteristic == OnePoleFilterCharacteristic::LowPass)
        {
            return 1.0f / std::sqrt(1.0f + w * w);
        }
        if (FilterCharacteristic == OnePoleFilterCharacteristic::HighPass)
        {
            return w / std::sqrt(1.0f + w * w);
        }
        if (FilterCharacteristic == OnePoleFilterCharacteristic::AllPass)
        {
            return 1.0f; // Allpass filters have unity gain at all frequencies
        }
        return 0.0f; // This line should never be reached
    }

    void setFeedback(const float value)
    {
        m_fdbk = value;
    }

    void step(float* inPlace)
    {
        for (size_t channel = 0; channel < NumChannels; ++channel)
        {
            if (FilterCharacteristic == OnePoleFilterCharacteristic::AllPass)
            {
                const auto out = m_fdbk * inPlace[channel] + m_v[channel];
                m_v[channel] = inPlace[channel] - m_fdbk * out;
                if (ClampValues)
                {
                    m_v[channel] = std::clamp(m_v[channel], -1.f, 1.f);
                }
                inPlace[channel] = out;
            }
            else
            {
                m_v[channel] = inPlace[channel] + m_fdbk * (m_v[channel] - inPlace[channel]);
                if (ClampValues)
                {
                    m_v[channel] = std::clamp(m_v[channel], -1.f, 1.f);
                }
                if (FilterCharacteristic == OnePoleFilterCharacteristic::LowPass)
                {
                    inPlace[channel] = m_v[channel];
                }
                else if (FilterCharacteristic == OnePoleFilterCharacteristic::HighPass)
                {
                    inPlace[channel] = inPlace[channel] - m_v[channel];
                }
            }
        }
    }

    void processBlock(float* inPlace, const size_t numSamples)
    {
        if (m_fdbk <= 1E-8f)
        {
            return;
        }
        for (size_t i = 0; i < numSamples; ++i)
        {
            step(inPlace + i * NumChannels);
        }
    }

    void processBlock(const float* in, float* out, const size_t numSamples)
    {
        if (m_fdbk == 0)
        {
            std::copy_n(in, numSamples * NumChannels, out);
            return;
        }
        for (size_t i = 0; i < numSamples; ++i)
        {
            std::copy_n(in + i * NumChannels, NumChannels, out + i * NumChannels);
            step(out + i * NumChannels);
        }
    }

    void reset()
    {
        m_v.fill(0.0f);
    }

private:
    float m_sampleRate{};
    float m_cutoff{0};
    float m_fdbk{0};
    std::array<float, NumChannels> m_v{};
};
}
