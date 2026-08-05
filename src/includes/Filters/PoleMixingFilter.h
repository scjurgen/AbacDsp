#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstddef>
#include <functional>
#include <numbers>
#include <stdexcept>
#include <string_view>
#include <vector>

/**
 * @file
 * @ingroup filters
 * @brief Four-stage one-pole (pole-mixing / multimode VCF) filter family.
 *
 * All variants share one topology: four cascaded one-pole lowpass stages, an
 * optional resonance feedback, and an output that is a weighted mix of the
 * stage outputs (pole mixing). Small integer weights yield low-, high-, band-,
 * all-pass and notch responses; see poleMixingList for named presets.
 *
 * The economy of the structure is that all of those responses cost the same
 * four one-pole stages, and switching between them is a change of five weights
 * with no coefficient redesign and no discontinuity in the stage states.
 *
 * @see http://electronotes.netfirms.com/EN85VCF.pdf
 * @see https://expeditionelectronics.com/Diy/Polemixing
 */

namespace AbacDsp
{

// --- shared four-stage core ---

/// @ingroup filters
/// @brief Advances the four cascaded one-pole stages by one sample, in place.
/// Each stage feeds the next within the same sample, so the cascade has no internal delay.
inline void advanceFourStages(std::array<float, 4>& v, const float feed, const float pole) noexcept
{
    v[0] = feed + pole * (v[0] - feed);
    v[1] = v[0] + pole * (v[1] - v[0]);
    v[2] = v[1] + pole * (v[2] - v[1]);
    v[3] = v[2] + pole * (v[3] - v[2]);
}

/// @ingroup filters
/// @brief Second-order bandpass tap of the cascade, -v3 + 2*v2 - v1, used for resonance feedback.
/// A second difference of adjacent taps; feeding this back peaks the response without dragging the cutoff.
[[nodiscard]] inline float bandpassTap(const std::array<float, 4>& v) noexcept
{
    return -v[3] + 2.f * v[2] - v[1];
}

/// @ingroup filters
/// @brief Weighted sum of the input tap and the four stage outputs, weights fixed at compile time.
/// tap0 is the pre-cascade input, or the saturated feedback where the variant applies one.
template <int w0, int w1, int w2, int w3, int w4>
[[nodiscard]] constexpr float mixTaps(const float tap0, const std::array<float, 4>& v) noexcept
{
    return w0 * tap0 + w1 * v[0] + w2 * v[1] + w3 * v[2] + w4 * v[3];
}

/// @ingroup filters
/// @brief Runtime-weighted overload of mixTaps(), for variants whose response is chosen at runtime.
[[nodiscard]] inline float mixTaps(const std::array<float, 5>& w, const float tap0,
                                   const std::array<float, 4>& v) noexcept
{
    return w[0] * tap0 + w[1] * v[0] + w[2] * v[1] + w[3] * v[2] + w[4] * v[3];
}


// --- named coefficient presets ---

/// @ingroup filters
/// @brief A named set of five mixing weights.
struct PoleMixingList
{
    std::string_view name;
    std::array<float, 5> cf;
};

/**
 * @ingroup filters
 * @brief Named mixing-weight presets covering the responses the topology can reach.
 *
 * The highpass rows are binomial: HP1 is (1, -1), HP2 (1, -2, 1), HP4
 * (1, -4, 6, -4, 1). That is (1 - LP)^n expanded over the stage taps, since a
 * one-pole highpass is the input minus its lowpass and the cascade supplies
 * every power of the lowpass in one place. The allpass rows follow the same
 * pattern with the ratio doubled, and the notch rows are highpass rows detuned
 * so a zero lands on the unit circle.
 */
inline const std::vector<PoleMixingList> poleMixingList = {
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
    {"HP2 + LP1", {0, -1.5f, 3, -1.5f, 0}},
    {"BP Alt.", {0, -3, 6, -4, 1}},
    {"Notch", {1, -2, 2, 0, 0}},
    {"Res Before Notch", {0.3f, -0.6f, 1.5f, 0, 0}},
    {"Res After Notch", {1.5f, -3, 2, 0, 0}},
    {"Notch B", {1, -3, 6, -4, 1}},
    {"LP1 + Notch A", {0, -1, 2, -2, 0}},
    {"LP1 + Notch B (octave down)", {0, -4, 8, -5, 0}},
    {"LP1 + Notch C", {0, -3, 6, -4, 0}},
    {"LP2 + Notch A", {0, 0, 1, -2, 2}},
    {"LP2 + Notch B (octave down)", {0, 0, 4, -8, 5}},
    {"LP2 + Notch C", {0, 0, 3, -6, 4}},
    {"LP2 + Notch D (Brickwall)", {0, 0, 0.3f, -0.6f, 1.5f}},
    {"LP1 Notch B", {0, -2, 4, -3, 0}},
    {"BP Notch Offset Left", {0, -3, 9, -10, 4}},
    {"BP Notch", {0, -4, 12, -16, 8}},
    {"BP Notch Offset Right", {0, -1, 3, -6, 4}},
    {"BP Notch Octave Up", {0, -1, 3, -7, 5}},
    {"HP + Notch A", {1, -3, 4, -2, 0}},
    {"HP + Notch B", {1, -3, 6, -4, 0}},
    {"HP + Notch C (Octave)", {1, -3, 7, -5, 0}},
    {"HP + Notch D (3rd)", {0.5f, -1.5f, 6, -5, 0}},
    {"HP2 + Notch A", {1, -4, 7, -6, 2}},
    {"HP2 + Notch B", {1, -4, 8, -8, 3}},
    {"HP2 + Notch C", {1, -4, 9, -10, 4}},
    {"HP2 + Notch D", {1, -4, 10, -12, 5}},
    {"Double Notch A (almost Octaves)", {1, -4, 10, -12, 6}},
    {"Double Notch Octaves", {1, -4, 10.25f, -12.5f, 6.25f}},
    {"Double Notch B", {1, -4, 11, -14, 7}},
    {"Double Notch C (Dixon)", {1, -4, 12, -16, 8}},
    {"Double Notch D", {1, -4, 14, -20, 10}},
    {"Double Notch (approx. 3rd Harmonic)", {1, -4, 15, -22, 11}},

    {"20db LP shelf", {0.1f, -0.6f, 1.1f, -2.8f, 3.2f}},
};

/// @ingroup filters
/// @brief Index of a preset by name. Throws std::out_of_range when the name is unknown.
[[nodiscard]] inline size_t findFilterIndex(std::string_view target)
{
    auto it = std::ranges::find_if(poleMixingList, [target](const PoleMixingList& pm) { return pm.name == target; });
    if (it != poleMixingList.end())
    {
        return static_cast<size_t>(std::distance(poleMixingList.begin(), it));
    }
    throw std::out_of_range("PoleMixingList name not found");
}

/**
 * @ingroup filters
 * @brief Scales a requested resonance down as the cutoff approaches the upper eighth of the rate.
 *
 * Above fs/8 the effective resonance is multiplied by (fs/8)/f, falling to zero
 * at Nyquist. The four-stage feedback loop grows unstable as the poles crowd
 * the top of the band, and this keeps a fixed user setting usable across the
 * whole sweep rather than only in the lower octaves.
 */
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

