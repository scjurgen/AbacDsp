#pragma once

#include "Filters/Biquad.h"

namespace AbacDsp
{
/**
 * @ingroup filters
 * @brief Double-precision twin of the BiquadCoefficients designs, filter type chosen at runtime.
 *
 * Designs coefficients only: there is no step or processBlock. The algebra
 * mirrors BiquadCoefficients rather than being derived independently, so a
 * disagreement between the two points at float round-off, not at the design.
 *
 * calculateCoefficients() throws std::invalid_argument for OnePole and
 * FreeCoefficients, which have no design to run.
 */
class BiquadReference
{
  public:
    BiquadReference(const double sampleRate, const BiquadFilterType type)
        : m_sampleRate(sampleRate)
        , m_type(type)
    {
    }

    void calculateCoefficients(const double f, const double q, const double gain)
    {
        switch (m_type)
        {
            case BiquadFilterType::AllPass:
                allpass(f, q);
                break;
            case BiquadFilterType::LowPass:
                lowpass(f, q);
                break;
            case BiquadFilterType::BandPass:
                bandpass(f, q);
                break;
            case BiquadFilterType::HighPass:
                highpass(f, q);
                break;
            case BiquadFilterType::Peak:
                peak(f, q, gain);
                break;
            case BiquadFilterType::Notch:
                notch(f, q);
                break;
            case BiquadFilterType::LoShelf:
                loshelf(f, q, gain);
                break;
            case BiquadFilterType::HiShelf:
                hishelf(f, q, gain);
                break;
            default:
                throw(std::invalid_argument("non valid filter type for testing"));
        }
    }

    void signalGain(const double gain) noexcept
    {
        b0 = gain;
        b1 = b2 = a1 = a2 = 0;
    }

    void peak(const double f, const double q, const double gain) noexcept
    {
        const auto fCutoff = f / m_sampleRate;
        const auto V = std::pow(10, std::abs(gain) / 20.0);
        const auto K = std::tan(std::numbers::pi_v<double> * fCutoff);
        const auto kSquare = K * K;
        double norm{};
        if (gain >= 0)
        {
            norm = 1 / (1 + 1 / q * K + kSquare);
            b0 = (1 + V / q * K + kSquare) * norm;
            b2 = (1 - V / q * K + kSquare) * norm;
            a2 = (1 - 1 / q * K + kSquare) * norm;
        }
        else
        {
            norm = 1 / (1 + V / q * K + kSquare);
            b0 = (1 + 1 / q * K + kSquare) * norm;
            b2 = (1 - 1 / q * K + kSquare) * norm;
            a2 = (1 - V / q * K + kSquare) * norm;
        }
        a1 = b1 = 2 * (kSquare - 1) * norm;
    }

    void highpass(const double f, const double q) noexcept
    {
        const auto fCutoff = f / m_sampleRate;
        const auto K = std::tan(std::numbers::pi_v<double> * fCutoff);
        const auto kSquare = K * K;
        const auto norm = 1 / (1 + K / q + kSquare);
        b0 = 1 * norm;
        b1 = -2 * b0;
        b2 = b0;
        a1 = 2 * (kSquare - 1) * norm;
        a2 = (1 - K / q + kSquare) * norm;
    }

    void lowpass(const double f, const double q) noexcept
    {
        const auto fCutoff = f / m_sampleRate;
        const auto K = std::tan(std::numbers::pi_v<double> * fCutoff);
        const auto kSquare = K * K;
        const auto norm = 1 / (1 + K / q + kSquare);
        b0 = kSquare * norm;
        b1 = 2 * b0;
        b2 = b0;
        a1 = 2 * (kSquare - 1) * norm;
        a2 = (1 - K / q + kSquare) * norm;
    }

    void bandpass(const double f, const double q) noexcept
    {
        const auto fCutoff = f / m_sampleRate;
        const auto K = std::tan(std::numbers::pi_v<double> * fCutoff);
        const auto kSquare = K * K;
        const auto norm = 1 / (1 + K / q + kSquare);
        b0 = K / q * norm;
        b1 = 0;
        b2 = -b0;
        a1 = 2 * (kSquare - 1) * norm;
        a2 = (1 - K / q + kSquare) * norm;
    }

