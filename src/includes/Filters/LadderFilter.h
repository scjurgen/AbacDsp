#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <functional>
#include <iostream>
#include <numbers>

namespace AbacDsp
{
struct PoleMixingList
{
    std::string_view name;
    std::array<float, 5> cf;
};

static std::vector<PoleMixingList> poleMixingList = {
    {"LP1", {0, -1, 0, 0, 0}},
    {"LP2", {0, 0, 1, 0, 0}},
    {"LP3", {0, 0, 0, -1, 0}},
    {"LP4", {0, 0, 0, 0, 1}},
    {"HP1 + LP3", {0, 0, 0, -3, 3}},
    {"HP1 + LP2", {0, 0, 3, -3, 0}},
    {"BP2", {0, -2, 2, 0, 0}},
    {"BP4", {0, 0, 2, -4, 2}},
    {"HP1", {1, -1, 0, 0, 0}},
    {"HP2", {1, -2, 1, 0, 0}},
    {"HP3", {1, -3, 3, -1, 0}},
    {"HP4", {1, -4, 6, -4, 1}},
    {"AP1", {1, -2, 0, 0, 0}},
    {"AP2", {1, -4, 4, 0, 0}},
    {"AP3", {1, -6, 12, -8, 0}},
    {"AP4", {1, -8, 24, -32, 16}},
    {"HP3 + LP1", {0, -2, 6, -6, 2}},
    {"HP2 + LP1", {0, -1.5, 3, -1.5, 0}},
    {"BP Alt.", {0, -3, 6, -4, 1}},
    {"Notch", {1, -2, 2, 0, 0}},
    {"Res Before Notch", {0.3, -0.6, 1.5, 0, 0}},
    {"Res After Notch", {1.5, -3, 2, 0, 0}},
    {"Notch B", {1, -3, 6, -4, 1}},
    {"LP1 + Notch A", {0, -1, 2, -2, 0}},
    {"LP1 + Notch B (octave down)", {0, -4, 8, -5, 0}},
    {"LP1 + Notch C", {0, -3, 6, -4, 0}},
    {"LP2 + Notch A", {0, 0, 1, -2, 2}},
    {"LP2 + Notch B (octave down)", {0, 0, 4, -8, 5}},
    {"LP2 + Notch C", {0, 0, 3, -6, 4}},
    {"LP2 + Notch D (Brickwall)", {0, 0, 0.3, -0.6, 1.5}},
    {"LP1 Notch B", {0, -2, 4, -3, 0}},
    {"BP Notch Offset Left", {0, -3, 9, -10, 4}},
    {"BP Notch", {0, -4, 12, -16, 8}},
    {"BP Notch Offset Right", {0, -1, 3, -6, 4}},
    {"BP Notch Octave Up", {0, -1, 3, -7, 5}},
    {"HP + Notch A", {1, -3, 4, -2, 0}},
    {"HP + Notch B", {1, -3, 6, -4, 0}},
    {"HP + Notch C (Octave)", {1, -3, 7, -5, 0}},
    {"HP + Notch D (3rd)", {0.5, -1.5, 6, -5, 0}},
    {"HP2 + Notch A", {1, -4, 7, -6, 2}},
    {"HP2 + Notch B", {1, -4, 8, -8, 3}},
    {"HP2 + Notch C", {1, -4, 9, -10, 4}},
    {"HP2 + Notch D", {1, -4, 10, -12, 5}},
    {"Double Notch A (almost Octaves)", {1, -4, 10, -12, 6}},
    {"Double Notch Octaves", {1, -4, 10.25, -12.5, 6.25}},
    {"Double Notch B", {1, -4, 11, -14, 7}},
    {"Double Notch C (Dixon)", {1, -4, 12, -16, 8}},
    {"Double Notch D", {1, -4, 14, -20, 10}},
    {"Double Notch (approx. 3rd Harmonic)", {1, -4, 15, -22, 11}},

    {"20db LP shelf", {0.1, -0.6, 1.1, -2.8, 3.2}},
};

size_t findFilterIndex(std::string_view target)
{
    auto it = std::ranges::find_if(poleMixingList, [target](const PoleMixingList& pm) { return pm.name == target; });
    if (it != poleMixingList.end())
        return static_cast<size_t>(std::distance(poleMixingList.begin(), it));
    throw std::out_of_range("PoleMixingList name not found");
}

class ResonanceFrequencyModifier
{
  public:
    explicit ResonanceFrequencyModifier(const float sampleRate) noexcept
        : m_sampleRate(sampleRate)
        , m_frequency(1000.f)
        , m_resonance(0.f)
        , m_userResonance(0.0f)

