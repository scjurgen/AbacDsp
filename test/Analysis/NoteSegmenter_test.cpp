#include <algorithm>
#include <cmath>
#include <gtest/gtest.h>
#include <numbers>
#include <random>
#include <vector>

#include "Analysis/NoteSegmenter.h"
#include "Generators/KarplusStrongVoice.h"
#include "Numbers/Convert.h"

using namespace AbacDsp;

namespace
{
constexpr float kSampleRate{48000.f};

float dbToAmp(const double db)
{
    return static_cast<float>(std::pow(10.0, db / 20.0));
}

struct Tone
{
    double start{0.0};
    double duration{0.5};
    double hz{440.0};
    double levelDb{-20.0};
    double attackMs{5.0};
    double releaseMs{10.0};
};

double envelopeAt(const Tone& tone, const double t)
{
    const double rel = t - tone.start;
    if (rel < 0.0 || rel > tone.duration + tone.releaseMs * 0.001)
    {
        return 0.0;
    }
    const double attack = std::min(1.0, rel / (tone.attackMs * 0.001));
    const double release = rel <= tone.duration ? 1.0 : 1.0 - (rel - tone.duration) / (tone.releaseMs * 0.001);
    return attack * release * dbToAmp(tone.levelDb);
}

std::vector<float> render(const std::vector<Tone>& tones, const double seconds, const double noiseDb = -120.0)
{
    std::vector<float> out(static_cast<size_t>(seconds * kSampleRate), 0.f);
    std::mt19937 rng{1};
    std::normal_distribution<float> noise{0.f, dbToAmp(noiseDb)};
    for (const auto& tone : tones)
    {
        double phase = 0.0;
        for (size_t i = 0; i < out.size(); ++i)
        {
            const double t = static_cast<double>(i) / kSampleRate;
            phase += 2.0 * std::numbers::pi * tone.hz / kSampleRate;
            out[i] += static_cast<float>(envelopeAt(tone, t) * std::sin(phase));
        }
    }
    for (auto& v : out)
    {
        v += noise(rng);
    }
    return out;
}

float midiOf(const double hz)
{
    return Convert::frequencyToNote<float>(static_cast<float>(hz), 440.f);
}

std::vector<PlayedNote> segment(const std::vector<float>& audio, const NoteSegmenterConfig& config = {})
{
    return NoteSegmenter{kSampleRate, config}.analyze(audio);
}
}

TEST(NoteSegmenter, SilenceAndTinyInputGiveNoNotes)
{
    EXPECT_TRUE(segment(std::vector<float>(48000, 0.f)).empty());
    EXPECT_TRUE(segment(std::vector<float>(100, 0.f)).empty());
    EXPECT_TRUE(segment({}).empty());
}

TEST(NoteSegmenter, NoiseAloneGivesNoNotes)
{
    EXPECT_TRUE(segment(render({}, 2.0, -60.0)).empty());
}

TEST(NoteSegmenter, FindsEachNoteWithOnsetPitchAndOffset)
{
    const std::vector<Tone> tones{
        {0.30, 0.40, 220.0}, {1.00, 0.30, 329.63}, {1.50, 0.50, 440.0}, {2.30, 0.25, 130.81}, {2.80, 0.35, 587.33}};
    const auto notes = segment(render(tones, 3.5));
    ASSERT_EQ(notes.size(), tones.size());
    for (size_t i = 0; i < tones.size(); ++i)
    {
        const double expectedOnset = tones[i].start + 0.1 * tones[i].attackMs * 0.001;
        EXPECT_NEAR(notes[i].onsetSeconds, expectedOnset, 0.002) << "note " << i;
        EXPECT_NEAR(notes[i].midiNote, midiOf(tones[i].hz), 0.05) << "note " << i;
        // the offset is where the release has fallen 20 dB, i.e. 90 % of a linear release ramp
        EXPECT_NEAR(notes[i].offsetSeconds, tones[i].start + tones[i].duration + 0.9 * tones[i].releaseMs * 0.001,
                    0.005)
            << "note " << i;
        EXPECT_TRUE(notes[i].pitched());
    }
}

