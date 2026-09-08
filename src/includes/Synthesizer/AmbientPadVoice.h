#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <memory>

#include "Filters/PoleMixingFilter.h"
#include "Generators/AdsEnvelope.h"
#include "Generators/OrnsteinUhlenbeckProcess.h"
#include "NonLinear/WaveShaperTables.h"
#include "Numbers/Convert.h"
#include "Parameters/SmoothingParameter.h"
#include "Wavetables/WaveTableOscillator.h"

namespace AbacDsp
{

/// @ingroup generators
/// @brief Named, safe filter responses AmbientPadVoice interpolates between; see
/// PoleMixingFilter.h's poleMixingList for the underlying weight sets.
enum class FilterCharacter
{
    Velvet, ///< dark 4-pole lowpass
    Open,   ///< lighter lowpass, more body and air
    Veil,   ///< lowpass with a subtractive notch-like focus
    Hollow, ///< restrained notch
    Reed,   ///< band-focused
    Glass   ///< open, bright
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
 * @brief One sustained ambient-pad voice: two morphing wavetable layers through a pole-mixing
 * filter and a long attack/release VCA, kept alive by four correlated Ornstein-Uhlenbeck
 * modulators rather than a conventional LFO/ADSR-per-destination matrix.
 *
 * The oscillator/filter/envelope chain runs mono; stereo depth is left to the caller's own
 * effects chain (chorus etc.), the same split MorphexsynthVoice uses. Control-rate work (the OU
 * steps, the musical-intent-to-destination mapping) happens once every kFGranularity samples;
 * the filter's mix weights are recomputed every sample from an already-smoothed position so a
 * character change never steps.
 */
class AmbientPadVoice
{
  public:
    static constexpr size_t kFGranularity{16};
    static constexpr size_t kNumOscillators{2};