    const float m_sampleRate;
    float m_frequency;
    float m_resonance;
    float m_userResonance;
};


/**
 * @ingroup filters
 * @brief Closed-form magnitude and phase of the pole-mixing topology, for plotting.
 *
 * Analytic rather than measured, so it costs nothing to sweep and carries no
 * state. setCoefficients() folds the five mixing weights into the numerator
 * powers x0..x4 once, leaving magnitude() a polynomial evaluation.
 *
 * magnitude() works in analog terms, w normalised to the cutoff, and matches
 * the discrete filter only well below Nyquist. magnitudeBP() and
 * magnitudeBP2() evaluate the actual discrete cascade on the unit circle and
 * agree with the running filter across the whole band.
 * @see https://expeditionelectronics.com/Diy/Polemixing
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

    [[nodiscard]] T magnitude(const T cutoff, const T testFrequency, const T resonance) const
    {
        const T w = testFrequency / cutoff; // normalized to cutoff
        // |G| = |N| / |D|
        const auto nReal = x0 + x4 * w * w * w * w - x2 * w * w;
        const auto nImg = -x1 * w + x3 * w * w * w;
        const auto nMag = std::sqrt(nReal * nReal + nImg * nImg);

        const auto dReal = 1.0 - 6.0 * w * w + w * w * w * w + resonance;
        const auto dImg = -4.0 * w + 4 * w * w * w;
        const auto dMag = std::sqrt(dReal * dReal + dImg * dImg);

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
        while (phaseValue > std::numbers::pi_v<T>)
        {
            phaseValue -= T{2} * std::numbers::pi_v<T>;
        }
        while (phaseValue < -std::numbers::pi_v<T>)
        {
            phaseValue += T{2} * std::numbers::pi_v<T>;
        }
        phase = phaseValue;
    }

    [[nodiscard]] static T calcPhase(const T img, const T real)
    {
        return real > 0 ? -std::atan(img / real) : -(std::numbers::pi_v<T> - std::atan(img / std::abs(real)));
    }

    [[nodiscard]] T magnitudeBP(const T cutoff, const T testFrequency, const T resonance) const
    {
        const T pole = std::exp(-T(2) * std::numbers::pi_v<T> * cutoff / m_sampleRate);
        const T w = T(2) * std::numbers::pi_v<T> * testFrequency / m_sampleRate;
        const auto y = stage_outputs(pole, w);
        const auto band = bandpass(y);
        const auto num = numerator(m_coefficients, y);
        const auto denom = T(1) + resonance * band;
        return std::abs(num / denom);
    }

    [[nodiscard]] T magnitudeBP2(const T pole, const T w, const T resonance) const
    {
        const auto y = stage_outputs(pole, w);
        const std::complex<T> band = bandpass(y);
        const std::complex<T> num = numerator(m_coefficients, y);
        const std::complex<T> denom = T(1.0) + resonance * band;
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
        {
            sum += coeffs[i] * y[i];
        }
        return sum;
    }

    const T m_sampleRate;
    std::array<T, 5> m_coefficients;
    T x0{0}, x1{0}, x2{0}, x3{0}, x4{0};
};


/**
 * @ingroup filters
 * @brief Resonant four-stage filter with atan saturation and linearly ramped cutoff.
 *
 * Resonance is fed back from the last stage rather than the bandpass tap, which
 * gives the steeper, more classic self-oscillation of the two variants here.
 * The atan is what keeps that loop bounded once the feedback exceeds unity.
 *
 * setCutoff() ramps the pole linearly over setSmoothingSteps() samples and
 * snaps to the target at the end of the ramp, so a sweep cannot leave the pole
 * short of where it was asked to go. Steps of 0 means an immediate jump.
 *
 * Derived classes fold compile-time integer weights into the output; see the
 * aliases below.
 */
