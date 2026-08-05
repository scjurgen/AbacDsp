#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <iomanip>
#include <iostream>
#include <numbers>
#include <tuple>

#include "Numbers/Convert.h"

namespace AbacDsp
{
/// @ingroup filters
/// @brief Biquad magnitude in dB at normalised frequency cf = f/fs, valid over [0, 0.5].
/// Evaluated through the phi = 4*sin^2(pi*cf) substitution, so no complex arithmetic is needed.
[[nodiscard]] inline float biquadMagnitudeInDb(const float cf, const float b0, const float b1, const float b2,
                                               const float a1, const float a2)
{
    const auto phi = 4 * std::pow(std::sin(2.f * std::numbers::pi_v<float> * cf / 2.f), 2.f);
    const auto db =
        10 * std::log10(std::pow((b0 + b1 + b2), 2.f) + (b0 * b2 * phi - (b1 * (b0 + b2) + 4 * b0 * b2)) * phi) -
        10 * std::log10(std::pow((1 + a1 + a2), 2.f) + (a2 * phi - (a1 * (1 + a2) + 4 * a2)) * phi);
    return db;
}

/// @ingroup filters
/// @brief Biquad magnitude as a linear ratio at normalised frequency cf = f/fs.
/// Accumulates in double, so it stays usable in the deep notches where the float dB path loses digits.
[[nodiscard]] inline float biquadMagnitudeLinear(const float cf, const float b0, const float b1, const float b2,
                                                 const float a1, const float a2)
{
    const auto phi = 4 * std::pow(std::sin(std::numbers::pi_v<double> * static_cast<double>(cf)), 2.0);

    const auto b_sum = static_cast<double>(b0 + b1 + b2);
    const auto numerator = b_sum * b_sum + (b0 * b2 * phi - (b1 * (b0 + b2) + 4 * b0 * b2)) * phi;

    const auto a_sum = static_cast<double>(1 + a1 + a2);
    const auto denominator = a_sum * a_sum + (a2 * phi - (a1 * (1 + a2) + 4 * a2)) * phi;

    return static_cast<float>(std::sqrt(numerator / denominator));
}

/// @ingroup filters
/// @brief Raises 10 to the power of biquadMagnitudeInDb().
/// That is 10^dB, not the 10^(dB/20) a dB-to-ratio conversion calls for; biquadMagnitudeLinear() is the ratio.
[[nodiscard]] inline float biquadMagnitude(const float cf, const float b0, const float b1, const float b2,
                                           const float a1, const float a2)
{
    return std::pow(10.0f, biquadMagnitudeInDb(cf, b0, b1, b2, a1, a2));
}


/**
 * @ingroup filters
 * @brief Response shape, fixed at compile time by Biquad and BiquadStereo.
 *
 * LowPass, HighPass, BandPass, Notch and Peak are designed from the prewarped
 * K = tan(pi*f/fs); AllPass, LoShelf and HiShelf use the cookbook alpha =
 * sin(w0)/(2Q) form instead. Both are bilinear designs, only the algebra differs.
 *
 * OnePole and FreeCoefficients are not designed at all: coefficients()
 * leaves them at pass-through and the coefficients must be set directly.
 */
enum class BiquadFilterType
{
    LowPass,
    HighPass,
    BandPass,
    Notch,
    Peak,
    LoShelf,
    HiShelf,
    AllPass,
    OnePole,
    FreeCoefficients,
};


/**
 * @ingroup filters
 * @brief Coefficient design and frequency-response evaluation for one biquad section.
 *
 * Coefficients are divided through by a0 at design time, so the difference
 * equation never divides. Frequency is clamped to [1 Hz, fs/2] before the
 * prewarp. Q is the analog Q, putting Butterworth at 1/sqrt(2). peakGain is in
 * dB and is read only by Peak, LoShelf and HiShelf.
 * @see https://webaudio.github.io/Audio-EQ-Cookbook/audio-eq-cookbook.html
 */
class BiquadCoefficients
{
  public:
    void coefficients(BiquadFilterType type, const float sampleRate, const float frequency, const float Q,
                      const float peakGain)
    {
        const auto f = std::clamp(frequency, 1.f, sampleRate / 2.f);
        const auto Fc = f / sampleRate;
        const auto K = std::tan(std::numbers::pi_v<float> * Fc);
        const auto kSquare = K * K;
        auto norm = 1 / (1 + K / Q + kSquare);
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wswitch-enum"
        switch (type)
        {
            case BiquadFilterType::AllPass:
            {
                const auto w0 = 2.f * std::numbers::pi_v<float> * f / sampleRate;
                const auto cosW0 = std::cos(w0);
                const auto alpha = std::sin(w0) / (2 * Q);
                const auto a0 = 1 + alpha;
                a2 = b0 = (1 - alpha) / a0;
                a1 = b1 = (-2 * cosW0) / a0;
                b2 = (1 + alpha) / a0;
                break;
            }
            case BiquadFilterType::LowPass:
                b0 = kSquare * norm;
                b1 = 2 * b0;
                b2 = b0;
                a1 = 2 * (kSquare - 1) * norm;
                a2 = (1 - K / Q + kSquare) * norm;
                break;
            case BiquadFilterType::HighPass:
                b0 = 1 * norm;
                b1 = -2 * b0;
                b2 = b0;
                a1 = 2 * (kSquare - 1) * norm;
                a2 = (1 - K / Q + kSquare) * norm;
                break;
            case BiquadFilterType::BandPass:
                b0 = K / Q * norm;
                b1 = 0;
                b2 = -b0;
                a1 = 2 * (kSquare - 1) * norm;
                a2 = (1 - K / Q + kSquare) * norm;
                break;
            case BiquadFilterType::Notch:
                b2 = b0 = (1 + kSquare) * norm;
                a1 = b1 = 2 * (kSquare - 1) * norm;
                a2 = (1 - K / Q + kSquare) * norm;
                break;
            case BiquadFilterType::Peak:
            {
                auto V1 = 1.f;
                auto V = std::pow(10.f, std::abs(peakGain) / 20.0f);
                if (peakGain < 0)
                {
                    std::swap(V, V1);
                }
                norm = 1 / (1 + V1 / Q * K + kSquare);
                b0 = (1 + V / Q * K + kSquare) * norm;
                b2 = (1 - V / Q * K + kSquare) * norm;
                a2 = (1 - V1 / Q * K + kSquare) * norm;
                a1 = b1 = 2 * (kSquare - 1) * norm;
            }
            break;
            case BiquadFilterType::LoShelf:
            {
                const auto v2 = std::pow(10.f, peakGain / 40.f);
                const auto v = std::sqrt(v2);
                const auto w0 = 2.f * std::numbers::pi_v<float> * f / sampleRate;
                const auto cosW0 = std::cos(w0);
                const auto alpha = std::sin(w0) / (2 * Q);
                const auto scale = (v2 + 1) + (v2 - 1) * cosW0 + 2 * v * alpha;

                a1 = (-2 * ((v2 - 1) + (v2 + 1) * cosW0)) / scale;
                a2 = ((v2 + 1) + (v2 - 1) * cosW0 - 2 * v * alpha) / scale;
                b0 = (v2 * ((v2 + 1) - (v2 - 1) * cosW0 + 2 * v * alpha)) / scale;
                b1 = (2 * v2 * ((v2 - 1) - (v2 + 1) * cosW0)) / scale;
                b2 = (v2 * ((v2 + 1) - (v2 - 1) * cosW0 - 2 * v * alpha)) / scale;
            }
            break;
            case BiquadFilterType::HiShelf:
            {
                const auto v2 = std::pow(10.f, peakGain / 40.f);
                const auto v = std::sqrt(v2);
                const auto w0 = 2.f * std::numbers::pi_v<float> * f / sampleRate;
                const auto cosW0 = std::cos(w0);
                const auto alpha = std::sin(w0) / (2 * Q);
                const auto scale = (v2 + 1) - (v2 - 1) * cosW0 + 2 * v * alpha;

                a1 = (2 * ((v2 - 1) - (v2 + 1) * cosW0)) / scale;
                a2 = ((v2 + 1) - (v2 - 1) * cosW0 - 2 * v * alpha) / scale;
                b0 = (v2 * ((v2 + 1) + (v2 - 1) * cosW0 + 2 * v * alpha)) / scale;
                b1 = (-2 * v2 * ((v2 - 1) + (v2 + 1) * cosW0)) / scale;
                b2 = (v2 * ((v2 + 1) + (v2 - 1) * cosW0 - 2 * v * alpha)) / scale;
            }
            break;
            default:
                b0 = 1.f; // just pass through: returns (b0 * inValue)
                b1 = 0.f;
                b2 = 0.f;
                a1 = 0.f;
                a2 = 0.f;
                break;
        }
#pragma GCC diagnostic pop
    }

