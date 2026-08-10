#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <numbers>
#include <random>
#include <vector>

#include "Filters/OnePoleFilter.h"
#include "Filters/PinkFilter.h"
#include "Filters/PoleMixingFilter.h"
#include "Numbers/Convert.h"
#include "Numbers/Interpolation.h"

namespace AbacDsp
{

/// @ingroup generators
/// @brief Excitation source used to kick off a plucked string.
enum class PluckType : uint8_t
{
    WhiteRoundRobin,
    WhiteStatic,
    Pink,
    Brown,
    Triangle,
    Saw,
    Square,
    Spike,
};

/**
 * @ingroup generators
 * @brief Single plucked-string voice: a damped delay-line loop excited by a short burst.
 *
 * The delay line is oversampled 4x and read with linear interpolation, so the
 * loop length (and therefore pitch) can be tuned to a fraction of a sample
 * rather than snapping to the nearest integer period. The loop itself is
 * self-feeding: each period the oldest sample is damped and decayed back into
 * the same buffer position, which is what gives a plucked string its
 * characteristic exponential, brightness-losing decay.
 *
 * MaxLength bounds the buffer and therefore the lowest playable pitch, e.g.
 * MaxLength = 10000 supports down to about 4 * sampleRate/10000 (19.2 Hz at
 * 48 kHz).
 * @see https://ccrma.stanford.edu/~jos/pasp/Karplus_Strong_Algorithm.html
 */
template <size_t MaxLength>
class KarplusStrongString
{
  public:
    explicit KarplusStrongString(const float sampleRate)
        : m_sampleRate(sampleRate)
        , m_dynamicWaveTableBuffer(MaxLength + kReadLookback, 0.f)
        , m_dcFilter(sampleRate)
        , m_damper(sampleRate)
        , m_initFilter(sampleRate)
        , m_brownDamper(sampleRate)
    {
        m_dcFilter.setCutoff(20.f);
        m_damper.setCutoff(22000.f);
        m_brownDamper.setCutoff(10.f);
        m_initFilter.setParameterSmoothTimeMs(0.f);
        m_initFilter.setFilterCoefficients({0.f, 0.f, 0.f, 0.f, 1.f}); // Lp24: pass the last stage only
        setInitialFilterFactor(2.f);
    }

    void setPluckType(const PluckType pluckType) noexcept
    {
        m_pluckType = pluckType;
    }

    [[nodiscard]] float step() noexcept
    {
        switch (m_phase)
        {
            case Phase::Attack:
                if (m_stepsForNextPhase == 0)
                {
                    m_phase = Phase::Sustain;
                }
                else
                {
                    m_currentGain += m_gainAdvance;
                    --m_stepsForNextPhase;
                }
                break;
            case Phase::Release:
                if (m_stepsForNextPhase == 0)
                {
                    m_phase = Phase::Stopped;
                }
                else
                {
                    m_currentGain += m_gainAdvance;
                    --m_stepsForNextPhase;
                }
                break;
            case Phase::Stopped:
                return 0.f;
            case Phase::Sustain:
                break;
        }
        const auto stepsToAdvance = advance();
        for (size_t i = 0; i < stepsToAdvance; ++i)
        {
            computeNext(m_phase != Phase::Release);
        }
        const auto readIndex = (m_activePluck + m_currentBufferSize - kReadLookback) % m_currentBufferSize;
        const auto v = Interpolation::linearPt2(&m_dynamicWaveTableBuffer[readIndex], m_fraction);
        return m_dcFilter.step(v * m_currentGain);
    }

    void setFrequency(const float f) noexcept
    {
        m_baseFrequency = m_oversampleFactor * f;
        if (f <= 0.f)
        {
            return;
        }
        m_currentBufferSize =
            static_cast<size_t>(m_oversampleFactor) * static_cast<size_t>(std::lround(m_sampleRate / f));
        m_currentBufferSize = std::clamp<size_t>(m_currentBufferSize, kMinBufferSize, MaxLength);
        computeDecay();
    }

    void setSizeByNote(const float note, const float orchestraTuning = 440.f) noexcept
    {
        setFrequency(Convert::noteToFrequency(note - 12.f, orchestraTuning));
        m_damper.setCutoff(computeDamperCutoff(m_baseFrequency, m_damperFactor, 2.f, 0.25f));
        auto idealSize = m_oversampleFactor * m_sampleRate / Convert::noteToFrequency(note, orchestraTuning);
        idealSize = correctForDamperPhaseDelay(idealSize);
        m_pitchRatioBaseValue = m_oversampleFactor * static_cast<float>(m_currentBufferSize) / idealSize;
    }