class FourStageFilter
{
  public:
    explicit FourStageFilter(const float sampleRate)
        : FourStageFilter(sampleRate, 1000.f)
    {
    }

    FourStageFilter(const float sampleRate, const float defaultCutoff)
        : m_sampleRate(sampleRate)
    {
        setCutoff(defaultCutoff);
        m_stepsAdvance = 0;
        m_pole = m_newPole;
    }

    virtual ~FourStageFilter() = default;
    FourStageFilter(const FourStageFilter&) = default;
    FourStageFilter& operator=(const FourStageFilter&) = default;
    FourStageFilter(FourStageFilter&&) noexcept = default;
    FourStageFilter& operator=(FourStageFilter&&) noexcept = default;

    void reset() noexcept
    {
        std::ranges::fill(m_v, 0.f);
    }

    void setSmoothingSteps(const size_t steps) noexcept
    {
        m_stepsAdvanceSetting = steps;
    }

    void setResonance(const float value) noexcept
    {
        m_reso = value * 4.f;
        m_gain = std::clamp(1 + m_adaptGain, 1.f, 10.f);
    }

    void setAdaptGain(const float value) noexcept
    {
        m_adaptGain = value;
    }

    float setCutoff(const float cutoff)
    {
        if (std::equal_to<float>{}(cutoff, m_lastCutoffIn))
        {
            return 0.f;
        }
        m_lastCutoffIn = cutoff;
        const auto cF = std::clamp(warpCutoffForSampleRate(cutoff), 10.f, 22000.f);
        m_cutoff = cF;
        m_newPole = std::exp(-2.f * std::numbers::pi_v<float> * cF / m_sampleRate);
        if (m_stepsAdvanceSetting == 0)
        {
            m_pole = m_newPole;
        }
        else
        {
            m_advance = (m_newPole - m_pole) / static_cast<float>(m_stepsAdvanceSetting);
        }
        m_stepsAdvance = m_stepsAdvanceSetting;
        return m_cutoff;
    }

