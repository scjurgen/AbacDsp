#pragma once

#include <algorithm>
#include <array>
#include <memory>
#include <random>
#include <string_view>

#include "Filters/OnePoleFilter.h"
#include "Filters/PoleMixingFilter.h"
#include "Generators/AdsEnvelope.h"
#include "Generators/SynthLfo.h"
#include "NonLinear/WaveShaperTables.h"
#include "Numbers/Convert.h"
#include "Synthesizer/MpeCurveMap.h"
#include "Synthesizer/PitchQuantize.h"
#include "Synthesizer/ValueConnector.h"
#include "Wavetables/WaveTableOscillator.h"

namespace AbacDsp
{

/// @ingroup generators
/// @brief The five MPE-routing control dimensions a slot can read from.
enum class CtrlDimension
{
    X, ///< MPE per-note pitch bend
    Y, ///< MPE per-note CC74 (timbre)
    Z, ///< MPE per-note channel pressure
    Velocity,
    Note,
    EnvelopeFilter,
    EnvelopeAmplitude
};

/// @ingroup generators
/// @brief Response shape a routing slot applies before depth scaling; indexes MpeCurveMap.
enum class CtrlCurve
{
    CubeRoot,
    SquareRoot,
    Linear,
    Square,
    Cube
};

/// @ingroup generators
/// @brief Destination a routing slot's value is added into.
enum class CtrlTarget
{
    None,
    PitchBend,
    PitchBendSecondary,
    FilterCutoff,
    FilterResonance,
    Pan,
    OscFreq2,
    OscFreq3,
    OscLevel1,
    OscLevel2,
    OscLevel3,
    LfoLevel,
    SustainVol,
    AttackTime,
    DecayTime,
    ReleaseTime,
    Distortion
};

enum class CtrlValueType
{
    Abs,
    BiPolar
};

/// @ingroup generators
/// @brief One MPE routing slot: reads `source`, shapes it through `curve`, scales by
/// `ctrlDepth`, and adds the result into `target`.
struct MpeCtrl
{
    CtrlDimension source{CtrlDimension::X};
    CtrlCurve curve{CtrlCurve::Linear};
    CtrlTarget target{CtrlTarget::None};
    CtrlValueType ctrlType{CtrlValueType::Abs};
    float ctrlDepth{0.f};
};

/// @ingroup generators
/// @brief Sources a ValueConnector slot inside MorphexsynthConnectors can be fed from.
enum class CtrlSource
{
    None,
    BaseValue,
    Key,
    KeyFollow,
    Velocity,
    VelocityFollow,
    ContourEnvFrequency,
    EnvelopeFilter,
    Dimension,
    Lfo,
    EnvelopePitch,
    EnvelopeGlide,
    Pitch
};

/// @ingroup generators
/// @brief One per-voice modulation matrix: sums each target's contributing sources through
/// ValueConnector, smoothing the combined result.
struct MorphexsynthConnectors
{
    // log value, in semitones relative to A4
    ValueConnectorWithPreset<128, CtrlSource, CtrlSource::BaseValue, CtrlSource::Key, CtrlSource::KeyFollow,
                             CtrlSource::Velocity, CtrlSource::VelocityFollow, CtrlSource::ContourEnvFrequency,
                             CtrlSource::EnvelopeFilter, CtrlSource::Dimension, CtrlSource::Lfo>
        filterCutoff{[](const auto& v) { return v[1] * v[2] + v[3] * v[4]; },
                     [](const float fixedPart, const auto& v) { return fixedPart + v[0] + v[5] * v[6] + v[7] + v[8]; }};

    ValueConnector<128, CtrlSource, CtrlSource::BaseValue, CtrlSource::Dimension> filterResonance{
        [](const auto& v) { return v[0] * 1.5f + v[1]; }};

    ValueConnector<128, CtrlSource, CtrlSource::BaseValue, CtrlSource::Dimension> sustainLevel{
        [](const auto& v) { return std::clamp(v[0] + v[1], -1.f, 1.f); }};

    ValueConnector<128, CtrlSource, CtrlSource::BaseValue, CtrlSource::Dimension> osc1Level{
        [](const auto& v) { return std::clamp(v[0] + v[1], -1.f, 1.f); }};

    ValueConnector<128, CtrlSource, CtrlSource::BaseValue, CtrlSource::Dimension> osc2Level{
        [](const auto& v) { return std::clamp(v[0] + v[1], -1.f, 1.f); }};

