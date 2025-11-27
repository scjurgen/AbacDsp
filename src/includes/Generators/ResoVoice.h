#pragma once

#include <array>
#include <iostream>
#include <memory>
#include <random>
#include "Filters/BiquadResoBP.h"
#include "Excitation.h"
#include "Numbers/Convert.h"


/* Resonance voice with NumElements independent generators
 *
 * TODO:
 *   for MPE instruments:
 *     - the active voices need to be marked with a flag so
 *     - the excitation should be used only on them.
 *     - the excitation strength should be relative and have an
 *       extra setter, e.g. excitation only on high overtones or fifth etc.
 *
 *
 *       ratio, level,decayMs, excitation, pan, waitMs
 *
 */
namespace AbacDsp::Reso
{
struct VoiceSetting
{
    explicit VoiceSetting(const float frequency_, const float impulseLevel_, const float decayTime_,
                          const float sustainFeedLevel_, const float pan_, const size_t impulseWaitBlocks_)
        : frequency(frequency_)
        , impulseLevel(impulseLevel_)
        , decayTime(decayTime_)
        , sustainFeedLevel(sustainFeedLevel_)
        , pan(pan_)
        , impulseWaitBlocks(impulseWaitBlocks_)
    {
    }
    float frequency;
    float impulseLevel;
    float decayTime;
    float sustainFeedLevel;
    float pan;
    size_t impulseWaitBlocks;
};


// Optimized 2D compensation lookup table for resonator synthesis
// Perfect exponential grid spacing with rounding to handle floating-point errors
//
// Grid: 10 frequencies × 15 decay times = 150 floats = 600 bytes
// Frequency: 16.3516 Hz × 2^n (n=0..9)
// Decay: 0.001000 s × 2^n (n=0..14)

class ResoCompensationLUT
{
  public:
    static constexpr int kNumFreqs = 10;
    static constexpr int kNumDecays = 15;

    // Grid base values for direct index calculation
    static constexpr float kFreqBase = 16.35160000f;
    static constexpr float kDecayBase = 0.00100000f;
    static constexpr float kLogFreqBase = 2.79432575f;
    static constexpr float kLogDecayBase = -6.90775528f;
    static constexpr float kInvLog2 = 1.44269504f;

    // Precomputed log(LUT) values for interpolation [freq][decay]
    static constexpr std::array<std::array<float, kNumDecays>, kNumFreqs> kLogLUT = {
        {{{0.47928981f, 0.47647251f, 0.46854549f, 0.45475567f, 0.43465811f, 0.41621417f, 0.42906439f, 0.52604756f,
           0.77455899f, 1.13745187f, 1.55665573f, 2.10298042f, 2.72553263f, 3.85932620f, 4.02325629f}},
         {{0.47655464f, 0.46829948f, 0.45455872f, 0.43467510f, 0.41682539f, 0.42902446f, 0.52714981f, 0.77051647f,
           1.13725229f, 1.55671739f, 2.10176077f, 2.71903431f, 3.42162256f, 4.03724048f, 4.83457580f}},
         {{0.46835060f, 0.45442009f, 0.43465193f, 0.41674954f, 0.42891389f, 0.52695162f, 0.77108278f, 1.13764838f,
           1.55670316f, 2.10183440f, 2.71792479f, 3.37355790f, 4.08156655f, 5.06209176f, 5.86852505f}},
         {{0.45446734f, 0.43458089f, 0.41674954f, 0.42890315f, 0.52698549f, 0.77121035f, 1.13749866f, 1.55674585f,
           2.10185894f, 2.71799297f, 3.37184054f, 4.03684387f, 4.69120204f, 5.21349472f, 5.71486668f}},
         {{0.43459324f, 0.41681629f, 0.42901218f, 0.52714303f, 0.77132712f, 1.13772949f, 1.55684072f, 2.10198985f,
           2.71810054f, 3.37205323f, 4.03796612f, 4.71684392f, 5.44585033f, 6.36728736f, 7.20997586f}},
         {{0.41699684f, 0.42904596f, 0.52728196f, 0.77164076f, 1.13803838f, 1.55769972f, 2.10250548f, 2.71926788f,
           3.37275571f, 4.03853906f, 4.71567949f, 5.40069345f, 6.09029090f, 6.73312993f, 7.38560239f}},
         {{0.43082599f, 0.52785480f, 0.77515148f, 1.13923743f, 1.55985283f, 2.10457885f, 2.72153218f, 3.37610290f,
           4.04080000f, 4.71799068f, 5.40315802f, 6.09471174f, 6.85536209f, 7.90156171f, 8.77164038f}},
         {{0.53900050f, 0.77992368f, 1.15150370f, 1.56679997f, 2.11288251f, 2.73052436f, 3.38544117f, 4.05833863f,
           4.73356167f, 5.41559753f, 6.10318301f, 6.79099499f, 7.41599758f, 7.83823368f, 8.07319671f}},
         {{0.82640204f, 1.16989349f, 1.59514554f, 2.14656473f, 2.76692638f, 3.42320682f, 4.09508684f, 4.77250658f,
           5.45777980f, 6.14412634f, 6.83304677f, 7.52082591f, 8.13838696f, 8.54845685f, 8.77529057f}},
         {{1.25547665f, 1.71819865f, 2.28957016f, 2.92011102f, 3.58151854f, 4.24865726f, 4.92844351f, 5.61490384f,
           6.30470240f, 6.99442817f, 7.68442756f, 8.36779522f, 8.94604720f, 9.30905634f, 9.50471143f}}}};

