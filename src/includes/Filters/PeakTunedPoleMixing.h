#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstddef>
#include <numbers>
#include <string_view>

#include "Filters/OnePoleFilter.h"

/**
 * @file
 * @ingroup filters
 * @brief Four-stage pole-mixing filter whose resonance peak is placed analytically at a given frequency.
 *
 * Same topology as PoleMixingFilter.h (four cascaded one-pole lowpass stages, an output that is a weighted mix of
 * the input and the stage outputs), built for a different job, so both exist. PoleMixingFilter.h holds filters
 * with a character of their own: a saturator in the feedback path, a cutoff warp fitted to measurements at 48 kHz,
 * and a resonance normalised by a measured table. This header holds a filter whose control is the pitch of the
 * resonance: set the frequency the peak should sit at and it does, at any sample rate, with a self-oscillation edge
 * that is known in closed form. Neither replaces the other; they sound different.
 *
 * | | PoleMixingFilter.h (FourStageFilter, Filter1Pole4StageSmooth) | this header |
 * |---|---|---|
 * | Feedback tap | last stage (FourStageFilter), second-difference band tap (Filter1Pole4StageSmooth) | last stage, one
 * sample delayed | | Limiting in the loop | smooth saturator, atan or x / sqrt(1 + x^2) | none; every stage state is
 * clamped to [-1, 1] | | Cutoff to pole | polynomial fitted to measured resonance peaks at 48 kHz, plus a measured
 * critical resonance | closed form from the loop phase, any sample rate | | Resonance meaning | normalised, 1.0 is the
 * measured edge of oscillation (Filter1Pole4StageSmooth) or a raw amount | a raw amount r; the edge is r * a =
 * poleMixingOscillationThreshold(), exactly known | | Presets | five weights by name, no per-preset resonance shaping |
 * 35 rows, each with its own feedback scale a and input gain g | | Parameter smoothing | built in (linear ramp or
 * one-pole) | none; the caller sets values per sample if it wants them smoothed | | Response model | magnitude()
 * polynomial for plotting | poleMixingResponse(), the exact small-signal transfer function including the loop delay |
 *
 * Loop and tuning. The output is w_in * feed - w0 * s1 + w1 * s2 - w2 * s3 + w3 * s4 with feed = in * (1 + r g) -
 * r a * s4[n-1], and the loop gain is r * a. With L(w) = (1 - p) / (1 - p e^-jw) the loop phase is -pi where
 * w + 4 phi = pi, phi = arg L(w). Solving for the pole that puts that frequency at w = 2 pi peakHz / fs gives
 *   phi = (pi - w) / 4,  p = sin(phi) / sin(w + phi),  k = ((1 - 2 p cos(w) + p^2) / (1 - p)^2)^2,
 * where k is the loop gain at which the filter starts to oscillate, there. The result agrees with the measured
 * correction of FourStageFilter (same loop) within 0.2 percent up to 4 kHz at 48 kHz.
 * @see https://electricdruid.net/multimode-filters-part-2-pole-mixing-filters
 * @see https://expeditionelectronics.com/Diy/Polemixing
 */