    ValueConnector<128, CtrlSource, CtrlSource::BaseValue, CtrlSource::Dimension> osc3Level{
        [](const auto& v) { return std::clamp(v[0] + v[1], -1.f, 1.f); }};

    ValueConnector<128, CtrlSource, CtrlSource::BaseValue, CtrlSource::Pitch> oscFrequency{
        [](const auto& v) { return std::clamp(v[0] + v[1], 0.f, 120.f); }};

    ValueConnector<32, CtrlSource, CtrlSource::BaseValue, CtrlSource::Dimension, CtrlSource::EnvelopePitch,
                   CtrlSource::EnvelopeGlide>
        pitchBend{[](const auto& v) { return Convert::noteIntervalToRatio(v[0] + v[1] + v[2] + v[3]); }};

    FixedSmoothing<128> panning{};
    FixedSmoothing<128> distortion{};
};

/**
 * @ingroup generators
 * @brief The Morphexsynth subtractive synth voice: three oscillators through a resonant filter,
 * amp/filter/pitch envelopes, an LFO, a 10-slot MPE routing matrix, and a distortion stage.
 *
 * Control-rate parameters (filter cutoff/resonance, LFO, pitch bend, panning) update once per
 * FGranularity samples rather than every sample; the block-processing budget of the pole-mixing
 * filter and the modulation matrix's evaluator calls both depend on that subsampling.
 *
 * `waveShaperTables` and `curveMap` are large, read-only, and identical across every voice of an
 * instrument; the voice pool that owns several of these is expected to build one of each and
 * share them by reference rather than have every voice build its own copy.
 */
class MorphexsynthVoice
{
  public:
    static constexpr size_t kFGranularity{16};
    static constexpr size_t kNumOscillators{3};
    static constexpr size_t kNumMpeSlots{10};
    static constexpr float kGlideCurve{0.6f};

    MorphexsynthVoice(const float sampleRate, const WaveShaperTableStore& waveShaperTables, const MpeCurveMap& curveMap)
        : m_lfo(sampleRate / static_cast<float>(kFGranularity))
        , m_envAmplitude(sampleRate)
        , m_envFilter(sampleRate / static_cast<float>(kFGranularity))
        , m_envPitch(sampleRate / static_cast<float>(kFGranularity))
        , m_envGlide(sampleRate / static_cast<float>(kFGranularity))
        , m_filter(sampleRate)
        , m_lowpass(sampleRate)
        , m_waveShaperTables(waveShaperTables)
        , m_curveMap(curveMap)
    {
        for (auto& osc : m_oscillators)
        {
            osc.oscillator = std::make_unique<WaveTableOscillator>(sampleRate);
            osc.oscillator->setMorph(-1.f);
            osc.oscillator->setWaveset(0, BasicWave::Triangle);
        }
        setFilterType("LP4");
        m_lp4Index = m_filterIndex;
        m_filter.setResonance(0.f);
        m_filter.setCutoffFrequency(5000.f);
        m_filter.setParameterSmoothTimeMs(3.f);
        setEnvelopeAmplitudeShape();
        setEnvelopeFilterShape();
    }

    // --- oscillators ---

    void setWaveForm(const size_t oscillatorIndex, const size_t index)
    {
        static constexpr std::array<BasicWave, 5> kWaveforms{BasicWave::Triangle, BasicWave::SharkFin, BasicWave::Saw,
                                                             BasicWave::Square, BasicWave::White};
        m_oscillators[oscillatorIndex].oscillator->setWaveset(0, kWaveforms[std::min(index, kWaveforms.size() - 1)]);
    }

    void setOsc3KeyFollow(const float octaves) noexcept
    {
        m_osc3KeyFollowFactor = octaves;
    }

    void setLevelOscillator(const size_t oscillatorIndex, const float value) noexcept
    {
        switch (oscillatorIndex)
        {
            case 0:
                m_connectors.osc1Level.set(CtrlSource::BaseValue, value);
                break;
            case 1:
                m_connectors.osc2Level.set(CtrlSource::BaseValue, value);
                break;
            case 2:
                m_connectors.osc3Level.set(CtrlSource::BaseValue, value);
                break;
            default:
                break;
        }
    }