    [[nodiscard]] float magnitudeInDb(const float cf) const
    {
        return biquadMagnitudeInDb(cf, b0, b1, b2, a1, a2);
    }

    [[nodiscard]] float magnitudeLinear(const float cf) const
    {
        return biquadMagnitudeLinear(cf, b0, b1, b2, a1, a2);
    }

    [[nodiscard]] float magnitude(const float cf) const
    {
        return std::pow(10.0f, magnitudeInDb(cf));
    }

  protected:
    float b0{1.f}, b1{0.f}, b2{0.f}, a1{0.f}, a2{0.f};
};


/**
 * @ingroup filters
 * @brief Mono biquad section in transposed Direct Form II.
 *
 * Two state words, and the form usually preferred for floating point; Direct
 * Form I is the safer choice when the arithmetic is fixed point.
 *
 * Each response gets its own step function so that the coefficient identities
 * the design produces collapse into fewer multiplies: BandPass has b1 = 0 and
 * b2 = -b0, Notch has b2 = b0 and a1 = b1, Peak and AllPass have a1 = b1. The
 * shelves and FreeCoefficients fall back to the generic step.
 * @see https://ccrma.stanford.edu/~jos/filters/Transposed_Direct_Forms.html
 */
template <BiquadFilterType type>
class Biquad : public BiquadCoefficients
{
  public:
    void setCoefficients(const float b0_, const float b1_, const float b2_, const float a1_, const float a2_)
    {
        b0 = b0_;
        b1 = b1_;
        b2 = b2_;
        a1 = a1_;
        a2 = a2_;
    }