namespace AbacDsp
{

/// @ingroup filters
/// @brief Response families of the pole-mixing presets, combinable as flags with operator|.
enum class PoleMixingFamily : unsigned
{
    kOberheim = 0x001, ///< one of the 15 filter modes of the Oberheim Xpander / Matrix-12 (or a variant)
    kLowPass = 0x004,
    kHighPass = 0x008,
    kBandPass = 0x010,
    kNotch = 0x020,
    kAllPass = 0x040,
    kPhaser = 0x080, ///< allpass and notch mixes meant for phaser-like sweeps
    kMixed = 0x100,  ///< combinations, e.g. highpass plus lowpass or notch plus lowpass
};

[[nodiscard]] constexpr PoleMixingFamily operator|(const PoleMixingFamily a, const PoleMixingFamily b) noexcept
{
    return static_cast<PoleMixingFamily>(static_cast<unsigned>(a) | static_cast<unsigned>(b));
}

/// @ingroup filters
/// @brief True if the family set contains the given flag.
[[nodiscard]] constexpr bool contains(const PoleMixingFamily set, const PoleMixingFamily flag) noexcept
{
    return (static_cast<unsigned>(set) & static_cast<unsigned>(flag)) != 0;
}

/**
 * @ingroup filters
 * @brief One pole-mixing preset: the five tap weights and the resonance shaping.
 *
 * The output is w_in * feed - w[0] * s1 + w[1] * s2 - w[2] * s3 + w[3] * s4, with s1..s4 the outputs of the four
 * cascaded one-pole stages. The alternating signs are applied by the filter, so the weights read like the
 * coefficients of a polynomial in -LP: highpass n is (1 - LP)^n, e.g. 2-pole highpass = {1, {2, 1, 0, 0}}.
 */
struct PoleMixingPreset
{
    float inputWeight;
    std::array<float, 4> stageWeights;
    float resonanceFeedback;  ///< scales the stage-4 feedback for this mix (how fast it reaches self-oscillation)
    float resonanceInputGain; ///< input boost in * (1 + r * gain) that compensates the level loss of resonance
    const char* name;
    PoleMixingFamily family;
};

// clang-format off
/**
 * @ingroup filters
 * @brief Mix table. Entries 0..14 are the 15 modes of the Oberheim Xpander / Matrix-12 in manual order, entry 15
 * repeats entry 11 with different resonance shaping, and names starting with '*' are additions.
 * @see https://electricdruid.net/multimode-filters-part-2-pole-mixing-filters
 */
inline constexpr auto kPoleMixingPresets = std::to_array<PoleMixingPreset>({
    {0,    {-1,   0,    0,    0},    1.0f,   0.5f,  "6 lp(inv)",                     PoleMixingFamily::kOberheim | PoleMixingFamily::kLowPass},
    {0,    {0,    1,    0,    0},    1.25f,  0.7f,  "2 pole lp",                     PoleMixingFamily::kOberheim | PoleMixingFamily::kLowPass},
    {0,    {0,    0,    -1,   0},    1.5f,   0.5f,  "3 pole lp(inv)",                PoleMixingFamily::kOberheim | PoleMixingFamily::kLowPass},
    {0,    {0,    0,    0,    1},    1.7f,   0.5f,  "4 pole lp",                     PoleMixingFamily::kOberheim | PoleMixingFamily::kLowPass},
    {1,    {1,    0,    0,    0},    1.6f,   0.0f,  "1 pole hp",                     PoleMixingFamily::kOberheim | PoleMixingFamily::kHighPass},
    {1,    {2,    1,    0,    0},    1.8f,   0.0f,  "2 pole hp",                     PoleMixingFamily::kOberheim | PoleMixingFamily::kHighPass},
    {1,    {3,    3,    1,    0},    1.88f,  0.0f,  "3 pole hp",                     PoleMixingFamily::kOberheim | PoleMixingFamily::kHighPass},
    {0,    {2,    2,    0,    0},    1.5f,   0.0f,  "2 pole bp",                     PoleMixingFamily::kOberheim | PoleMixingFamily::kBandPass},
    {0,    {0,    4,    8,    4},    1.5f,   0.0f,  "4 pole bp",                     PoleMixingFamily::kOberheim | PoleMixingFamily::kBandPass},
    {1,    {2,    2,    0,    0},    2.0f,   0.0f,  "2 pole notch",                  PoleMixingFamily::kOberheim | PoleMixingFamily::kNotch},
    {-1,   {-6,   -12,  -8,   0},    1.2f,   0.2f,  "3 pole ap (inv)",               PoleMixingFamily::kOberheim | PoleMixingFamily::kAllPass},
    {0,    {3,    6,    3,    0},    1.4f,   0.0f,  "2 pole hp + 1 pole lp",         PoleMixingFamily::kOberheim | PoleMixingFamily::kMixed},
    {0,    {3,    9,    9,    3},    1.55f,  0.0f,  "3 pole hp + 1 pole lp",         PoleMixingFamily::kOberheim | PoleMixingFamily::kMixed},
    {0,    {1,    2,    2,    0},    1.75f,  1.0f,  "2 pole notch + 1 pole lp",      PoleMixingFamily::kOberheim | PoleMixingFamily::kMixed},
    {0,    {1,    3,    6,    4},    1.5f,   0.25f, "3 pole ap + 1 pole lp",         PoleMixingFamily::kOberheim | PoleMixingFamily::kMixed},
    {0,    {3,    6,    3,    0},    1.2f,   0.25f, "2 pole hp + 1 pole lp B",       PoleMixingFamily::kOberheim | PoleMixingFamily::kMixed},
    {-1,   {-2,   0,    0,    0},    1.3f,   0.2f,  "* 1 pole ap (inv)",             PoleMixingFamily::kPhaser},
    {1,    {4,    4,    0,    0},    1.3f,   0.2f,  "* 2 pole ap",                   PoleMixingFamily::kPhaser},
    {-1,   {-6,   -12,  -8,   0},    1.3f,   0.2f,  "* 3 pole ap (inv)",             PoleMixingFamily::kPhaser},
    {1,    {8,    24,   32,   16},   1.3f,   0.2f,  "* 4 pole ap",                   PoleMixingFamily::kPhaser},
    {1,    {2,    2,    0,    0},    2.0f,   0.0f,  "* 2 pole phaser",               PoleMixingFamily::kPhaser},
    {1,    {3,    6,    4,    0},    1.5f,   0.0f,  "* 3 pole phaser",               PoleMixingFamily::kPhaser},
    {1,    {4,    12,   16,   8},    1.5f,   0.0f,  "* 4 pole phaser",               PoleMixingFamily::kPhaser},
    {1,    {4,    6,    4,    1},    1.975f, 0.0f,  "* 4 pole hp",                   PoleMixingFamily::kHighPass},
    {0,    {-4,   -7,   -4,   0},    1.5f,   0.0f,  "* double lp exp(inv)",          PoleMixingFamily::kMixed | PoleMixingFamily::kLowPass},
    {0,    {-4,   -8,   -5,   0},    1.3f,   0.25f, "* LP 1 + Notch -12(inv)",       PoleMixingFamily::kMixed},
    {0,    {0,    4,    8,    5},    1.3f,   0.35f, "* LP 2 + Notch -12",            PoleMixingFamily::kMixed},
    {0.25f,{0.5f, 1.25f,0,    0},    1.6f,   0.3f,  "* Reso before Notch",           PoleMixingFamily::kMixed},
    {1.5f, {3,    2,    0,    0},    1.6f,   0.3f,  "* Reso after Notch",            PoleMixingFamily::kMixed},
    {1,    {4,    10,   12,   6},    1.7f,   0.0f,  "* Double Notch A (+- Octaves)", PoleMixingFamily::kMixed},
    {1,    {4,    10.25f,12.5f,6.25f},1.7f,  0.0f,  "* Double Notch Octaves",        PoleMixingFamily::kMixed},
    {1,    {4,    11,   14,   7},    1.5f,   0.0f,  "* Double Notch B",              PoleMixingFamily::kMixed},
    {1,    {4,    12,   16,   8},    1.4f,   0.0f,  "* Double Notch C (Dixon)",      PoleMixingFamily::kMixed},
    {0.75f,{3,    10.5f,15,   7.5f}, 1.4f,   0.0f,  "* Double Notch D",              PoleMixingFamily::kMixed},
    {1,    {0,    4,    8,    4},    1.0f,   0.0f,  "Bell",                          PoleMixingFamily::kBandPass},
});
// clang-format on

inline constexpr size_t kPoleMixingPresetNotFound{kPoleMixingPresets.size()};

/// @ingroup filters
/// @brief Index of the preset with exactly this name, or kPoleMixingPresetNotFound.
[[nodiscard]] constexpr size_t findPoleMixingPreset(const std::string_view name) noexcept
{
    for (size_t i = 0; i < kPoleMixingPresets.size(); ++i)
    {
        if (name == kPoleMixingPresets[i].name)
        {
            return i;
        }
    }
    return kPoleMixingPresetNotFound;
}

/**
 * @ingroup filters
 * @brief Small-signal transfer function (no stage clamping) of a preset at one frequency, for analysis and tests.
 *
 * H = (1 + r g) M(L) / (1 + r a z^-1 L^4), L = (1 - p) / (1 - p z^-1), M(L) = w_in - w0 L + w1 L^2 - w2 L^3 + w3 L^4.
 * The z^-1 in the loop is the one sample of delay of the fed-back stage-4 output.
 */
[[nodiscard]] inline std::complex<double> poleMixingResponse(const PoleMixingPreset& preset, const double pole,
                                                             const double resonance, const double frequencyHz,
                                                             const double sampleRate) noexcept
{
    constexpr double twoPi{6.283185307179586476925286766559};
    const std::complex<double> zInv = std::polar(1.0, -twoPi * frequencyHz / sampleRate);
    const std::complex<double> lp = (1.0 - pole) / (1.0 - pole * zInv);
    const std::complex<double> lp2 = lp * lp;
    const std::complex<double> lp4 = lp2 * lp2;
    const auto& w = preset.stageWeights;
    const std::complex<double> mix = static_cast<double>(preset.inputWeight) - static_cast<double>(w[0]) * lp +
                                     static_cast<double>(w[1]) * lp2 - static_cast<double>(w[2]) * lp2 * lp +
                                     static_cast<double>(w[3]) * lp4;
    const double inputGain = 1.0 + resonance * preset.resonanceInputGain;
    const double feedback = resonance * preset.resonanceFeedback;
    return inputGain * mix / (1.0 + feedback * zInv * lp4);
}

/**
 * @ingroup filters
 * @brief Loop gain k = r * a at which the linear filter starts to self-oscillate, for a stage pole p in (0, 1).
 *
 * The smallest k for which 1 + k z^-1 L^4 has a root on the unit circle, found by a scan and bisection (about 1000
 * trig calls): for analysis, tests and setup code, not per sample. For p <= 1/3 (a cutoff above fs ln(3) / (2 pi),
 * about 0.175 fs) the loop can only oscillate at Nyquist, where k = ((1 + p) / (1 - p))^4.
 */
[[nodiscard]] inline double poleMixingOscillationThreshold(const double pole) noexcept
{
    const double pi = std::numbers::pi;
    const double nyquistThreshold = std::pow((1.0 + pole) / (1.0 - pole), 4.0);
    const auto phaseError = [pole, pi](const double w)
    { return w + 4.0 * std::atan2(pole * std::sin(w), 1.0 - pole * std::cos(w)) - pi; };
    const auto loopGainAt = [pole](const double w)
    {
        const double magnitudeSquared = (1.0 - 2.0 * pole * std::cos(w) + pole * pole) / ((1.0 - pole) * (1.0 - pole));
        return magnitudeSquared * magnitudeSquared;
    };
    constexpr int scanPoints{512};
    double threshold = nyquistThreshold;
    double previousW = 0.0;
    double previousError = phaseError(0.0);
    for (int i = 1; i < scanPoints; ++i)
    {
        const double w = pi * i / scanPoints;
        const double error = phaseError(w);
        if ((previousError < 0.0) != (error < 0.0))
        {
            double lo = previousW;
            double hi = w;
            for (int iteration = 0; iteration < 60; ++iteration)
            {
                const double mid = 0.5 * (lo + hi);
                if ((phaseError(mid) < 0.0) == (previousError < 0.0))
                {
                    lo = mid;
                }
                else
                {
                    hi = mid;
                }
            }
            threshold = std::min(threshold, loopGainAt(0.5 * (lo + hi)));
        }
        previousW = w;
        previousError = error;
    }
    return threshold;
}

/// @ingroup filters
/// @brief Stage pole and oscillation threshold that put the resonance peak at a given frequency.
struct PoleMixingTuning
{
    float pole;
    float oscillationThreshold; ///< loop gain r * a at which the filter starts to self-oscillate
};

/**
 * @ingroup filters
 * @brief Tuning that makes the filter self-oscillate exactly at peakHz, for 0 < peakHz < fs / 2.
 *
 * The inverse of poleMixingOscillationThreshold() in closed form (formulas in the file brief), four trig calls, cheap
 * enough per sample. p stays above 1/3 below Nyquist, so k is the real threshold; peakHz = fs / 6 gives p = 0.5, k = 9.
 * Below the threshold the response peak sits slightly lower: 1 to 2 percent at 95 percent of k, 4 to 5.5 percent at 85.
 */
[[nodiscard]] inline PoleMixingTuning poleMixingTuningForPeak(const float peakHz, const float sampleRate) noexcept
{
    constexpr float pi{std::numbers::pi_v<float>};
    const float w = 2.0f * pi * peakHz / sampleRate;
    const float phi = 0.25f * (pi - w);
    const float sinWPlusPhi = std::sin(w + phi);
    const float pole = std::sin(phi) / sinWPlusPhi;
    const float sinHalfW = std::sin(0.5f * w);
    const float oneMinusPole = 2.0f * std::cos(phi + 0.5f * w) * sinHalfW / sinWPlusPhi;
    const float ratio = 2.0f * sinHalfW / oneMinusPole;
    const float magnitudeSquared = 1.0f + pole * ratio * ratio;
    return {pole, magnitudeSquared * magnitudeSquared};
}

/**
 * @ingroup filters
 * @brief Pole-mixing multimode filter: four one-pole lowpass stages with resonance feedback from stage 4, output a
 * weighted mix of the input and the stage outputs. The variant whose resonance peak is placed by
 * poleMixingTuningForPeak(); see the file brief for how it differs from PoleMixingFilter.h.
 *
 * Resonance r >= 0 feeds -r * a * s4[n-1] back into the cascade input and boosts the input by (1 + r * g), a and g
 * taken from the preset. The loop self-oscillates when r * a exceeds a threshold that depends on the pole
 * (linear model, fs = 48 kHz): 4.05 at a stage cutoff of 100 Hz, 4.58 at 1 kHz, 6.2 at 3 kHz, 10.2 at 6 kHz, because
 * the stages lose phase lag and the one-sample feedback delay adds lag as the cutoff rises. The resonance peak then
 * lies above the stage cutoff (1.07 kHz at 1 kHz, 3.7 kHz at 3 kHz, 9.8 kHz at 6 kHz); to place the peak instead,
 * set the pole from poleMixingTuningForPeak() with setPole().
 *
 * Every stage clamps its state to [-1, 1]. That bounds the output for any resonance and also clips input above full
 * scale even at r = 0; it is part of the character, not a safety margin. Switching presets changes five weights and
 * keeps the stage states, so it does not click. A silent input lets the state decay towards denormals below the
 * threshold; denormal handling is up to the caller. The digital allpass mixes use 1 - 2 L, which is allpass only
 * approximately for an impulse-invariant one-pole, the error growing with the cutoff. Real-time safe.
 */
class PeakTunedPoleMixingFilter
{
  public:
    PeakTunedPoleMixingFilter() noexcept
    {
        updatePole();
        setPreset(0);
    }

