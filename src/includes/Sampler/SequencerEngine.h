#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <concepts>

#include "Generators/BeatSequencer.h"
#include "Numbers/Interpolation.h"
#include "Sampler/SequencePattern.h"
#include "Sampler/SliceLibrary.h"

namespace AbacDsp
{

// A per-voice insert effect: mono, one instance per channel so stereo
// channels never share state. reset() re-primes the effect for a fresh
// trigger (voice start or a stolen voice's new note).
template <typename T>
concept VoiceEffect = requires(T effect, float sample) {
    { effect.process(sample) } -> std::same_as<float>;
    { effect.reset() } -> std::same_as<void>;
};

// Identity insert: stands in for the real per-voice effects (compression,
// distortion) that are dropped in later; satisfies VoiceEffect so
// SequencerEngine's insert slot exists and is exercised now.
struct PassthroughEffect
{
    [[nodiscard]] float process(const float sample) const noexcept
    {
        return sample;
    }

    void reset() noexcept {}
};

// Polyphonic voice pool that plays SliceLibrary slices on the beat grid,
// scheduled by a SequencePattern's step events. Driven sample-by-sample by
// the same BeatSequencer::GridEvent stream the looper's own clock already
// produces (not a separate clock), so the sequencer can never drift from
// the loop or its click. Each voice reads its slice at a fractional rate
// set by SequenceEvent::pitchRatio (hermite interpolation, resampling only,
// no time-stretch); reverse reads the slice back-to-front. Oldest-voice
// steal and edge-fades mirror SlicePlayer. At trigger time a voice's level
// is SequenceEvent::gain scaled by the slice's stored peak (normalizing
// each slice to unity before the event's own gain is applied), then routed
// through a per-channel Effect insert (a no-op PassthroughEffect by
// default; swap in a real one via the template parameter once
// Dynamics/Compressor.h etc. exist). Not thread-safe; library/pattern
// pointers are borrowed and must outlive the engine.
template <VoiceEffect Effect = PassthroughEffect>
class SequencerEngine
{
  public:
    static constexpr size_t kChannels = 2;
    static constexpr size_t kMaxVoices = 16;

    explicit SequencerEngine(const float sampleRate) noexcept
        : m_sampleRate(sampleRate)
    {
        setFadeMs(2.f);
    }

    void setLibrary(const SliceLibrary* library) noexcept
    {
        m_library = library;
    }

    void setPattern(const SequencePattern* pattern) noexcept
    {
        m_pattern = pattern;
        m_barIndex = 0;
    }

    void setFadeMs(const float ms) noexcept
    {
        m_fadeFrames = std::max<size_t>(1, static_cast<size_t>(ms / 1000.f * m_sampleRate));
    }

    void reset() noexcept
    {
        for (Voice& voice : m_voices)
        {
            voice.active = false;
        }
        m_barIndex = 0;
    }

    [[nodiscard]] size_t activeVoiceCount() const noexcept
    {
        size_t count = 0;
        for (const Voice& voice : m_voices)
        {
            count += voice.active ? 1 : 0;
        }
        return count;
    }

    [[nodiscard]] size_t barIndex() const noexcept
    {
        return m_barIndex;
    }

    // Triggers a slice immediately, bypassing the pattern/beat-grid entirely.
    void triggerManual(const size_t track, const size_t sliceIndex, const float gain = 1.f,
                       const float pitchRatio = 1.f, const bool reverse = false) noexcept
    {
        SequenceEvent event{};
        event.track = track;
        event.sliceIndex = sliceIndex;
        event.gain = gain;
        event.pitchRatio = pitchRatio;
        event.reverse = reverse;
        triggerVoice(event);
    }

    // Advances the shared beat clock by one sample: triggers any pattern
    // events landing on this sample's step boundary, renders every active
    // voice, and returns the mixed stereo output for this one sample.
    [[nodiscard]] std::array<float, kChannels> advanceSample(const BeatSequencer::GridEvent& event,
                                                             const size_t samplesPerBeat) noexcept
    {
        checkTriggers(event, samplesPerBeat);
        std::array<float, kChannels> out{0.f, 0.f};
        for (Voice& voice : m_voices)
        {
            if (voice.active)
            {
                renderVoice(voice, out);
            }
        }
        return out;
    }

  private:
    struct Voice
    {
        bool active{false};
        bool reverse{false};
        size_t track{0};
        size_t indexInTrack{0};
        size_t lengthFrames{0};
        size_t playLen{0};
        size_t pos{0};
        size_t effectiveFade{1};
        double readPos{0.0};
        double readStep{1.0};
        float gain{1.f};
        uint64_t startOrder{0};
        std::array<Effect, kChannels> effects{};
    };

    // O(1) regardless of stepsPerBeat (which can be as fine as one sample):
    // solve for the candidate step directly instead of scanning every step.
    void checkTriggers(const BeatSequencer::GridEvent& event, const size_t samplesPerBeat) noexcept
    {
        if (m_pattern != nullptr && m_library != nullptr && samplesPerBeat > 0)
        {
            const size_t stepsPerBeat = m_pattern->stepsPerBeat();
            const size_t stepInBeat = event.beatSamplePos * stepsPerBeat / samplesPerBeat;
            if (stepInBeat * samplesPerBeat / stepsPerBeat == event.beatSamplePos)
            {
                triggerStep(event.beatIndexInBar, stepInBeat);
            }
        }
        if (event.barWrapped)
        {
            advanceBar();
        }
    }