    void trigger(const float note, const float gain, const float orchestraTuning = 440.f) noexcept
    {
        m_lastTriggeredNote = note;
        if (m_attackTimeSamples <= 1.f)
        {
            m_gainAdvance = 0.f;
            m_currentGain = gain;
            m_phase = Phase::Sustain;
            m_stepsForNextPhase = 0;
        }
        else
        {
            m_gainAdvance = gain / m_attackTimeSamples;
            m_currentGain = 0.f;
            m_phase = Phase::Attack;
            m_stepsForNextPhase = static_cast<int>(std::ceil(m_attackTimeSamples));
        }
        setSizeByNote(note, orchestraTuning);
        m_dcFilter.reset();
        m_damper.reset();
        m_initFilter.setCutoffFrequency(m_baseFrequency * m_transientFactor);
        m_initFilter.reset();
        std::ranges::fill(m_dynamicWaveTableBuffer, 0.f);
        m_activePluck = 0;
        m_pluckOffset = m_pluckType == PluckType::WhiteRoundRobin ? nextPluckOffset() : 0;
        m_fraction = 0.f;
        setRelativePitch(m_currentPitchBend);
        computeDecay();
    }

    void muteString() noexcept
    {
        m_phase = Phase::Stopped;
        m_currentGain = 0.f;
        m_stepsForNextPhase = 0;
    }

    void stopString() noexcept
    {
        if (m_releaseTimeSamples >= 1.f)
        {
            m_gainAdvance = -m_currentGain / m_releaseTimeSamples;
            m_phase = Phase::Release;
            m_stepsForNextPhase = static_cast<int>(std::ceil(m_releaseTimeSamples));
        }
        else
        {
            muteString();
        }
    }

    void attackTime(const float timeInMsecs) noexcept
    {
        m_attackTimeSamples = m_sampleRate * timeInMsecs * 0.001f;
    }

    void releaseTime(const float timeInMsecs) noexcept
    {
        m_releaseTimeSamples = m_sampleRate * timeInMsecs * 0.001f;
    }

    void bendInCents(const float cents) noexcept
    {
        m_currentPitchBend = Convert::centsToRelativePitch(cents);
        setRelativePitch(m_currentPitchBend);
        computeDecay();
    }

    void setConstFeed(const float value) noexcept
    {
        m_constantFeed = value * value * value;
    }

    void setDecayByTime(const float msecs) noexcept
    {
        m_decayInMilliseconds = msecs;
        computeDecay();
    }

    void setInitialFilterFactor(const float factor) noexcept
    {
        m_transientFactor = factor;
        m_initFilter.setCutoffFrequency(m_baseFrequency * m_transientFactor);
    }

    void setTransientResonance(const float reso) noexcept
    {
        m_initFilter.setResonance(reso);
    }

    void setDamper(const float damperFactor) noexcept
    {
        m_damperFactor = damperFactor;
        m_damper.setCutoff(computeDamperCutoff(m_baseFrequency, m_damperFactor, 2.f, 0.25f));
    }

    void setDamperCutoff(const float cutoff) noexcept
    {
        m_damper.setCutoff(cutoff);
    }

    void setDecayOctaveFactor(const float factor) noexcept
    {
        m_decayOctaveFactor = factor;
        computeDecay();
    }


    [[nodiscard]] bool isActive() const noexcept
    {
        return m_phase != Phase::Stopped;
    }

  private:
    enum class Phase : uint8_t
    {
        Attack,
        Sustain,
        Release,
        Stopped
    };

    static constexpr size_t kReadLookback{6}; // also the mirrored-wrap padding count; the two must match
    static constexpr size_t kMinBufferSize{113};
    static constexpr float kReferenceNote{60.f}; // C4; setDecayByTime() is exact at this note
    static constexpr size_t kPluckNoiseSize{MaxLength * 16};
    static constexpr unsigned kPluckNoiseSeed{2};

