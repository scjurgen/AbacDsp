#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <memory>

#include "Filters/PoleMixingFilter.h"
#include "Generators/AdsEnvelope.h"
#include "Generators/OrnsteinUhlenbeckProcess.h"
#include "Generators/SynthLfo.h"
#include "NonLinear/WaveShaperTables.h"
#include "Numbers/Convert.h"
#include "Parameters/SmoothingParameter.h"
#include "Wavetables/WaveTableOscillator.h"

namespace AbacDsp
{

/// @ingroup generators
/// @brief Discrete filter responses AmbientSynthVoice can select, named after their
/// PoleMixingFilter.h topology - see poleMixingList for each one's weight set.
enum class FilterType
{
    LP4,      ///< 4-pole lowpass
    LP2,      ///< 2-pole lowpass, lighter, more body and air
    LP1Notch, ///< 1-pole lowpass with a notch-like focus
    Notch,    ///< restrained notch
    BP2,      ///< 2-pole bandpass
    HP1LP3,   ///< highpass+lowpass blend, open and bright
    AP4       ///< 4-pole allpass; flat magnitude, only phase moves - meant for phasing
              ///< when Cutoff is modulated
};

/// @ingroup generators
/// @brief Named routes a WaveTableOscillator's 3 morph slots follow; SetOscillator picks one per
/// oscillator, the Material control then sweeps a continuous position along it.
enum class MaterialPath
{
    SawSineSquare,     ///< the vision doc's own example path: warm -> hollow -> reedy
    TriangleSineShark, ///< softer alternative
    SquareWhiteSaw     ///< noisier alternative
};

/**
 * @ingroup generators
 * @brief One sustained ambient voice for real-time performance: two morphing wavetable
 * layers through a pole-mixing filter and a long attack/release VCA. Cutoff, Resonance,
 * Material, Breath, and Pitch each carry their own Ornstein-Uhlenbeck wander plus an
 * independent deterministic LFO on top; Drift is LFO-only, spreading the two oscillator
 * layers in opposite directions. Filter type is a discrete choice, not modulated.
 *
 * The oscillator/filter/envelope chain runs mono; stereo depth is left to the caller's own
 * effects chain, the same split AmbientPadVoice uses. Control-rate work (the OU steps, the
 * musical-intent-to-destination mapping) happens once every kFGranularity samples.
 */
class AmbientSynthVoice
{
  public:
    static constexpr size_t kFGranularity{16};
    static constexpr size_t kNumOscillators{2};

    AmbientSynthVoice(const float sampleRate, const WaveShaperTableStore& waveShaperTables)
        : m_sampleRate(sampleRate)
        , m_ampEnvelope(sampleRate)
        , m_ouMaterial(sampleRate / static_cast<float>(kFGranularity))
        , m_ouCutoff(sampleRate / static_cast<float>(kFGranularity))
        , m_ouResonance(sampleRate / static_cast<float>(kFGranularity))
        , m_ouBreath(sampleRate / static_cast<float>(kFGranularity))
        , m_ouPitch(sampleRate / static_cast<float>(kFGranularity))
        , m_lfoCutoff(sampleRate / static_cast<float>(kFGranularity))
        , m_lfoMaterial(sampleRate / static_cast<float>(kFGranularity))
        , m_lfoResonance(sampleRate / static_cast<float>(kFGranularity))
        , m_lfoPitch(sampleRate / static_cast<float>(kFGranularity))
        , m_lfoBreath(sampleRate / static_cast<float>(kFGranularity))
        , m_lfoDrift(sampleRate / static_cast<float>(kFGranularity))
        , m_filter(sampleRate)
        , m_waveShaperTables(waveShaperTables)
    {
        for (auto& osc : m_oscillators)
        {
            osc.oscillator = std::make_unique<WaveTableOscillator>(sampleRate);
        }
        setOscillator(0, MaterialPath::SawSineSquare, 0.7f, 0.f, 0.f);
        setOscillator(1, MaterialPath::SawSineSquare, 0.7f, 0.f, 0.f);
        m_filter.setParameterSmoothTimeMs(20.f);
        m_ampEnvelope.markSustainAt(0);
        setBloom(0.3f);
        setCutoff(0.5f);
        setResonance(0.1f);
        setMaterial(0.5f);
        setFilterType(FilterType::LP4);
        m_ouMaterial.setSigma(kOuSigma);
        m_ouCutoff.setSigma(kOuSigma);
        m_ouResonance.setSigma(kOuSigma);
        m_ouBreath.setSigma(kOuSigma);
        m_ouPitch.setSigma(kOuSigma);
    }

