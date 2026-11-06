#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <limits>

#include "Filters/RampedBiquad.h"

namespace AbacDsp
{

/**
 * @ingroup filters
 * @brief Chebyshev type 1 lowpass design of order 4 as two biquad sections, in closed form.
 *
 * The analog prototype has an equiripple passband up to the cutoff (the edge of the ripple band, not the -3 dB point)
 * and a monotonic stopband. Its poles sit on an ellipse set by the ripple: with eps = sqrt(10^(ripple / 10) - 1) and
 * a = asinh(1 / eps) / 4, the pole pair k = 0, 1 lies at s = K (-sinh(a) sin(th) + j cosh(a) cos(th)),
 * th = (2k + 1) pi / 8, K = tan(pi fc / fs) being the prewarped cutoff. The bilinear map z = (1 + s) / (1 - s) puts
 * the poles in the z plane and the zeros at z = -1. Each section is scaled to unity gain at DC, |s|^2 / |1 - s|^2 (the
 * sum 1 + a1 + a2 cancels badly at low cutoffs, this form does not); an even order peaks at 0 dB in the passband, so
 * the first section is scaled once more by 10^(-ripple / 20): DC sits at -ripple dB. No complex arithmetic is needed.
 * Real is the type of the design arithmetic, the coefficients are float.
 * @see https://en.wikipedia.org/wiki/Chebyshev_filter
 */
template <typename Real>
[[nodiscard]] std::array<BiquadSection, 2> designChebyshev4(const double sampleRate, const double cutoff,
                                                            const double rippleDb)
{
    constexpr Real pi{static_cast<Real>(3.14159265358979323846)};
    const auto k = std::tan(pi * static_cast<Real>(cutoff / sampleRate));
    const auto eps = std::sqrt(std::pow(Real{10}, static_cast<Real>(rippleDb) / Real{10}) - Real{1});
    const auto a = std::asinh(Real{1} / eps) / Real{4};
    std::array<BiquadSection, 2> sections{};
    for (size_t i = 0; i < 2; ++i)
    {
        const auto th = static_cast<Real>(2 * i + 1) * pi / Real{8};
        const auto sigma = -k * std::sinh(a) * std::sin(th);
        const auto omega = k * std::cosh(a) * std::cos(th);
        const auto s2 = sigma * sigma + omega * omega;
        const auto d = (Real{1} - sigma) * (Real{1} - sigma) + omega * omega;
        const auto re = (Real{1} - s2) / d;
        const auto mag2 = ((Real{1} + sigma) * (Real{1} + sigma) + omega * omega) / d;
        auto gain = s2 / d;
        if (i == 0)
        {
            gain *= std::pow(Real{10}, -static_cast<Real>(rippleDb) / Real{20});
        }
        sections[i] =
            BiquadSection{static_cast<float>(gain), static_cast<float>(Real{2} * gain), static_cast<float>(gain),
                          static_cast<float>(-Real{2} * re), static_cast<float>(mag2)};
    }
    return sections;
}

/**
 * @ingroup filters
 * @brief Mono Chebyshev type 1 lowpass of order 4 (24 dB per octave) with a cutoff and a ripple that move without
 * clicks.
 *
 * Two RampedBiquad sections in series, designed by designChebyshev4() in double (the design runs once per change,
 * never per sample; below the floor the sum 1 + a1 + a2 of a section, which fixes its DC gain, is lost in float
 * digits and the response is off by more than 0.05 dB). A change of cutoff or ripple sets new targets and the
 * coefficients move linearly to them over setRampSamples() samples; with 0 they apply at once. The cutoff runs from
 * kMinCutoffRatio of the sample rate to kMaxCutoffRatio of it and the ripple from kMinRippleDb to kMaxRippleDb, both
 * clamped. A cutoff at the top of the range is open: the filter is bypassed and process() returns its input unchanged
 * once the ramp to the top has ended; the state is cleared when the filter is left, so the first samples after closing
 * come from a rested filter. Real-time safe: no allocation, the state is four floats.
 * @see https://en.wikipedia.org/wiki/Chebyshev_filter
 */
class Chebyshev4Lowpass
{
  public:
    static constexpr float kMinCutoffRatio{0.00125f};
    static constexpr float kMaxCutoffRatio{0.45f};
    static constexpr float kMinRippleDb{0.1f};
    static constexpr float kMaxRippleDb{3.0f};
    static constexpr float kOpenCutoff{std::numeric_limits<float>::max()};

    explicit Chebyshev4Lowpass(const float sampleRate = 48000.0f) noexcept
        : m_sampleRate(sampleRate)
    {
    }

    void setSampleRate(const float sampleRate) noexcept
    {
        m_sampleRate = sampleRate;
    }

    void setRampSamples(const size_t samples) noexcept
    {
        for (auto& section : m_sections)
        {
            section.setRampSamples(samples);
        }
    }

    /// Sets the targets (clamped); immediate skips the ramp. The same targets again change nothing, so a caller may set
    /// them every control block without restarting the ramp or leaving the bypass.
    void set(const float cutoffHz, const float rippleDb, const bool immediate = false) noexcept
    {
        const float top = kMaxCutoffRatio * m_sampleRate;
        const float cutoff = std::clamp(cutoffHz, kMinCutoffRatio * m_sampleRate, top);
        const float ripple = std::clamp(rippleDb, kMinRippleDb, kMaxRippleDb);
        if (!immediate && cutoff == m_cutoff && ripple == m_ripple)
        {
            return;
        }
        m_cutoff = cutoff;
        m_ripple = ripple;
        const bool wasOpen = m_open;
        m_open = cutoff >= top;
        if (wasOpen && !m_open)
        {
            reset();
        }
        const auto design = designChebyshev4<double>(m_sampleRate, cutoff, ripple);
        for (size_t i = 0; i < 2; ++i)
        {
            m_sections[i].setTarget(design[i]);
            if (immediate)
            {
                m_sections[i].snapToTarget();
            }
        }
    }

    [[nodiscard]] float process(const float in) noexcept
    {
        if (m_open && !m_sections[0].isRamping() && !m_sections[1].isRamping())
        {
            return in;
        }
        return m_sections[1].process(m_sections[0].process(in));
    }

    void reset() noexcept
    {
        for (auto& section : m_sections)
        {
            section.reset();
        }
    }

    [[nodiscard]] bool isOpen() const noexcept
    {
        return m_open;
    }

    /// The two sections now in use (the target once a ramp has ended).
    [[nodiscard]] const RampedBiquad& section(const size_t index) const noexcept
    {
        return m_sections[index];
    }

  private:
    float m_sampleRate;
    float m_cutoff{0.f};
    float m_ripple{0.f};
    bool m_open{false};
    std::array<RampedBiquad, 2> m_sections{};
};

} // namespace AbacDsp