    void setPwmOscillator(const size_t oscillatorIndex, const float value) noexcept
    {
        m_oscillators[oscillatorIndex].oscillator->setPwmMode(value > 0.f ? WaveTableOscillator::PwmMode::Soft
                                                                          : WaveTableOscillator::PwmMode::Off);
        m_oscillators[oscillatorIndex].oscillator->setPwm(0.5f + 0.5f * std::clamp(value, -1.f, 1.f));
    }

    void setDetune(const size_t oscillatorIndex, const float value) noexcept
    {
        m_oscillators[oscillatorIndex].detune = Convert::noteIntervalToRatio(value);
        updateOscillatorFrequency(oscillatorIndex);
    }

    void setPitchFactor(const size_t oscillatorIndex, const float value) noexcept
    {
        m_oscillators[oscillatorIndex].pitchFactor = value;
    }

    // --- LFO ---

    void setLfoWaveForm(const LfoType type) noexcept
    {
        m_lfo.setWaveForm(type);
    }

    void setLfoPitchFactor(const float value) noexcept
    {
        m_lfoBaseFrequency = value;
    }

    void setLfoOscModulationDepth(const float value) noexcept
    {
        m_lfoOscModulationDepth = value;
    }

    void setLfoFilterModulationDepth(const float value) noexcept
    {
        m_lfoFilterModulationDepth = value;
    }

    void setLfoKeyFollow(const float octaves) noexcept
    {
        m_lfoKeyFollowFactor = octaves;
    }

    // --- amplitude / filter envelopes ---

    void setEnvelopeAttack(const float t) noexcept
    {
        m_adsrAmplitude.attack = t;
        m_envAmplitude.setSegment(0, t, 1.f, 1.f);
    }

    void setEnvelopeDecay(const float t) noexcept
    {
        m_adsrAmplitude.decay = t;
        m_envAmplitude.setSegment(1, t, m_adsrAmplitude.sustainLevel, 1.f);
    }

    void setEnvelopeSustainLevel(const float v) noexcept
    {
        m_adsrAmplitude.sustainLevel = v;
        m_connectors.sustainLevel.set(CtrlSource::BaseValue, v);
    }

    void setEnvelopeRelease(const float t) noexcept
    {
        m_adsrAmplitude.release = t;
        m_envAmplitude.setSegment(3, t, 0.f, 1.f);
    }

    void setEmergencyReleaseTime(const float t) noexcept
    {
        m_envAmplitude.emergencyRelease(t);
    }

    void setEnvelopeAttackFilter(const float t) noexcept
    {
        m_adsrFilter.attack = t;
        m_envFilter.setSegment(0, t, 1.f, 1.f);
    }

    void setEnvelopeDecayFilter(const float t) noexcept
    {
        m_adsrFilter.decay = t;
        m_envFilter.setSegment(1, t, m_adsrFilter.sustainLevel, 1.f);
    }

    void setEnvelopeSustainLevelFilter(const float v) noexcept
    {
        m_adsrFilter.sustainLevel = v;
        m_envFilter.setSegment(1, m_adsrFilter.decay, v, 1.f);
        m_envFilter.setSegmentHoldPreviousValue(2, 1000.f);
        m_envFilter.modifyTargetIfActive(1);
        m_envFilter.quickModifyIfSegmentActive(2, 1000);
    }

    void setEnvelopeReleaseFilter(const float t) noexcept
    {
        m_adsrFilter.release = t;
        m_envFilter.setSegment(3, t, 0.f, 1.f);
    }

    // --- pitch envelope / glide ---

    void setPitchAttack(const float t) noexcept
    {
        m_pitchAttackTime = t;
        m_envPitch.setSegment(0, m_pitchAttackTime, m_pitchFactorAD, 1.f);
        m_envPitch.noSustain();
    }

    void setPitchDecay(const float t) noexcept
    {
        m_envPitch.setSegment(1, t, 0.f, 1.f);
        m_envPitch.noSustain();
    }

    void setPitchEnvelopeDepth(const float t) noexcept
    {
        m_pitchFactorAD = t;
        m_envPitch.setSegment(0, m_pitchAttackTime, m_pitchFactorAD, 1.f);
        m_envPitch.noSustain();
    }

    void setGlide(const float glideMsPerOctave) noexcept
    {
        m_glideTimePerOctave = glideMsPerOctave;
    }

    void setQuantizePitchbend(const float quantize) noexcept
    {
        m_quantizePitchbend = quantize;
    }