    // --- oscillators ---

    void setOscillator(const size_t index, const MaterialPath path, const float level, const float heightSemitones,
                       const float cents) noexcept
    {
        if (index >= kNumOscillators)
        {
            return;
        }
        auto& osc = m_oscillators[index];
        const auto [waveA, waveB, waveC] = materialPathWaves(path);
        osc.oscillator->setWaveset(0, waveA);
        osc.oscillator->setWaveset(1, waveB);
        osc.oscillator->setWaveset(2, waveC);
        osc.level = std::clamp(level, -1.f, 1.f);
        osc.heightSemitones = heightSemitones;
        osc.cents = cents;
        updateOscillatorFrequency(index, m_pitch.getLastValue());
    }

    /// @brief Smoothed per-voice output trim, independent of the oscillator level balance.
    void setGain(const float gainDb) noexcept
    {
        m_gainSmoothed.newTransition(Convert::dbToGain(gainDb), kGainSmoothingSeconds, m_sampleRate);
    }

    // --- musical-intent controls ---

    void setMaterial(const float value) noexcept
    {
        m_material = std::clamp(value, 0.f, 1.f);
    }

    /// @brief Full width, in Material's own 0..1 units, of the OU-driven sweep around the
    /// Material center - e.g. center 0.5 / range 0.5 wanders roughly between 0.25 and 0.75.
    void setMaterialRange(const float value) noexcept
    {
        m_materialRange = std::clamp(value, 0.f, 1.f);
    }

    /// @brief Base filter cutoff position (0..1, mapped onto kMinCutoffNote..kMaxCutoffNote) -
    /// independent of filter type, which is a separate discrete choice (see setFilterType()).
    void setCutoff(const float value) noexcept
    {
        m_cutoff = std::clamp(value, 0.f, 1.f);
    }

    /// @brief Max semitone depth the Cutoff OU process can pull the filter cutoff away from
    /// setCutoff()'s own base.
    void setCutoffOuRange(const float semitones) noexcept
    {
        m_cutoffOuRange = std::clamp(semitones, 0.f, 48.f);
    }

    /// @brief Base filter resonance (0..1, normalized to the self-oscillation threshold).
    void setResonance(const float value) noexcept
    {
        m_resonance = std::clamp(value, 0.f, 1.f);
    }

    /// @brief Max depth (0..1, on top of setResonance()'s own base) the Resonance OU process
    /// can add.
    void setResonanceRange(const float amount) noexcept
    {
        m_resonanceRange = std::clamp(amount, 0.f, 1.f);
    }

    /// @brief Max cents of Pitch-OU-driven vibrato depth, identical-phase on both oscillators
    /// (unlike Drift's opposite-sign spread) - combines additively with setPitchLfo().
    void setPitchOuRange(const float cents) noexcept
    {
        m_pitchOuRange = std::clamp(cents, 0.f, 100.f);
    }

    /// @brief Max depth of the Breath OU process's output-gain ripple (bipolar around unity) -
    /// combines additively with setBreathLfo().
    void setBreathOuRange(const float amount) noexcept
    {
        m_breathOuRange = std::clamp(amount, 0.f, 10.f);
    }

