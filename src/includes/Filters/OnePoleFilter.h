#pragma once

#include <array>
#include <algorithm>
#include <cmath>
#include <numbers>
/*
enum class OnePoleFilterCharacteristic
{
    LowPass, HighPass, AllPass, HighPassLeaky
};

OnePoleFilter<OnePoleFilterCharacteristic::LowPass> lp;
OnePoleFilter<OnePoleFilterCharacteristic::HighPass> hp;
OnePoleFilter<OnePoleFilterCharacteristic::AllPass> ap;
OnePoleFilter<OnePoleFilterCharacteristic::HighPassLeaky> hpl;
float result = lp.setCutoff(const float cutoff);
float  result = lp.step(const float in);

Stereo version:
template <OnePoleFilterCharacteristic FilterCharacteristic, bool ClampValues = false>
class OnePoleFilterStereo
    : public OnePoleBase<OnePoleFilterStereo<FilterCharacteristic, ClampValues>, FilterCharacteristic>
{
public:
    explicit OnePoleFilterStereo(const float sampleRate, const float cutoff = 100.0f);

    void stepStereo(float left, float right, float& outLeft, float& outRight);
    void processBlock(float* inPlaceLeft, float* inPlaceRight, const size_t numSamples);
    void processBlock(const float* inLeft, const float* inRight, float* outLeft, float* outRight,
                      const size_t numSamples);
}

 */
namespace AbacDsp
{
/**
 * @brief First-order (one-pole) filters N.B.: there are 2 slightly difference Highpass versions.
 */
enum class OnePoleFilterCharacteristic
{
    /**
     * @brief One-pole lowpass (leaky integrator).
     */
    LowPass,

    /**
     * @brief Canonical one-pole highpass (textbook digital HPF).
     */
    HighPass,

    AllPass,

    /**
     * @brief Leaky highpass (input minus lowpass).
     * Computes y[n] = x[n] − lowpass(x[n]), an alternate highpass structure.
     * N.B.: Simpler, but high-frequency gain is always slightly less than 0dB; output rolls off slightly at Nyquist,
     * unlike the canonical HighPass.
     */
    HighPassLeaky
};


template <typename Derived, OnePoleFilterCharacteristic FilterCharacteristic>
class OnePoleBase
{
  public:
    explicit OnePoleBase(const float sr)
        : m_sampleRate(sr)
    {
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
            const auto w0 = 2.0f * std::numbers::pi_v<float> * cutoff / m_sampleRate;
            if constexpr (FilterCharacteristic == OnePoleFilterCharacteristic::AllPass)
            {
                const auto tanW = std::tan(w0 * 0.5f);
                m_fdbk = (tanW - 1.0f) / (tanW + 1.0f);
            }
            else
            {
                m_fdbk = std::exp(-w0);
            }
        }
    }

    void setFeedback(const float value) noexcept
    {
        m_fdbk = value;
    }

    /**
     * @brief Sets the filter decay time, specifying how long it takes the filter's response to decrease to a given
     * fraction of its initial value, as set by the target ratio parameter. Typical fractions are -20dB (0.1), -40dB
     * (0.01), and -60dB (0.001).
     * N.B.: use this for explicit control of smoothing
     */
    void setDecayTime(const float timeInSeconds, const float fraction = 0.1f) noexcept
    {
        m_fdbk = std::pow(fraction, 1.0f / (timeInSeconds * m_sampleRate));
    }

    [[nodiscard]] float magnitude(const float hz) const noexcept
    {
        if constexpr (FilterCharacteristic == OnePoleFilterCharacteristic::LowPass)
        {
            const double x = std::exp(-2.0 * std::numbers::pi_v<double> * m_cutoff / m_sampleRate);
            const double numerator = 1.0 - x;
            const double denominator =
                std::sqrt(1.0 + x * x - 2.0 * x * std::cos(2.0 * std::numbers::pi_v<double> * hz / m_sampleRate));
            return static_cast<float>(numerator / denominator);
        }
        if constexpr (FilterCharacteristic == OnePoleFilterCharacteristic::HighPassLeaky)
        {
            const double fdbk = m_fdbk;
            const double omega = 2.0 * std::numbers::pi_v<double> * hz / m_sampleRate;
            const double numerator = 2.0 * fdbk * std::sin(omega * 0.5);
            const double denominator = std::sqrt(1.0 + fdbk * fdbk - 2.0 * fdbk * std::cos(omega));
            return static_cast<float>(numerator / denominator);
        }
        if constexpr (FilterCharacteristic == OnePoleFilterCharacteristic::HighPass)
        {
            const double a0 = 0.5 * (1.0 + m_fdbk);
            const double omega = 2.0 * std::numbers::pi_v<double> * hz / m_sampleRate;
            const double numerator = 2.0 * a0 * std::sin(omega * 0.5);
            const double denominator = std::sqrt(1.0 + m_fdbk * m_fdbk - 2.0 * m_fdbk * std::cos(omega));
            return static_cast<float>(numerator / denominator);
        }
        if constexpr (FilterCharacteristic == OnePoleFilterCharacteristic::AllPass)
        {
            return 1.0f;
        }
        return 0.0f;
    }