    AmbientPadVoice(const float sampleRate, const WaveShaperTableStore& waveShaperTables)
        : m_sampleRate(sampleRate)
        , m_ampEnvelope(sampleRate)
        , m_ouBreath(sampleRate / static_cast<float>(kFGranularity))
        , m_ouMaterial(sampleRate / static_cast<float>(kFGranularity))
        , m_ouLens(sampleRate / static_cast<float>(kFGranularity))
        , m_ouDrift(sampleRate / static_cast<float>(kFGranularity))
        , m_filter(sampleRate)
        , m_waveShaperTables(waveShaperTables)
    {
        for (auto& osc : m_oscillators)
        {
            osc.oscillator = std::make_unique<WaveTableOscillator>(sampleRate);
        }
        setOscillator(0, MaterialPath::SawSineSquare, 0.7f, 0.f, 0.f);
        setOscillator(1, MaterialPath::SawSineSquare, 0.7f, 0.f, 0.f);
        m_filter.setResonance(kBaseResonance);
        m_filter.setParameterSmoothTimeMs(20.f);
        m_ampEnvelope.markSustainAt(0);
        setBloom(0.3f);
        setLight(0.5f);
        setMaterial(0.5f);
        applyFilterCoefficients();
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

    void setLight(const float value) noexcept
    {
        m_light = std::clamp(value, 0.f, 1.f);
    }

    void setMotion(const float value) noexcept
    {
        m_motion = std::clamp(value, 0.f, 1.f);
        const auto sigma = m_motion * kMotionMaxSigma;
        m_ouBreath.setSigma(sigma);
        m_ouMaterial.setSigma(sigma);
        m_ouLens.setSigma(sigma);
        m_ouDrift.setSigma(sigma);
    }

    void setBreath(const float value) noexcept
    {
        m_breath = std::clamp(value, 0.f, 1.f);
    }

    void setStability(const float value) noexcept
    {
        m_stability = std::clamp(value, 0.f, 1.f);
    }

    void setBloom(const float value) noexcept
    {
        m_bloom = std::clamp(value, 0.f, 1.f);
        const auto attackMs = kMinAttackMs + m_bloom * (kMaxAttackMs - kMinAttackMs);
        const auto releaseMs = kMinReleaseMs + m_bloom * (kMaxReleaseMs - kMinReleaseMs);
        m_ampEnvelope.setSegment(0, attackMs, 1.f, kEnvelopeCurve);
        m_ampEnvelope.setSegment(1, releaseMs, 0.f, kEnvelopeCurve);
    }

    void setHold(const bool hold) noexcept
    {
        m_hold = hold;
    }

    /// @brief 0 bypasses; 1.. selects a WaveShaperTables.h preset (1-indexed).
    void setDistortion(const size_t presetIndex) noexcept
    {
        m_distortionIndex = presetIndex;
    }

    /// @brief A snapshot of this voice's current modulation state, for diagnostics/logging.
    struct ModulationSnapshot
    {
        float material{}, light{}, motion{}, breath{}, stability{}, bloom{};
        bool hold{};
        float ouBreath{}, ouMaterial{}, ouLens{}, ouDrift{};
        float filterCutoffHz{}, filterCharacterPos{}, filterResonance{};
        float osc0Hz{}, osc1Hz{};
        float envelope{}, gain{};
    };

    [[nodiscard]] ModulationSnapshot snapshot() const noexcept
    {
        return {.material = m_material,
                .light = m_light,
                .motion = m_motion,
                .breath = m_breath,
                .stability = m_stability,
                .bloom = m_bloom,
                .hold = m_hold,
                .ouBreath = m_lastBreath,
                .ouMaterial = m_lastMaterial,
                .ouLens = m_lastLens,
                .ouDrift = m_lastDrift,
                .filterCutoffHz = m_diagCutoffHz,
                .filterCharacterPos = m_filterCharacterPos.getLastValue(),
                .filterResonance = m_diagResonance,
                .osc0Hz = m_oscillators[0].currentHz,
                .osc1Hz = m_oscillators[1].currentHz,
                .envelope = m_lastEnvelope,
                .gain = m_gainSmoothed.getLastValue()};
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
        float driftCents{0.f};       ///< Drift-OU wobble, opposite sign between the two layers
        float instabilityCents{0.f}; ///< fixed micro-detune between layers, narrows as Stability rises
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

    [[nodiscard]] static std::array<float, 5> characterWeights(const FilterCharacter character) noexcept
    {
        switch (character)
        {
            case FilterCharacter::Velvet:
                return {0.f, 0.f, 0.f, 0.f, 1.f}; // LP4
            case FilterCharacter::Open:
                return {0.f, 0.f, 1.f, 0.f, 0.f}; // LP2
            case FilterCharacter::Veil:
                return {0.f, -1.f, 2.f, -2.f, 0.f}; // LP1 + Notch A
            case FilterCharacter::Hollow:
                return {1.f, -2.f, 2.f, 0.f, 0.f}; // Notch
            case FilterCharacter::Reed:
                return {0.f, -2.f, 2.f, 0.f, 0.f}; // BP2
            case FilterCharacter::Glass:
            default:
                return {0.f, 0.f, 0.f, -3.f, 3.f}; // HP1 + LP3
        }
    }

    void updateOscillatorFrequency(const size_t index, const float pitchSemitones) noexcept
    {
        auto& osc = m_oscillators[index];
        const auto baseFrequency = Convert::noteToFrequency<float>(pitchSemitones);
        const auto totalInterval = osc.heightSemitones + (osc.cents + osc.driftCents + osc.instabilityCents) / 100.f;
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

    /// @brief Blends two adjacent named characters the same way WaveTableOscillator crossfades
    /// adjacent wave slots: a continuous 0..(N-1) position picks a pair and a fractional mix.
    void applyFilterCoefficients() noexcept
    {
        static constexpr std::array<FilterCharacter, 6> kOrder{FilterCharacter::Velvet, FilterCharacter::Open,
                                                               FilterCharacter::Veil,   FilterCharacter::Hollow,
                                                               FilterCharacter::Reed,   FilterCharacter::Glass};
        const auto pos = m_filterCharacterPos.getValue() * static_cast<float>(kOrder.size() - 1);
        auto idx = static_cast<size_t>(pos);
        auto frac = pos - static_cast<float>(idx);
        if (idx >= kOrder.size() - 1)
        {
            idx = kOrder.size() - 2;
            frac = 1.f;
        }
        const auto weightsA = characterWeights(kOrder[idx]);
        const auto weightsB = characterWeights(kOrder[idx + 1]);
        std::array<float, 5> blended{};
        for (size_t i = 0; i < 5; ++i)
        {
            blended[i] = std::lerp(weightsA[i], weightsB[i], frac);
        }
        m_filter.setFilterCoefficients(blended);
    }

    void controlRateUpdate() noexcept
    {
        const auto breathValue = m_hold ? m_lastBreath : m_ouBreath.step();
        const auto materialValue = m_hold ? m_lastMaterial : m_ouMaterial.step();
        const auto lensValue = m_hold ? m_lastLens : m_ouLens.step();
        const auto driftValue = m_hold ? m_lastDrift : m_ouDrift.step();
        m_lastBreath = breathValue;
        m_lastMaterial = materialValue;
        m_lastLens = lensValue;
        m_lastDrift = driftValue;

        // Stability=1 must mean stable: it scales down how much of every OU source
        // reaches its destination, not just pitch drift/detune.
        const auto stabilityRestraint = 1.f - m_stability;

        const auto materialTarget =
            std::clamp(m_material * 2.f - 1.f + materialValue * kMaterialOuDepth * stabilityRestraint, -1.f, 1.f);
        m_materialSmoothed.newTransition(materialTarget, kControlSmoothingSeconds, controlRate());
        const auto material = m_materialSmoothed.getValue();
        for (auto& osc : m_oscillators)
        {
            osc.oscillator->setMorph(material);
        }

        const auto cutoffNote = kMinCutoffNote + m_light * (kMaxCutoffNote - kMinCutoffNote) +
                                lensValue * kLensCutoffDepthSemitones * stabilityRestraint;
        m_diagCutoffHz = Convert::noteToFrequency<float>(std::clamp(cutoffNote, 0.f, 127.f));
        m_filter.setCutoffFrequency(m_diagCutoffHz);
        m_diagResonance = std::clamp(kBaseResonance + lensValue * kLensResonanceDepth * stabilityRestraint, 0.f, 1.f);
        m_filter.setResonance(m_diagResonance);

        const auto characterTarget =
            std::clamp(m_light + lensValue * kLensCharacterDepth * stabilityRestraint, 0.f, 1.f);
        m_filterCharacterPos.newTransition(characterTarget, kControlSmoothingSeconds, controlRate());

        const auto driftDepth = kDriftDepthCents * stabilityRestraint;
        m_oscillators[0].driftCents = driftValue * driftDepth;
        m_oscillators[1].driftCents = -driftValue * driftDepth;
        const auto instability = kInterOscDetuneCents * stabilityRestraint;
        m_oscillators[0].instabilityCents = -0.5f * instability;
        m_oscillators[1].instabilityCents = 0.5f * instability;
        (void) m_pitch.getValue(); // advances any pending pitch glide by one control-rate step
        updateAllOscillatorFrequencies();

        m_breathRippleGain = 1.f + breathValue * m_breath * kBreathVcaDepth * stabilityRestraint;
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

        applyFilterCoefficients();
        const auto filtered = m_filter.step(sum);

        m_lastEnvelope = m_ampEnvelope.step();
        const auto out = filtered * m_lastEnvelope * m_gain * m_breathRippleGain * m_gainSmoothed.getValue();
        return std::clamp(out, -4.f, 4.f);
    }

    static void blockScale(const float gain, float* data, const size_t numSamples) noexcept
    {
        std::transform(data, data + numSamples, data, [gain](const float v) { return v * gain; });
    }

    static constexpr float kBaseResonance{0.1f};
    static constexpr float kEnvelopeCurve{0.4f};
    static constexpr float kMinAttackMs{200.f};
    static constexpr float kMaxAttackMs{6000.f};
    static constexpr float kMinReleaseMs{500.f};
    static constexpr float kMaxReleaseMs{12000.f};
    static constexpr float kMinCutoffNote{48.f};
    static constexpr float kMaxCutoffNote{110.f};
    static constexpr float kMotionMaxSigma{0.4f};
    static constexpr float kMaterialOuDepth{0.35f};
    static constexpr float kLensCutoffDepthSemitones{6.f};
    static constexpr float kLensResonanceDepth{0.15f};
    static constexpr float kLensCharacterDepth{0.3f};
    static constexpr float kDriftDepthCents{15.f};
    static constexpr float kInterOscDetuneCents{6.f};
    static constexpr float kBreathVcaDepth{0.08f};
    static constexpr float kGainSmoothingSeconds{0.05f};
    static constexpr float kControlSmoothingSeconds{0.05f};
    static constexpr float kDistortionDriveGain{2.f};

    const float m_sampleRate;

    std::array<Oscillator, kNumOscillators> m_oscillators;
    Envelope<2> m_ampEnvelope;

    OrnsteinUhlenbeckProcess m_ouBreath;
    OrnsteinUhlenbeckProcess m_ouMaterial;
    OrnsteinUhlenbeckProcess m_ouLens;
    OrnsteinUhlenbeckProcess m_ouDrift;
    float m_lastBreath{0.f};
    float m_lastMaterial{0.f};
    float m_lastLens{0.f};
    float m_lastDrift{0.f};

    Filter1Pole4StageSmooth m_filter;
    LinearSmoothing m_filterCharacterPos{0.5f};
    LinearSmoothing m_materialSmoothed{0.f};
    LinearSmoothing m_gainSmoothed{1.f};
    LinearSmoothing m_pitch{69.f}; ///< continuous semitones (note + cents/100), glide target/position

    float m_material{0.5f};
    float m_light{0.5f};
    float m_motion{0.f};
    float m_breath{0.f};
    float m_stability{0.5f};
    float m_bloom{0.3f};
    bool m_hold{false};
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