    void setRandomDetune(const float detuneCents) noexcept
    {
        m_randDetune = detuneCents;
    }

    void setDynamicRangeForVelocity(const float range) noexcept
    {
        m_dynamicRange = range;
        m_dynamicRangeFactor = 1.f - std::cbrt(m_dynamicRange);
    }

    // --- filter ---

    void setCutoff(const float semitone) noexcept
    {
        m_connectors.filterCutoff.set(CtrlSource::BaseValue, semitone);
        m_connectors.filterCutoff.presetFast();
    }

    void setResonance(const float reso) noexcept
    {
        m_connectors.filterResonance.set(CtrlSource::BaseValue, reso);
    }

    void setFilterType(const std::string_view name)
    {
        m_filterIndex = findFilterIndex(name);
        m_filter.setFilterCoefficients(poleMixingList[m_filterIndex].cf);
    }

    void setLowpassCutoff(const float value) noexcept
    {
        m_lowpass.setCutoff(value);
    }

    void setKeyFollow(const float octaves) noexcept
    {
        m_keyFollowFactor = octaves;
        m_connectors.filterCutoff.set(CtrlSource::KeyFollow, octaves * 12.f);
    }

    void setVeloFollow(const float octaves) noexcept
    {
        m_filterVeloKeyFollowFactor = octaves;
        m_connectors.filterCutoff.set(CtrlSource::VelocityFollow, octaves * 12.f);
    }

    void setContourF(const float contour) noexcept
    {
        m_connectors.filterCutoff.set(CtrlSource::ContourEnvFrequency, contour * 12.f);
    }

    // --- distortion ---

    void setPresetWaveTable(const size_t index) noexcept
    {
        m_waveTableIndex = index;
    }

    // --- MPE routing matrix (10 slots) ---

    void setCtrlDimension(const size_t slot, const CtrlDimension dimension) noexcept
    {
        m_mpeCtrl[slot].source = dimension;
    }

    void setCtrlCurve(const size_t slot, const CtrlCurve curve) noexcept
    {
        m_mpeCtrl[slot].curve = curve;
    }

    void setCtrlTarget(const size_t slot, const CtrlTarget target) noexcept
    {
        m_mpeCtrl[slot].target = target;
    }

    void setCtrlType(const size_t slot, const CtrlValueType type) noexcept
    {
        m_mpeCtrl[slot].ctrlType = type;
    }

    void setCtrlDepth(const size_t slot, const float value) noexcept
    {
        m_mpeCtrl[slot].ctrlDepth = value;
    }

    void mpeX(const int value14Bit) noexcept
    {
        applyCtrl(CtrlDimension::X, value14Bit);
    }

    void mpeY(const int value14Bit) noexcept
    {
        applyCtrl(CtrlDimension::Y, value14Bit);
    }

    void mpeZ(const int value14Bit) noexcept
    {
        applyCtrl(CtrlDimension::Z, value14Bit);
    }

    // --- voice lifecycle ---