    void reset() noexcept
    {
        static_cast<Derived*>(this)->resetImpl();
    }

  protected:
    float m_sampleRate{48000.0f};
    float m_cutoff{100.0f};
    float m_fdbk{0.0f};
};


// --- Mono version ---
template <OnePoleFilterCharacteristic FilterCharacteristic, bool ClampValues = false>
class OnePoleFilter : public OnePoleBase<OnePoleFilter<FilterCharacteristic, ClampValues>, FilterCharacteristic>
{
  public:
    explicit OnePoleFilter(float sampleRate, float cutoff = 1000.0f) noexcept(false)
        : OnePoleBase<OnePoleFilter, FilterCharacteristic>(sampleRate)
    {
        this->setCutoff(cutoff);
        resetImpl();
    }

    float step(const float in) noexcept
    {
        if constexpr (FilterCharacteristic == OnePoleFilterCharacteristic::AllPass)
        {
            float out = this->m_fdbk * in + m_v;
            m_v = in - this->m_fdbk * out;
            if constexpr (ClampValues)
            {
                m_v = std::clamp(m_v, -1.f, 1.f);
            }
            return out;
        }
        if constexpr (FilterCharacteristic == OnePoleFilterCharacteristic::LowPass)
        {
            m_v = in + this->m_fdbk * (m_v - in);
            if constexpr (ClampValues)
            {
                m_v = std::clamp(m_v, -1.f, 1.f);
            }
            return m_v;
        }
        if constexpr (FilterCharacteristic == OnePoleFilterCharacteristic::HighPass)
        {
            const auto a0 = (1 + this->m_fdbk) * 0.5f;
            const auto out = a0 * (in - m_x1) + this->m_fdbk * m_v;
            m_x1 = in;
            m_v = out;
            if constexpr (ClampValues)
            {
                m_v = std::clamp(m_v, -1.f, 1.f);
            }
            return out;
        }
        if constexpr (FilterCharacteristic == OnePoleFilterCharacteristic::HighPassLeaky)
        {
            m_v = in + this->m_fdbk * (m_v - in);
            if constexpr (ClampValues)
            {
                m_v = std::clamp(m_v, -1.f, 1.f);
            }
            return in - m_v;
        }
        return 0.0f;
    }

    void processBlock(float* inPlace, const size_t numSamples) noexcept
    {
        if (this->m_fdbk <= 1E-8f)
        {
            return;
        }
        std::transform(inPlace, inPlace + numSamples, inPlace, [this](const float x) { return this->step(x); });
    }

    void processBlock(const float* in, float* out, const size_t numSamples) noexcept
    {
        if (this->m_fdbk == 0)
        {
            std::copy_n(in, numSamples, out);
            return;
        }
        std::transform(in, in + numSamples, out, [this](const float x) { return this->step(x); });
    }

    void resetImpl() noexcept
    {
        m_v = 0.0f;
    }

  private:
    float m_v{0.0f};
    float m_x1{0.f};
};


