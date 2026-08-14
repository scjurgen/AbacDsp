#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>

#include "Filters/PoleMixingFilter.h"
#include "Generators/AdsEnvelope.h"
#include "Generators/ExcitationTechnique.h"
#include "Generators/KarplusStrongString.h"
#include "Generators/OrnsteinUhlenbeckProcess.h"
#include "Modulation/SineModulation.h"
#include "Numbers/Convert.h"

namespace AbacDsp
{

/**
 * @ingroup generators
 * @brief One plucked-string voice: KarplusStrongString feeding a resonant VCF.
 *
 * The VCF's cutoff is expressed in musical terms rather than a fixed Hz
 * value: a reference cutoff set at C4 (note 60), moved by keyboard tracking
 * relative to the plucked note (Minimoog "KYBD"-style: 0 fixed, 1 tracks 1:1,
 * 2 double tracking), a filter envelope, and an LFO, all summed in octaves
 * before being applied. There is no separate VCA: KarplusStrongString already
 * carries its own attack/decay/sustain (attackTime(), setDecayByTime(),
 * setConstFeed()), which is the plucked string's natural amplitude behaviour.
 */
template <size_t MaxLength>
class KarplusStrongVoice
{
  public:
    static constexpr float kReferenceNote{60.f};   // C4
    static constexpr float kFilterEnvOctaves{4.f}; // placeholder depth, needs tuning by ear

    explicit KarplusStrongVoice(const float sampleRate)
        : m_sampleRate(sampleRate)
        , m_string(sampleRate)
        , m_vcf(sampleRate)
        , m_filterEnvelope(sampleRate)
        , m_filterLfo(sampleRate)
        , m_windOu(sampleRate)
        , m_rubOu(sampleRate)
    {
        m_string.setDamperCutoff(24000);
        m_vcf.setFilterCoefficients({0.f, 0.f, 0.f, 0.f, 1.f}); // Lp24: pass the last stage only
        m_vcf.setParameterSmoothTimeMs(kVcfSmoothingMs);
        m_filterLfo.setModulationDepth(1.f);
        m_windOu.setSigma(kWindSigma);
        m_rubOu.setSigma(kRubSigma);
        updateReferenceCutoff();
    }

    void trigger(const float note, const float gain, const float tuning = 440.f) noexcept
    {
        m_pluckedNote = note;
        m_tuning = tuning;
        m_string.trigger(note, gain, tuning);
        m_filterEnvelope.trigger();
        updateReferenceCutoff();
    }

    void stopString() noexcept
    {
        m_string.stopString();
    }

    void muteString() noexcept
    {
        m_string.muteString();
    }

    [[nodiscard]] bool isActive() const noexcept
    {
        return m_string.isActive();
    }

    // String-level pass-throughs (KarplusStrongString's own amplitude and excitation controls).
    void setPluckType(const PluckType pluckType) noexcept
    {
        m_string.setPluckType(pluckType);
    }

    void setDamper(const float damperFactor) noexcept
    {
        m_string.setDamper(damperFactor);
    }

    [[nodiscard]] float damperFactor() const noexcept
    {
        return m_string.damperFactor();
    }

    [[nodiscard]] float initialFilterFactor() const noexcept
    {
        return m_string.initialFilterFactor();
    }

    [[nodiscard]] float attackTimeMsecs() const noexcept
    {
        return m_string.attackTimeMsecs();
    }

    void setDecayByTime(const float msecs) noexcept
    {
        m_string.setDecayByTime(msecs);
    }

    void setDecayOctaveFactor(const float factor) noexcept
    {
        m_string.setDecayOctaveFactor(factor);
    }

    // No longer a direct pass-through: kept as the "base" sustain level (the UI dial's own
    // per-block gate) and combined with any active excitation technique's own contribution
    // each step(), so the two can't silently overwrite each other.
    void setConstFeed(const float value) noexcept
    {
        m_baseConstFeed = value;
    }