    [[nodiscard]] float currentFactor() const noexcept
    {
        return m_pole;
    }

    virtual float step(float in) = 0;

    void processBlockInplace(float* source, const size_t numSamples)
    {
        processBlock(source, source, numSamples);
    }

    void processBlock(const float* source, float* target, const size_t numSamples)
    {
        size_t index = 0;
        size_t toIndex = numSamples;

        // split into if-less blocks
        if (m_stepsAdvance)
        {
            if (m_stepsAdvance < numSamples)
            {
                toIndex = m_stepsAdvance;
                m_stepsAdvance = 0;
            }
            else
            {
                m_stepsAdvance -= numSamples;
            }
            while (index < toIndex)
            {
                m_pole += m_advance;
                target[index] = step(source[index]);
                ++index;
            }
            if (!m_stepsAdvance)
            {
                m_pole = m_newPole;
            }
        }
        while (index < numSamples)
        {
            target[index] = step(source[index]);
            ++index;
        }
    }

    [[nodiscard]] float correctGain() const noexcept
    {
        return m_gain;
    }

  private:
    /// @brief Per-rate cubic corrections so a requested cutoff lands on the measured response.
    /// Fitted for 44.1, 48, 96, 192 and 384 kHz only; every other rate falls through uncorrected.
    [[nodiscard]] float warpCutoffForSampleRate(const float cutoff) const noexcept
    {
        const float x = cutoff;
        if (m_sampleRate == 44100.f)
        {
            return 0.1070741493f + 1.000163615f * x + -6.77430211e-05f * x * x + 3.441634626e-09f * x * x * x;
        }
        if (m_sampleRate == 48000.f)
        {
            return 0.1409743683f + 0.9999793344f * x + -6.203395634e-05f * x * x + 2.855230937e-09f * x * x * x;
        }
        if (m_sampleRate == 96000.f)
        {
            return 0.2635405566f + 1.000099839f * x + -3.105817148e-05f * x * x + 7.1736266e-10f * x * x * x;
        }
        if (m_sampleRate == 192000.f)
        {
            return -0.01065210969f + 1.001839706f * x + -1.61077305e-05f * x * x + 2.229968241e-10f * x * x * x;
        }
        if (m_sampleRate == 384000.f)
        {
            return -0.1060540674f + 1.002323759f * x + -8.204298197e-06f * x * x + 6.640562956e-11f * x * x * x;
        }
        return x;
    }

    float m_sampleRate;
    float m_lastCutoffIn{1.f};
    float m_advance{0.f};
    size_t m_stepsAdvance{0};
    size_t m_stepsAdvanceSetting{0};
    float m_newPole{0.5f};
    float m_cutoff{1000.f};
    float m_gain{1.f};
    float m_adaptGain{0.f};

  protected:
    float m_reso{0.f};
    float m_pole{0.5f};
    std::array<float, 4> m_v{};
};