    void computeCoefficients(const float sampleRate, const float frequency, const float Q, const float peakGain)
    {
        coefficients(type, sampleRate, frequency, Q, peakGain);
    }

    void processBlock(const float* in, float* outBuffer, size_t numSamples)
    {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wswitch-enum"
        switch (type)
        {
            case BiquadFilterType::AllPass:
                std::transform(in, in + numSamples, outBuffer, [this](const float v) { return singleStepAllPass(v); });
                break;
            case BiquadFilterType::LowPass:
                std::transform(in, in + numSamples, outBuffer, [this](const float v) { return singleStepLowPass(v); });
                break;
            case BiquadFilterType::HighPass:
                std::transform(in, in + numSamples, outBuffer, [this](const float v) { return singleStepHighPass(v); });
                break;
            case BiquadFilterType::BandPass:
                std::transform(in, in + numSamples, outBuffer, [this](const float v) { return singleStepBandPass(v); });
                break;
            case BiquadFilterType::Notch:
                std::transform(in, in + numSamples, outBuffer, [this](const float v) { return singleStepNotch(v); });
                break;
            case BiquadFilterType::Peak:
                std::transform(in, in + numSamples, outBuffer, [this](const float v) { return singleStepPeak(v); });
                break;
            default: // low and high shelf and free coefficients
                std::transform(in, in + numSamples, outBuffer, [this](const float v) { return singleStepGeneric(v); });
                break;
        }
#pragma GCC diagnostic pop
    }

    [[nodiscard]] float singleStepGeneric(const float in) noexcept
    {
        const auto out = in * b0 + m_z[0];
        m_z[0] = in * b1 + m_z[1] - a1 * out;
        m_z[1] = in * b2 - a2 * out;
        return out;
    }

    [[nodiscard]] float singleStepBandPass(const float in) noexcept
    {
        // b1 == 0
        // b2 == -b0 -> 3 mul
        const auto b0s = in * b0;
        const auto out = b0s + m_z[0];
        m_z[0] = m_z[1] - a1 * out;
        m_z[1] = -b0s - a2 * out;
        return out;
    }

    [[nodiscard]] float singleStepNotch(const float in) noexcept
    {
        // b2 = b0
        // a1 = b1 --> 3  mul
        const auto b0s = in * b0;
        const auto out = b0s + m_z[0];
        m_z[0] = b1 * (in - out) + m_z[1];
        m_z[1] = b0s - a2 * out;
        return out;
    }

    [[nodiscard]] float singleStepLowPass(const float in) noexcept
    {
        // b1 = -2 * b0 -> doesn't really help?
        // b2 = b0 -> 4 mul (maybe 3?)
        const auto b0s = in * b0;
        const auto out = b0s + m_z[0];
        m_z[0] = b0s * 2 + m_z[1] - a1 * out;
        m_z[1] = b0s - a2 * out;
        return out;
    }

    [[nodiscard]] float singleStepHighPass(const float in) noexcept
    {
        // b1 = -2 * b0 -> doesn't really help?
        // b2 = b0 -> 4 mul (maybe 3?)
        const auto b0s = in * b0;
        const auto out = b0s + m_z[0];
        m_z[0] = -2 * b0s + m_z[1] - a1 * out;
        m_z[1] = b0s - a2 * out;
        return out;
    }

    [[nodiscard]] float singleStepPeak(const float in) noexcept
    {
        const auto out = in * b0 + m_z[0];
        m_z[0] = b1 * (in - out) + m_z[1];
        m_z[1] = in * b2 - a2 * out;
        return out;
    }

    [[nodiscard]] float singleStepAllPass(const float in) noexcept
    {
        const auto out = in * b0 + m_z[0];
        m_z[0] = b1 * (in - out) + m_z[1];
        m_z[1] = in * b2 - b0 * out;
        return out;
    }

    void reset() noexcept
    {
        std::fill_n(m_z.data(), m_z.size(), 0.f);
    }

  private:
    std::array<float, 2> m_z{};
};

/**
 * @ingroup filters
 * @brief Two-channel biquad sharing one coefficient set, state kept per channel.
 *
 * Both channels advance within the same loop iteration, so the coefficients are
 * read once per frame instead of once per channel. BiquadFilterType::OnePole is
 * meaningful only here: it selects a first-order step that ignores b2 and a2.
 */
template <BiquadFilterType type>
class BiquadStereo : public BiquadCoefficients
{
  public:
    void computeCoefficients(const float sampleRate, const float frequency, const float Q, const float peakGain)
    {
        coefficients(type, sampleRate, frequency, Q, peakGain);
    }