    void attackTime(const float timeInMsecs) noexcept
    {
        m_string.attackTime(timeInMsecs);
    }

    // An idle m_current takes the event directly; an occupied one replaces the single
    // m_next slot instead, so calls queue at most one deep. startMs/endMs are measured
    // from promotion to m_current, not from this call.
    void scheduleExcitation(const ExcitationEvent& event) noexcept
    {
        const auto scheduled = makeScheduledExcitation(event);
        if (m_current.phase == ExcitationPhase::Idle)
        {
            m_current = scheduled;
        }
        else
        {
            m_next = scheduled;
        }
    }

    void bendInCents(const float cents) noexcept
    {
        m_string.bendInCents(cents);
    }

    // Filter-stage controls.
    void setFilterCutoffSemitones(const float semitones) noexcept
    {
        m_filterCutoffSemitones = semitones;
        updateReferenceCutoff();
    }

    void setFilterResonance(const float resonance) noexcept
    {
        m_vcf.setResonance(resonance);
    }

    void setKeyTracking(const float contourFilter) noexcept
    {
        m_keyTracking = contourFilter;
    }

    void setFilterLfoRateHz(const float rateHz) noexcept
    {
        m_filterLfo.setModulationSpeed(rateHz);
    }

    void setFilterLfoSpeed(const float rateHz) noexcept
    {
        m_filterLfo.setModulationSpeed(rateHz);
    }

    void setFilterLfoDepthOctaves(const float depthOctaves) noexcept
    {
        m_lfoDepthOctaves = depthOctaves;
    }


    void setFilterEnvelope(const float attackMsecs, const float decayMsecs, const float sustainLevel,
                           const float curve = 0.f) noexcept
    {
        m_filterEnvelope.setAttackDecaySustain(attackMsecs, decayMsecs, sustainLevel, curve);
    }

    [[nodiscard]] float step() noexcept
    {
        tickExcitation();
        m_string.setConstFeed(std::clamp(m_baseConstFeed + m_excitationConstFeed, 0.f, 1.f));
        const auto envValue = m_filterEnvelope.step();
        m_filterLfo.tick();
        const auto trackedOctaves = m_keyTracking * (m_pluckedNote - kReferenceNote) / 12.f;
        const auto envOctaves = envValue * kFilterEnvOctaves;
        const auto lfoOctaves = m_lfoDepthOctaves * m_filterLfo.lastValue();
        const auto cutoffHz = std::clamp(m_referenceCutoffHz * std::exp2(trackedOctaves + envOctaves + lfoOctaves),
                                         20.f, m_sampleRate * 0.45f);
        m_vcf.setCutoffFrequency(cutoffHz);
        return m_vcf.step(m_string.step());
    }

  private:
    static constexpr float kVcfSmoothingMs{5.f};          // absorbs knob jumps without lagging the LFO/envelope
    static constexpr float kStrikeBrightnessFactor{1.5f}; // Strike vs. Pluck initial-filter brightness
    static constexpr float kStrikeMaxAttackMsecs{2.f};    // Strike's attack is clamped this short or shorter
    static constexpr float kPalmMuteDamperRange{0.9f};    // full-strength PalmMute raises the damper by this much
    // A coherent sine reinforces the loop's resonance far more efficiently than Bow's noise
    // at the same constFeed setting, hence the much smaller scale here.
    static constexpr float kSympatheticFeedScale{0.08f};
    static constexpr float kWindSigma{0.02f}; // slow reversion, wide excursions: gusting
    static constexpr float kWindFluctuationDepth{0.5f};
    static constexpr float kRubSigma{0.5f}; // fast reversion, tight excursions: scraping jitter
    static constexpr float kRubFluctuationDepth{0.4f};

    enum class ExcitationPhase : uint8_t
    {
        Idle,
        WaitingToStart,
        Active,
    };

