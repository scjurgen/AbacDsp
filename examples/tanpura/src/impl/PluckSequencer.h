#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <random>

#include "Generators/AdsEnvelope.h"
#include "Generators/KarplusStrongEnsemble.h"

/// The pluck role assigned to one step of a pattern; Pause is a silent rest of the same
/// length as a plucked step and does not consume a voice index.
enum class PluckRole : uint8_t
{
    Harmonic1,
    Harmonic2,
    Root,
    Octave,
    Pause,
};

struct PluckStep
{
    PluckRole role{PluckRole::Pause};
    size_t voiceIndex{0}; // unused when role == Pause
};

/**
 * Auto-pluck sequencer for the tanpura's 5-string ensemble: walks a fixed role pattern at a
 * fixed interval, maps each role to a note (key/harmonic offsets), and independently rolls
 * a slide-in chance each time the Harmonic1 role comes up. The trailing dash every pattern
 * ends with is a virtual, silent pluck one interval after the last real step; the pause gap
 * is an additional wait after that virtual pluck before the pattern repeats from step 0.
 */
template <size_t MaxLength>
class PluckSequencer
{
  public:
    static constexpr size_t kNumVoices{5};
    static constexpr size_t kMaxPatternSteps{6};

    struct PatternDef
    {
        std::array<PluckStep, kMaxPatternSteps> steps{};
        size_t stepCount{0};
    };

    // clang-format off
    static constexpr auto kPatterns = std::to_array<PatternDef>({
        {{{{PluckRole::Harmonic1, 0}, {PluckRole::Harmonic2, 1}, {PluckRole::Root, 2}}}, 3},
        {{{{PluckRole::Harmonic1, 0}, {PluckRole::Harmonic2, 1}, {PluckRole::Octave, 2}, {PluckRole::Root, 3}}}, 4},
        {{{{PluckRole::Harmonic1, 0}, {PluckRole::Harmonic2, 1}, {PluckRole::Octave, 2}, {PluckRole::Octave, 3},
           {PluckRole::Root, 4}}}, 5},
        {{{{PluckRole::Harmonic1, 0}, {PluckRole::Harmonic2, 1}, {PluckRole::Pause, 0}, {PluckRole::Octave, 2},
           {PluckRole::Octave, 3}, {PluckRole::Root, 4}}}, 6},
    });
    // clang-format on

    explicit PluckSequencer(const float sampleRate) noexcept
        : m_sampleRate(sampleRate)
    {
    }

    void setKey(const size_t keyIndex) noexcept
    {
        m_key = static_cast<float>(keyIndex) + kKeyIndexToNoteOffset;
    }

    void setTuning(const float tuningHz) noexcept
    {
        m_tuning = tuningHz;
    }

    void setHarmonicFirst(const size_t index) noexcept
    {
        m_harmonicFirstOffset = static_cast<float>(index) - kHarmonicIndexCenter;
    }

    void setHarmonicSecond(const size_t index) noexcept
    {
        m_harmonicSecondOffset = static_cast<float>(index) - kHarmonicIndexCenter;
    }

    void setPattern(const size_t index) noexcept
    {
        m_patternIndex = std::min(index, kPatterns.size() - 1);
        m_stepIndex = 0;
        m_samplesUntilNextEvent = 0;
    }

    void setSlidePercent(const float percent) noexcept
    {
        m_slidePercent = percent;
    }

    void setSlideTimeMs(const float ms) noexcept
    {
        m_slideTimeMs = ms;
    }

    void setIntervalMs(const float ms) noexcept
    {
        m_intervalMs = ms;
    }

    void setPauseGapMs(const float ms) noexcept
    {
        m_pauseGapMs = ms;
    }

    void setDetuneCents(const size_t voiceIndex, const float cents) noexcept
    {
        m_detuneCents[voiceIndex] = cents;
    }

    void setPlaying(const bool playing) noexcept
    {
        if (playing && !m_playing)
        {
            m_stepIndex = 0;
            m_samplesUntilNextEvent = 0;
        }
        m_playing = playing;
    }