// --- Stereo version ---
template <OnePoleFilterCharacteristic FilterCharacteristic, bool ClampValues = false>
class OnePoleFilterStereo
    : public OnePoleBase<OnePoleFilterStereo<FilterCharacteristic, ClampValues>, FilterCharacteristic>
{
  public:
    explicit OnePoleFilterStereo(const float sampleRate, const float cutoff = 100.0f) noexcept
        : OnePoleBase<OnePoleFilterStereo, FilterCharacteristic>(sampleRate)
    {
        this->m_sampleRate = sampleRate;
        this->setCutoff(cutoff);
    }

    void stepStereo(float left, float right, float& outLeft, float& outRight) noexcept
    {
        if constexpr (FilterCharacteristic == OnePoleFilterCharacteristic::AllPass)
        {
            const auto tmpL = this->m_fdbk * left + m_v[0];
            const auto tmpR = this->m_fdbk * right + m_v[1];
            m_v[0] = left - this->m_fdbk * tmpL;
            m_v[1] = right - this->m_fdbk * tmpR;
            if constexpr (ClampValues)
            {
                m_v[0] = std::clamp(m_v[0], -1.f, 1.f);
                m_v[1] = std::clamp(m_v[1], -1.f, 1.f);
            }
            outLeft = tmpL;
            outRight = tmpR;
        }
        else if constexpr (FilterCharacteristic == OnePoleFilterCharacteristic::LowPass)
        {
            m_v[0] = left + this->m_fdbk * (m_v[0] - left);
            m_v[1] = right + this->m_fdbk * (m_v[1] - right);
            if constexpr (ClampValues)
            {
                m_v[0] = std::clamp(m_v[0], -1.f, 1.f);
                m_v[1] = std::clamp(m_v[1], -1.f, 1.f);
            }
            outLeft = m_v[0];
            outRight = m_v[1];
        }
        else if constexpr (FilterCharacteristic == OnePoleFilterCharacteristic::HighPass)
        {
            const auto a0 = (1.0f + this->m_fdbk) * 0.5f;
            float outL = a0 * (left - m_x1[0]) + this->m_fdbk * m_v[0];
            float outR = a0 * (right - m_x1[1]) + this->m_fdbk * m_v[1];
            m_x1[0] = left;
            m_x1[1] = right;
            if constexpr (ClampValues)
            {
                outL = std::clamp(outL, -1.f, 1.f);
                outR = std::clamp(outR, -1.f, 1.f);
            }
            m_v[0] = outL;
            m_v[1] = outR;
            outLeft = outL;
            outRight = outR;
        }
    }

    void processBlock(float* inPlaceLeft, float* inPlaceRight, const size_t numSamples) noexcept
    {
        for (size_t i = 0; i < numSamples; ++i)
        {
            stepStereo(inPlaceLeft[i], inPlaceRight[i], inPlaceLeft[i], inPlaceRight[i]);
        }
    }

    void processBlock(const float* inLeft, const float* inRight, float* outLeft, float* outRight,
                      const size_t numSamples) noexcept
    {
        for (size_t i = 0; i < numSamples; ++i)
        {
            stepStereo(inLeft[i], inRight[i], outLeft[i], outRight[i]);
        }
    }

    void resetImpl() noexcept
    {
        m_v[0] = m_v[1] = 0.0f;
        m_x1[0] = m_x1[1] = 0.0f;
    }

  private:
    std::array<float, 2> m_v{};
    std::array<float, 2> m_x1{};
};


// --- Arbitrary channel count version (MultiChannel) ---
template <OnePoleFilterCharacteristic FilterCharacteristic, size_t NumChannels, bool ClampValues = false>
class MultiChannelOnePoleFilter
    : public OnePoleBase<MultiChannelOnePoleFilter<FilterCharacteristic, ClampValues, NumChannels>,
                         FilterCharacteristic>
{
  public:
    explicit MultiChannelOnePoleFilter(const float sampleRate, const float cutoff = 100.0f) noexcept
        : OnePoleBase<MultiChannelOnePoleFilter, FilterCharacteristic>(sampleRate)
    {
        this->m_sampleRate = sampleRate;
        this->setCutoff(cutoff);
        m_v.fill(0.0f);
        m_x1.fill(0.0f);
    }

    void step(float* inPlace) noexcept
    {
        for (size_t ch = 0; ch < NumChannels; ++ch)
        {
            if constexpr (FilterCharacteristic == OnePoleFilterCharacteristic::AllPass)
            {
                const auto out = this->m_fdbk * inPlace[ch] + m_v[ch];
                m_v[ch] = inPlace[ch] - this->m_fdbk * out;
                if constexpr (ClampValues)
                {
                    m_v[ch] = std::clamp(m_v[ch], -1.f, 1.f);
                }
                inPlace[ch] = out;
            }
            else if constexpr (FilterCharacteristic == OnePoleFilterCharacteristic::LowPass)
            {
                m_v[ch] = inPlace[ch] + this->m_fdbk * (m_v[ch] - inPlace[ch]);
                if constexpr (ClampValues)
                {
                    m_v[ch] = std::clamp(m_v[ch], -1.f, 1.f);
                }
                inPlace[ch] = m_v[ch];
            }
            else if constexpr (FilterCharacteristic == OnePoleFilterCharacteristic::HighPass)
            {
                const auto a0 = (1.0f + this->m_fdbk) * 0.5f;
                float out = a0 * (inPlace[ch] - m_x1[ch]) + this->m_fdbk * m_v[ch];
                m_x1[ch] = inPlace[ch];
                if constexpr (ClampValues)
                {
                    out = std::clamp(out, -1.f, 1.f);
                }
                m_v[ch] = out;
                inPlace[ch] = out;
            }
        }
    }

    void processBlock(float* inPlace, const size_t numSamples) noexcept
    {
        if (this->m_fdbk <= 1E-8f)
        {
            return;
        }
        for (size_t i = 0; i < numSamples; ++i)
        {
            step(inPlace + i * NumChannels);
        }
    }

    void processBlock(const float* in, float* out, const size_t numSamples) noexcept
    {
        if (this->m_fdbk == 0)
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

    void resetImpl() noexcept
    {
        m_v.fill(0.0f);
        m_x1.fill(0.0f);
    }

  private:
    std::array<float, NumChannels> m_v{};
    std::array<float, NumChannels> m_x1{};
};
}