/// @ingroup filters
/// @brief FourStageFilter with the mixing weights fixed at compile time.
/// Small integer weights fold into adds and shifts, so the output mix costs almost nothing.
template <int f0, int f1, int f2, int f3, int f4>
class FixedFourStageFilter final : public FourStageFilter
{
  public:
    explicit FixedFourStageFilter(const float sampleRate)
        : FourStageFilter(sampleRate, 1000.f)
    {
    }

    float step(const float in) override
    {
        const auto feed = std::atan(in - m_v[3] * m_reso);
        advanceFourStages(m_v, feed, m_pole);
        return mixTaps<f0, f1, f2, f3, f4>(feed, m_v);
    }
};

/// @ingroup filters
/// @brief Passes the input tap alone. Not a no-op: with resonance up, the atan still saturates.
using ByPassSmooth = FixedFourStageFilter<1, 0, 0, 0, 0>;
using Lp6Smooth = FixedFourStageFilter<0, 1, 0, 0, 0>;
using Lp12Smooth = FixedFourStageFilter<0, 0, 1, 0, 0>;
using Lp18Smooth = FixedFourStageFilter<0, 0, 0, 1, 0>;
using Lp24Smooth = FixedFourStageFilter<0, 0, 0, 0, 1>;

using Ap6Smooth = FixedFourStageFilter<1, -2, 0, 0, 0>;
using Ap12Smooth = FixedFourStageFilter<1, -4, 4, 0, 0>;
using Ap18Smooth = FixedFourStageFilter<1, -6, 12, -8, 0>;
using Ap24Smooth = FixedFourStageFilter<1, -8, 24, -32, 16>;

using Bp12Smooth = FixedFourStageFilter<0, -2, 2, 0, 0>;
using Bp24Smooth = FixedFourStageFilter<0, 0, 4, -8, 4>;

using Hp6Smooth = FixedFourStageFilter<1, -1, 0, 0, 0>;
using Hp12Smooth = FixedFourStageFilter<1, -2, 1, 0, 0>;
using Hp18Smooth = FixedFourStageFilter<1, -3, 3, -1, 0>;
using Hp24Smooth = FixedFourStageFilter<1, -4, 6, -4, 1>;

using Phaser12Smooth = FixedFourStageFilter<1, -2, 2, 0, 0>;
using Phaser24Smooth = FixedFourStageFilter<1, -4, 12, -16, 8>;

using DoubleNotch = FixedFourStageFilter<1, -4, 11, -14, 7>;
using Notch12Smooth = FixedFourStageFilter<1, -2, 2, 0, 0>;
using Hp12Lp6Smooth = FixedFourStageFilter<0, -3, 6, -3, 0>;
using Hp18Lp6Smooth = FixedFourStageFilter<0, -3, 9, -9, 3>;
using Notch12Lp6Smooth = FixedFourStageFilter<0, -1, 2, -2, 0>;
using Allpass18Lp6Smooth = FixedFourStageFilter<0, -1, 3, -6, 4>;


/**
 * @ingroup filters
 * @brief Four-stage filter with runtime mixing weights and exponentially smoothed controls.
 *
 * Differs from FourStageFilter on three counts: the weights are settable per
 * call rather than baked in, resonance is fed back from bandpassTap() instead
 * of the last stage, and both pole and resonance follow one-pole smoothers
 * instead of a fixed-length linear ramp. The smoother never quite arrives,
 * which is the trade for absorbing a continuous stream of control changes
 * without stair-stepping.
 *
 * Saturation is x/sqrt(1+x^2), chosen over tanh and atan for cost; see the
 * table in compress().
 */
class Filter1Pole4StageSmooth
{
  public:
    explicit Filter1Pole4StageSmooth(const float sampleRate)
        : m_sampleRate(sampleRate)
    {
        setCutoffFrequency(m_cutoffFrequency);
        m_pole = m_targetPole;
    }