    void triggerVoice(const int note, const int velocity, const int lastNote)
    {
        if (m_glideTimePerOctave > 0.f && lastNote != 0 && lastNote != note)
        {
            const auto semitoneDistance = static_cast<float>(lastNote - note);
            const auto glideTime = m_glideTimePerOctave * std::abs(semitoneDistance) / 12.f;
            m_envGlide.setSegment(0, glideTime, 0.f, kGlideCurve);
            m_envGlide.noSustain();
            m_envGlide.triggerFrom(semitoneDistance);
        }
        else
        {
            m_envGlide.triggerFrom(0.f);
        }

        m_connectors.filterCutoff.set(CtrlSource::Key, static_cast<float>(note - 60) / 12.f);
        m_connectors.filterCutoff.set(CtrlSource::Velocity, static_cast<float>(velocity - 64) / 48.f);
        m_connectors.filterCutoff.set(CtrlSource::Dimension, 0.f);

        m_connectors.filterResonance.setForced(CtrlSource::Dimension, 0.f);
        m_connectors.osc1Level.setForced(CtrlSource::Dimension, 0.f);
        m_connectors.osc2Level.setForced(CtrlSource::Dimension, 0.f);
        m_connectors.osc3Level.setForced(CtrlSource::Dimension, 0.f);
        m_connectors.oscFrequency.setForced(CtrlSource::Pitch, 0.f);
        m_connectors.panning.forceTarget(0.f);
        m_connectors.sustainLevel.set(CtrlSource::Dimension, 0.f);

        m_attackModify = 0.f;
        m_decayModify = 0.f;
        m_releaseModify = 0.f;
        applyCtrl(CtrlDimension::Note, note * 128);
        applyCtrl(CtrlDimension::Velocity, velocity * 128);

        m_stepCount = kFGranularity - 1;
        const auto normalizedVolume = static_cast<float>(velocity) / 127.f;
        m_lfo.setSpeed(std::max(0.01f, m_lfoBaseFrequency * Convert::noteIntervalToRatio(static_cast<float>(note - 60) *
                                                                                         m_lfoKeyFollowFactor)));
        m_connectors.oscFrequency.setForced(CtrlSource::BaseValue, static_cast<float>(note));

        std::normal_distribution<float> detuneDistribution{0.f, m_randDetune / 100.f};
        const auto detuneCents = m_randDetune != 0.f ? detuneDistribution(m_randGenerator) : 0.f;
        const auto f = Convert::noteToFrequency<float>(static_cast<float>(note)) *
                       Convert::noteIntervalToRatio(detuneCents * 100.f);

        for (size_t i = 0; i < m_oscillators.size(); ++i)
        {
            auto& osc = m_oscillators[i];
            if (i == 2 && m_osc3KeyFollowFactor != 1.f)
            {
                osc.baseFrequency = osc.pitchFactor * Convert::noteToFrequency<float>(
                                                          60.f + static_cast<float>(note - 60) * m_osc3KeyFollowFactor);
            }
            else
            {
                osc.baseFrequency = osc.pitchFactor * f;
            }
            osc.bend = 1.f;
            osc.detune = 1.f;
            updateOscillatorFrequency(i);
        }

        m_connectors.distortion.forceTarget(0.125f);
        m_connectors.pitchBend.setForced(CtrlSource::Dimension, 0.f);
        m_connectors.pitchBend.setForced(CtrlSource::BaseValue, 0.f);
        m_connectors.pitchBend.setForced(CtrlSource::EnvelopePitch, 0.f);
        m_connectors.pitchBend.setForced(CtrlSource::EnvelopeGlide, m_envGlide.isDone() ? 0.f : m_envGlide.step());

        const auto fA = Convert::noteIntervalToRatio(static_cast<float>(note - 60) * m_attackModify);
        const auto fD = Convert::noteIntervalToRatio(static_cast<float>(note - 60) * m_decayModify);
        const auto fR = Convert::noteIntervalToRatio(static_cast<float>(note - 60) * m_releaseModify);
        m_envAmplitude.setSegment(0, m_adsrAmplitude.attack * fA, 1.f, 1.f);
        m_envAmplitude.setSegment(1, m_adsrAmplitude.decay * fD, m_adsrAmplitude.sustainLevel, 1.f);
        m_envAmplitude.setSegment(3, m_adsrAmplitude.release * fR, 0.f, 1.f);
        m_envFilter.setSegment(0, m_adsrFilter.attack * fA, 1.f, 1.f);
        m_envFilter.setSegment(1, m_adsrFilter.decay * fD, m_adsrFilter.sustainLevel, 1.f);
        m_envFilter.setSegment(3, m_adsrFilter.release * fR, 0.f, 1.f);

        m_filter.reset();
        m_envAmplitude.trigger();
        m_envFilter.trigger();
        m_envPitch.trigger();
        m_panLeft = m_panRight = std::sqrt(0.5f);

        m_gain = getVelocityResponse(normalizedVolume);
        m_connectors.filterCutoff.presetFast();
    }

    void stopVoice() noexcept
    {
        m_envAmplitude.release();
        m_envFilter.release();
    }

    void emergencyStop(const float releaseTimeMs) noexcept
    {
        m_envAmplitude.emergencyRelease(releaseTimeMs);
        m_envFilter.emergencyRelease(releaseTimeMs);
    }

    [[nodiscard]] bool isPlaying() const noexcept
    {
        return !m_envAmplitude.isDone();
    }