    /// @brief Discrete filter-type switch - no blending, unlike AmbientPadVoice's continuous
    /// character crossfade. Coefficients are set once here, not recomputed every sample.
    void setFilterType(const FilterType type) noexcept
    {
        m_filterType = type;
        m_filter.setFilterCoefficients(characterWeights(type));
    }

    /// @brief Adds a slow filter-cutoff sweep on top of Cutoff's own setting. depth 0 (default)
    /// is off; phaseDegrees (0..360, wrapped) sets where in the cycle it starts.
    void setCutoffLfo(const float rateCyclesPerMinute, const float depthSemitones, const float phaseDegrees) noexcept
    {
        m_lfoCutoff.setFrequency(std::clamp(rateCyclesPerMinute, 0.f, kMaxLfoCyclesPerMinute) / 60.f);
        m_lfoCutoff.setPhase(phaseDegrees);
        m_cutoffLfoDepthSemitones = std::clamp(depthSemitones, 0.f, 48.f);
    }

    /// @brief Adds a slow wavetable-morph sweep on top of Material's own setting. depth 0
    /// (default) is off; phaseDegrees (0..360, wrapped) sets where in the cycle it starts.
    void setMaterialLfo(const float rateCyclesPerMinute, const float depth, const float phaseDegrees) noexcept
    {
        m_lfoMaterial.setFrequency(std::clamp(rateCyclesPerMinute, 0.f, kMaxLfoCyclesPerMinute) / 60.f);
        m_lfoMaterial.setPhase(phaseDegrees);
        m_materialLfoDepth = std::clamp(depth, 0.f, 1.f);
    }

    /// @brief Unipolar, always-upward pull on resonance on top of Resonance's own setting.
    /// depth 0 (default) is off; unbounded above (deliberately allows self-oscillation).
    void setResonanceLfo(const float rateCyclesPerMinute, const float depth, const float phaseDegrees) noexcept
    {
        m_lfoResonance.setFrequency(std::clamp(rateCyclesPerMinute, 0.f, kMaxLfoCyclesPerMinute) / 60.f);
        m_lfoResonance.setPhase(phaseDegrees);
        m_resonanceLfoDepth = std::max(depth, 0.f);
    }

    /// @brief Ordinary vibrato, identically on both oscillators (unlike Drift's opposite-sign
    /// spread). depth 0 (default) is off; phaseDegrees (0..360, wrapped) sets its start point.
    void setPitchLfo(const float rateCyclesPerMinute, const float depthCents, const float phaseDegrees) noexcept
    {
        m_lfoPitch.setFrequency(std::clamp(rateCyclesPerMinute, 0.f, kMaxLfoCyclesPerMinute) / 60.f);
        m_lfoPitch.setPhase(phaseDegrees);
        m_pitchLfoDepthCents = std::clamp(depthCents, 0.f, 100.f);
    }

    /// @brief Slow output-gain ripple (bipolar around unity) - combines additively with
    /// setBreathOuRange(). depth 0 (default) is off; phaseDegrees sets its start point.
    void setBreathLfo(const float rateCyclesPerMinute, const float depth, const float phaseDegrees) noexcept
    {
        m_lfoBreath.setFrequency(std::clamp(rateCyclesPerMinute, 0.f, kMaxLfoCyclesPerMinute) / 60.f);
        m_lfoBreath.setPhase(phaseDegrees);
        m_breathLfoDepth = std::clamp(depth, 0.f, 10.f);
    }

    /// @brief Slow, opposite-sign detune spread between the two oscillator layers. depth 0
    /// (default) is off; phaseDegrees (0..360, wrapped) sets its start point.
    void setDriftLfo(const float rateCyclesPerMinute, const float depthCents, const float phaseDegrees) noexcept
    {
        m_lfoDrift.setFrequency(std::clamp(rateCyclesPerMinute, 0.f, kMaxLfoCyclesPerMinute) / 60.f);
        m_lfoDrift.setPhase(phaseDegrees);
        m_driftLfoDepthCents = std::clamp(depthCents, 0.f, 100.f);
    }