    void setFilterCoefficients(const std::array<float, 5>& cf) noexcept
    {
        m_coefficients = cf;
    }

    void setParameterSmoothTimeMs(const float ms) noexcept
    {
        const float T = ms * 1e-3f;
        m_smoothingAlpha = 1.f - std::exp(-1.f / (T * m_sampleRate));
    }

    void setResonance(const float value) noexcept
    {
        m_targetResonance = value;
    }

    /// @brief Warps a requested cutoff onto the frequency the discrete cascade actually resonates at.
    /// Piecewise cubic fit with a break at 2800 Hz, cheaper than solving the pole placement directly.
    [[nodiscard]] static float adaptResonanceFrequency(const float x) noexcept
    {
        if (x > 2800.f)
        {
            return std::clamp(-11224.374f + 6.806917f * x - 7.332340e-4f * x * x + 3.496297e-8f * x * x * x, 10.f,
                              22000.f);
        }
        return std::clamp(-0.03223791634f + 1.005588288f * x - 3.971210551e-06f * x * x + 3.645793593e-09f * x * x * x,
                          10.f, 22000.f);
    }

    void setCutoffFrequency(const float cutoffFrequency) noexcept
    {
        m_cutoff = cutoffFrequency;
        const float x = adaptResonanceFrequency(cutoffFrequency);
        m_targetPole = std::exp(-2.0f * std::numbers::pi_v<float> * x / m_sampleRate);
    }

    /// @brief Sets the cutoff with no warp, placing the pole straight from the requested frequency.
    void setCutoffFrequencyClean(const float cutoffFrequency) noexcept
    {
        m_cutoff = cutoffFrequency;
        m_targetPole = std::exp(-2.0f * std::numbers::pi_v<float> * cutoffFrequency / m_sampleRate);
    }

    [[nodiscard]] static float compress(const float in) noexcept
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

    [[nodiscard]] float step(const float in) noexcept
    {
        m_pole += m_smoothingAlpha * (m_targetPole - m_pole);
        m_reso += m_smoothingAlpha * (m_targetResonance - m_reso);

        const auto feedback = compress(in - bandpassTap(m_v) * m_reso);
        advanceFourStages(m_v, feedback, m_pole);
        return mixTaps(m_coefficients, feedback, m_v);
    }

    void processBlock(const float* source, float* target, const size_t numSamples) noexcept
    {
        for (size_t i = 0; i < numSamples; ++i)
        {
            target[i] = step(source[i]);
        }
    }

    void reset() noexcept
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

    std::array<float, 4> m_v{};
    std::array<float, 5> m_coefficients{0.f, -1.f, 0.f, 0.f, 0.f};
};

/**
 * @ingroup filters
 * @brief Four one-pole stages with compile-time weights, no resonance and no saturation.
 *
 * With no feedback loop the response is exactly the product of four one-poles,
 * so it is unconditionally stable and entirely linear. That makes it the
 * variant to reach for when the filter has to be analysed or inverted rather
 * than played.
 */
template <int f0, int f1, int f2, int f3, int f4>
class FourStageOnePoleFilterNoResonance
{
  public:
    explicit FourStageOnePoleFilterNoResonance(const float sampleRate)
        : m_sampleRate(sampleRate)
    {
    }

    void setCutoff(const float cutoff) noexcept
    {
        m_pole = std::exp(-2.0f * std::numbers::pi_v<float> * cutoff / m_sampleRate);
    }

    void reset() noexcept
    {
        std::ranges::fill(m_v, 0.f);
    }

    [[nodiscard]] float singleStep(const float in) noexcept
    {
        advanceFourStages(m_v, in, m_pole);
        return mixTaps<f0, f1, f2, f3, f4>(in, m_v);
    }

    void processBlock(const float* source, float* target, const size_t numSamples) noexcept
    {
        std::transform(source, source + numSamples, target, [this](const float value) { return singleStep(value); });
    }

  private:
    const float m_sampleRate;
    float m_pole{0.5f};
    std::array<float, 4> m_v{};
};
}