    // endMs's meaning is per-type (see Excitation.h): a window end for the continuous/hold
    // families, or Mute's fade duration. durationSamples is precomputed from it at schedule
    // time since m_current's own startMs delay consumes samplesUntilStart before it's used.
    struct ScheduledExcitation
    {
        ExcitationPhase phase{ExcitationPhase::Idle};
        ExcitationType type{ExcitationType::Pluck};
        float strength{0.f};
        std::optional<float> harmonic;
        size_t samplesUntilStart{0};
        size_t durationSamples{0};
        size_t samplesRemaining{0};
        float savedDamperFactor{0.f}; // PalmMute only
    };

    [[nodiscard]] size_t msToSamples(const float ms) const noexcept
    {
        return static_cast<size_t>(std::max(0.f, ms) * m_sampleRate * 0.001f);
    }

    [[nodiscard]] ScheduledExcitation makeScheduledExcitation(const ExcitationEvent& event) const noexcept
    {
        ScheduledExcitation scheduled{};
        scheduled.phase = ExcitationPhase::WaitingToStart;
        scheduled.type = event.type;
        scheduled.strength = event.strength;
        scheduled.harmonic = event.harmonic;
        scheduled.samplesUntilStart = msToSamples(event.startMs);
        const auto durationMs =
            event.type == ExcitationType::Mute ? event.endMs : std::max(0.f, event.endMs - event.startMs);
        scheduled.durationSamples = msToSamples(durationMs);
        return scheduled;
    }

    void beginStrike(const ScheduledExcitation& s) noexcept
    {
        const auto savedFactor = m_string.initialFilterFactor();
        const auto savedAttackMs = m_string.attackTimeMsecs();
        m_string.setInitialFilterFactor(savedFactor * kStrikeBrightnessFactor);
        m_string.attackTime(std::min(savedAttackMs, kStrikeMaxAttackMsecs));
        m_string.trigger(m_pluckedNote, s.strength, m_tuning);
        m_string.setInitialFilterFactor(savedFactor);
        m_string.attackTime(savedAttackMs);
    }

    void beginPalmMute(ScheduledExcitation& s) noexcept
    {
        s.savedDamperFactor = m_string.damperFactor();
        const auto raised = std::clamp(s.savedDamperFactor + s.strength * kPalmMuteDamperRange, 0.f, 1.f);
        m_string.setDamper(raised);
    }

    void beginContinuous(const ScheduledExcitation& s) noexcept
    {
        if (!m_string.isActive())
        {
            m_string.wakeSustain(s.strength);
        }
        if (s.type == ExcitationType::Sympathetic)
        {
            m_string.setSympatheticExcitation(true, s.harmonic.value_or(1.f));
        }
    }

    void beginExcitation(ScheduledExcitation& s) noexcept
    {
        s.samplesRemaining = 0; // fire-and-forget default; the holding families override below
        switch (s.type)
        {
            case ExcitationType::Pluck:
                m_string.trigger(m_pluckedNote, s.strength, m_tuning);
                break;
            case ExcitationType::Strike:
                beginStrike(s);
                break;
            case ExcitationType::Mute:
                m_string.muteWithFade(s.durationSamples);
                break;
            case ExcitationType::PalmMute:
                beginPalmMute(s);
                s.samplesRemaining = s.durationSamples;
                break;
            case ExcitationType::Bow:
            case ExcitationType::Sympathetic:
            case ExcitationType::Wind:
            case ExcitationType::Rub:
                beginContinuous(s);
                s.samplesRemaining = s.durationSamples;
                break;
        }
    }