    void setBloom(const float value) noexcept
    {
        m_bloom = std::clamp(value, 0.f, 1.f);
        const auto attackMs = kMinAttackMs + m_bloom * (kMaxAttackMs - kMinAttackMs);
        const auto releaseMs = kMinReleaseMs + m_bloom * (kMaxReleaseMs - kMinReleaseMs);
        m_ampEnvelope.setSegment(0, attackMs, 1.f, kEnvelopeCurve);
        m_ampEnvelope.setSegment(1, releaseMs, 0.f, kEnvelopeCurve);
    }

    /// @brief 0 bypasses; 1.. selects a WaveShaperTables.h preset (1-indexed).
    void setDistortion(const size_t presetIndex) noexcept
    {
        m_distortionIndex = presetIndex;
    }

    /// @brief A snapshot of this voice's current modulation state, for diagnostics/logging.
    struct ModulationSnapshot
    {
        float material{}, materialRange{}, cutoff{}, resonance{}, bloom{};
        float materialMorph{}; ///< actual OU-modulated morph position (0..1), unlike `material`

        float ouMaterial{}, ouCutoff{}, ouResonance{}, ouBreath{}, ouPitch{};
        float filterCutoffHz{}, filterResonance{};
        FilterType filterType{FilterType::LP4};
        float osc0Hz{}, osc1Hz{};
        float envelope{}, gain{};
        float breathRippleGain{}; ///< Breath-driven VCA wobble, multiplies onto envelope
        float velocityGain{};     ///< fixed per-note velocity response, set once at trigger
        float pitchSemitones{};
        bool isPlaying{};
    };

    [[nodiscard]] ModulationSnapshot snapshot() const noexcept
    {
        return {.material = m_material,
                .materialRange = m_materialRange,
                .cutoff = m_cutoff,
                .resonance = m_resonance,
                .bloom = m_bloom,
                .materialMorph = (m_materialSmoothed.getLastValue() + 1.f) * 0.5f,
                .ouMaterial = m_lastMaterialOu,
                .ouCutoff = m_lastCutoffOu,
                .ouResonance = m_lastResonanceOu,
                .ouBreath = m_lastBreathOu,
                .ouPitch = m_lastPitchOu,
                .filterCutoffHz = m_diagCutoffHz,
                .filterResonance = m_diagResonance,
                .filterType = m_filterType,
                .osc0Hz = m_oscillators[0].currentHz,
                .osc1Hz = m_oscillators[1].currentHz,
                .envelope = m_lastEnvelope,
                .gain = m_gainSmoothed.getLastValue(),
                .breathRippleGain = m_breathRippleGain,
                .velocityGain = m_gain,
                .pitchSemitones = m_pitch.getLastValue(),
                .isPlaying = isPlaying()};
    }

    /// @brief Repitches the held note live, over glideTimeSeconds (0 = instant) - the envelope
    /// and every modulation source stay exactly as they were.
    void setPitch(const int note, const float cents, const float glideTimeSeconds) noexcept
    {
        m_pitch.newTransition(static_cast<float>(note) + cents / 100.f, glideTimeSeconds, controlRate());
        updateAllOscillatorFrequencies();
    }

    /// @brief Instant repitch - `setPitch(note, 0.f, 0.f)`.
    void setNote(const int note) noexcept
    {
        setPitch(note, 0.f, 0.f);
    }

    // --- voice lifecycle ---

    void triggerVoice(const int note, const int velocity) noexcept
    {
        setPitch(note, 0.f, 0.f);
        m_gain = getVelocityResponse(static_cast<float>(velocity) / 127.f);
        m_ampEnvelope.trigger();
    }

    void stopVoice() noexcept
    {
        m_ampEnvelope.release();
    }

    [[nodiscard]] bool isPlaying() const noexcept
    {
        return !m_ampEnvelope.isDone();
    }