    void processBlock(const float* left, const float* right, float* outLeft, float* outRight, size_t numSamples)
    {
        switch (type)
        {
            case BiquadFilterType::OnePole:
                for (size_t i = 0; i < numSamples; ++i)
                {
                    lastStepChebyshev(*left++, *right++, *outLeft++, *outRight++);
                }
                break;
            case BiquadFilterType::AllPass:
                for (size_t i = 0; i < numSamples; ++i)
                {
                    singleStepAllPass(*left++, *right++, *outLeft++, *outRight++);
                }
                break;
            case BiquadFilterType::LowPass:
                for (size_t i = 0; i < numSamples; ++i)
                {
                    singleStepLowPass(*left++, *right++, *outLeft++, *outRight++);
                }
                break;
            case BiquadFilterType::HighPass:
                for (size_t i = 0; i < numSamples; ++i)
                {
                    singleStepHighPass(*left++, *right++, *outLeft++, *outRight++);
                }
                break;
            case BiquadFilterType::BandPass:
                for (size_t i = 0; i < numSamples; ++i)
                {
                    singleStepBandPass(*left++, *right++, *outLeft++, *outRight++);
                }
                break;
            case BiquadFilterType::Notch:
                for (size_t i = 0; i < numSamples; ++i)
                {
                    singleStepNotch(*left++, *right++, *outLeft++, *outRight++);
                }
                break;
            case BiquadFilterType::Peak:
                for (size_t i = 0; i < numSamples; ++i)
                {
                    singleStepPeak(*left++, *right++, *outLeft++, *outRight++);
                }
                break;
            default:
                for (size_t i = 0; i < numSamples; ++i)
                {
                    singleStepGeneric(*left++, *right++, *outLeft++, *outRight++);
                }
                break;
        }
    }

  private:
    void lastStepChebyshev(const float inLeft, const float inRight, float& outLeft, float& outRight)
    {
        outLeft = inLeft * b0 + m_z[0][0];
        m_z[0][0] = inLeft * b1 - a1 * outLeft;
        outRight = inRight * b0 + m_z[1][0];
        m_z[1][0] = inRight * b1 - a1 * outRight;
    }

    void singleStepGeneric(const float inLeft, const float inRight, float& outLeft, float& outRight)
    {
        outLeft = inLeft * b0 + m_z[0][0];
        m_z[0][0] = inLeft * b1 + m_z[0][1] - a1 * outLeft;
        m_z[0][1] = inLeft * b2 - a2 * outLeft;
        outRight = inRight * b0 + m_z[1][0];
        m_z[1][0] = inRight * b1 + m_z[1][1] - a1 * outRight;
        m_z[1][1] = inRight * b2 - a2 * outRight;
    }

    void singleStepBandPass(const float inLeft, const float inRight, float& outLeft, float& outRight)
    {
        const auto b0s = inLeft * b0;
        outLeft = b0s + m_z[0][0];
        m_z[0][0] = m_z[0][1] - a1 * outLeft;
        m_z[0][1] = -b0s - a2 * outLeft;
        const auto b1s = inRight * b0;
        outRight = b1s + m_z[1][0];
        m_z[1][0] = m_z[1][1] - a1 * outRight;
        m_z[1][1] = -b1s - a2 * outRight;
    }

    void singleStepNotch(const float inLeft, const float inRight, float& outLeft, float& outRight)
    {
        const auto b0s = inLeft * b0;
        outLeft = b0s + m_z[0][0];
        m_z[0][0] = b1 * (inLeft - outLeft) + m_z[0][1];
        m_z[0][1] = b0s - a2 * outLeft;
        const auto b1s = inRight * b0;
        outRight = b1s + m_z[1][0];
        m_z[1][0] = b1 * (inRight - outRight) + m_z[1][1];
        m_z[1][1] = b1s - a2 * outRight;
    }

    void singleStepLowPass(const float inLeft, const float inRight, float& outLeft, float& outRight)
    {
        const auto b0s = inLeft * b0;
        outLeft = b0s + m_z[0][0];
        m_z[0][0] = b0s * 2 + m_z[0][1] - a1 * outLeft;
        m_z[0][1] = b0s - a2 * outLeft;
        const auto b1s = inRight * b0;
        outRight = b1s + m_z[1][0];
        m_z[1][0] = b1s * 2 + m_z[1][1] - a1 * outRight;
        m_z[1][1] = b1s - a2 * outRight;
    }

    void singleStepHighPass(const float inLeft, const float inRight, float& outLeft, float& outRight)
    {
        const auto b0s = inLeft * b0;
        outLeft = b0s + m_z[0][0];
        m_z[0][0] = -2 * b0s + m_z[0][1] - a1 * outLeft;
        m_z[0][1] = b0s - a2 * outLeft;
        const auto b1s = inRight * b0;
        outRight = b1s + m_z[1][0];
        m_z[1][0] = -2 * b1s + m_z[1][1] - a1 * outRight;
        m_z[1][1] = b1s - a2 * outRight;
    }