    // Normalizes the OU process's own sigma-coupled amplitude out via its theoretical
    // stationary stddev (sigma/2.33/sqrt(2*theta) - see documentation/OrnsteinUhlenbeck),
    // so sigma only selects fluctuation *speed* here; strength/depth set the audible size.
    [[nodiscard]] static float fluctuatingFeed(OrnsteinUhlenbeckProcess& ou, const float sigma, const float strength,
                                               const float depth) noexcept
    {
        const auto theta = sigma * 20.f + 1.f;
        const auto stdDev = sigma / 2.33f / std::sqrt(2.f * theta);
        const auto normalized = stdDev > 0.f ? (ou.step() - sigma) / stdDev : 0.f;
        return std::clamp(strength * (1.f + depth * normalized), 0.f, 1.f);
    }

    void stepActiveExcitation(const ScheduledExcitation& s) noexcept
    {
        switch (s.type)
        {
            case ExcitationType::Bow:
                m_excitationConstFeed = s.strength;
                break;
            case ExcitationType::Sympathetic:
                m_excitationConstFeed = s.strength * kSympatheticFeedScale;
                break;
            case ExcitationType::Wind:
                m_excitationConstFeed = fluctuatingFeed(m_windOu, kWindSigma, s.strength, kWindFluctuationDepth);
                break;
            case ExcitationType::Rub:
                m_excitationConstFeed = fluctuatingFeed(m_rubOu, kRubSigma, s.strength, kRubFluctuationDepth);
                break;
            case ExcitationType::Pluck:
            case ExcitationType::Strike:
            case ExcitationType::Mute:
            case ExcitationType::PalmMute:
                break; // PalmMute just holds via the sample counters; nothing per-sample here
        }
    }

    void endExcitation(const ScheduledExcitation& s) noexcept
    {
        switch (s.type)
        {
            case ExcitationType::PalmMute:
                m_string.setDamper(s.savedDamperFactor);
                break;
            case ExcitationType::Bow:
            case ExcitationType::Wind:
            case ExcitationType::Rub:
                m_excitationConstFeed = 0.f;
                break;
            case ExcitationType::Sympathetic:
                m_excitationConstFeed = 0.f;
                m_string.setSympatheticExcitation(false, 1.f);
                break;
            case ExcitationType::Pluck:
            case ExcitationType::Strike:
            case ExcitationType::Mute:
                break; // fire-and-forget; nothing to tear down
        }
    }

    void promoteNext() noexcept
    {
        m_current = m_next;
        m_next = ScheduledExcitation{};
    }

    void tickExcitation() noexcept
    {
        if (m_current.phase == ExcitationPhase::WaitingToStart)
        {
            if (m_current.samplesUntilStart > 0)
            {
                --m_current.samplesUntilStart;
                return;
            }
            beginExcitation(m_current);
            m_current.phase = ExcitationPhase::Active;
        }
        if (m_current.phase != ExcitationPhase::Active)
        {
            return;
        }
        stepActiveExcitation(m_current);
        if (m_current.samplesRemaining == 0)
        {
            endExcitation(m_current);
            promoteNext();
        }
        else
        {
            --m_current.samplesRemaining;
        }
    }

    void updateReferenceCutoff() noexcept
    {
        m_referenceCutoffHz =
            Convert::noteToFrequency(kReferenceNote, m_tuning) * Convert::noteIntervalToRatio(m_filterCutoffSemitones);
    }

    float m_sampleRate;
    KarplusStrongString<MaxLength> m_string;
    Filter1Pole4StageSmooth m_vcf;
    AdsEnvelope m_filterEnvelope;
    SineModulation m_filterLfo;
    OrnsteinUhlenbeckProcess m_windOu;
    OrnsteinUhlenbeckProcess m_rubOu;

    float m_pluckedNote{kReferenceNote};
    float m_tuning{440.f};
    float m_filterCutoffSemitones{0.f};
    float m_keyTracking{0.f};
    float m_lfoDepthOctaves{0.f};
    float m_referenceCutoffHz{1000.f};
    float m_baseConstFeed{0.f};
    float m_excitationConstFeed{0.f};
    ScheduledExcitation m_current{};
    ScheduledExcitation m_next{};
};

}