    /// @brief The pitch (note + cents/100) this voice is currently at - mid-glide, this is
    /// where it presently sits, not the glide's eventual target.
    [[nodiscard]] float currentPitchSemitones() const noexcept
    {
        return m_pitch.getLastValue();
    }

    void processBlock(float* mono, const size_t numSamples) noexcept
    {
        std::generate_n(mono, numSamples, [this]() { return step(); });
        if (m_distortionIndex != 0)
        {
            blockScale(kDistortionDriveGain, mono, numSamples);
            m_waveShaperTables.processBlock(m_distortionIndex - 1, mono, numSamples);
            blockScale(1.f / kDistortionDriveGain, mono, numSamples);
        }
    }

  private:
    struct Oscillator
    {
        std::unique_ptr<WaveTableOscillator> oscillator;
        float level{0.f};
        float heightSemitones{0.f};
        float cents{0.f};
        float driftCents{0.f};       ///< Drift-LFO wobble, opposite sign between the two layers
        float instabilityCents{0.f}; ///< fixed micro-detune between layers
        float currentHz{0.f};        ///< last frequency set on `oscillator`, cached for diagnostics
    };

    [[nodiscard]] static std::array<BasicWave, 3> materialPathWaves(const MaterialPath path) noexcept
    {
        switch (path)
        {
            case MaterialPath::TriangleSineShark:
                return {BasicWave::Triangle, BasicWave::Sine, BasicWave::SharkFin};
            case MaterialPath::SquareWhiteSaw:
                return {BasicWave::Square, BasicWave::White, BasicWave::Saw};
            case MaterialPath::SawSineSquare:
            default:
                return {BasicWave::Saw, BasicWave::Sine, BasicWave::Square};
        }
    }

    /// @brief Named PoleMixingFilter.h tap-mix weights for each FilterType, reused verbatim
    /// from poleMixingList's own "LP4"/"BP2"/"AP4" etc. entries.
    [[nodiscard]] static std::array<float, 5> characterWeights(const FilterType type) noexcept
    {
        switch (type)
        {
            case FilterType::LP2:
                return {0.f, 0.f, 1.f, 0.f, 0.f};
            case FilterType::LP1Notch:
                return {0.f, -1.f, 2.f, -2.f, 0.f};
            case FilterType::Notch:
                return {1.f, -2.f, 2.f, 0.f, 0.f};
            case FilterType::BP2:
                return {0.f, -2.f, 2.f, 0.f, 0.f};
            case FilterType::HP1LP3:
                return {0.f, 0.f, 0.f, -3.f, 3.f};
            case FilterType::AP4:
                return {1.f, -8.f, 24.f, -32.f, 16.f};
            case FilterType::LP4:
            default:
                return {0.f, 0.f, 0.f, 0.f, 1.f};
        }
    }

    void updateOscillatorFrequency(const size_t index, const float pitchSemitones) noexcept
    {
        auto& osc = m_oscillators[index];
        const auto baseFrequency = Convert::noteToFrequency<float>(pitchSemitones);
        const auto totalInterval =
            osc.heightSemitones + (osc.cents + osc.driftCents + osc.instabilityCents + m_pitchModCents) / 100.f;
        osc.currentHz = baseFrequency * Convert::noteIntervalToRatio(totalInterval);
        osc.oscillator->setFrequency(osc.currentHz);
    }

    /// @brief Reflects the pitch glide's current position onto both oscillators, without
    /// advancing it - advancing happens once per control-rate tick, in controlRateUpdate().
    void updateAllOscillatorFrequencies() noexcept
    {
        const auto pitchSemitones = m_pitch.getLastValue();
        for (size_t i = 0; i < m_oscillators.size(); ++i)
        {
            updateOscillatorFrequency(i, pitchSemitones);
        }
    }

    [[nodiscard]] static float getVelocityResponse(const float x) noexcept
    {
        return x * x;
    }