    void processBlock(float* left, float* right, const size_t numSamples)
    {
        for (size_t s = 0; s < numSamples; ++s)
        {
            left[s] = step();
        }

        // A little fixed low-level dither keeps the resonant filter's feedback loop from idling
        // on an exact-zero input between notes, where denormals would otherwise stall it.
        static constexpr std::array<float, 4> kDither{1e-6f, 2.5e-6f, -1.4e-6f, -2.2e-6f};
        for (size_t s = 0; s < numSamples; ++s)
        {
            left[s] += kDither[m_ditherIndex];
            m_ditherIndex = (m_ditherIndex + 1) % kDither.size();
        }

        m_filter.processBlock(left, left, numSamples);
        blockScale(m_gain, left, numSamples);
        for (size_t s = 0; s < numSamples; ++s)
        {
            m_lastEnvelopeAmplitude = m_envAmplitude.step();
            left[s] = std::clamp(left[s] * m_lastEnvelopeAmplitude, -4.f, 4.f);
        }

        if (m_filterIndex != m_lp4Index)
        {
            m_lowpass.processBlock(left, numSamples);
        }

        if (m_waveTableIndex != 0)
        {
            blockScale(std::clamp(m_gainDistortion, 0.01f, 100.f), left, numSamples);
            m_waveShaperTables.processBlock(m_waveTableIndex - 1, left, numSamples);
            blockScale(std::clamp(m_gainDistortionReciprocal * 2.f, 0.01f, 100.f), left, numSamples);
        }

        for (size_t i = 0; i < numSamples; ++i)
        {
            right[i] = left[i] * m_panRight;
            left[i] = left[i] * m_panLeft;
        }
    }

  private:
    struct AdsrTimes
    {
        float attack{10.f};
        float decay{5.f};
        float sustainLevel{0.5f};
        float release{100.f};
    };

    struct Oscillator
    {
        std::unique_ptr<WaveTableOscillator> oscillator;
        float pitchFactor{1.f};
        float baseFrequency{440.f};
        float bend{1.f};
        float detune{1.f};
    };

    void updateOscillatorFrequency(const size_t index) noexcept
    {
        auto& osc = m_oscillators[index];
        osc.oscillator->setFrequency(osc.baseFrequency * osc.bend * osc.detune);
    }

    [[nodiscard]] float curvedValue(const CtrlValueType type, const CtrlCurve curve, const int value) const noexcept
    {
        if (type == CtrlValueType::BiPolar)
        {
            return curvedValueBiPolar(curve, value);
        }
        return m_curveMap.get(static_cast<size_t>(curve), value);
    }

    [[nodiscard]] float curvedValueBiPolar(const CtrlCurve curve, const int value) const noexcept
    {
        if (value >= 8192)
        {
            return m_curveMap.get(static_cast<size_t>(curve), (value - 8192) * 2);
        }
        return -m_curveMap.get(static_cast<size_t>(curve), (8191 - value) * 2);
    }

    void applyCtrl(const CtrlDimension dimension, const int value) noexcept
    {
        for (const auto& mpe : m_mpeCtrl)
        {
            if (mpe.source != dimension)
            {
                continue;
            }
            const auto curved = [&]() { return curvedValue(mpe.ctrlType, mpe.curve, value) * mpe.ctrlDepth; };
            switch (mpe.target)
            {
                case CtrlTarget::None:
                    break;
                case CtrlTarget::PitchBend:
                    if (m_quantizePitchbend > 0.f)
                    {
                        m_connectors.pitchBend.set(
                            CtrlSource::BaseValue,
                            pitchQuantize(curvedValueBiPolar(mpe.curve, value) * mpe.ctrlDepth * 12.f,
                                          m_quantizePitchbend));
                    }
                    else
                    {
                        m_connectors.pitchBend.set(CtrlSource::BaseValue,
                                                   curvedValueBiPolar(mpe.curve, value) * mpe.ctrlDepth * 12.f);
                    }
                    break;
                case CtrlTarget::PitchBendSecondary:
                    m_connectors.pitchBend.set(CtrlSource::Dimension,
                                               curvedValueBiPolar(mpe.curve, value) * mpe.ctrlDepth * 12.f);
                    break;
                case CtrlTarget::FilterCutoff:
                    m_connectors.filterCutoff.set(CtrlSource::Dimension, 12.f * 2.f * curved());
                    break;
                case CtrlTarget::FilterResonance:
                    m_connectors.filterResonance.set(CtrlSource::Dimension, curved());
                    break;
                case CtrlTarget::Pan:
                    m_connectors.panning.setTarget(curved());
                    break;
                case CtrlTarget::SustainVol:
                    m_connectors.sustainLevel.set(CtrlSource::Dimension, curved());
                    break;
                case CtrlTarget::OscLevel1:
                    m_connectors.osc1Level.set(CtrlSource::Dimension, curved());
                    break;
                case CtrlTarget::OscLevel2:
                    m_connectors.osc2Level.set(CtrlSource::Dimension, curved());
                    break;
                case CtrlTarget::OscLevel3:
                    m_connectors.osc3Level.set(CtrlSource::Dimension, curved());
                    break;
                case CtrlTarget::OscFreq2:
                    m_oscillators[1].detune = Convert::noteIntervalToRatio(curved());
                    updateOscillatorFrequency(1);
                    break;
                case CtrlTarget::OscFreq3:
                    m_oscillators[2].detune = Convert::noteIntervalToRatio(curved());
                    updateOscillatorFrequency(2);
                    break;
                case CtrlTarget::AttackTime:
                    m_attackModify = curved();
                    break;
                case CtrlTarget::DecayTime:
                    m_decayModify = curved();
                    break;
                case CtrlTarget::ReleaseTime:
                    m_releaseModify = curved();
                    break;
                case CtrlTarget::LfoLevel:
                    break;
                case CtrlTarget::Distortion:
                    m_connectors.distortion.setTarget(0.125f + curved() * 10.f);
                    break;
            }
        }
    }

