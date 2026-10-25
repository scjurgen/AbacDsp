#include <algorithm>
#include <cmath>
#include <gtest/gtest.h>
#include <numbers>
#include <random>
#include <vector>

#include "Analysis/NoteVerifier.h"

using namespace AbacDsp;

namespace
{
constexpr float kSampleRate{48000.f};
constexpr double kSeconds{3.2};
constexpr double kNoiseDb{-70.0};
constexpr double kNoteDb{-20.0};

float dbToAmp(const double db)
{
    return static_cast<float>(std::pow(10.0, db / 20.0));
}

double hzOf(const double midi)
{
    return 440.0 * std::pow(2.0, (midi - 69.0) / 12.0);
}

struct Tone
{
    double start{0.0};
    double duration{0.3};
    double hz{440.0};
    double levelDb{kNoteDb};
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

std::vector<float> render(const std::vector<Tone>& tones, const double noiseDb = kNoiseDb)
{
    std::vector<float> out(static_cast<size_t>(kSeconds * kSampleRate), 0.f);
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

std::vector<ExpectedNote> score()
{
    return {{0.5f, 60.f}, {1.0f, 62.f}, {1.5f, 64.f}, {2.0f, 65.f}};
}

// The score as played: every note on its time, with a pause of 0.2 s before the next.
std::vector<Tone> played(const double shift = 0.0)
{
    std::vector<Tone> tones;
    for (const auto& note : score())
    {
        tones.push_back(
            {static_cast<double>(note.onsetSeconds) + shift, 0.3, hzOf(static_cast<double>(note.midiNote))});
    }
    return tones;
}

VerificationReport verify(const std::vector<Tone>& tones, const NoteVerifierConfig& config = {})
{
    const auto audio = render(tones);
    const auto expected = score();
    return NoteVerifier{kSampleRate, config}.verify(audio, expected);
}
}

TEST(NoteVerifier, EmptyInputsGiveAReportWithoutNotesOrSound)
{
    const NoteVerifier verifier{kSampleRate};
    const auto expected = score();
    const std::vector<float> silence(48000, 0.f);
    const auto report = verifier.verify(silence, expected);
    ASSERT_EQ(report.notes.size(), expected.size());
    for (const auto& note : report.notes)
    {
        EXPECT_FALSE(note.found);
        EXPECT_EQ(note.verdict, PitchVerdict::Missing);
    }
    EXPECT_TRUE(report.unexpected.empty());
    EXPECT_TRUE(verifier.verify(std::vector<float>(100, 0.f), expected).unexpected.empty());
    EXPECT_TRUE(verifier.verify(silence, {}).notes.empty());
}

TEST(NoteVerifier, NoiseAloneFindsNoNoteAndSetsNoReferenceLevel)
{
    const auto audio = render({}, -60.0);
    const auto report = NoteVerifier{kSampleRate}.verify(audio, score());
    EXPECT_TRUE(std::ranges::none_of(report.notes, [](const VerifiedNote& n) { return n.found; }));
    EXPECT_TRUE(report.unexpected.empty());
    EXPECT_FLOAT_EQ(report.thresholdDb, -120.f);
}

TEST(NoteVerifier, NotesPlayedAsWrittenAreRightWithSmallTimingAndPitchErrors)
{
    const auto report = verify(played());
    ASSERT_EQ(report.notes.size(), 4u);
    for (size_t i = 0; i < report.notes.size(); ++i)
    {
        const auto& note = report.notes[i];
        ASSERT_TRUE(note.found) << i;
        EXPECT_EQ(note.verdict, PitchVerdict::Right) << i;
        EXPECT_NEAR(note.onsetErrorSeconds(), 0.f, 0.005f) << i;
        EXPECT_NEAR(note.pitchErrorCents, 0.f, 5.f) << i;
        EXPECT_NEAR(note.midiNote, score()[i].midiNote, 0.05f) << i;
        EXPECT_NEAR(note.peakDb, -23.f, 1.5f) << i;
        EXPECT_GT(note.offsetSeconds, note.onsetSeconds + 0.25f) << i;
    }
    EXPECT_TRUE(report.unexpected.empty());
    EXPECT_NEAR(report.alignmentSeconds, 0.f, 0.005f);
    EXPECT_FLOAT_EQ(report.thresholdDb, report.medianNoteDb - 15.f);
    EXPECT_GT(report.marginDb(), 20.f);
}

TEST(NoteVerifier, ACommonTimingOffsetIsFittedAndRemovedFromTheOnsetErrors)
{
    const auto report = verify(played(0.08));
    EXPECT_NEAR(report.alignmentSeconds, 0.08f, 0.006f);
    for (const auto& note : report.notes)
    {
        ASSERT_TRUE(note.found);
        EXPECT_NEAR(note.onsetErrorSeconds(), 0.f, 0.006f);
    }
}

TEST(NoteVerifier, AnIndividuallyLateNoteShowsInItsOnsetError)
{
    auto tones = played();
    tones[2].start += 0.1;
    const auto report = verify(tones);
    ASSERT_TRUE(report.notes[2].found);
    EXPECT_NEAR(report.notes[2].onsetErrorSeconds(), 0.1f, 0.01f);
    EXPECT_NEAR(report.notes[1].onsetErrorSeconds(), 0.f, 0.01f);
}

TEST(NoteVerifier, ASharpAndAFlatNoteAreNamedButNotCalledWrong)
{
    auto tones = played();
    tones[1].hz = hzOf(62.0 - 0.35);
    tones[2].hz = hzOf(64.0 + 0.35);
    const auto report = verify(tones);
    EXPECT_EQ(report.notes[1].verdict, PitchVerdict::Flat);
    EXPECT_NEAR(report.notes[1].pitchErrorCents, -35.f, 5.f);
    EXPECT_EQ(report.notes[2].verdict, PitchVerdict::Sharp);
    EXPECT_NEAR(report.notes[2].pitchErrorCents, 35.f, 5.f);
    EXPECT_EQ(report.notes[0].verdict, PitchVerdict::Right);
}

TEST(NoteVerifier, AWrongNoteAndAnOctaveErrorAreMeasuredNotForcedToTheExpectedPitch)
{
    auto tones = played();
    tones[1].hz = hzOf(64.0);
    tones[2].hz = hzOf(76.0);
    const auto report = verify(tones);
    EXPECT_EQ(report.notes[1].verdict, PitchVerdict::WrongNote);
    EXPECT_NEAR(report.notes[1].midiNote, 64.f, 0.1f);
    EXPECT_NEAR(report.notes[1].pitchErrorCents, 200.f, 10.f);
    EXPECT_EQ(report.notes[2].verdict, PitchVerdict::WrongNote);
    EXPECT_NEAR(report.notes[2].midiNote, 76.f, 0.1f);
    EXPECT_NEAR(report.notes[2].pitchErrorCents, 1200.f, 10.f);
}

TEST(NoteVerifier, ANoteThatIsNotPlayedIsMissingAndTheOthersAreUnaffected)
{
    auto tones = played();
    tones.erase(tones.begin() + 2);
    const auto report = verify(tones);
    EXPECT_FALSE(report.notes[2].found);
    EXPECT_EQ(report.notes[2].verdict, PitchVerdict::Missing);
    for (const size_t i : {size_t{0}, size_t{1}, size_t{3}})
    {
        EXPECT_TRUE(report.notes[i].found) << i;
        EXPECT_EQ(report.notes[i].verdict, PitchVerdict::Right) << i;
    }
    EXPECT_TRUE(report.unexpected.empty());
}

TEST(NoteVerifier, ANoteOutsideItsWindowIsMissingAndItsSoundIsUnexpected)
{
    auto tones = played();
    tones[2].start += 0.25;
    tones[2].duration = 0.15;
    const auto report = verify(tones);
    EXPECT_FALSE(report.notes[2].found);
    ASSERT_EQ(report.unexpected.size(), 1u);
    EXPECT_NEAR(report.unexpected[0].fromSeconds, 1.75f, 0.03f);
    EXPECT_NEAR(report.unexpected[0].midiNote, 64.f, 0.2f);
}

TEST(NoteVerifier, AnExtraNoteAfterTheLastOneIsReportedWithTimeLevelAndPitch)
{
    auto tones = played();
    tones.push_back({2.6, 0.25, hzOf(69.0)});
    const auto report = verify(tones);
    ASSERT_EQ(report.unexpected.size(), 1u);
    const auto& extra = report.unexpected[0];
    EXPECT_NEAR(extra.fromSeconds, 2.6f, 0.03f);
    EXPECT_NEAR(extra.toSeconds, 2.85f, 0.05f);
    EXPECT_NEAR(extra.midiNote, 69.f, 0.2f);
    EXPECT_NEAR(extra.peakDb, -23.f, 1.5f);
    EXPECT_EQ(report.shortBursts, 0u);
}

TEST(NoteVerifier, QuietSoundsInFrontOfANoteAreIgnoredAndDoNotMoveTheOnset)
{
    auto tones = played();
    tones.push_back({0.93, 0.07, hzOf(79.0), kNoteDb - 28.0});
    tones.push_back({1.43, 0.07, hzOf(75.0), kNoteDb - 28.0});
    const auto report = verify(tones);
    EXPECT_TRUE(report.unexpected.empty());
    for (const auto& note : report.notes)
    {
        ASSERT_TRUE(note.found);
        EXPECT_NEAR(note.onsetErrorSeconds(), 0.f, 0.006f);
        EXPECT_EQ(note.verdict, PitchVerdict::Right);
    }
    EXPECT_GT(report.marginDb(), 0.f);
    EXPECT_LT(report.marginDb(), 30.f);
}

TEST(NoteVerifier, ABandWiderThanTheGapToTheQuietSoundPullsTheOnsetBackToIt)
{
    auto tones = played();
    tones.push_back({0.93, 0.07, hzOf(79.0), kNoteDb - 28.0});
    NoteVerifierConfig wide;
    wide.noteBandDb = 40.f;
    const auto report = verify(tones, wide);
    ASSERT_TRUE(report.notes[1].found);
    EXPECT_NEAR(report.notes[1].onsetErrorSeconds(), -0.07f, 0.01f);
}

TEST(NoteVerifier, ASoundBelowTheThresholdIsIgnoredAndOneAboveItIsReported)
{
    auto quiet = played();
    quiet.push_back({2.6, 0.25, hzOf(69.0), kNoteDb - 18.0});
    const auto ignored = verify(quiet);
    EXPECT_TRUE(ignored.unexpected.empty());
    EXPECT_GT(ignored.marginDb(), 0.f);
    EXPECT_LT(ignored.marginDb(), 6.f);

    auto loud = played();
    loud.push_back({2.6, 0.25, hzOf(69.0), kNoteDb - 12.0});
    EXPECT_EQ(verify(loud).unexpected.size(), 1u);
}

TEST(NoteVerifier, AShortBurstAboveTheThresholdIsCountedButNotReported)
{
    auto tones = played();
    tones.push_back({2.6, 0.03, hzOf(69.0)});
    const auto report = verify(tones);
    EXPECT_TRUE(report.unexpected.empty());
    EXPECT_EQ(report.shortBursts, 1u);
}

TEST(NoteVerifier, ALoudDoubleOnsetIsOneNoteStartingAtTheFirstRise)
{
    auto tones = played();
    tones.push_back({0.96, 0.04, hzOf(55.0)});
    const auto report = verify(tones);
    ASSERT_TRUE(report.notes[1].found);
    EXPECT_NEAR(report.notes[1].onsetErrorSeconds(), -0.04f, 0.01f);
    EXPECT_EQ(report.notes[1].verdict, PitchVerdict::Right);
    EXPECT_TRUE(report.unexpected.empty());
}