    void controlRateUpdate() noexcept
    {
        const auto materialOu = m_ouMaterial.step();
        const auto materialLfoValue = m_lfoMaterial.step();
        m_lastMaterialOu = materialOu;
        m_lastMaterialLfo = materialLfoValue;
        const auto materialTarget = std::clamp(
            m_material * 2.f - 1.f + materialOu * m_materialRange + materialLfoValue * m_materialLfoDepth, -1.f, 1.f);
        m_materialSmoothed.newTransition(materialTarget, kControlSmoothingSeconds, controlRate());
        const auto material = m_materialSmoothed.getValue();
        for (auto& osc : m_oscillators)
        {
            osc.oscillator->setMorph(material);
        }

        const auto cutoffOu = m_ouCutoff.step();
        const auto cutoffLfoValue = m_lfoCutoff.step();
        m_lastCutoffOu = cutoffOu;
        m_lastCutoffLfo = cutoffLfoValue;
        const auto cutoffNote = kMinCutoffNote + m_cutoff * (kMaxCutoffNote - kMinCutoffNote) +
                                cutoffOu * m_cutoffOuRange + cutoffLfoValue * m_cutoffLfoDepthSemitones;
        m_diagCutoffHz = Convert::noteToFrequency<float>(std::clamp(cutoffNote, 0.f, 127.f));
        m_filter.setCutoffFrequency(m_diagCutoffHz);

        const auto resonanceOu = m_ouResonance.step();
        const auto resonanceLfoValue = m_lfoResonance.step();
        m_lastResonanceOu = resonanceOu;
        m_lastResonanceLfo = resonanceLfoValue;
        const auto resonanceLfoUnipolar = (resonanceLfoValue + 1.f) * 0.5f;
        m_diagResonance =
            std::max(m_resonance + resonanceOu * m_resonanceRange + resonanceLfoUnipolar * m_resonanceLfoDepth, 0.f);
        m_filter.setResonance(m_diagResonance);

        const auto breathOu = m_ouBreath.step();
        const auto breathLfoValue = m_lfoBreath.step();
        m_lastBreathOu = breathOu;
        m_lastBreathLfo = breathLfoValue;
        m_breathRippleGain = 1.f + breathOu * m_breathOuRange + breathLfoValue * m_breathLfoDepth;

        const auto pitchOu = m_ouPitch.step();
        const auto pitchLfoValue = m_lfoPitch.step();
        m_lastPitchOu = pitchOu;
        m_lastPitchLfo = pitchLfoValue;
        m_pitchModCents = pitchOu * m_pitchOuRange + pitchLfoValue * m_pitchLfoDepthCents;

        const auto driftLfoValue = m_lfoDrift.step();
        m_lastDriftLfo = driftLfoValue;
        const auto driftCents = driftLfoValue * m_driftLfoDepthCents;
        m_oscillators[0].driftCents = driftCents;
        m_oscillators[1].driftCents = -driftCents;
        m_oscillators[0].instabilityCents = -0.5f * kInterOscDetuneCents;
        m_oscillators[1].instabilityCents = 0.5f * kInterOscDetuneCents;

        (void) m_pitch.getValue(); // advances any pending pitch glide by one control-rate step
        updateAllOscillatorFrequencies();
    }

    [[nodiscard]] float controlRate() const noexcept
    {
        return m_sampleRate / static_cast<float>(kFGranularity);
    }

    [[nodiscard]] float step() noexcept
    {
        if (m_stepCount == 0)
        {
            controlRateUpdate();
        }
        m_stepCount = (m_stepCount + 1) % kFGranularity;

        float sum = 0.f;
        for (auto& osc : m_oscillators)
        {
            sum += osc.oscillator->process() * osc.level;
        }

        const auto filtered = m_filter.step(sum);

        m_lastEnvelope = m_ampEnvelope.step();
        const auto out = filtered * m_lastEnvelope * m_gain * m_breathRippleGain * m_gainSmoothed.getValue();
        return std::clamp(out, -4.f, 4.f);
    }