    {
        updateResonance();
    }

    [[nodiscard]] float getCurrentFrequency() const noexcept
    {
        return m_frequency;
    }

    [[nodiscard]] float getCurrentResonance() const noexcept
    {
        return m_resonance;
    }

    [[nodiscard]] float getUserResonance() const noexcept
    {
        return m_userResonance;
    }

    void setUserResonance(const float userResonance) noexcept
    {
        m_userResonance = userResonance;
        updateResonance();
    }

    void setFrequency(const float newFrequency) noexcept
    {
        m_frequency = newFrequency;
        updateResonance();
    }

  private:
    void updateResonance() noexcept
    {
        m_resonance = m_frequency > m_sampleRate * 0.125f ? m_userResonance * m_sampleRate * 0.125f / m_frequency
                                                          : m_userResonance;
    }

    float m_sampleRate;
    float m_frequency;
    float m_resonance;
    float m_userResonance;
};


/*
 * this class calculates the magnitude and phase response of the filter as theoretical values
 * the math for it is found here: https://expeditionelectronics.com/Diy/Polemixing/math
 * this class can be used to generate a visual plot
 */
template <std::floating_point T>
class FourStageFilterTheoretical
{
  public:
    explicit FourStageFilterTheoretical(const float sampleRate, const std::array<T, 5>& coefficients)
        : m_sampleRate(sampleRate)
    {
        setCoefficients(coefficients);
    }

    void setCoefficients(const std::array<T, 5>& coefficients)
    {
        m_coefficients = coefficients;
        const auto a{coefficients[0]};
        const auto b{-coefficients[1]};
        const auto c{coefficients[2]};
        const auto d{-coefficients[3]};
        const auto e{coefficients[4]};
        x4 = a;
        x3 = 4 * a - b;
        x2 = 6 * a - 3 * b + c;
        x1 = 4 * a - 3 * b + 2 * c - d;
        x0 = a - b + c - d + e;
    }

    [[nodiscard]] T magnitude(T cutoff, T testFrequency, T resonance) const
    {
        const T w = testFrequency / cutoff; // normalized to cutoff
        // |G| = |N| / |D|
        const auto nReal = x0 + x4 * w * w * w * w - x2 * w * w;
        const auto nImg = -x1 * w + x3 * w * w * w;
        const auto nMag = sqrt(nReal * nReal + nImg * nImg);

        const auto dReal = 1.0 - 6.0 * w * w + w * w * w * w + resonance;
        const auto dImg = -4.0 * w + 4 * w * w * w;
        const auto dMag = sqrt(dReal * dReal + dImg * dImg);

        return nMag / dMag;
    }

    [[maybe_unused]] void phase(const T w, const T resonance, T& phase) const
    {
        // |G| = |N| / |D|
        // P = Pn - Pd
        const auto nReal = x0 + x4 * w * w * w * w - x2 * w * w;
        const auto nImg = -x1 * w + x3 * w * w * w;
        const auto nPhase = calcPhase(nImg, nReal);

        const auto dReal = 1.0 - 6.0 * w * w + w * w * w * w + resonance;
        const auto dImg = -4.0 * w + 4 * w * w * w;
        const auto dPhase = calcPhase(dImg, dReal);

        auto phaseValue = nPhase - dPhase;
        while (phaseValue > std::numbers::pi)
        {
            phaseValue -= 2 * std::numbers::pi;
        }
        while (phaseValue < -std::numbers::pi)
        {
            phaseValue += 2 * std::numbers::pi;
        }
        phase = phaseValue;
    }

    static T calcPhase(const T img, const T real)
    {
        return real > 0 ? -atan(img / real) : -(std::numbers::pi - std::atan(img / std::abs(real)));
    }

