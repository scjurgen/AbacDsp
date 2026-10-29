#include <algorithm>
#include <array>
#include <cmath>
#include <gtest/gtest.h>
#include <span>
#include <vector>

#include "Analysis/YinPitchDetector.h"
#include "Numbers/Convert.h"
#include "impl/PhrasePlayer.h"

using namespace NotePattern;
using KsInstrument::Id;

namespace
{
constexpr float kSampleRate{48000.f};
constexpr float kBpm{120.f};
constexpr size_t kSamplesPerBeat{24000};
constexpr size_t kBlock{16};
constexpr auto kAllPresets = std::to_array<Id>({Id::Guitar, Id::Bass, Id::Harp, Id::Nylon, Id::Ukulele, Id::MusicBox});
// YIN reads its lag in whole samples, which is coarser than 5 cents above about 1 kHz.
constexpr float kPitchToleranceCents{5.f};
constexpr float kHighPitchToleranceCents{25.f};
constexpr float kHighPitchHz{1000.f};

void addNote(Pattern& pattern, const float position, const float duration, const uint8_t pitch,
             const uint8_t velocity = 127)
{
    pattern.notes[pattern.noteCount++] = Note{position, duration, pitch, velocity};
    pattern.active = true;
}

std::vector<float> render(PhrasePlayer& player, const size_t samples)
{
    std::vector<float> out(samples);
    for (size_t i = 0; i < samples; i += kBlock)
    {
        player.process(std::span<float>{out}.subspan(i, std::min(kBlock, samples - i)));
    }
    return out;
}

std::vector<float> renderPhrase(const Pattern& pattern, const Id instrument = Id::Guitar, const size_t delay = 0)
{
    PhrasePlayer player{kSampleRate};
    player.start(pattern, kBpm, instrument, delay);
    return render(player, player.lengthSamples() + 1000);
}

double rms(const std::vector<float>& x, const size_t from, const size_t count)
{
    double sum = 0.0;
    for (size_t i = from; i < from + count && i < x.size(); ++i)
    {
        sum += static_cast<double>(x[i]) * static_cast<double>(x[i]);
    }
    return std::sqrt(sum / static_cast<double>(count));
}

double rmsDb(const std::vector<float>& x, const size_t from, const size_t count)
{
    return 20.0 * std::log10(rms(x, from, count) + 1e-12);
}

float maxAbs(const std::vector<float>& x, const size_t from, const size_t to)
{
    float peak = 0.f;
    for (size_t i = from; i < to && i < x.size(); ++i)
    {
        peak = std::max(peak, std::abs(x[i]));
    }
    return peak;
}

float measuredHz(const std::vector<float>& x, const size_t from, const size_t count, const float expectedHz)
{
    AbacDsp::YinPitchDetector yin{kSampleRate, expectedHz * 0.6f, expectedHz * 1.8f, 20.f};
    float hz = 0.f;
    for (size_t i = from; i < from + count; ++i)
    {
        const float pitch = yin.step(x[i]);
        if (yin.hasNewPitch() && pitch > 0.f)
        {
            hz = pitch;
        }
    }
    return hz;
}
}

TEST(PhrasePlayerTest, NotesStartOnTheirBeatsAndRestsAreSilent)
{
    Pattern pattern{};
    addNote(pattern, 0.f, 1.f, 60);
    addNote(pattern, 2.f, 1.f, 64);
    const auto out = renderPhrase(pattern);
    constexpr size_t kFade = 1200 + 100;
    EXPECT_LT(maxAbs(out, kSamplesPerBeat + kFade, 2 * kSamplesPerBeat), 1e-3f);
    EXPECT_GT(maxAbs(out, 2 * kSamplesPerBeat, 2 * kSamplesPerBeat + 96), 1e-3f);
}

TEST(PhrasePlayerTest, FirstSoundAppearsAtSampleZeroForANoteAtThePhraseStart)
{
    Pattern pattern{};
    addNote(pattern, 0.f, 1.f, 60);
    const auto out = renderPhrase(pattern);
    EXPECT_GT(maxAbs(out, 0, 96), 1e-3f);
}

