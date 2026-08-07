#pragma once

#include <algorithm>
#include <cmath>

#include "Filters/PoleMixingFilter.h"
#include "Generators/AdsEnvelope.h"
#include "Generators/KarplusStrongString.h"
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
    {
        m_vcf.setFilterCoefficients({0.f, 0.f, 0.f, 0.f, 1.f}); // Lp24: pass the last stage only
        m_vcf.setParameterSmoothTimeMs(kVcfSmoothingMs);
        m_filterLfo.setModulationDepth(1.f);
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

    void setDecayByTime(const float msecs) noexcept
    {
        m_string.setDecayByTime(msecs);
    }

    void setConstFeed(const float value) noexcept
    {
        m_string.setConstFeed(value);
    }

    void attackTime(const float timeInMsecs) noexcept
    {
        m_string.attackTime(timeInMsecs);
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
    static constexpr float kVcfSmoothingMs{5.f}; // absorbs knob jumps without lagging the LFO/envelope

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

    float m_pluckedNote{kReferenceNote};
    float m_tuning{440.f};
    float m_filterCutoffSemitones{0.f};
    float m_keyTracking{0.f};
    float m_lfoDepthOctaves{0.f};
    float m_referenceCutoffHz{1000.f};
};

}
