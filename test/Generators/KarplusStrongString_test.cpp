#include <cmath>
#include <tuple>
#include <vector>

#include "gtest/gtest.h"

#include "Analysis/YinPitchDetector.h"
#include "Generators/KarplusStrongString.h"

namespace AbacDsp::Test
{

namespace
{
constexpr float kSampleRate = 48000.f;
using TestString = KarplusStrongString<10000>;

// Median of every pitch estimate the detector reports while a fixed-length block of
// already-rendered samples is fed through it a second time (first pass primes its buffer).
[[nodiscard]] float detectMedianPitch(const std::vector<float>& signal, const float minFreq, const float maxFreq)
{
    YinPitchDetector detector(kSampleRate, minFreq, maxFreq, 50.f);
    for (const auto sample : signal)
    {
        std::ignore = detector.step(sample);
    }
    std::vector<float> pitches;
    for (const auto sample : signal)
    {
        const float pitch = detector.step(sample);
        if (detector.hasNewPitch())
        {
            pitches.push_back(pitch);
        }
    }
    if (pitches.empty())
    {
        return 0.f;
    }
    std::ranges::sort(pitches);
    return pitches[pitches.size() / 2];
}

// dB drop from the pluck's initial peak to the peak elapsedSeconds later, with the damper
// bypassed so only setDecayByTime()/setDecayOctaveFactor() are under test.
[[nodiscard]] float measureDecayDb(const float note, const float decayMs, const float octaveFactor,
                                   const float elapsedSeconds)
{
    TestString sut{kSampleRate};
    sut.setPluckType(PluckType::WhiteStatic);
    sut.setDecayByTime(decayMs);
    sut.setDecayOctaveFactor(octaveFactor);
    sut.trigger(note, 1.f);
    sut.setDamperCutoff(24000.f);

    float earlyPeak = 0.f;
    for (int i = 0; i < 1000; ++i)
    {
        earlyPeak = std::max(earlyPeak, std::abs(sut.step()));
    }
    const auto elapsedSamples = static_cast<int>(kSampleRate * elapsedSeconds);
    for (int i = 0; i < elapsedSamples; ++i)
    {
        std::ignore = sut.step();
    }
    float latePeak = 0.f;
    for (int i = 0; i < 1000; ++i)
    {
        latePeak = std::max(latePeak, std::abs(sut.step()));
    }
    return 20.f * std::log10(latePeak / earlyPeak);
}
}

TEST(KarplusStrongString, roundRobinPluckDiverges)
{
    TestString sut{kSampleRate};
    TestString sut2{kSampleRate};
    sut.setPluckType(PluckType::WhiteRoundRobin);
    sut2.setPluckType(PluckType::WhiteRoundRobin);
    sut.trigger(60.f, 1.f);
    sut2.trigger(60.f, 1.f);
    EXPECT_NE(sut.step(), sut2.step());
    EXPECT_NE(sut.step(), sut2.step());
    EXPECT_NE(sut.step(), sut2.step());
    EXPECT_NE(sut.step(), sut2.step());
}

TEST(KarplusStrongString, staticPluckMatchesAcrossInstances)
{
    TestString sut{kSampleRate};
    TestString sut2{kSampleRate};
    sut.setPluckType(PluckType::WhiteStatic);
    sut2.setPluckType(PluckType::WhiteStatic);
    sut.trigger(60.f, 1.f);
    sut2.trigger(60.f, 1.f);
    EXPECT_NE(sut.step(), 0.f);
    EXPECT_NE(sut2.step(), 0.f);
    EXPECT_EQ(sut.step(), sut2.step());
    EXPECT_EQ(sut.step(), sut2.step());
    EXPECT_EQ(sut.step(), sut2.step());
    EXPECT_EQ(sut.step(), sut2.step());
}

TEST(KarplusStrongString, outputStaysBoundedAndFinite)
{
    TestString sut{kSampleRate};
    sut.setPluckType(PluckType::WhiteRoundRobin);
    sut.trigger(48.f, 1.f);
    for (int i = 0; i < 20000; ++i)
    {
        const float v = sut.step();
        ASSERT_TRUE(std::isfinite(v));
        ASSERT_LE(std::abs(v), 2.f);
    }
}

TEST(KarplusStrongString, decaysTowardsSilenceOverTime)
{
    TestString sut{kSampleRate};
    sut.setPluckType(PluckType::WhiteStatic);
    sut.setDecayByTime(200.f);
    sut.trigger(60.f, 1.f);

    float earlyPeak = 0.f;
    for (int i = 0; i < 1000; ++i)
    {
        earlyPeak = std::max(earlyPeak, std::abs(sut.step()));
    }
    float latePeak = 0.f;
    for (int i = 0; i < 47000; ++i)
    {
        std::ignore = sut.step();
    }
    for (int i = 0; i < 1000; ++i)
    {
        latePeak = std::max(latePeak, std::abs(sut.step()));
    }
    EXPECT_GT(earlyPeak, latePeak * 2.f);
}

TEST(KarplusStrongString, extremeDecayTime)
{
    TestString sut{kSampleRate};
    sut.setPluckType(PluckType::WhiteStatic);
    sut.setDecayByTime(100000.f); // 100 secs
    sut.trigger(60.f, 1.f);
    sut.setDamperCutoff(24000); // bypass the damper so only decayGain is under test

    float earlyPeak = 0.f;
    for (int i = 0; i < 1000; ++i)
    {
        earlyPeak = std::max(earlyPeak, std::abs(sut.step()));
    }
    float latePeak = 0.f;
    for (int i = 0; i < 48000 * 100; ++i)
    {
        std::ignore = sut.step();
    }
    for (int i = 0; i < 1000; ++i)
    {
        latePeak = std::max(latePeak, std::abs(sut.step()));
    }
    EXPECT_GT(latePeak, 0.0001f);
}

TEST(KarplusStrongString, decayScalesPerOctave)
{
    constexpr float decayMs = 2000.f;
    const auto dbAt60 = measureDecayDb(60.f, decayMs, 1.f, decayMs / 1000.f);
    const auto dbAt72 = measureDecayDb(72.f, decayMs, 1.f, decayMs / 1000.f);
    const auto dbAt48 = measureDecayDb(48.f, decayMs, 1.f, decayMs / 1000.f);

    EXPECT_NEAR(dbAt60, -20.f, 2.f); // reference note: unaffected by the octave factor
    EXPECT_LT(dbAt72, dbAt60);       // octave up: decays faster (shorter string)
    EXPECT_GT(dbAt48, dbAt60);       // octave down: decays slower (longer string)
}

TEST(KarplusStrongString, decayOctaveFactorZeroIsFlatAcrossNotes)
{
    constexpr float decayMs = 2000.f;
    const auto dbAt60 = measureDecayDb(60.f, decayMs, 0.f, decayMs / 1000.f);
    const auto dbAt72 = measureDecayDb(72.f, decayMs, 0.f, decayMs / 1000.f);
    const auto dbAt48 = measureDecayDb(48.f, decayMs, 0.f, decayMs / 1000.f);

    EXPECT_NEAR(dbAt60, -20.f, 2.f);
    EXPECT_NEAR(dbAt72, dbAt60, 2.f);
    EXPECT_NEAR(dbAt48, dbAt60, 2.f);
}

TEST(KarplusStrongString, isActiveTracksTriggerStopAndMute)
{
    TestString sut{kSampleRate};
    EXPECT_FALSE(sut.isActive());

    sut.attackTime(0.f);
    sut.releaseTime(50.f);
    sut.trigger(60.f, 1.f);
    EXPECT_TRUE(sut.isActive());

    sut.stopString();
    EXPECT_TRUE(sut.isActive()); // still releasing
    for (int i = 0; i < 10000 && sut.isActive(); ++i)
    {
        std::ignore = sut.step();
    }
    EXPECT_FALSE(sut.isActive());

    sut.trigger(60.f, 1.f);
    EXPECT_TRUE(sut.isActive());
    sut.muteString();
    EXPECT_FALSE(sut.isActive());
}

TEST(KarplusStrongString, frequencyMatchesTargetNote)
{
    for (const float note : {48.f, 57.f, 69.f, 81.f})
    {
        TestString sut{kSampleRate};
        sut.setPluckType(PluckType::WhiteStatic);
        sut.setDamper(0.f);
        sut.setDecayByTime(2000.f);
        sut.trigger(note, 1.f);

        std::vector<float> rendered(20000);
        std::ranges::generate(rendered, [&sut] { return sut.step(); });

        const float targetFrequency = Convert::noteToFrequency(note);
        const float detected = detectMedianPitch(rendered, targetFrequency * 0.5f, targetFrequency * 2.f);
        EXPECT_NEAR(detected, targetFrequency, targetFrequency * 0.03f) << "note " << note;
    }
}

TEST(KarplusStrongString, pitchBendShiftsFrequencyUpward)
{
    constexpr float note = 60.f;
    TestString sut{kSampleRate};
    sut.setPluckType(PluckType::WhiteStatic);
    sut.setDamper(0.f);
    sut.setDecayByTime(2000.f);
    sut.trigger(note, 1.f);
    sut.bendInCents(1200.f); // one octave up

    std::vector<float> rendered(20000);
    std::ranges::generate(rendered, [&sut] { return sut.step(); });

    const float baseFrequency = Convert::noteToFrequency(note);
    const float expectedFrequency = baseFrequency * 2.f;
    const float detected = detectMedianPitch(rendered, baseFrequency, baseFrequency * 4.f);
    EXPECT_NEAR(detected, expectedFrequency, expectedFrequency * 0.03f);
}

}