    void singleStepPeak(const float inLeft, const float inRight, float& outLeft, float& outRight)
    {
        outLeft = inLeft * b0 + m_z[0][0];
        m_z[0][0] = b1 * (inLeft - outLeft) + m_z[0][1];
        m_z[0][1] = inLeft * b2 - a2 * outLeft;
        outRight = inRight * b0 + m_z[1][0];
        m_z[1][0] = b1 * (inRight - outRight) + m_z[1][1];
        m_z[1][1] = inRight * b2 - a2 * outRight;
    }

    void singleStepAllPass(const float inLeft, const float inRight, float& outLeft, float& outRight)
    {
        outLeft = inLeft * b0 + m_z[0][0];
        m_z[0][0] = b1 * (inLeft - outLeft) + m_z[0][1];
        m_z[0][1] = inLeft * b2 - b0 * outLeft;
        outRight = inRight * b0 + m_z[1][0];
        m_z[1][0] = b1 * (inRight - outRight) + m_z[1][1];
        m_z[1][1] = inRight * b2 - b0 * outRight;
    }

    void reset() noexcept
    {
        for (auto& c : m_z)
        {
            std::fill_n(c.data(), c.size(), 0.f);
        }
    }

    std::array<std::array<float, 2>, 2> m_z{};
};

/**
 * @ingroup filters
 * @brief Chebyshev lowpass or highpass up to MAX_ORDER, as a cascade of biquad sections.
 *
 * computeType1() gives an equiripple passband and a monotonic stopband;
 * computeType2() flattens the passband and moves the ripple into the stopband,
 * placing finite zeros there. Ripple is in dB for both.
 *
 * Prototype poles sit on an ellipse whose eccentricity follows from the ripple,
 * and reach the z plane through the bilinear transform. An odd order leaves one
 * real pole over, which becomes a first-order section at the end of the cascade.
 * @see https://en.wikipedia.org/wiki/Chebyshev_filter
 */
class ChebyshevBiquad
{
  public:
    static constexpr auto MAX_ORDER = 12u;

    /// @brief One section's transfer function, already normalised by a0.
    struct Coefficients
    {
        float b0, b1, b2;
        float a1, a2;
    };

    explicit ChebyshevBiquad(const float sampleRate)
        : m_sampleRate(sampleRate)
    {
    }

    void setSampleRate(const float sampleRate)
    {
        m_sampleRate = sampleRate;
    }

    void assignToBiquads()
    {
        for (size_t i = 0; i < m_elements; ++i)
        {
            for (size_t c = 0; c < 2; c++)
            {
                m_biquads[c][i].setCoefficients(coefficients[i].b0, coefficients[i].b1, coefficients[i].b2,
                                                coefficients[i].a1, coefficients[i].a2);
            }
        }
    }

    /// @brief fC is the prewarped cutoff tan(pi*f/fs), beta the cos(w0) constant of the
    /// lowpass-to-highpass transform, a the ripple-derived asinh(1/eps)/order that sets the pole ellipse.
    struct InitialFactors
    {
        float fC, beta, a;
    };

    [[nodiscard]] InitialFactors computeFactors(const size_t order, const float fc, const float ripple)
    {
        m_elements = (order + 1) / 2;
        m_order = order;
        m_ripple = ripple;
        m_fc = fc;
        const auto fNorm = fc / m_sampleRate;
        const auto rippleLimited = std::max(0.001f, ripple);
        const auto eps = std::sqrt(std::pow(10.0f, rippleLimited / 10.0f) - 1);

        return InitialFactors{std::tan(std::numbers::pi_v<float> * fNorm),
                              std::cos(fNorm * 2.f * std::numbers::pi_v<float>),
                              std::log(1.f / eps + std::sqrt(1.f / (eps * eps) + 1)) / static_cast<float>(order)};
    }