    // Deterministic and per-instance (not a shared static) so two independently-constructed
    // voices read identical content in WhiteStatic mode without relying on hidden shared state.
    [[nodiscard]] static std::vector<float> makePluckNoiseBuffer()
    {
        std::mt19937 rng{kPluckNoiseSeed};
        std::uniform_real_distribution<float> dist{-2.f, 2.f};
        std::vector<float> buffer(kPluckNoiseSize);
        std::ranges::generate(buffer, [&rng, &dist] { return dist(rng); });
        return buffer;
    }

    // m_damper is stepped once per oversampled loop tap (m_oversampleFactor times per audio
    // sample) but its coefficient is computed against m_sampleRate, so a cutoff c here behaves
    // like c * m_oversampleFactor in real audio terms; sampleRate/4 is the top of that range
    // that stays under OnePoleFilter's own sampleRate/2 bypass clamp.
    [[nodiscard]] float computeDamperCutoff(const float baseFrequency, const float damperFactor, const float midFactor,
                                            const float lowFactor) const noexcept
    {
        constexpr float halfPoint = 0.5f;
        const float nyquist2 = m_sampleRate * 0.25f;
        const float midFreq = baseFrequency * midFactor;
        const float lowFreq = baseFrequency * lowFactor;
        return damperFactor < halfPoint ? nyquist2 * std::pow(midFreq / nyquist2, 2.0f * damperFactor)
                                        : midFreq * std::pow(lowFreq / midFreq, 2.0f * (damperFactor - halfPoint));
    }

    // The damper sits inside the feedback loop, so its phase lag at the loop's own resonant
    // frequency (2*pi/currentBufferSize radians per tap) adds to the effective loop period;
    // this scales idealSize down by that same ratio so the resulting pitch stays on target.
    [[nodiscard]] float correctForDamperPhaseDelay(const float idealSize) const noexcept
    {
        const auto bufferSize = static_cast<float>(m_currentBufferSize);
        const auto omega = 2.f * std::numbers::pi_v<float> / bufferSize;
        const auto p = m_damper.feedback();
        const auto delayTaps = std::atan2(p * std::sin(omega), 1.f - p * std::cos(omega)) / omega;
        return idealSize * bufferSize / (bufferSize + delayTaps);
    }

    void computeDecay() noexcept
    {
        const auto octavesFromReference = (m_lastTriggeredNote - kReferenceNote) / 12.f;
        const auto effectiveDecayMs = m_decayInMilliseconds * std::exp2(-m_decayOctaveFactor * octavesFromReference);
        const auto periodInSamples = static_cast<float>(m_currentBufferSize) / m_advancePhase;
        const auto decayFactor = periodInSamples / (m_sampleRate * effectiveDecayMs / 1000.f);
        m_decayGain = std::pow(0.1f, decayFactor); // -20 dB assumed as the decay-time reference level
    }

    void setRelativePitch(const float relativePitchChange) noexcept
    {
        m_advancePhase = m_pitchRatioBaseValue * relativePitchChange;
    }

    [[nodiscard]] size_t nextPluckOffset() noexcept
    {
        std::uniform_int_distribution<size_t> dist(0, m_pluckNoiseBuffer.size() - 1);
        return dist(m_rng);
    }

    [[nodiscard]] float nextWhiteSample() noexcept
    {
        return m_uniformDist(m_rng);
    }

    [[nodiscard]] float nextBrownSample() noexcept
    {
        m_brownState = std::clamp(m_brownState + nextWhiteSample() * 0.125f, -1.f, 1.f);
        return m_brownDamper.step(m_brownState);
    }

    [[nodiscard]] float nextPluckValue() noexcept
    {
        switch (m_pluckType)
        {
            case PluckType::WhiteRoundRobin:
            case PluckType::WhiteStatic:
                m_pluckOffset = (m_pluckOffset + 1) % m_pluckNoiseBuffer.size();
                return m_pluckNoiseBuffer[m_pluckOffset];
            case PluckType::Pink:
                return m_pinkFilter.step(nextWhiteSample());
            case PluckType::Brown:
                return nextBrownSample() * 0.25f;
            case PluckType::Square:
                m_pluckOffset = (m_pluckOffset + 1) % m_currentBufferSize;
                return m_pluckOffset * 2 > m_currentBufferSize ? 0.25f : -0.25f;
            case PluckType::Triangle:
            {
                m_pluckOffset = (m_pluckOffset + 1) % m_currentBufferSize;
                const auto ratio = 4.0f * static_cast<float>(m_pluckOffset) / static_cast<float>(m_currentBufferSize);
                return 0.5f * (m_pluckOffset < m_currentBufferSize / 2 ? ratio - 1.0f : 3.0f - ratio);
            }
            case PluckType::Spike:
                m_pluckOffset = (m_pluckOffset + 1) % m_currentBufferSize;
                return m_pluckOffset < m_currentBufferSize / 16.f  ? -0.5f
                       : m_pluckOffset < m_currentBufferSize / 8.f ? 0.5f
                                                                   : 0.f;
            case PluckType::Saw:
                m_pluckOffset = (m_pluckOffset + 1) % m_currentBufferSize;
                return static_cast<float>(m_pluckOffset) / static_cast<float>(m_currentBufferSize) - 0.5f;
        }
        return 0.f;
    }

