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

struct AutocorrelationResult
{
    float frequency;
    float peakCorrelation;
};

// Compares a 2-period window against the next at lags near the known-good expected period,
// refined to sub-sample precision by parabolic interpolation - sidesteps the octave/harmonic
// confusion a wide-range or zero-crossing search would hit on this wave's rich harmonics.
[[nodiscard]] AutocorrelationResult measureFrequencyByAutocorrelation(const std::vector<float>& signal,
                                                                      const float sampleRate,
                                                                      const float expectedFrequency)
{
    const float expectedPeriod = sampleRate / expectedFrequency;
    const auto windowLength = static_cast<size_t>(std::lround(2.f * expectedPeriod));
    const auto baseLag = static_cast<size_t>(std::lround(expectedPeriod));
    const auto searchRadius = std::max<size_t>(2, static_cast<size_t>(std::lround(0.1f * expectedPeriod)));
    // Skip past the initial-fill transient (the loop isn't truly recursing until its first
    // full traversal) so the waveform has settled before the windows are compared.
    const size_t referenceStart = 6 * static_cast<size_t>(std::lround(expectedPeriod));

    const auto correlationAt = [&](const size_t lag) noexcept
    {
        float dot = 0.f;
        float refEnergy = 0.f;
        float cmpEnergy = 0.f;
        for (size_t i = 0; i < windowLength; ++i)
        {
            const float a = signal[referenceStart + i];
            const float b = signal[referenceStart + lag + i];
            dot += a * b;
            refEnergy += a * a;
            cmpEnergy += b * b;
        }
        return dot / std::sqrt(refEnergy * cmpEnergy + 1e-12f);
    };

    size_t bestLag = baseLag - searchRadius;
    float bestCorrelation = correlationAt(bestLag);
    for (size_t lag = baseLag - searchRadius + 1; lag <= baseLag + searchRadius; ++lag)
    {
        const float correlation = correlationAt(lag);
        if (correlation > bestCorrelation)
        {
            bestCorrelation = correlation;
            bestLag = lag;
        }
    }

    const float y1 = correlationAt(bestLag - 1);
    const float y2 = bestCorrelation;
    const float y3 = correlationAt(bestLag + 1);
    const float denom = y1 - 2.f * y2 + y3;
    const float offset = std::abs(denom) < 1e-9f ? 0.f : 0.5f * (y1 - y3) / denom;
    const float refinedLag = static_cast<float>(bestLag) + offset;
    return {sampleRate / refinedLag, bestCorrelation};
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

TEST(KarplusStrongString, frequencyStaysOnPitchAcrossDamperRange)
{
    constexpr float note = 60.f;
    const float targetFrequency = Convert::noteToFrequency(note);

    for (const float damper : {0.5f, 0.6f, 0.7f, 0.8f, 0.9f, 1.f})
    {
        TestString sut{kSampleRate};
        sut.setPluckType(PluckType::WhiteStatic);
        sut.setDecayByTime(5000.f);
        sut.setDamper(damper);
        sut.trigger(note, 1.f);

        std::vector<float> rendered(2500);
        std::ranges::generate(rendered, [&sut] { return sut.step(); });

        const auto result = measureFrequencyByAutocorrelation(rendered, kSampleRate, targetFrequency);
        ASSERT_GT(result.peakCorrelation, 0.9f) << "damper " << damper;
        EXPECT_NEAR(result.frequency, targetFrequency, targetFrequency * 0.005f) << "damper " << damper;
    }
}

TEST(KarplusStrongString, wakeSustainResumesFrozenBufferAfterMute)
{
    TestString sut{kSampleRate};
    sut.setPluckType(PluckType::WhiteStatic);
    sut.setDecayByTime(50000.f);
    sut.trigger(60.f, 1.f);
    for (int i = 0; i < 2000; ++i)
    {
        std::ignore = sut.step();
    }
    sut.muteString();
    ASSERT_FALSE(sut.isActive());

    sut.wakeSustain(1.f);
    ASSERT_TRUE(sut.isActive());
    // muteString() freezes the buffer instead of clearing it (step() skips computeNext()
    // entirely while Stopped), so waking resumes the still-resonating content immediately.
    EXPECT_NE(sut.step(), 0.f);
}

TEST(KarplusStrongString, wakeSustainDoesNotOverrideAnAlreadyActiveGain)
{
    TestString sut{kSampleRate};
    sut.setPluckType(PluckType::WhiteStatic);
    sut.setDecayByTime(50000.f);
    sut.trigger(60.f, 1.f);
    for (int i = 0; i < 2000; ++i)
    {
        std::ignore = sut.step();
    }

    float peakBefore = 0.f;
    for (int i = 0; i < 1000; ++i)
    {
        peakBefore = std::max(peakBefore, std::abs(sut.step()));
    }

    sut.wakeSustain(0.01f); // no-op: already active, must not clobber the running gain

    float peakAfter = 0.f;
    for (int i = 0; i < 1000; ++i)
    {
        peakAfter = std::max(peakAfter, std::abs(sut.step()));
    }
    EXPECT_GT(peakAfter, peakBefore * 0.5f);
}

TEST(KarplusStrongString, muteWithFadeEndsAfterRequestedDuration)
{
    TestString sut{kSampleRate};
    sut.trigger(60.f, 1.f);
    constexpr size_t fadeSamples = 480;
    sut.muteWithFade(fadeSamples);

    size_t stepsTaken = 0;
    while (sut.isActive() && stepsTaken < fadeSamples * 2)
    {
        std::ignore = sut.step();
        ++stepsTaken;
    }
    EXPECT_FALSE(sut.isActive());
    EXPECT_NEAR(static_cast<float>(stepsTaken), static_cast<float>(fadeSamples), 2.f);
}

TEST(KarplusStrongString, muteWithFadeFloorsZeroDurationToAvoidAClick)
{
    TestString sut{kSampleRate};
    sut.trigger(60.f, 1.f);
    sut.muteWithFade(0);

    size_t stepsTaken = 0;
    while (sut.isActive() && stepsTaken < 1000)
    {
        std::ignore = sut.step();
        ++stepsTaken;
    }
    EXPECT_FALSE(sut.isActive());
    EXPECT_GT(stepsTaken, 10U); // floored to a few ms, not an instant same-sample cut
}

TEST(KarplusStrongString, muteWithFadeIsNoOpWhenAlreadyStopped)
{
    TestString sut{kSampleRate};
    EXPECT_FALSE(sut.isActive());
    sut.muteWithFade(480);
    EXPECT_FALSE(sut.isActive());
    EXPECT_EQ(sut.step(), 0.f);
}

TEST(KarplusStrongString, liveDamperChangeDoesNotShiftPitch)
{
    constexpr float note = 60.f;
    const float targetFrequency = Convert::noteToFrequency(note);

    TestString sut{kSampleRate};
    sut.setPluckType(PluckType::WhiteStatic);
    sut.setDecayByTime(10000.f);
    sut.setDamper(0.2f);
    sut.trigger(note, 1.f);

    // Settle past the initial transient, then change the damper live - mid-note, not via a
    // fresh trigger() - and confirm pitch does not drift as a result.
    for (int i = 0; i < 5000; ++i)
    {
        std::ignore = sut.step();
    }
    sut.setDamper(0.8f);

    std::vector<float> rendered(4000);
    std::ranges::generate(rendered, [&sut] { return sut.step(); });

    const auto result = measureFrequencyByAutocorrelation(rendered, kSampleRate, targetFrequency);
    ASSERT_GT(result.peakCorrelation, 0.8f);
    EXPECT_NEAR(result.frequency, targetFrequency, targetFrequency * 0.01f);
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