    void setSampleRate(const float sampleRate) noexcept
    {
        m_sampleRate = sampleRate;
        updatePole();
    }

    /// Cutoff of each one-pole stage (not the resonance peak, see poleMixingTuningForPeak()).
    void setCutoff(const float cutoffHz) noexcept
    {
        m_cutoff = cutoffHz;
        updatePole();
    }

    /// Sets the stage pole directly, e.g. from poleMixingTuningForPeak(); a later setCutoff() or setSampleRate()
    /// replaces it.
    void setPole(const float pole) noexcept
    {
        m_pole = pole;
        for (auto& stage : m_stages)
        {
            stage.setFeedback(pole);
        }
    }

    /// Resonance r >= 0 (negative values are treated as 0).
    void setResonance(const float resonance) noexcept
    {
        m_resonance = resonance > 0.0f ? resonance : 0.0f;
    }

    /// Selects a row of kPoleMixingPresets; an index outside the table is ignored.
    void setPreset(const size_t index) noexcept
    {
        if (index >= kPoleMixingPresets.size())
        {
            return;
        }
        m_presetIndex = index;
        m_preset = &kPoleMixingPresets[index];
    }

    [[nodiscard]] size_t getPresetIndex() const noexcept
    {
        return m_presetIndex;
    }

    [[nodiscard]] float getPole() const noexcept
    {
        return m_pole;
    }