TEST(PhrasePlayerTest, PickupNotesLieBeforeTheDownbeat)
{
    Pattern pattern{};
    pattern.pickupBeats = 1.f;
    addNote(pattern, -1.f, 0.5f, 55);
    addNote(pattern, 0.f, 1.f, 60);
    PhrasePlayer player{kSampleRate};
    player.start(pattern, kBpm, Id::Guitar);
    EXPECT_EQ(player.downbeatSample(), kSamplesPerBeat);
    const auto out = render(player, player.lengthSamples());
    EXPECT_GT(maxAbs(out, 0, 96), 1e-3f);
    EXPECT_LT(maxAbs(out, kSamplesPerBeat / 2 + 1300, kSamplesPerBeat), 1e-3f);
    EXPECT_GT(maxAbs(out, kSamplesPerBeat, kSamplesPerBeat + 96), 1e-3f);
}

TEST(PhrasePlayerTest, TouchingNotesAreSeparatedByADipBeforeTheSecondAttack)
{
    Pattern pattern{};
    addNote(pattern, 0.f, 1.f, 60);
    addNote(pattern, 1.f, 1.f, 60);
    const auto out = renderPhrase(pattern);
    EXPECT_LT(maxAbs(out, kSamplesPerBeat - 600, kSamplesPerBeat), 1e-3f);
    EXPECT_GT(maxAbs(out, kSamplesPerBeat, kSamplesPerBeat + 96), 1e-3f);
}

TEST(PhrasePlayerTest, LengthCoversTheLastNoteAndItsTail)
{
    Pattern pattern{};
    addNote(pattern, 0.f, 2.f, 60);
    PhrasePlayer player{kSampleRate};
    player.start(pattern, kBpm, Id::Guitar);
    EXPECT_GT(player.lengthSamples(), 2 * kSamplesPerBeat + 1200);
    EXPECT_TRUE(player.isPlaying());
    static_cast<void>(render(player, player.lengthSamples()));
    EXPECT_FALSE(player.isPlaying());
}

TEST(PhrasePlayerTest, EveryPresetPlaysTheRightPitchForConsecutiveNotes)
{
    constexpr size_t kNoteCount{6};
    for (const auto id : kAllPresets)
    {
        const auto& preset = KsInstrument::presetOf(id);
        Pattern pattern{};
        std::vector<uint8_t> pitches;
        for (size_t i = 0; i < kNoteCount; ++i)
        {
            const int span = preset.highestNote() - preset.lowestNote;
            const auto pitch = static_cast<uint8_t>(preset.lowestNote + span * static_cast<int>(i) / (kNoteCount - 1));
            pitches.push_back(pitch);
            addNote(pattern, static_cast<float>(i), 0.9f, pitch);
        }
        const auto out = renderPhrase(pattern, id);
        for (size_t i = 0; i < kNoteCount; ++i)
        {
            const float expected = Convert::noteToFrequency<float>(static_cast<float>(pitches[i]), 440.f);
            const float hz = measuredHz(out, i * kSamplesPerBeat + 4800, 14400, expected);
            const double cents = 1200.0 * std::log2(static_cast<double>(hz) / static_cast<double>(expected));
            EXPECT_NEAR(cents, 0.0,
                        static_cast<double>(expected > kHighPitchHz ? kHighPitchToleranceCents : kPitchToleranceCents))
                << preset.name << " note " << static_cast<int>(pitches[i]);
        }
    }
}

TEST(PhrasePlayerTest, VelocityStepsComeOutAsTheirDecibelSteps)
{
    Pattern pattern{};
    addNote(pattern, 0.f, 1.f, 60, 55);
    addNote(pattern, 2.f, 1.f, 60, 80);
    addNote(pattern, 4.f, 1.f, 60, 105);
    pattern.patternBars = 2;
    const auto out = renderPhrase(pattern);
    const double soft = rmsDb(out, 0, 4800);
    const double middle = rmsDb(out, 2 * kSamplesPerBeat, 4800);
    const double loud = rmsDb(out, 4 * kSamplesPerBeat, 4800);
    EXPECT_NEAR(middle - soft, 25.0 * static_cast<double>(KsInstrument::kDbPerVelocityStep), 1.0);
    EXPECT_NEAR(loud - middle, 25.0 * static_cast<double>(KsInstrument::kDbPerVelocityStep), 1.0);
}