TEST(NoteSegmenter, LevelOfTheAttackFollowsTheNoteLevel)
{
    const std::vector<Tone> tones{{0.3, 0.4, 330.0, -30.0}, {1.0, 0.4, 330.0, -24.0}, {1.7, 0.4, 330.0, -18.0}};
    const auto notes = segment(render(tones, 2.5));
    ASSERT_EQ(notes.size(), 3u);
    for (size_t i = 0; i < 3; ++i)
    {
        EXPECT_NEAR(notes[i].attackRmsDb, tones[i].levelDb - 3.01, 1.0) << "note " << i;
    }
    EXPECT_NEAR(notes[1].attackRmsDb - notes[0].attackRmsDb, 6.0, 0.5);
    EXPECT_NEAR(notes[2].attackRmsDb - notes[1].attackRmsDb, 6.0, 0.5);
}

TEST(NoteSegmenter, KeepsRestsAndSeparatesRepeatedNotes)
{
    std::vector<Tone> tones;
    for (int i = 0; i < 6; ++i)
    {
        tones.push_back({0.3 + 0.25 * i, 0.20, 262.0});
    }
    const auto notes = segment(render(tones, 2.5));
    ASSERT_EQ(notes.size(), tones.size());
    for (size_t i = 1; i < notes.size(); ++i)
    {
        EXPECT_NEAR(notes[i].onsetSeconds - notes[i - 1].onsetSeconds, 0.25, 0.003);
    }
}

TEST(NoteSegmenter, FindsQuietNotesAboveTheNoiseFloorOnly)
{
    const std::vector<Tone> tones{{0.3, 0.4, 330.0, -45.0}, {1.0, 0.4, 330.0, -78.0}, {1.7, 0.4, 330.0, -45.0}};
    const auto notes = segment(render(tones, 2.5, -75.0));
    ASSERT_EQ(notes.size(), 2u);
    EXPECT_NEAR(notes[0].onsetSeconds, 0.3, 0.004);
    EXPECT_NEAR(notes[1].onsetSeconds, 1.7, 0.004);
}

TEST(NoteSegmenter, SlowAttackIsOneOnset)
{
    const std::vector<Tone> tones{{0.3, 0.8, 294.0, -20.0, 100.0, 20.0}};
    EXPECT_EQ(segment(render(tones, 1.6)).size(), 1u);
}

TEST(NoteSegmenter, ShortClickIsNotANote)
{
    const std::vector<Tone> tones{{0.3, 0.005, 1000.0, -10.0, 1.0, 1.0}, {1.0, 0.4, 330.0}};
    const auto notes = segment(render(tones, 1.8));
    ASSERT_EQ(notes.size(), 1u);
    EXPECT_NEAR(notes[0].onsetSeconds, 1.0, 0.003);
}

TEST(NoteSegmenter, NoiseBurstIsANoteWithoutPitch)
{
    std::vector<float> audio(static_cast<size_t>(1.5 * kSampleRate), 0.f);
    std::mt19937 rng{3};
    std::normal_distribution<float> noise{0.f, 0.1f};
    for (size_t i = static_cast<size_t>(0.4 * kSampleRate); i < static_cast<size_t>(0.8 * kSampleRate); ++i)
    {
        audio[i] = noise(rng);
    }
    const auto notes = segment(audio);
    ASSERT_EQ(notes.size(), 1u);
    EXPECT_FALSE(notes[0].pitched());
    EXPECT_FLOAT_EQ(notes[0].midiNote, 0.f);
}