    void setEnvelopeAmplitudeShape() noexcept
    {
        m_envAmplitude.setSegment(0, m_adsrAmplitude.attack, 1.f, 1.f);
        m_envAmplitude.setSegment(1, m_adsrAmplitude.decay, m_adsrAmplitude.sustainLevel, 1.f);
        m_envAmplitude.setSegmentHoldPreviousValue(2, 1000.f);
        m_envAmplitude.markSustainAt(2);
        m_envAmplitude.setSegment(3, m_adsrAmplitude.release, 0.f, 1.f);
    }

    void setEnvelopeFilterShape() noexcept
    {
        m_envFilter.setSegment(0, m_adsrFilter.attack, 1.f, 1.f);
        m_envFilter.setSegment(1, m_adsrFilter.decay, m_adsrFilter.sustainLevel, 1.f);
        m_envFilter.setSegmentHoldPreviousValue(2, 1000.f);
        m_envFilter.markSustainAt(2);
        m_envFilter.setSegment(3, m_adsrFilter.release, 0.f, 1.f);
    }

    [[nodiscard]] float getVelocityResponse(const float x) const noexcept
    {
        const auto gain = x * m_dynamicRangeFactor + 1.f - m_dynamicRangeFactor;
        return gain * gain * gain;
    }

    static void blockScale(const float gain, float* data, const size_t numSamples) noexcept
    {
        std::transform(data, data + numSamples, data, [gain](const float v) { return v * gain; });
    }

    [[nodiscard]] float step() noexcept
    {
        totalCount++;
        if (++m_stepCount == kFGranularity)
        {
            m_stepCount = 0;
            controlRateUpdate();
        }
        auto sum = m_tmp[m_tmpRead++];
        if (m_tmpRead == kFGranularity)
        {
            m_tmpRead = 0;
            std::array<std::array<float, kFGranularity>, kNumOscillators> tmpSum{};
            m_oscillators[0].oscillator->processBlock(tmpSum[0].data(), kFGranularity);
            blockScale(m_connectors.osc1Level.get(), tmpSum[0].data(), kFGranularity);
            m_oscillators[1].oscillator->processBlock(tmpSum[1].data(), kFGranularity);
            blockScale(m_connectors.osc2Level.get(), tmpSum[1].data(), kFGranularity);
            m_oscillators[2].oscillator->processBlock(tmpSum[2].data(), kFGranularity);
            blockScale(m_connectors.osc3Level.get(), tmpSum[2].data(), kFGranularity);
            for (size_t i = 0; i < kFGranularity; ++i)
            {
                m_tmp[i] = tmpSum[0][i] + tmpSum[1][i] + tmpSum[2][i];
            }
        }
        return sum;
    }