    // O(1) compensation with rounding to handle floating-point errors
    static float Compensate(const float frequency_hz, const float decay_seconds)
    {
        const auto log_f = std::log(frequency_hz);
        const auto log_d = std::log(decay_seconds);

        // Direct index calculation
        const auto freq_index_float = (log_f - kLogFreqBase) * kInvLog2;
        const auto decay_index_float = (log_d - kLogDecayBase) * kInvLog2;

        // Round to nearest integer to handle FP errors (e.g., 5.9999 → 6.0)
        const auto freq_idx_rounded = std::round(freq_index_float);
        const auto decay_idx_rounded = std::round(decay_index_float);

        // Clamp rounded indices to valid range
        const auto i = std::clamp<size_t>(static_cast<size_t>(freq_idx_rounded), 0, kNumFreqs - 2);
        const auto j = std::clamp<size_t>(static_cast<size_t>(decay_idx_rounded), 0, kNumDecays - 2);

        // Bilinear interpolation weights
        const auto wf = freq_index_float - static_cast<float>(i);
        const auto wd = decay_index_float - static_cast<float>(j);

        const auto wf_clamped = std::clamp(wf, 0.0f, 1.0f);
        const auto wd_clamped = std::clamp(wd, 0.0f, 1.0f);

        // Bilinear interpolation in log space
        const auto log_v = (1.0f - wf_clamped) * (1.0f - wd_clamped) * kLogLUT[i][j] +
                           (1.0f - wf_clamped) * wd_clamped * kLogLUT[i][j + 1] +
                           wf_clamped * (1.0f - wd_clamped) * kLogLUT[i + 1][j] +
                           wf_clamped * wd_clamped * kLogLUT[i + 1][j + 1];

        return std::exp(log_v);
    }
};

template <size_t BlockSize, size_t NumElements>
class ResoVoice
{
  public:
    enum class PlayState
    {
        Off,
        Active,
        Waiting
    };

    explicit ResoVoice(const float sampleRate)
        : m_sampleRate(sampleRate)
        , m_excitation(1024)
        , m_rng(std::random_device{}())
    {
        for (auto& f : m_bq)
        {
            f.setSampleRate(sampleRate);
        }
    }
    void resetVoices()
    {
        m_lastGen = 0;
    }

    void setImpulseMix(const float value) noexcept
    {
        m_excitation.setNoise(value);
    }

    void setSustainFeedLevel(const float value)
    {
        m_freeExcitationAmount = value;
    }