TEST(NoteSegmenter, LegatoPitchStepsSplitIntoNotes)
{
    constexpr double kSteps[]{261.63, 293.66, 329.63};
    std::vector<float> audio(static_cast<size_t>(1.8 * kSampleRate), 0.f);
    double phase = 0.0;
    for (size_t i = 0; i < audio.size(); ++i)
    {
        const double t = static_cast<double>(i) / kSampleRate;
        const double envelope = envelopeAt({0.3, 0.9, 1.0, -20.0, 5.0, 10.0}, t);
        const double hz = kSteps[std::min<size_t>(2, static_cast<size_t>(std::max(0.0, t - 0.3) / 0.3))];
        phase += 2.0 * std::numbers::pi * hz / kSampleRate;
        audio[i] = static_cast<float>(envelope * std::sin(phase));
    }
    const auto notes = segment(audio);
    ASSERT_EQ(notes.size(), 3u);
    for (size_t i = 0; i < 3; ++i)
    {
        EXPECT_NEAR(notes[i].midiNote, midiOf(kSteps[i]), 0.1) << "note " << i;
    }
    EXPECT_NEAR(notes[1].onsetSeconds, 0.6, 0.05);
    EXPECT_NEAR(notes[2].onsetSeconds, 0.9, 0.05);
}

TEST(NoteSegmenter, VibratoDoesNotSplitANote)
{
    std::vector<float> audio(static_cast<size_t>(1.8 * kSampleRate), 0.f);
    double phase = 0.0;
    for (size_t i = 0; i < audio.size(); ++i)
    {
        const double t = static_cast<double>(i) / kSampleRate;
        const double cents = 40.0 * std::sin(2.0 * std::numbers::pi * 5.5 * t);
        const double hz = 440.0 * std::pow(2.0, cents / 1200.0);
        phase += 2.0 * std::numbers::pi * hz / kSampleRate;
        audio[i] = static_cast<float>(envelopeAt({0.3, 1.2, 1.0, -20.0, 5.0, 10.0}, t) * std::sin(phase));
    }
    const auto notes = segment(audio);
    ASSERT_EQ(notes.size(), 1u);
    EXPECT_NEAR(notes[0].midiNote, 69.f, 0.3f);
}

TEST(NoteSegmenter, FindsPluckedStringNotesWithTheirPitch)
{
    using Voice = KarplusStrongVoice<10000>;
    Voice voice{kSampleRate};
    voice.seed(1u);
    voice.setPluckType(PluckType::WhiteStatic);
    voice.setDecayByTime(1500.f);
    voice.setFilterCutoffSemitones(24.f);
    voice.setKeyTracking(1.f);
    voice.setFilterEnvelope(1.f, 1.f, 1.f);
    const std::vector<float> notesPlayed{52.f, 59.f, 64.f, 55.f};
    constexpr double kSpacing{0.6};
    std::vector<float> audio(static_cast<size_t>(3.2 * kSampleRate));
    for (size_t i = 0; i < audio.size(); ++i)
    {
        const double t = static_cast<double>(i) / kSampleRate - 0.3;
        const auto index = static_cast<long>(std::floor(t / kSpacing));
        const double within = t - static_cast<double>(index) * kSpacing;
        const auto first = static_cast<size_t>(std::llround((t - within + 0.3) * kSampleRate));
        if (index >= 0 && index < 4 && i == first)
        {
            voice.trigger(notesPlayed[static_cast<size_t>(index)], 0.3f);
        }
        if (index >= 0 && index < 4 && i == first + static_cast<size_t>(0.45 * kSampleRate))
        {
            voice.scheduleExcitation({0.f, 25.f, ExcitationType::Mute, 1.f, std::nullopt});
        }
        audio[i] = voice.step();
    }
    const auto notes = segment(audio);
    ASSERT_EQ(notes.size(), 4u);
    for (size_t i = 0; i < 4; ++i)
    {
        EXPECT_NEAR(notes[i].onsetSeconds, 0.3 + kSpacing * static_cast<double>(i), 0.004) << "note " << i;
        EXPECT_NEAR(notes[i].midiNote, notesPlayed[i], 0.1f) << "note " << i;
    }
}