    void computeType1(const size_t order, const float fc, const float ripple, const bool isLowPass)
    {
        m_isLowPass = isLowPass;
        m_isType1 = true;
        const auto [fC_, beta_, a_] = computeFactors(order, fc, ripple);

        auto calcPoleZero =
            [&](const std::complex<float> fSPole, const bool lowPass, const float beta, const bool isOdd)
        {
            auto fZPole = BilinearTransform(fSPole);
            std::complex<float> fZZero;
            float fDCPoleDistance;
            if (lowPass)
            {
                fZZero = {-1, 0};
                fDCPoleDistance = isOdd ? std::sqrt(std::norm(std::complex<float>{1, 0} - fZPole)) / 2
                                        : std::norm(std::complex<float>{1, 0} - fZPole) / 4;
            }
            else
            {
                fZPole = (std::complex{beta - fZPole.real(), -fZPole.imag()}) /
                         (std::complex{1 - beta * fZPole.real(), -beta * fZPole.imag()});
                fZZero = {1, 0};
                fDCPoleDistance = isOdd ? std::sqrt(std::norm(std::complex<float>{-1, 0} - fZPole)) / 2
                                        : std::norm(std::complex<float>{-1, 0} - fZPole) / 4;
            }
            return std::make_tuple(fZPole, fZZero, fDCPoleDistance);
        };

        for (auto iPair = 0u; iPair < order / 2; iPair++)
        {
            const auto f =
                static_cast<float>(2 * iPair + 1) * std::numbers::pi_v<float> / static_cast<float>(2 * order);
            const std::complex fSPole{-fC_ * std::sinh(a_) * std::sin(f), fC_ * std::cosh(a_) * std::cos(f)};
            auto [fZPole, fZZero, fDCPoleDistance] = calcPoleZero(fSPole, isLowPass, beta_, false);
            coefficients[iPair].b0 = fDCPoleDistance;
            coefficients[iPair].b1 = -2 * fZZero.real() * fDCPoleDistance;
            coefficients[iPair].b2 = fDCPoleDistance;
            coefficients[iPair].a1 = -2 * fZPole.real();
            coefficients[iPair].a2 = std::norm(fZPole);
        }

        if ((order & 1) == 0)
        {
            m_isOdd = false;
            const auto fTemp = Convert::dbToGain(-std::max(0.00001f, ripple));
            coefficients[0].b0 *= fTemp;
            coefficients[0].b1 *= fTemp;
            coefficients[0].b2 *= fTemp;
        }
        else
        {
            m_isOdd = true;
            const std::complex fSPole{-fC_ * std::sinh(a_), 0.f};
            auto [fZPole, fZZero, fDCPoleDistance] = calcPoleZero(fSPole, isLowPass, beta_, true);
            coefficients[m_elements - 1].b0 = fDCPoleDistance;
            coefficients[m_elements - 1].b1 = -fZZero.real() * fDCPoleDistance;
            coefficients[m_elements - 1].b2 = 0;
            coefficients[m_elements - 1].a1 = -fZPole.real();
            coefficients[m_elements - 1].a2 = 0;
        }
        assignToBiquads();
    }

    void computeType2(const size_t order, const float fc, const float ripple, const bool isLowPass)
    {
        m_isLowPass = isLowPass;
        m_isType1 = false;
        const auto [fC, beta, a] = computeFactors(order, fc, ripple);

        for (unsigned iPair = 0; iPair < static_cast<unsigned>(order / 2); iPair++)
        {
            auto f = static_cast<float>(2 * iPair + 1) * std::numbers::pi_v<float> / static_cast<float>(2 * order);
            std::complex<float> z1{fC, 0};
            std::complex<float> z2{-std::sinh(a) * std::sin(f), std::cosh(a) * std::cos(f)};
            const auto fSPole = z1 / z2;
            auto fZPole = BilinearTransform(fSPole);
            const auto fSZero =
                std::complex<float>(0, fC / std::cos(((2 * iPair) + 1) * std::numbers::pi_v<float> / (2 * order)));
            auto fZZero = BilinearTransform(fSZero);
            float fDCPoleDistance;
            if (isLowPass)
            {
                fDCPoleDistance =
                    std::norm(std::complex<float>{1, 0} - fZPole) / std::norm(std::complex<float>{1, 0} - fZZero);
            }
            else
            {
                fZPole = std::complex<float>(beta - fZPole.real(), -fZPole.imag()) /
                         std::complex<float>(1 - beta * fZPole.real(), -beta * fZPole.imag());
                fZZero = std::complex<float>(beta - fZZero.real(), -fZZero.imag()) /
                         std::complex<float>(1 - beta * fZZero.real(), -beta * fZZero.imag());
                fDCPoleDistance =
                    std::norm(std::complex<float>(-1, 0) - fZPole) / std::norm(std::complex<float>(-1, 0) - fZZero);
            }
            coefficients[iPair].b0 = fDCPoleDistance;
            coefficients[iPair].b1 = -2 * fZZero.real() * fDCPoleDistance;
            coefficients[iPair].b2 = std::norm(fZZero) * fDCPoleDistance;
            coefficients[iPair].a1 = -2 * fZPole.real();
            coefficients[iPair].a2 = std::norm(fZPole);
        }
        if (order & 1)
        {
            m_isOdd = true;
            const auto iiPair = (static_cast<int>(order) - 1) / 2;
            const auto iPair = static_cast<float>(iiPair);
            auto fSPole = std::complex<float>(fC, 0) /
                          std::complex<float>(-std::sinh(a) * std::sin((2 * iPair + 1) * std::numbers::pi_v<float> /
                                                                       static_cast<float>(2 * order)),
                                              std::cosh(a) * std::cos((2 * iPair + 1) * std::numbers::pi_v<float> /
                                                                      static_cast<float>(2 * order)));
            auto fZPole = BilinearTransform(fSPole);
            float fZZeroReal;
            float fDCPoleDistance;
            if (isLowPass)
            {
                fDCPoleDistance = std::sqrt(std::norm(std::complex<float>(1, 0) - fZPole)) / 2;
                fZZeroReal = -1.f;
            }
            else
            {
                fZPole = std::complex<float>(beta - fZPole.real(), -fZPole.imag()) /
                         std::complex<float>(1 - beta * fZPole.real(), -fZPole.imag());
                fZZeroReal = 1.f;
                fDCPoleDistance = std::sqrt(std::norm(std::complex<float>(-1, 0) - fZPole)) / 2;
            }
            // this could be optimized in the single step because fZZeroReal = 1 or -1 -> b1 = +-b0
            coefficients[m_elements - 1].b0 = fDCPoleDistance;
            coefficients[m_elements - 1].b1 = -fZZeroReal * fDCPoleDistance;
            coefficients[m_elements - 1].b2 = 0;
            coefficients[m_elements - 1].a1 = -fZZeroReal;
            coefficients[m_elements - 1].a2 = 0;
        }
        else
        {
            m_isOdd = false;
        }
        assignToBiquads();
    }