    void step(AbacDsp::KarplusStrongEnsemble<kNumVoices, MaxLength>& ensemble) noexcept
    {
        if (m_sliding)
        {
            ensemble.voice(m_slideVoiceIndex).bendInCents(m_detuneCents[m_slideVoiceIndex] + m_slideShaper.step());
            m_sliding = !m_slideShaper.isDone();
        }
        if (!m_playing)
        {
            return;
        }
        if (m_samplesUntilNextEvent > 0)
        {
            --m_samplesUntilNextEvent;
            return;
        }
        advanceStep(ensemble);
    }

    [[nodiscard]] size_t stepIndex() const noexcept
    {
        return m_stepIndex;
    }

    [[nodiscard]] bool isSliding() const noexcept
    {
        return m_sliding;
    }

  private:
    static constexpr float kKeyIndexToNoteOffset{12.f}; // KEY dropdown index 0 = C0 = note 12 (60 = C4)
    static constexpr float kHarmonicIndexCenter{12.f};  // harmonic dropdown index 12 = unison with key
    static constexpr float kPluckGain{1.f};

    [[nodiscard]] size_t msToSamples(const float ms) const noexcept
    {
        return static_cast<size_t>(std::max(0.f, ms) * m_sampleRate * 0.001f);
    }

    [[nodiscard]] float noteForRole(const PluckRole role) const noexcept
    {
        switch (role)
        {
            case PluckRole::Harmonic1:
                return m_key + m_harmonicFirstOffset;
            case PluckRole::Harmonic2:
                return m_key + m_harmonicSecondOffset;
            case PluckRole::Root:
                return m_key;
            case PluckRole::Octave:
                return m_key + 12.f;
            case PluckRole::Pause:
                break;
        }
        return m_key;
    }

    [[nodiscard]] bool rollSlide() noexcept
    {
        return m_uniformDist(m_rng) < m_slidePercent;
    }

    void triggerStep(AbacDsp::KarplusStrongEnsemble<kNumVoices, MaxLength>& ensemble,
                     const PluckStep& thisStep) noexcept
    {
        auto& voice = ensemble.voice(thisStep.voiceIndex);
        voice.trigger(noteForRole(thisStep.role), kPluckGain, m_tuning);
        voice.bendInCents(m_detuneCents[thisStep.voiceIndex]);
        if (thisStep.role != PluckRole::Harmonic1)
        {
            return;
        }
        if (rollSlide())
        {
            m_slideShaper.reset(100.f * (m_harmonicSecondOffset - m_harmonicFirstOffset));
            m_slideShaper.setNewFramesAndTarget(msToSamples(m_slideTimeMs), 0.f, 0.f);
            m_slideVoiceIndex = thisStep.voiceIndex;
            m_sliding = true;
        }
        else
        {
            // Cancel a still-ramping slide from an earlier cycle - this voice is H1's, and
            // this pluck did not roll a slide, so nothing should keep bending it further.
            m_sliding = false;
        }
    }

    void advanceStep(AbacDsp::KarplusStrongEnsemble<kNumVoices, MaxLength>& ensemble) noexcept
    {
        const auto& pattern = kPatterns[m_patternIndex];
        const auto& thisStep = pattern.steps[m_stepIndex];
        if (thisStep.role != PluckRole::Pause)
        {
            triggerStep(ensemble, thisStep);
        }
        ++m_stepIndex;
        if (m_stepIndex >= pattern.stepCount)
        {
            m_stepIndex = 0;
            m_samplesUntilNextEvent = msToSamples(m_intervalMs) + msToSamples(m_pauseGapMs);
        }
        else
        {
            m_samplesUntilNextEvent = msToSamples(m_intervalMs);
        }
    }

    float m_sampleRate;
    float m_key{24.f};
    float m_tuning{440.f};
    float m_harmonicFirstOffset{0.f};
    float m_harmonicSecondOffset{0.f};
    float m_slidePercent{0.f};
    float m_slideTimeMs{150.f};
    float m_intervalMs{600.f};
    float m_pauseGapMs{10.f};
    std::array<float, kNumVoices> m_detuneCents{};
    bool m_playing{false};

    size_t m_patternIndex{0};
    size_t m_stepIndex{0};
    size_t m_samplesUntilNextEvent{0};

    bool m_sliding{false};
    size_t m_slideVoiceIndex{0};
    AbacDsp::EnvelopeShaper m_slideShaper;

    std::mt19937 m_rng{std::random_device{}()};
    std::uniform_real_distribution<float> m_uniformDist{0.f, 100.f};
};