    void addGenerator(const VoiceSetting& vs)
    {
        if (m_lastGen >= NumElements)
        {
            return;
        }
        m_bq[m_lastGen].setByDecay(0, vs.frequency, vs.decayTime);
        Convert::getPanFactorNormalized(vs.pan, m_pan[0][m_lastGen], m_pan[1][m_lastGen]);
        m_sustainFeedLevel[m_lastGen] = vs.sustainFeedLevel;
        if (vs.impulseLevel == 0.f) // no power, only sustainFeed
        {
            m_activeState[m_lastGen] = PlayState::Active;
        }
        else
        {
            m_impulse[m_lastGen] = static_cast<float>(m_excitation.getPatternLength() - 1);
            m_impulseGain[m_lastGen] = vs.impulseLevel * ResoCompensationLUT::Compensate(vs.frequency, vs.decayTime);
            m_impulseWait[m_lastGen] = vs.impulseWaitBlocks;
            m_activeState[m_lastGen] = vs.impulseWaitBlocks == 0 ? PlayState::Active : PlayState::Waiting;
        }
        const auto patternLength = static_cast<float>(m_excitation.getPatternLength());
        const auto samplesForTwoPeriods = 2.f / vs.frequency * m_sampleRate;
        m_phaseAdvance[m_lastGen] = patternLength / samplesForTwoPeriods;
        m_lastGen++;
    }

    void processBlock(std::array<std::array<float, BlockSize>, 2>& out) noexcept
    {
        std::ranges::fill(out[0], 0.f);
        std::ranges::fill(out[1], 0.f);
        for (size_t j = 0; j < m_bq.size(); ++j)
        {
            if (m_activeState[j] == PlayState::Off)
            {
                continue;
            }
            if (m_activeState[j] == PlayState::Waiting)
            {
                --m_impulseWait[j];
                if (m_impulseWait[j] == 0)
                {
                    m_activeState[j] = PlayState::Active;
                }
            }
            if (m_activeState[j] == PlayState::Active)
            {
                if (m_impulse[j] <= 0.0f)
                {
                    if (m_freeExcitationAmount == 0.f)
                    {
                        for (size_t i = 0; i < BlockSize; ++i)
                        {
                            const auto v = m_bq[j].step0();
                            out[0][i] += v * m_pan[0][j];
                            out[1][i] += v * m_pan[1][j];
                        }
                    }
                    else
                    {
                        for (size_t i = 0; i < BlockSize; ++i)
                        {
                            const float ex = m_freeExcitationAmount * m_dist(m_rng);
                            const auto v = m_bq[j].step(ex * m_sustainFeedLevel[j]);
                            out[0][i] += v * m_pan[0][j];
                            out[1][i] += v * m_pan[1][j];
                        }
                    }
                }
                else
                {
                    for (size_t i = 0; i < BlockSize; ++i)
                    {
                        if (m_impulse[j] > 0.0f)
                        {
                            const auto excitationValue = m_excitation.getInterpolatedValue(m_impulse[j]);
                            const auto ex = m_impulseGain[j] * excitationValue;
                            const auto v = m_bq[j].step(ex);
                            out[0][i] += v * m_pan[0][j];
                            out[1][i] += v * m_pan[1][j];
                            m_impulse[j] -= m_phaseAdvance[j];
                            if (m_impulse[j] <= 0.0f)
                            {
                                m_impulseGain[j] = 0.f;
                            }
                        }
                        else
                        {
                            const auto v = m_bq[j].step0();
                            out[0][i] += v * m_pan[0][j];
                            out[1][i] += v * m_pan[1][j];
                        }
                    }
                }
            }
        }
    }

    void setDampMode(const bool mode)
    {
        for (auto& b : m_bq)
        {
            b.damp(mode);
        }
        if (mode)
        {
            std::ranges::fill(m_impulseWait, 0);
        }
    }

  private:
    float m_sampleRate;
    size_t m_lastGen{0};
    Excitation m_excitation;
    float m_freeExcitationAmount{0.f};

    std::array<BiquadResoBP, NumElements> m_bq{};
    std::array<size_t, NumElements> m_impulseWait{};
    std::array<float, NumElements> m_impulse{};
    std::array<float, NumElements> m_impulseGain{};
    std::array<float, NumElements> m_phaseAdvance{};
    std::array<float, NumElements> m_sustainFeedLevel{};
    std::array<std::array<float, NumElements>, 2> m_pan{};
    std::array<PlayState, NumElements> m_activeState{};
    std::mt19937 m_rng;
    std::uniform_real_distribution<float> m_dist{-1.0f, 1.0f};
};
}