    void controlRateUpdate() noexcept
    {
        if (m_connectors.distortion.isActive())
        {
            m_gainDistortion = m_connectors.distortion.getSmoothed();
            m_gainDistortionReciprocal = 1.f / m_gainDistortion;
        }

        const auto envFilterValue = m_envFilter.step();
        const auto lfoValue = m_lfo.getValue();
        m_connectors.filterCutoff.set(CtrlSource::EnvelopeFilter, envFilterValue);
        applyCtrl(CtrlDimension::EnvelopeFilter, static_cast<int>(envFilterValue * 8191.f));
        const auto cf = m_connectors.filterCutoff.getSmoothed();
        m_connectors.filterCutoff.set(CtrlSource::Lfo, lfoValue * m_lfoFilterModulationDepth * 12.f);
        m_filter.setCutoffFrequency(Convert::noteToFrequency<float>(cf));
        const auto reso = std::clamp(m_connectors.filterResonance.getSmoothed(), 0.f, 2.f);
        m_filter.setResonance(reso);

        if (!m_envPitch.isDone() || m_connectors.oscFrequency.hasChangedValue())
        {
            m_connectors.pitchBend.set(CtrlSource::EnvelopePitch, m_envPitch.step());
        }
        if (!m_envGlide.isDone())
        {
            m_connectors.pitchBend.set(CtrlSource::EnvelopeGlide, m_envGlide.step());
        }
        if (m_connectors.sustainLevel.hasChangedValue())
        {
            m_envAmplitude.setSegment(1, m_adsrAmplitude.decay, m_connectors.sustainLevel.get(), 1.f);
            m_envAmplitude.setSegmentHoldPreviousValue(2, 1000.f);
            m_envAmplitude.modifyTargetIfActive(1);
            m_envAmplitude.quickModifyIfSegmentActive(2, 1000);
        }
        if (m_connectors.panning.isActive())
        {
            Convert::getPanFactorNormalized(m_connectors.panning.getSmoothed(), m_panLeft, m_panRight);
        }

        applyCtrl(CtrlDimension::EnvelopeAmplitude, static_cast<int>(m_lastEnvelopeAmplitude * 8191.f));

        if (m_connectors.pitchBend.isActive())
        {
            const auto bend = m_connectors.pitchBend.get();
            for (auto& osc : m_oscillators)
            {
                osc.bend = bend;
            }
            updateOscillatorFrequency(0);
            updateOscillatorFrequency(1);
            updateOscillatorFrequency(2);
        }
    }

    float m_quantizePitchbend{0.f};
    float m_attackModify{0.f};
    float m_decayModify{0.f};
    float m_releaseModify{0.f};

    std::array<MpeCtrl, kNumMpeSlots> m_mpeCtrl{};
    float m_panLeft{0.7f};
    float m_panRight{0.7f};

    float m_keyFollowFactor{0.f};
    float m_filterVeloKeyFollowFactor{0.f};

    float m_dynamicRange{0.15f};
    float m_dynamicRangeFactor{0.1f};
    float m_gain{1.f};
    float m_lastEnvelopeAmplitude{0.f};

    float m_pitchAttackTime{0.f};
    float m_pitchFactorAD{1.2f};
    float m_glideTimePerOctave{0.f};

    float m_lfoBaseFrequency{1.f};
    float m_lfoOscModulationDepth{0.f};
    float m_lfoFilterModulationDepth{0.f};
    float m_lfoKeyFollowFactor{0.f};
    float m_osc3KeyFollowFactor{1.f};

    std::array<Oscillator, kNumOscillators> m_oscillators;
    SynthLfo m_lfo;
    AdsrTimes m_adsrAmplitude{};
    AdsrTimes m_adsrFilter{0.f, 1.f, 1.f, 50.f};
    Envelope<4> m_envAmplitude;
    Envelope<4> m_envFilter;
    Envelope<2> m_envPitch;
    Envelope<2> m_envGlide;
    Filter1Pole4StageSmooth m_filter;
    size_t m_filterIndex{0};
    size_t m_lp4Index{0};
    OnePoleFilter<OnePoleFilterCharacteristic::LowPass, false> m_lowpass;
    MorphexsynthConnectors m_connectors;

    const WaveShaperTableStore& m_waveShaperTables;
    const MpeCurveMap& m_curveMap;
    size_t m_waveTableIndex{0};
    float m_gainDistortion{2.f};
    float m_gainDistortionReciprocal{0.5f};

    float m_randDetune{0.f};
    std::mt19937 m_randGenerator{std::random_device{}()};

    std::array<float, kFGranularity> m_tmp{};
    size_t m_tmpRead{0};
    size_t m_stepCount{kFGranularity - 1};
    size_t totalCount{0};
    size_t m_ditherIndex{0};
};

}