    T magnitudeBP(T cutoff, T testFrequency, T resonance) const
    {
        const T pole = std::exp(-T(2) * std::numbers::pi_v<T> * cutoff / m_sampleRate);
        const T w = T(2) * std::numbers::pi_v<T> * testFrequency / m_sampleRate;
        auto y = stage_outputs(pole, w);
        auto band = bandpass(y);
        auto num = numerator(m_coefficients, y);
        auto denom = T(1) + resonance * band;
        return std::abs(num / denom);
    }

    T magnitudeBP2(T pole, T w, T resonance) const
    {
        // w in radians (0...pi) at 48kHz
        // m_pole: (your filter pole coefficient)
        std::array<std::complex<T>, 5> y = stage_outputs(pole, w);
        std::complex<T> band = bandpass(y);
        std::complex<T> num = numerator(m_coefficients, y);
        std::complex<T> denom = T(1.0) + resonance * band;
        return std::abs(num / denom);
    }

  private:
    constexpr std::complex<T> first_order_stage(T p, T w) const
    {
        // w: normalized frequency (0...pi)
        auto z = std::polar(T(1.0), -w);
        return (T(1.0) - p) / (T(1.0) - p * z);
    }

    constexpr std::array<std::complex<T>, 5> stage_outputs(T p, T w) const
    {
        // output of each stage for input 1 (not including numerator weights)
        std::complex<T> z = std::polar(T(1), T(-w));
        std::array<std::complex<T>, 5> y{};
        y[0] = T(1.0); // Input
        for (size_t i = 1; i < 5; ++i)
        {
            y[i] = y[i - 1] * first_order_stage(p, w);
        }
        return y;
    }

    std::complex<T> bandpass(const std::array<std::complex<T>, 5>& y) const
    {
        return -y[4] + T(2.0) * y[3] - y[2];
    }

    constexpr std::complex<T> numerator(const std::array<T, 5>& coeffs, const std::array<std::complex<T>, 5>& y) const
    {
        std::complex<T> sum{};
        for (size_t i = 0; i < 5; ++i)
            sum += coeffs[i] * y[i];
        return sum;
    }

    T m_sampleRate;
    std::array<T, 5> m_coefficients;
    T x0{0}, x1{0}, x2{0}, x3{0}, x4{0};
};

/*
 * 4 one pole lowpass filters (standard 6dB/oct, -3dB at cutoff) in series,
 * mixing their outputs yields various filter characteristics (called multimode mixing)
 * this filter is very suitable for fast voltage control (CC).
 *
 * refs:
 * http://electronotes.netfirms.com/EN85VCF.pdf
 * https://expeditionelectronics.com/Diy/Polemixing/math
 *
 */

class Filter1Pole4StageSmooth
{
  public:
    explicit Filter1Pole4StageSmooth(float sampleRate)
        : m_sampleRate(sampleRate)
    {
        setCutoffFrequency(m_cutoffFrequency);
        m_pole = m_targetPole;
    }

    void setFilterCoefficients(const std::array<float, 5>& cf)
    {
        m_coefficients = cf;
    }

    void setParameterSmoothTimeMs(float ms)
    {
        const float T = ms * 1e-3f;
        m_smoothingAlpha = 1.f - std::exp(-1.f / (T * m_sampleRate));
    }

    void setResonance(float value)
    {
        m_targetResonance = value;
    }

    // because we are in the digital domain we need to adapt the resonance frequency
    // this solution seems to be more efficient than implementing
    static float adaptResonanceFrequency(const float x)
    {
        if (x > 2800.f)
        {
            return std::clamp(-11224.374f + 6.806917f * x - 7.332340e-4f * x * x + 3.496297e-8f * x * x * x, 10.f,
                              22000.f);
        }
        return std::clamp(-0.03223791634f + 1.005588288f * x - 3.971210551e-06f * x * x + 3.645793593e-09f * x * x * x,
                          10.f, 22000.f);
    }

    void setCutoffFrequency(float cutoffFrequency)
    {
        m_cutoff = cutoffFrequency;
        const float x = adaptResonanceFrequency(cutoffFrequency);
        m_targetPole = std::exp(-std::numbers::pi * 2.0f * x / m_sampleRate);
    }

