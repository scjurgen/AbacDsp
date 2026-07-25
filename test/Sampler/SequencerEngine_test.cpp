#include <gtest/gtest.h>
#include <vector>

#include "Generators/BeatSequencer.h"
#include "Sampler/SequencerEngine.h"

namespace AbacDsp::test
{

namespace
{
constexpr float kSampleRate = 1000.f; // fade in ms maps 1:1 to frames
using Engine = SequencerEngine<>;

// Doubles every sample; used to verify the per-voice insert slot is genuinely
// wired (not just accepting a template parameter it never calls).
struct DoublingEffect
{
    [[nodiscard]] float process(const float sample) const noexcept
    {
        return sample * 2.f;
    }

    void reset() noexcept {}
};

// Interleaved stereo loop where L = frame index, R = -(frame index).
[[nodiscard]] std::vector<float> makeRampLoop(const size_t frames)
{
    std::vector<float> loop(frames * 2, 0.f);
    for (size_t f = 0; f < frames; ++f)
    {
        loop[f * 2] = static_cast<float>(f);
        loop[f * 2 + 1] = -static_cast<float>(f);
    }
    return loop;
}

[[nodiscard]] std::vector<float> makeConstLoop(const size_t frames, const float l, const float r)
{
    std::vector<float> loop(frames * 2, 0.f);
    for (size_t f = 0; f < frames; ++f)
    {
        loop[f * 2] = l;
        loop[f * 2 + 1] = r;
    }
    return loop;
}

struct Rendered
{
    std::vector<float> left;
    std::vector<float> right;
};

// Runs `beats` beats of the shared clock through both the engine and a real
// BeatSequencer (mirroring how LooperImpl drives them from the same clock).
[[nodiscard]] Rendered render(Engine& engine, BeatSequencer& seq, const size_t beats)
{
    Rendered out{};
    const size_t samplesPerBeat = seq.samplesPerBeat();
    for (size_t b = 0; b < beats; ++b)
    {
        for (size_t i = 0; i < samplesPerBeat; ++i)
        {
            const auto event = seq.advance();
            const auto sample = engine.advanceSample(event, samplesPerBeat);
            out.left.push_back(sample[0]);
            out.right.push_back(sample[1]);
        }
    }
    return out;
}
}

TEST(SequencerEngineTest, SilentWithoutPatternOrLibrary)
{
    Engine engine(kSampleRate);
    BeatSequencer seq(kSampleRate);
    seq.setBpm(120.f);
    seq.setBeatsPerBar(4);

    const auto out = render(engine, seq, 2);
    for (const float v : out.left)
    {
        EXPECT_FLOAT_EQ(v, 0.f);
    }
    EXPECT_EQ(engine.activeVoiceCount(), 0u);
}

TEST(SequencerEngineTest, TriggersExactlyOnStepBoundary)
{
    Engine engine(kSampleRate);
    engine.setFadeMs(1.f); // 1 frame, near-instant so onset is visible immediately
    BeatSequencer seq(kSampleRate);
    seq.setBpm(120.f); // samplesPerBeat = 500 at 1000 Hz
    seq.setBeatsPerBar(4);

    const auto loop = makeConstLoop(100, 1.f, 1.f);
    SliceLibrary library(100);
    library.extractTrack(loop, std::vector<Slice>{{0, 100}});

    SequencePattern pattern(1, 4, 4); // 1 bar, 4 beats/bar, 4 steps/beat -> 16 steps
    SequenceEvent event{};
    event.stepPosition = 4; // second beat (step 4 of 16 -> beatIndexInBar 1, stepInBeat 0)
    pattern.addEvent(event);

    engine.setLibrary(&library);
    engine.setPattern(&pattern);

    const auto out = render(engine, seq, 2);
    const size_t samplesPerBeat = seq.samplesPerBeat();
    // Silent through the whole first beat.
    for (size_t i = 0; i < samplesPerBeat; ++i)
    {
        EXPECT_FLOAT_EQ(out.left[i], 0.f) << "frame " << i;
    }
    // Triggers right at the start of the second beat.
    EXPECT_GT(out.left[samplesPerBeat], 0.f);
}

TEST(SequencerEngineTest, PlateauMatchesSliceContent)
{
    Engine engine(kSampleRate);
    engine.setFadeMs(2.f); // 2 frames
    BeatSequencer seq(kSampleRate);
    seq.setBpm(120.f);
    seq.setBeatsPerBar(4);

    const auto loop = makeRampLoop(20);
    SliceLibrary library(20);
    library.extractTrack(loop, std::vector<Slice>{{10, 10}}); // L 10..19

    SequencePattern pattern(1, 4, 1); // 1 step per beat
    SequenceEvent event{};
    event.stepPosition = 0;
    event.gain = 19.f; // slice peak is 19 (frame 19); compensate the automatic
                       // peak-normalize so this test can assert raw content.
    pattern.addEvent(event);

    engine.setLibrary(&library);
    engine.setPattern(&pattern);

    const auto out = render(engine, seq, 1);
    // effectiveFade = min(2, 5) = 2; plateau where pos in [1, 8] plays the ramp unmodified.
    for (size_t pos = 1; pos <= 8; ++pos)
    {
        EXPECT_NEAR(out.left[pos], 10.f + static_cast<float>(pos), 1e-3f);
        EXPECT_NEAR(out.right[pos], -(10.f + static_cast<float>(pos)), 1e-3f);
    }
}

TEST(SequencerEngineTest, PeakNormalizeScalesSliceToUnity)
{
    Engine engine(kSampleRate);
    engine.setFadeMs(2.f);
    BeatSequencer seq(kSampleRate);
    seq.setBpm(120.f);
    seq.setBeatsPerBar(4);

    const auto loop = makeRampLoop(20);
    SliceLibrary library(20);
    library.extractTrack(loop, std::vector<Slice>{{10, 10}}); // L 10..19, peak = 19

    SequencePattern pattern(1, 4, 1);
    SequenceEvent event{}; // default gain 1.0: normalize alone should apply
    event.stepPosition = 0;
    pattern.addEvent(event);

    engine.setLibrary(&library);
    engine.setPattern(&pattern);

    const auto out = render(engine, seq, 1);
    // Plateau sample (pos 8, raw L = 18) normalized by peak 19 -> ~0.947, not 18.
    EXPECT_NEAR(out.left[8], 18.f / 19.f, 1e-3f);
}

TEST(SequencerEngineTest, EventGainScalesOutputRelativeToNormalized)
{
    Engine engine(kSampleRate);
    engine.setFadeMs(1.f);
    BeatSequencer seq(kSampleRate);
    seq.setBpm(120.f);
    seq.setBeatsPerBar(4);

    const auto loop = makeConstLoop(64, 1.f, 1.f); // peak = 1, normalize is a no-op
    SliceLibrary library(64);
    library.extractTrack(loop, std::vector<Slice>{{0, 64}});

    SequencePattern pattern(1, 4, 1);
    SequenceEvent event{};
    event.stepPosition = 0;
    event.gain = 0.5f;
    pattern.addEvent(event);

    engine.setLibrary(&library);
    engine.setPattern(&pattern);

    const auto out = render(engine, seq, 1);
    EXPECT_NEAR(out.left[10], 0.5f, 1e-3f); // steady-state, past the 1-frame fade-in
}

TEST(SequencerEngineTest, EffectSlotProcessesEveryOutputSample)
{
    SequencerEngine<DoublingEffect> engine(kSampleRate);
    engine.setFadeMs(1.f);
    BeatSequencer seq(kSampleRate);
    seq.setBpm(120.f);
    seq.setBeatsPerBar(4);

    const auto loop = makeConstLoop(64, 1.f, 1.f); // peak = 1, normalize is a no-op
    SliceLibrary library(64);
    library.extractTrack(loop, std::vector<Slice>{{0, 64}});

    SequencePattern pattern(1, 4, 1);
    SequenceEvent event{};
    event.stepPosition = 0;
    pattern.addEvent(event);

    engine.setLibrary(&library);
    engine.setPattern(&pattern);

    const size_t samplesPerBeat = seq.samplesPerBeat();
    std::vector<float> left;
    for (size_t i = 0; i < samplesPerBeat; ++i)
    {
        const auto gridEvent = seq.advance();
        const auto sample = engine.advanceSample(gridEvent, samplesPerBeat);
        left.push_back(sample[0]);
    }
    // DoublingEffect makes the steady-state plateau 2.0 instead of the raw 1.0.
    EXPECT_NEAR(left[10], 2.f, 1e-3f);
}

TEST(SequencerEngineTest, ReverseReadsSliceBackwards)
{
    Engine engine(kSampleRate);
    engine.setFadeMs(1.f);
    BeatSequencer seq(kSampleRate);
    seq.setBpm(120.f);
    seq.setBeatsPerBar(4);

    const auto loop = makeRampLoop(20);
    SliceLibrary library(20);
    library.extractTrack(loop, std::vector<Slice>{{0, 10}}); // L 0..9

    SequencePattern pattern(1, 4, 1);
    SequenceEvent event{};
    event.stepPosition = 0;
    event.reverse = true;
    event.gain = 9.f; // slice peak is 9; compensate the automatic peak-normalize.
    pattern.addEvent(event);

    engine.setLibrary(&library);
    engine.setPattern(&pattern);

    const auto out = render(engine, seq, 1);
    // Reverse starts near the slice's last sample (index 9) and decreases.
    EXPECT_NEAR(out.left[0], 9.f, 1.f);
    EXPECT_GT(out.left[0], out.left[5]);
}

TEST(SequencerEngineTest, PitchRatioAboveOneShortensPlayback)
{
    Engine engine(kSampleRate);
    engine.setFadeMs(1.f);
    BeatSequencer seq(kSampleRate);
    seq.setBpm(120.f);
    seq.setBeatsPerBar(4);

    const auto loop = makeConstLoop(100, 1.f, 1.f);
    SliceLibrary library(100);
    library.extractTrack(loop, std::vector<Slice>{{0, 40}});

    SequencePattern pattern(1, 4, 1);
    SequenceEvent event{};
    event.stepPosition = 0;
    event.pitchRatio = 2.f; // reads twice as fast -> half the output frames
    pattern.addEvent(event);

    engine.setLibrary(&library);
    engine.setPattern(&pattern);

    EXPECT_EQ(engine.activeVoiceCount(), 0u);
    static_cast<void>(render(engine, seq, 1)); // triggers on the very first sample
    // 40 frames at 2x -> ~20 output frames; well within one beat (500 frames),
    // so the voice should already be inactive by the time we check.
    EXPECT_EQ(engine.activeVoiceCount(), 0u);
}

TEST(SequencerEngineTest, TriggerIgnoredWhenTrackOrSliceOutOfRange)
{
    Engine engine(kSampleRate);
    BeatSequencer seq(kSampleRate);
    seq.setBpm(120.f);
    seq.setBeatsPerBar(4);

    const auto loop = makeConstLoop(20, 1.f, 1.f);
    SliceLibrary library(20);
    library.extractTrack(loop, std::vector<Slice>{{0, 20}});

    SequencePattern pattern(1, 4, 1);
    SequenceEvent badTrack{};
    badTrack.stepPosition = 0;
    badTrack.track = 5;
    pattern.addEvent(badTrack);

    engine.setLibrary(&library);
    engine.setPattern(&pattern);

    static_cast<void>(render(engine, seq, 1));
    EXPECT_EQ(engine.activeVoiceCount(), 0u);
}

TEST(SequencerEngineTest, VoiceStealingCapsPolyphony)
{
    Engine engine(kSampleRate);
    engine.setFadeMs(2.f);
    BeatSequencer seq(kSampleRate);
    seq.setBpm(120.f);
    seq.setBeatsPerBar(4);

    const auto loop = makeConstLoop(64, 1.f, 1.f);
    SliceLibrary library(64 * (Engine::kMaxVoices + 4));
    for (size_t i = 0; i < Engine::kMaxVoices + 4; ++i)
    {
        library.extractTrack(loop, std::vector<Slice>{{0, 64}}); // each i becomes its own track
    }

    SequencePattern pattern(1, 4, 1);
    for (size_t i = 0; i < Engine::kMaxVoices + 4; ++i)
    {
        SequenceEvent event{};
        event.stepPosition = 0;
        event.track = i;
        pattern.addEvent(event);
    }

    engine.setLibrary(&library);
    engine.setPattern(&pattern);

    // All 20 events land on the same step (sample 0), so check right after that
    // one sample - the 64-frame slices would otherwise finish well before a
    // full beat (500 samples) elapses.
    const auto event = seq.advance();
    static_cast<void>(engine.advanceSample(event, seq.samplesPerBeat()));
    EXPECT_EQ(engine.activeVoiceCount(), Engine::kMaxVoices);
}

TEST(SequencerEngineTest, MultiBarPatternOnlyTriggersOnItsOwnBar)
{
    Engine engine(kSampleRate);
    engine.setFadeMs(1.f);
    BeatSequencer seq(kSampleRate);
    seq.setBpm(120.f);
    seq.setBeatsPerBar(2); // small bar to keep the test short

    const auto loop = makeConstLoop(20, 1.f, 1.f);
    SliceLibrary library(20);
    library.extractTrack(loop, std::vector<Slice>{{0, 20}});

    SequencePattern pattern(2, 2, 1); // 2 bars, 2 beats/bar, 1 step/beat -> 4 steps
    SequenceEvent event{};
    event.stepPosition = 2; // first step of the SECOND bar
    pattern.addEvent(event);

    engine.setLibrary(&library);
    engine.setPattern(&pattern);

    const size_t samplesPerBeat = seq.samplesPerBeat();
    // First bar (2 beats): should stay silent.
    const auto firstBar = render(engine, seq, 2);
    for (const float v : firstBar.left)
    {
        EXPECT_FLOAT_EQ(v, 0.f);
    }
    EXPECT_EQ(engine.barIndex(), 1u);

    // Second bar: triggers on its very first sample.
    const auto secondBar = render(engine, seq, 1);
    EXPECT_GT(secondBar.left[0], 0.f);
    static_cast<void>(samplesPerBeat);
}

TEST(SequencerEngineTest, ResetSilencesVoices)
{
    Engine engine(kSampleRate);
    BeatSequencer seq(kSampleRate);
    seq.setBpm(120.f);
    seq.setBeatsPerBar(4);

    const auto loop = makeConstLoop(64, 1.f, 1.f);
    SliceLibrary library(64);
    library.extractTrack(loop, std::vector<Slice>{{0, 64}});

    SequencePattern pattern(1, 4, 1);
    SequenceEvent event{};
    event.stepPosition = 0;
    pattern.addEvent(event);

    engine.setLibrary(&library);
    engine.setPattern(&pattern);

    const auto firstEvent = seq.advance();
    static_cast<void>(engine.advanceSample(firstEvent, seq.samplesPerBeat()));
    ASSERT_EQ(engine.activeVoiceCount(), 1u);

    engine.reset();
    EXPECT_EQ(engine.activeVoiceCount(), 0u);
    EXPECT_EQ(engine.barIndex(), 0u);
}

}
