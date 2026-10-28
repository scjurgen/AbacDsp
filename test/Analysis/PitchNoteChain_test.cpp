#include <array>
#include <cmath>
#include <cstdint>
#include <gtest/gtest.h>
#include <optional>
#include <tuple>

#include "Analysis/PitchNoteTracker.h"
#include "Analysis/StampedYinPitchDetector.h"
#include "Numbers/Random.h"

namespace AbacDsp::Test
{

namespace
{
constexpr float kRate{48000.0f};
constexpr float kLowestHz{40.0f};
constexpr float kHighestHz{2000.0f};
constexpr float kHopHz{50.0f};
constexpr float kPi{3.14159265358979323846f};

struct Tone
{
    float hz;
    int note;
    float expectedMs; ///< stamp of the first note: window end after three steady readings
};

constexpr std::array<Tone, 5> kTones{
    {{41.2f, 28, 100.0f}, {82.4f, 40, 100.0f}, {220.0f, 57, 80.0f}, {1000.0f, 83, 80.0f}, {2000.0f, 95, 80.0f}}};

struct Notes
{
    int count{0};
    int releases{0};
    PitchNote first{};
};

template <typename Signal>
Notes listen(const size_t decimation, const size_t samples, Signal&& signal)
{
    StampedYinPitchDetector detector(kRate, kLowestHz, kHighestHz, kHopHz, {decimation});
    PitchNoteTracker tracker;
    Notes notes;
    for (size_t i = 0; i < samples; ++i)
    {
        std::ignore = detector.step(signal(i));
        if (!detector.hasNewPitch())
        {
            continue;
        }
        const std::optional<PitchNote> event = tracker.update({detector.getCurrentPitch(), detector.getLastConfidence(),
                                                               detector.getLastLevel(), detector.getLastWindowEnd()});
        if (event && !event->released)
        {
            notes.first = notes.count == 0 ? *event : notes.first;
            ++notes.count;
        }
        notes.releases += event && event->released ? 1 : 0;
    }
    return notes;
}

auto sineOf(const float hz, const float amplitude)
{
    return [=](const size_t i) { return amplitude * std::sin(2.0f * kPi * hz * static_cast<float>(i) / kRate); };
}
}

class PitchNoteChain : public ::testing::TestWithParam<size_t>
{
};

TEST_P(PitchNoteChain, EveryToneFrom41HzTo2kHzIsOneNoteWithTheRightNumber)
{
    for (const Tone& tone : kTones)
    {
        SCOPED_TRACE("tone " + std::to_string(tone.hz));
        const Notes notes = listen(GetParam(), 48000, sineOf(tone.hz, 0.5f));
        EXPECT_EQ(notes.count, 1);
        EXPECT_EQ(notes.first.note, tone.note);
        EXPECT_NEAR(notes.first.hz / tone.hz, 1.0f, 0.012f);
    }
}

TEST_P(PitchNoteChain, TheFirstNoteComesAfterThreeSteadyReadings)
{
    for (const Tone& tone : kTones)
    {
        SCOPED_TRACE("tone " + std::to_string(tone.hz));
        const Notes notes = listen(GetParam(), 48000, sineOf(tone.hz, 0.5f));
        const double ms = static_cast<double>(notes.first.stamp) / static_cast<double>(kRate) * 1000.0;
        EXPECT_NEAR(ms, static_cast<double>(tone.expectedMs), 1.0);
    }
}

TEST_P(PitchNoteChain, TonesAboveEightyHertzAreReportedWithinAboutEightyMilliseconds)
{
    const Notes notes = listen(GetParam(), 48000, sineOf(220.0f, 0.5f));
    EXPECT_EQ(notes.first.note, 57);
    EXPECT_NEAR(static_cast<double>(notes.first.stamp) / static_cast<double>(kRate), 0.080, 0.001);
}

TEST_P(PitchNoteChain, ATonePlayedDownToAmplitudeOneHundredthIsANoteAndBelowTheLevelGateIsNot)
{
    for (const Tone& tone : kTones)
    {
        SCOPED_TRACE("tone " + std::to_string(tone.hz));
        EXPECT_EQ(listen(GetParam(), 48000, sineOf(tone.hz, 0.01f)).count, 1);
        EXPECT_EQ(listen(GetParam(), 48000, sineOf(tone.hz, 1e-3f)).count, 0);
        EXPECT_EQ(listen(GetParam(), 48000, sineOf(tone.hz, 1e-4f)).count, 0);
    }
}

TEST_P(PitchNoteChain, NoiseAtAnyLevelIsNeverANote)
{
    for (const float amplitude : {1.0f, 0.5f, 0.05f, 1e-2f, 1e-3f, 1e-4f})
    {
        SCOPED_TRACE("amplitude " + std::to_string(amplitude));
        Random random(7);
        const Notes notes = listen(GetParam(), 96000, [&random, amplitude](const size_t)
                                   { return amplitude * (2.0f * random.uniform() - 1.0f); });
        EXPECT_EQ(notes.count, 0);
    }
}

TEST_P(PitchNoteChain, TheEndOfATonePlusSilenceIsOneRelease)
{
    const auto tone = sineOf(220.0f, 0.5f);
    const Notes notes = listen(GetParam(), 48000, [&tone](const size_t i) { return i < 24000 ? tone(i) : 0.0f; });
    EXPECT_EQ(notes.count, 1);
    EXPECT_EQ(notes.releases, 1);
}

INSTANTIATE_TEST_SUITE_P(Decimation, PitchNoteChain, ::testing::Values(1u, 4u));

TEST(StampedYinPitchDetectorRange, ReadsTheFiveTonesAtAmplitudeOneTenThousandth)
{
    for (const size_t decimation : {1u, 4u})
    {
        for (const Tone& tone : kTones)
        {
            SCOPED_TRACE("tone " + std::to_string(tone.hz) + " decimation " + std::to_string(decimation));
            StampedYinPitchDetector detector(kRate, kLowestHz, kHighestHz, kHopHz, {decimation});
            const auto signal = sineOf(tone.hz, 1e-4f);
            float pitch = 0.0f;
            for (size_t i = 0; i < 48000; ++i)
            {
                pitch = detector.step(signal(i));
            }
            EXPECT_NEAR(pitch / tone.hz, 1.0f, 0.012f);
            EXPECT_GT(detector.getLastConfidence(), 0.95f);
        }
    }
}

}