    void setCutoffFrequencyClean(float cutoffFrequency)
    {
        m_cutoff = cutoffFrequency;
        m_targetPole = std::exp(-std::numbers::pi * 2.0f * cutoffFrequency / m_sampleRate);
    }

    static float compress(const float in)
    {
        /*
        +--------------------+------------------------------+---------------------+-----------+
        | Function           | Formula                      | Shape               | CPU       |
        +--------------------+------------------------------+---------------------+-----------+
        | x/sqrt             | x / sqrt(1 + x^2)            | Soft, arcsine-like  | medium    |
        | tanh               | tanh(x)                      | Soft, classic       | slow      |
        | arctangent         | (2/pi) * atan(x)             | Soft, slowest knee  | slow      |
        | cubic soft-clip    | x - (1/3)x^3  (clamp -1..1)  | Soft, analog-like   | very fast |
        | reciprocal sigmoid | x / (1 + abs(x))             | Soft (different)    | fast      |
        | hard clip          | clamp(x, -1, 1)              | Hard                | fastest   |
        +--------------------+------------------------------+---------------------+-----------+
        */
        return in / std::sqrt(1 + in * in);
    }

    [[nodiscard]] float step(float in)
    {
        m_pole += m_smoothingAlpha * (m_targetPole - m_pole);
        m_reso += m_smoothingAlpha * (m_targetResonance - m_reso);

        const auto bandpass = -m_v[3] + 2 * m_v[2] - m_v[1];
        const auto feedback = compress(in - bandpass * m_reso);

        m_v[0] = feedback + m_pole * (m_v[0] - feedback);
        m_v[1] = m_v[0] + m_pole * (m_v[1] - m_v[0]);
        m_v[2] = m_v[1] + m_pole * (m_v[2] - m_v[1]);
        m_v[3] = m_v[2] + m_pole * (m_v[3] - m_v[2]);

        const float tmpSum = m_coefficients[0] * feedback + m_coefficients[1] * m_v[0] + m_coefficients[2] * m_v[1] +
                             m_coefficients[3] * m_v[2] + m_coefficients[4] * m_v[3];
        return tmpSum;
    }

    void processBlock(const float* source, float* target, size_t numSamples)
    {
        for (size_t i = 0; i < numSamples; ++i)
            target[i] = step(source[i]);
    }

    void reset()
    {
        std::ranges::fill(m_v, 0.f);
        m_pole = m_targetPole;
        m_reso = m_targetResonance;
    }

  private:
    float m_sampleRate{48000.f};
    float m_cutoffFrequency{1000.f};

    float m_cutoff{0.f};
    float m_pole{0.883824884f};
    float m_targetPole{0.883824884f};

    float m_reso{0.f};
    float m_targetResonance{0.f};

    float m_smoothingAlpha{0.01f};

    std::array<float, 4> m_v{0, 0, 0, 0};
    std::array<float, 5> m_coefficients{0, -1, 0, 0, 0};
};


template <int f0, int f1, int f2, int f3, int f4>
class FourStageOnePoleFilterNoResonance
{
  public:
    explicit FourStageOnePoleFilterNoResonance(const float sampleRate)
        : m_sampleRate(sampleRate)
    {
    }

    void setCutoff(const float cutoff)
    {
        m_pole = std::exp(-2.0f * std::numbers::pi_v<float> * cutoff / m_sampleRate);
    }

    float singleStep(const float in)
    {
        m_v[0] = in + m_pole * (m_v[0] - in);
        m_v[1] = m_v[0] + m_pole * (m_v[1] - m_v[0]);
        m_v[2] = m_v[1] + m_pole * (m_v[2] - m_v[1]);
        m_v[3] = m_v[2] + m_pole * (m_v[3] - m_v[2]);
        return f0 * in + f1 * m_v[0] + f2 * m_v[1] + f3 * m_v[2] + f4 * m_v[3];
    }

    void processBlock(const float* source, float* target, const size_t numSamples)
    {
        std::transform(source, source + numSamples, target, [this](auto value) { return singleStep(value); });
    }

  private:
    float m_sampleRate;
    float m_pole{0.5f};
    std::array<float, 4> m_v{0, 0, 0, 0};
};
}