    [[nodiscard]] Coefficients getCoefficients(const size_t index) const noexcept
    {
        return coefficients[index];
    }

    [[nodiscard]] size_t elements() const noexcept
    {
        return m_elements;
    }


    // 3 x mul
    [[nodiscard]] float stepChebyType1(const float in, const float b0, const float a1, const float a2, float& z0,
                                       float& z1) noexcept
    {
        const auto t = in * b0;
        const auto out = t + z0;
        z0 = z1 - a1 * (t + out);
        z1 = t - a2 * out;
        return out;
    }

    // 4 x mul
    [[nodiscard]] float stepChebyType2(const float in, const float b0, const float b1, const float a1, const float a2,
                                       float& z0, float& z1) noexcept
    {
        const auto t = in * b0;
        const auto out = t + z0;
        z0 = z1 + in * b1 - a1 * out;
        z1 = t - a2 * out;
        return out;
    }

    [[nodiscard]] float step1stOrder(const float in, const float b0, const float b1, const float a1, float& z0) noexcept
    {
        const auto t = in * b0;
        const auto out = t + z0;
        z0 = in * b1 - a1 * out;
        return out;
    }

    void process1stOrder(const size_t index, const float* in, float* out, const size_t numSamples)
    {
        for (size_t i = 0; i < numSamples; ++i)
        {
            out[i] = step1stOrder(in[i], coefficients[index].b0, coefficients[index].b1, coefficients[index].a1,
                                  z[index][0]);
        }
    }

    void process1stOrderStereo(const size_t index, const float* left, const float* right, float* outLeft,
                               float* outRight, const size_t numSamples)
    {
        for (size_t i = 0; i < numSamples; ++i)
        {
            outLeft[i] = step1stOrder(left[i], coefficients[index].b0, coefficients[index].b1, coefficients[index].a1,
                                      z[index][0]);
            outRight[i] = step1stOrder(right[i], coefficients[index].b0, coefficients[index].b1, coefficients[index].a1,
                                       z[index][2]);
        }
    }

    void processElement(const size_t index, const float* in, float* out, const size_t numSamples)
    {
        for (size_t i = 0; i < numSamples; ++i)
        {
            out[i] = stepChebyType2(in[i], coefficients[index].b0, coefficients[index].b1, coefficients[index].a1,
                                    coefficients[index].a2, z[index][0], z[index][1]);
        }
    }

    void processElementStereo(const size_t index, const float* left, const float* right, float* outLeft,
                              float* outRight, const size_t numSamples)
    {
        for (size_t i = 0; i < numSamples; ++i)
        {
            outLeft[i] = stepChebyType2(left[i], coefficients[index].b0, coefficients[index].b1, coefficients[index].a1,
                                        coefficients[index].a2, z[index][0], z[index][1]);
            outRight[i] = stepChebyType2(right[i], coefficients[index].b0, coefficients[index].b1,
                                         coefficients[index].a1, coefficients[index].a2, z[index][2], z[index][3]);
        }
    }

    void processBlock(const float* in, float* out, const size_t numSamples)
    {
        if (m_isOdd)
        {
            if (m_elements == 1)
            {
                process1stOrder(0, in, out, numSamples);
                return;
            }
            else
            {
                processElement(0, in, out, numSamples);
                for (size_t i = 1; i < m_elements - 1; ++i)
                {
                    processElement(i, out, out, numSamples);
                }
                process1stOrder(m_elements - 1, out, out, numSamples);
            }
        }
        else
        {
            processElement(0, in, out, numSamples);
            for (size_t i = 1; i < m_elements; ++i)
            {
                processElement(i, out, out, numSamples);
            }
        }
    }