    static void blockScale(const float gain, float* data, const size_t numSamples) noexcept
    {
        std::transform(data, data + numSamples, data, [gain](const float v) { return v * gain; });
    }

    /// @brief Fixed OU fluctuation magnitude for all 5 processes - no Motion dial drives this
    /// live any more; picked to match a typical AmbientPadVoice patch's Motion setting.
    static constexpr float kOuSigma{0.16f};
    static constexpr float kEnvelopeCurve{0.4f};
    static constexpr float kMinAttackMs{200.f};
    static constexpr float kMaxAttackMs{6000.f};
    static constexpr float kMinReleaseMs{500.f};
    static constexpr float kMaxReleaseMs{12000.f};
    static constexpr float kMinCutoffNote{48.f};
    static constexpr float kMaxCutoffNote{110.f};
    static constexpr float kMaxLfoCyclesPerMinute{60.f};
    static constexpr float kInterOscDetuneCents{6.f};
    static constexpr float kGainSmoothingSeconds{0.05f};
    static constexpr float kControlSmoothingSeconds{0.05f};
    static constexpr float kDistortionDriveGain{2.f};

    const float m_sampleRate;

    std::array<Oscillator, kNumOscillators> m_oscillators;
    Envelope<2> m_ampEnvelope;

    OrnsteinUhlenbeckProcess m_ouMaterial;
    OrnsteinUhlenbeckProcess m_ouCutoff;
    OrnsteinUhlenbeckProcess m_ouResonance;
    OrnsteinUhlenbeckProcess m_ouBreath;
    OrnsteinUhlenbeckProcess m_ouPitch;
    LfoGenerators m_lfoCutoff;
    LfoGenerators m_lfoMaterial;
    LfoGenerators m_lfoResonance;
    LfoGenerators m_lfoPitch;
    LfoGenerators m_lfoBreath;
    LfoGenerators m_lfoDrift;
    float m_cutoffLfoDepthSemitones{0.f};
    float m_materialLfoDepth{0.f};
    float m_resonanceLfoDepth{0.f};
    float m_pitchLfoDepthCents{0.f};
    float m_breathLfoDepth{0.f};
    float m_driftLfoDepthCents{0.f};
    float m_lastMaterialOu{0.f};
    float m_lastCutoffOu{0.f};
    float m_lastResonanceOu{0.f};
    float m_lastBreathOu{0.f};
    float m_lastPitchOu{0.f};
    float m_lastMaterialLfo{0.f};
    float m_lastCutoffLfo{0.f};
    float m_lastResonanceLfo{0.f};
    float m_lastPitchLfo{0.f};
    float m_lastBreathLfo{0.f};
    float m_lastDriftLfo{0.f};
    float m_pitchModCents{0.f};

    Filter1Pole4StageSmooth m_filter;
    FilterType m_filterType{FilterType::LP4};
    LinearSmoothing m_materialSmoothed{0.f};
    LinearSmoothing m_gainSmoothed{1.f};
    LinearSmoothing m_pitch{69.f}; ///< continuous semitones (note + cents/100), glide target/position

    float m_material{0.5f};
    float m_materialRange{0.35f};
    float m_cutoff{0.5f};
    float m_cutoffOuRange{6.f};
    float m_resonance{0.1f};
    float m_resonanceRange{0.15f};
    float m_pitchOuRange{0.f};
    float m_breathOuRange{2.f};
    float m_bloom{0.3f};
    float m_breathRippleGain{1.f};
    float m_diagCutoffHz{0.f};
    float m_diagResonance{0.f};
    float m_lastEnvelope{0.f};

    float m_gain{1.f};

    const WaveShaperTableStore& m_waveShaperTables;
    size_t m_distortionIndex{0};

    size_t m_stepCount{0};
};

}