    void advanceBar() noexcept
    {
        const size_t lengthBars = (m_pattern != nullptr) ? m_pattern->lengthBars() : 0;
        m_barIndex = lengthBars > 0 ? (m_barIndex + 1) % lengthBars : 0;
    }

    void triggerStep(const size_t beatIndexInBar, const size_t stepInBeat) noexcept
    {
        const size_t globalStep =
            (m_barIndex * m_pattern->beatsPerBar() + beatIndexInBar) * m_pattern->stepsPerBeat() + stepInBeat;
        for (const SequenceEvent& sequenceEvent : m_pattern->events())
        {
            if (sequenceEvent.stepPosition == globalStep)
            {
                triggerVoice(sequenceEvent);
            }
        }
    }

    void triggerVoice(const SequenceEvent& sequenceEvent) noexcept
    {
        if (m_library == nullptr || sequenceEvent.track >= m_library->trackCount() ||
            sequenceEvent.sliceIndex >= m_library->sliceCountInTrack(sequenceEvent.track))
        {
            return;
        }
        const auto& info = m_library->sliceInfo(sequenceEvent.track, sequenceEvent.sliceIndex);
        if (info.lengthFrames == 0)
        {
            return;
        }
        const auto pitchRatio = sequenceEvent.pitchRatio > 0.f ? sequenceEvent.pitchRatio : 1.f;
        const auto normalizeFactor = info.peak > 1e-6f ? 1.f / info.peak : 1.f;
        Voice& voice = allocateVoice();
        voice.active = true;
        voice.reverse = sequenceEvent.reverse;
        voice.track = sequenceEvent.track;
        voice.indexInTrack = sequenceEvent.sliceIndex;
        voice.lengthFrames = info.lengthFrames;
        voice.playLen =
            std::max<size_t>(1, static_cast<size_t>(std::llround(static_cast<double>(info.lengthFrames) / pitchRatio)));
        voice.pos = 0;
        voice.readStep = voice.reverse ? -static_cast<double>(pitchRatio) : static_cast<double>(pitchRatio);
        voice.readPos = voice.reverse ? static_cast<double>(info.lengthFrames - 1) : 0.0;
        voice.gain = sequenceEvent.gain * normalizeFactor;
        voice.effectiveFade = std::max<size_t>(1, std::min(m_fadeFrames, voice.playLen / 2));
        voice.startOrder = m_triggerCounter++;
        for (Effect& effect : voice.effects)
        {
            effect.reset();
        }
    }

    [[nodiscard]] Voice& allocateVoice() noexcept
    {
        for (Voice& voice : m_voices)
        {
            if (!voice.active)
            {
                return voice;
            }
        }
        Voice* oldest = &m_voices[0];
        for (Voice& voice : m_voices)
        {
            if (voice.startOrder < oldest->startOrder)
            {
                oldest = &voice;
            }
        }
        return *oldest;
    }

    void renderVoice(Voice& voice, std::array<float, kChannels>& out) noexcept
    {
        const auto gain = edgeGain(voice) * voice.gain;
        for (size_t channel = 0; channel < kChannels; ++channel)
        {
            const float sample = readInterpolated(voice, channel) * gain;
            out[channel] += voice.effects[channel].process(sample);
        }
        voice.readPos += voice.readStep;
        if (++voice.pos >= voice.playLen)
        {
            voice.active = false;
        }
    }

    [[nodiscard]] float readInterpolated(const Voice& voice, const size_t channel) const noexcept
    {
        const auto base = static_cast<long>(std::floor(voice.readPos));
        const auto frac = static_cast<float>(voice.readPos - static_cast<double>(base));
        const auto lastIndex = static_cast<long>(voice.lengthFrames) - 1;
        const auto clampedFrame = [lastIndex](const long idx) noexcept
        { return static_cast<size_t>(std::clamp(idx, 0L, lastIndex)); };
        const std::array<float, 4> taps{
            m_library->sample(voice.track, voice.indexInTrack, clampedFrame(base - 1), channel),
            m_library->sample(voice.track, voice.indexInTrack, clampedFrame(base), channel),
            m_library->sample(voice.track, voice.indexInTrack, clampedFrame(base + 1), channel),
            m_library->sample(voice.track, voice.indexInTrack, clampedFrame(base + 2), channel),
        };
        return Interpolation::hermite43x(taps.data(), frac);
    }

    [[nodiscard]] static float edgeGain(const Voice& voice) noexcept
    {
        const auto fade = static_cast<float>(voice.effectiveFade);
        const auto fadeIn = static_cast<float>(voice.pos + 1) / fade;
        const auto fadeOut = static_cast<float>(voice.playLen - voice.pos) / fade;
        return std::clamp(std::min(fadeIn, fadeOut), 0.f, 1.f);
    }

    float m_sampleRate;
    size_t m_fadeFrames{1};
    const SliceLibrary* m_library{nullptr};
    const SequencePattern* m_pattern{nullptr};
    size_t m_barIndex{0};
    std::array<Voice, kMaxVoices> m_voices{};
    uint64_t m_triggerCounter{1};
};

}