    void notch(const double f, const double q) noexcept
    {
        const auto fCutoff = f / m_sampleRate;
        const auto K = std::tan(std::numbers::pi_v<double> * fCutoff);
        const auto kSquare = K * K;
        const auto norm = 1 / (1 + K / q + kSquare);
        b2 = b0 = (1 + kSquare) * norm;
        a1 = b1 = 2 * (kSquare - 1) * norm;
        a2 = (1 - K / q + kSquare) * norm;
    }

    void loshelf(const double f, const double q, const double gain) noexcept
    {
        const auto v2 = std::pow(10, (gain / 40));
        const auto w0 = 2 * std::numbers::pi_v<double> * f / m_sampleRate;
        const auto alpha = std::sin(w0) / (2 * q);
        const auto scale = (v2 + 1) + (v2 - 1) * std::cos(w0) + 2 * std::sqrt(v2) * alpha;

        a1 = (-2 * ((v2 - 1) + (v2 + 1) * std::cos(w0))) / scale;
        a2 = ((v2 + 1) + (v2 - 1) * std::cos(w0) - 2 * std::sqrt(v2) * alpha) / scale;
        b0 = (v2 * ((v2 + 1) - (v2 - 1) * std::cos(w0) + 2 * std::sqrt(v2) * alpha)) / scale;
        b1 = (2 * v2 * ((v2 - 1) - (v2 + 1) * std::cos(w0))) / scale;
        b2 = (v2 * ((v2 + 1) - (v2 - 1) * std::cos(w0) - 2 * std::sqrt(v2) * alpha)) / scale;
    }

    void hishelf(const double f, const double q, const double gain) noexcept
    {
        const auto v2 = std::pow(10, (gain / 40));
        const auto w0 = 2 * std::numbers::pi_v<double> * f / m_sampleRate;
        const auto alpha = std::sin(w0) / (2 * q);
        const auto scale = (v2 + 1) - (v2 - 1) * std::cos(w0) + 2 * std::sqrt(v2) * alpha;

        a1 = (2 * ((v2 - 1) - (v2 + 1) * std::cos(w0))) / scale;
        a2 = ((v2 + 1) - (v2 - 1) * std::cos(w0) - 2 * std::sqrt(v2) * alpha) / scale;
        b0 = (v2 * ((v2 + 1) + (v2 - 1) * std::cos(w0) + 2 * std::sqrt(v2) * alpha)) / scale;
        b1 = (-2 * v2 * ((v2 - 1) + (v2 + 1) * std::cos(w0))) / scale;
        b2 = (v2 * ((v2 + 1) + (v2 - 1) * std::cos(w0) - 2 * std::sqrt(v2) * alpha)) / scale;
    }

    void allpass(const double f, const double q) noexcept
    {
        const auto w0 = 2 * std::numbers::pi_v<double> * f / m_sampleRate;
        const auto cosW0 = std::cos(w0);
        const auto alpha = std::sin(w0) / (2 * q);
        const auto a0 = 1 + alpha;

        a2 = b0 = (1 - alpha) / a0;
        a1 = b1 = (-2 * cosW0) / a0;
        b2 = (1 + alpha) / a0;
    }

    /// @brief Response at hz, returned in decibels despite the name.
    /// Argument is hertz, not the normalised f/fs the free functions in Biquad.h take.
    [[nodiscard]] double magnitude(const double hz) const noexcept
    {
        const auto phi = 4 * std::pow(std::sin(2 * std::numbers::pi_v<double> * hz / m_sampleRate / 2), 2);
        const auto db =
            10 * std::log10(std::pow((b0 + b1 + b2), 2) + (b0 * b2 * phi - (b1 * (b0 + b2) + 4 * b0 * b2)) * phi) -
            10 * std::log10(std::pow((1 + a1 + a2), 2) + (a2 * phi - (a1 * (1 + a2) + 4 * a2)) * phi);
        return db;
    }

    void getCoefficients(double& a1_, double& a2_, double& b0_, double& b1_, double& b2_) const noexcept
    {
        a1_ = a1;
        a2_ = a2;
        b0_ = b0;
        b1_ = b1;
        b2_ = b2;
    }

  private:
    const double m_sampleRate;
    const BiquadFilterType m_type;
    double a1{0}, a2{0}, b0{0}, b1{0}, b2{0};
};
}