TEST(PhrasePlayerTest, LevelIsEvenAcrossTheRangeOfEveryPreset)
{
    constexpr double kToleranceDb{1.5};
    for (const auto id : kAllPresets)
    {
        const auto& preset = KsInstrument::presetOf(id);
        Pattern pattern{};
        std::vector<uint8_t> pitches;
        for (int note = preset.lowestNote; note <= preset.highestNote(); note += 2)
        {
            addNote(pattern, static_cast<float>(pitches.size()), 0.9f, static_cast<uint8_t>(note));
            pitches.push_back(static_cast<uint8_t>(note));
        }
        const auto out = renderPhrase(pattern, id);
        for (size_t i = 0; i < pitches.size(); ++i)
        {
            EXPECT_NEAR(rmsDb(out, i * kSamplesPerBeat, 4800), static_cast<double>(KsInstrument::kReferenceLevelDb),
                        kToleranceDb)
                << preset.name << " note " << static_cast<int>(pitches[i]);
        }
    }
}

TEST(PhrasePlayerTest, SamePhraseRendersIdenticallyAndTheDelayShiftsItExactly)
{
    Pattern pattern{};
    addNote(pattern, 0.f, 1.f, 60);
    addNote(pattern, 1.f, 1.f, 67, 90);
    constexpr size_t kDelay{100};
    const auto plain = renderPhrase(pattern);
    const auto again = renderPhrase(pattern);
    const auto delayed = renderPhrase(pattern, Id::Guitar, kDelay);
    EXPECT_EQ(plain, again);
    for (size_t i = 0; i < kDelay; ++i)
    {
        ASSERT_EQ(delayed[i], 0.f);
    }
    double errorEnergy = 0.0;
    double signalEnergy = 0.0;
    for (size_t i = 0; i + kDelay < plain.size(); ++i)
    {
        const double error = static_cast<double>(delayed[i + kDelay]) - static_cast<double>(plain[i]);
        errorEnergy += error * error;
        signalEnergy += static_cast<double>(plain[i]) * static_cast<double>(plain[i]);
    }
    EXPECT_LT(errorEnergy, 1e-4 * signalEnergy);
}

TEST(PhrasePlayerTest, StopFadesTheOutputOutWithinAFewMilliseconds)
{
    Pattern pattern{};
    addNote(pattern, 0.f, 2.f, 60);
    PhrasePlayer player{kSampleRate};
    player.start(pattern, kBpm, Id::Guitar);
    static_cast<void>(render(player, 1000));
    player.stop();
    const auto out = render(player, 2000);
    EXPECT_GT(maxAbs(out, 0, 100), 0.f);
    EXPECT_LT(maxAbs(out, 500, out.size()), 1e-4f);
    EXPECT_FALSE(player.isPlaying());
}

TEST(PhrasePlayerTest, FastBusyPhraseStaysBounded)
{
    Pattern pattern{};
    pattern.beatsPerBar = 16;
    for (int i = 0; i < 32; ++i)
    {
        addNote(pattern, 0.25f * static_cast<float>(i), 0.25f, static_cast<uint8_t>(40 + (i * 7) % 40));
    }
    const auto out = renderPhrase(pattern);
    EXPECT_LT(maxAbs(out, 0, out.size()), 1.f);
    EXPECT_TRUE(std::ranges::all_of(out, [](const float v) { return std::isfinite(v); }));
}

TEST(PhrasePlayerTest, EmptyPhraseIsSilentAndFinishes)
{
    const Pattern pattern{};
    PhrasePlayer player{kSampleRate};
    player.start(pattern, kBpm, Id::Harp);
    const auto out = render(player, player.lengthSamples() + 10);
    EXPECT_EQ(maxAbs(out, 0, out.size()), 0.f);
    EXPECT_FALSE(player.isPlaying());
}

TEST(PhrasePlayerTest, RestartingDropsTheRingingPhrase)
{
    Pattern first{};
    addNote(first, 0.f, 4.f, 40);
    Pattern second{};
    addNote(second, 2.f, 1.f, 72);
    PhrasePlayer player{kSampleRate};
    player.start(first, kBpm, Id::Bass);
    static_cast<void>(render(player, 5000));
    player.start(second, kBpm, Id::Guitar);
    const auto out = render(player, kSamplesPerBeat);
    EXPECT_LT(maxAbs(out, 500, out.size()), 1e-4f);
}