    void processBlockStereo(const float* left, const float* right, float* leftOut, float* rightOut,
                            const size_t numSamples)
    {
        if (m_isOdd)
        {
            if (m_elements == 1 && m_isOdd)
            {
                process1stOrderStereo(0, left, right, leftOut, rightOut, numSamples);
                return;
            }
            processElementStereo(0, left, right, leftOut, rightOut, numSamples);
            for (size_t i = 1; i < m_elements - 1; ++i)
            {
                processElementStereo(i, leftOut, rightOut, leftOut, rightOut, numSamples);
            }
            process1stOrderStereo(m_elements - 1, leftOut, rightOut, leftOut, rightOut, numSamples);
            return;
        }
        processElementStereo(0, left, right, leftOut, rightOut, numSamples);
        for (size_t i = 1; i < m_elements; ++i)
        {
            processElementStereo(i, leftOut, rightOut, leftOut, rightOut, numSamples);
        }
    }

    [[nodiscard]] float getMagnitudeInDb(const float cf) const
    {
        float sum = 0;
        for (size_t i = 0; i < m_elements; ++i)
        {
            sum += m_biquads[0][i].magnitudeInDb(cf / m_sampleRate);
        }
        return sum;
    }

    void printCoefficients() const
    {
        std::cout << "// fc=" << m_fc << " ripple=" << m_ripple << " " << (m_isLowPass ? "low pass" : "high pass")
                  << std::endl;
        std::cout << "std::array<std::array<float, 5>, " << m_elements << "> coefficients = {{\n";
        for (size_t i = 0; i < m_elements; ++i)
        {
            std::cout << "\t{" << std::setprecision(14) << coefficients[i].b0 << "f, " << coefficients[i].b1 << "f, "
                      << coefficients[i].b2 << "f, " << coefficients[i].a1 << "f, " << coefficients[i].a2 << "f},\n";
        }
        std::cout << "}};" << std::endl;
    }

  private:
    static std::complex<float> BilinearTransform(const std::complex<float> fS) noexcept
    {
        const float fDenominator = std::norm(std::complex<float>{1, 0} - fS);
        return {(1 - fS.real() * fS.real() - fS.imag() * fS.imag()) / fDenominator, 2 * fS.imag() / fDenominator};
    }

    float m_sampleRate{48000.f};
    float m_ripple{6};
    float m_fc{0.1f};
    size_t m_order{0};
    size_t m_elements{0};

    bool m_isLowPass{false};
    bool m_isOdd{false};
    bool m_isType1{false};

    std::array<Coefficients, MAX_ORDER> coefficients{};
    std::array<std::array<float, 4>, MAX_ORDER> z{}; // 2*2 for stereo processing
    std::array<std::array<Biquad<BiquadFilterType::FreeCoefficients>, MAX_ORDER / 2 + 1>, 2> m_biquads;
    std::array<Biquad<BiquadFilterType::FreeCoefficients>, 2> m_biquadSinglePole;
};

/**
 * @ingroup filters
 * @brief Standalone peaking EQ section, coefficients recomputed on demand.
 *
 * Boost and cut swap which side of the design carries the gain factor V, so a
 * cut of -x dB inverts a boost of +x dB exactly. Q is floored at 0.01 to keep
 * the design finite. Transposed Direct Form II again, but with the output held
 * in the state array, hence three floats where two carry actual state.
 * @see https://webaudio.github.io/Audio-EQ-Cookbook/audio-eq-cookbook.html
 */
class PeakBiquad
{
  public:
    explicit PeakBiquad(const float sampleRate)
        : m_sampleRate(sampleRate)
    {
    }

    void computeCoefficients(const float frequency, const float peakGain, float Q) noexcept
    {
        const auto Fc = frequency / m_sampleRate;
        const auto V = std::pow(10.f, std::abs(peakGain) / 20.0f);
        const auto K = std::tan(std::numbers::pi_v<float> * Fc);
        const auto KSquare = K * K;
        if (Q <= 0)
        {
            Q = 0.01f;
        }
        if (peakGain >= 0)
        {
            const auto norm = 1 / (1 + 1 / Q * K + KSquare);
            m_b0 = (1 + V / Q * K + KSquare) * norm;
            m_b2 = (1 - V / Q * K + KSquare) * norm;
            m_a2 = (1 - 1 / Q * K + KSquare) * norm;
            m_b1 = 2 * (KSquare - 1) * norm;
        }
        else
        {
            const auto norm = 1 / (1 + V / Q * K + KSquare);
            m_b0 = (1 + 1 / Q * K + KSquare) * norm;
            m_b2 = (1 - 1 / Q * K + KSquare) * norm;
            m_a2 = (1 - V / Q * K + KSquare) * norm;
            m_b1 = 2 * (KSquare - 1) * norm;
        }
    }

    [[nodiscard]] float step(const float in) noexcept
    {
        m_z[2] = in * m_b0 + m_z[1];
        m_z[1] = m_b1 * (in - m_z[2]) + m_z[0];
        m_z[0] = in * m_b2 - m_a2 * m_z[2];
        return m_z[2];
    }

    void processBlock(const float* in, float* out, const size_t numSamples)
    {
        std::transform(in, in + numSamples, out, [this](const float v) { return step(v); });
    }

  private:
    const float m_sampleRate;
    float m_b0{1.f}, m_b1{0.f}, m_b2{0.f}, m_a2{0.f};
    std::array<float, 3> m_z{};
};

}