    void computeNext(const bool useConstFeed) noexcept
    {
        m_currentBufferSize = std::min(m_currentBufferSize, MaxLength - 1);
        if (m_activePluck >= m_currentBufferSize)
        {
            const auto i = m_activePluck % m_currentBufferSize;
            if (i == 0 && m_pluckType == PluckType::WhiteRoundRobin)
            {
                m_pluckOffset = nextPluckOffset();
            }
            const auto filtered = m_initFilter.step(nextPluckValue());
            if (useConstFeed)
            {
                m_dynamicWaveTableBuffer[i] = std::clamp(
                    m_damper.step(m_decayGain * m_dynamicWaveTableBuffer[i] + m_constantFeed * filtered), -2.f, 2.f);
            }
            else
            {
                m_dynamicWaveTableBuffer[i] =
                    std::clamp(m_damper.step(m_decayGain * m_dynamicWaveTableBuffer[i]), -2.f, 2.f);
            }
            if (i < kReadLookback)
            {
                m_dynamicWaveTableBuffer[i + m_currentBufferSize] = m_dynamicWaveTableBuffer[i];
            }
        }
        else
        {
            m_dynamicWaveTableBuffer[m_activePluck] = m_initFilter.step(nextPluckValue());
            if (m_activePluck < kReadLookback)
            {
                m_dynamicWaveTableBuffer[m_activePluck + m_currentBufferSize] = m_dynamicWaveTableBuffer[m_activePluck];
            }
        }
        ++m_activePluck;
    }

    [[nodiscard]] size_t advance() noexcept
    {
        m_fraction += m_advancePhase;
        if (m_fraction < 1.0f)
        {
            return 0;
        }
        const auto stepsToAdvance = static_cast<size_t>(m_fraction);
        m_fraction -= static_cast<float>(stepsToAdvance);
        return stepsToAdvance;
    }

    float m_sampleRate;
    PluckType m_pluckType{PluckType::WhiteRoundRobin};
    std::vector<float> m_dynamicWaveTableBuffer;
    std::vector<float> m_pluckNoiseBuffer{makePluckNoiseBuffer()};

    OnePoleFilter<OnePoleFilterCharacteristic::HighPass, false> m_dcFilter;
    OnePoleFilter<OnePoleFilterCharacteristic::LowPass, false> m_damper;
    Filter1Pole4StageSmooth m_initFilter;
    OnePoleFilter<OnePoleFilterCharacteristic::LowPass, false> m_brownDamper;
    PinkFilter<false> m_pinkFilter;

    std::mt19937 m_rng{std::random_device{}()};
    std::uniform_real_distribution<float> m_uniformDist{-1.f, 1.f};
    float m_brownState{0.f};

    float m_oversampleFactor{4.f};
    float m_pitchRatioBaseValue{1.0f};
    float m_baseFrequency{1000.f};
    float m_advancePhase{1.f};
    float m_fraction{0.f};
    size_t m_currentBufferSize{366U};

    float m_decayInMilliseconds{3500.f};
    float m_decayGain{0.99259f};
    float m_decayOctaveFactor{1.f};
    float m_lastTriggeredNote{kReferenceNote};

    size_t m_activePluck{0U};
    size_t m_pluckOffset{0};
    float m_currentPitchBend{1.0f};
    float m_damperFactor{0.f};
    float m_transientFactor{4.f};
    Phase m_phase{Phase::Stopped};
    float m_attackTimeSamples{0.f};
    float m_releaseTimeSamples{0.f};
    int m_stepsForNextPhase{0};
    float m_currentGain{0.f};
    float m_gainAdvance{0.f};
    float m_constantFeed{0.f};
};

}