    void reset() noexcept
    {
        for (auto& stage : m_stages)
        {
            stage.reset();
        }
        m_lastStage4 = 0.0f;
    }

    [[nodiscard]] float process(const float in) noexcept
    {
        const auto& p = *m_preset;
        const float feed =
            in * (1.0f + m_resonance * p.resonanceInputGain) - m_lastStage4 * m_resonance * p.resonanceFeedback;
        const float s1 = m_stages[0].step(feed);
        const float s2 = m_stages[1].step(s1);
        const float s3 = m_stages[2].step(s2);
        const float s4 = m_stages[3].step(s3);
        m_lastStage4 = s4;
        const auto& w = p.stageWeights;
        return p.inputWeight * feed - w[0] * s1 + w[1] * s2 - w[2] * s3 + w[3] * s4;
    }

    void process(const float* in, float* out, const size_t numSamples) noexcept
    {
        for (size_t i = 0; i < numSamples; ++i)
        {
            out[i] = process(in[i]);
        }
    }

  private:
    using Stage = OnePoleFilter<OnePoleFilterCharacteristic::LowPass, true>;

    void updatePole() noexcept
    {
        constexpr float twoPi{2.0f * std::numbers::pi_v<float>};
        setPole(std::exp(-twoPi * std::max(m_cutoff, 0.0f) / m_sampleRate));
    }

    float m_sampleRate{48000.0f};
    float m_cutoff{1000.0f};
    float m_resonance{0.0f};
    float m_pole{0.0f};
    float m_lastStage4{0.0f};
    size_t m_presetIndex{0};
    const PoleMixingPreset* m_preset{&kPoleMixingPresets[0]};
    std::array<Stage, 4> m_stages{Stage(48000.0f), Stage(48000.0f), Stage(48000.0f), Stage(48000.0f)};
};

}
