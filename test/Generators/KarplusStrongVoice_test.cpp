#include <array>
#include <cmath>
#include <vector>

#include "gtest/gtest.h"

#include "Analysis/ZeroCrossings.h"
#include "Generators/ExcitationTechnique.h"
#include "Generators/KarplusStrongVoice.h"

namespace AbacDsp::Test
{

namespace
{
constexpr float kSampleRate = 48000.f;
using TestVoice = KarplusStrongVoice<10000>;

[[nodiscard]] std::vector<float> renderAfterTrigger(TestVoice& voice, const float note, const size_t numSamples)
{
    voice.trigger(note, 1.f);
    std::vector<float> out(numSamples);
    std::ranges::generate(out, [&voice] { return voice.step(); });
    return out;
}
}

TEST(KarplusStrongVoice, outputStaysBoundedAndFinite)
{
    TestVoice voice{kSampleRate};
    voice.setPluckType(PluckType::WhiteRoundRobin);
    voice.setFilterCutoffSemitones(0.f);
    voice.setFilterResonance(0.5f);
    voice.setKeyTracking(1.f);
    voice.setFilterLfoRateHz(4.f);
    voice.setFilterLfoDepthOctaves(1.f);
    voice.setFilterEnvelope(10.f, 300.f, 0.3f);

    const auto out = renderAfterTrigger(voice, 60.f, 30000);
    for (const float v : out)
    {
        ASSERT_TRUE(std::isfinite(v));
        ASSERT_LE(std::abs(v), 4.f);
    }
}

TEST(KarplusStrongVoice, keyTrackingBrightensHighNotesRelativeToFixedCutoff)
{
    constexpr float highNote = 96.f;

    TestVoice fixedCutoff{kSampleRate};
    fixedCutoff.setPluckType(PluckType::WhiteRoundRobin);
    fixedCutoff.setFilterCutoffSemitones(0.f);
    fixedCutoff.setFilterResonance(0.1f);
    fixedCutoff.setKeyTracking(0.f);
    const auto fixedOut = renderAfterTrigger(fixedCutoff, highNote, 8000);

    TestVoice tracked{kSampleRate};
    tracked.setPluckType(PluckType::WhiteRoundRobin);
    tracked.setFilterCutoffSemitones(0.f);
    tracked.setFilterResonance(0.1f);
    tracked.setKeyTracking(1.f);
    const auto trackedOut = renderAfterTrigger(tracked, highNote, 8000);

    const auto fixedStats = calculateZeroCrossingStatistics<float>(fixedOut, true);
    const auto trackedStats = calculateZeroCrossingStatistics<float>(trackedOut, true);

    EXPECT_GT(trackedStats.periodCount, fixedStats.periodCount)
        << "fixed=" << fixedStats.periodCount << " tracked=" << trackedStats.periodCount;
}

TEST(KarplusStrongVoice, zeroKeyTrackingKeepsCutoffSimilarAcrossNotes)
{
    TestVoice low{kSampleRate};
    low.setPluckType(PluckType::WhiteRoundRobin);
    low.setFilterCutoffSemitones(0.f);
    low.setFilterResonance(0.1f);
    low.setKeyTracking(0.f);
    const auto lowOut = renderAfterTrigger(low, 36.f, 8000);

    TestVoice high{kSampleRate};
    high.setPluckType(PluckType::WhiteRoundRobin);
    high.setFilterCutoffSemitones(0.f);
    high.setFilterResonance(0.1f);
    high.setKeyTracking(0.f);
    const auto highOut = renderAfterTrigger(high, 84.f, 8000);

    const auto lowStats = calculateZeroCrossingStatistics<float>(lowOut, true);
    const auto highStats = calculateZeroCrossingStatistics<float>(highOut, true);

    // Some difference remains from the string's own pitch leaking through a resonance-widened
    // passband, but it should be far smaller than the >2x swing keyTracking=1 produces above.
    const auto ratio = static_cast<float>(highStats.periodCount) / static_cast<float>(lowStats.periodCount);
    EXPECT_LT(ratio, 2.f) << "low=" << lowStats.periodCount << " high=" << highStats.periodCount;
}

TEST(KarplusStrongVoice, higherResonanceStaysBoundedAndFinite)
{
    TestVoice voice{kSampleRate};
    voice.setPluckType(PluckType::WhiteRoundRobin);
    voice.setFilterCutoffSemitones(12.f);
    voice.setFilterResonance(1.8f);
    voice.setKeyTracking(1.f);

    const auto out = renderAfterTrigger(voice, 60.f, 20000);
    for (const float v : out)
    {
        ASSERT_TRUE(std::isfinite(v));
        ASSERT_LE(std::abs(v), 4.f);
    }
}

TEST(KarplusStrongVoice, isActiveTracksStringLifecycle)
{
    TestVoice voice{kSampleRate};
    EXPECT_FALSE(voice.isActive());
    voice.trigger(60.f, 1.f);
    EXPECT_TRUE(voice.isActive());
    voice.muteString();
    EXPECT_FALSE(voice.isActive());
}

TEST(KarplusStrongVoice, everyExcitationTypeProducesBoundedFiniteOutput)
{
    const std::array<ExcitationEvent, 8> events{{
        {0.f, 0.f, ExcitationType::Pluck, 0.8f, {}},
        {0.f, 0.f, ExcitationType::Strike, 0.8f, {}},
        {0.f, 10.f, ExcitationType::Mute, 0.f, {}},
        {0.f, 200.f, ExcitationType::PalmMute, 0.6f, {}},
        {0.f, 500.f, ExcitationType::Bow, 0.4f, {}},
        {0.f, 500.f, ExcitationType::Sympathetic, 0.4f, 3.f},
        {0.f, 500.f, ExcitationType::Wind, 0.4f, {}},
        {0.f, 500.f, ExcitationType::Rub, 0.4f, {}},
    }};
    for (const auto& event : events)
    {
        TestVoice voice{kSampleRate};
        voice.trigger(60.f, 0.5f);
        voice.scheduleExcitation(event);
        for (int i = 0; i < 30000; ++i)
        {
            const auto v = voice.step();
            ASSERT_TRUE(std::isfinite(v)) << "type " << static_cast<int>(event.type);
            ASSERT_LE(std::abs(v), 4.f) << "type " << static_cast<int>(event.type);
        }
    }
}

TEST(KarplusStrongVoice, strikeRestoresFilterAndAttackParametersAfterFiring)
{
    TestVoice voice{kSampleRate};
    const auto originalFactor = voice.initialFilterFactor();
    const auto originalAttackMs = voice.attackTimeMsecs();

    voice.scheduleExcitation({0.f, 0.f, ExcitationType::Strike, 0.8f, {}});
    std::ignore = voice.step(); // startMs=0: beginExcitation()/beginStrike() fire synchronously here

    EXPECT_FLOAT_EQ(voice.initialFilterFactor(), originalFactor);
    EXPECT_FLOAT_EQ(voice.attackTimeMsecs(), originalAttackMs);
    EXPECT_TRUE(voice.isActive());
}

TEST(KarplusStrongVoice, bowWakesAnInactiveStringAndSustainsIt)
{
    TestVoice voice{kSampleRate};
    EXPECT_FALSE(voice.isActive());

    voice.scheduleExcitation({0.f, 2000.f, ExcitationType::Bow, 0.5f, {}});
    std::ignore = voice.step();
    EXPECT_TRUE(voice.isActive());

    float peak = 0.f;
    for (int i = 0; i < 20000; ++i)
    {
        peak = std::max(peak, std::abs(voice.step()));
    }
    EXPECT_GT(peak, 1e-4f);
}

TEST(KarplusStrongVoice, muteFadesAnActiveVoiceToSilence)
{
    TestVoice voice{kSampleRate};
    voice.trigger(60.f, 1.f);
    ASSERT_TRUE(voice.isActive());

    voice.scheduleExcitation({0.f, 20.f, ExcitationType::Mute, 0.f, {}});
    for (int i = 0; i < 20000 && voice.isActive(); ++i)
    {
        std::ignore = voice.step();
    }
    EXPECT_FALSE(voice.isActive());
}

TEST(KarplusStrongVoice, excitationConstFeedAddsOnTopOfBaseSustainLevel)
{
    constexpr size_t settleSamples = 20000;

    TestVoice baseOnly{kSampleRate};
    baseOnly.setPluckType(PluckType::WhiteStatic);
    baseOnly.setConstFeed(0.3f);
    baseOnly.trigger(60.f, 1.f);
    for (size_t i = 0; i < settleSamples; ++i)
    {
        std::ignore = baseOnly.step();
    }
    float baseOnlyPeak = 0.f;
    for (int i = 0; i < 2000; ++i)
    {
        baseOnlyPeak = std::max(baseOnlyPeak, std::abs(baseOnly.step()));
    }

    TestVoice baseAndBow{kSampleRate};
    baseAndBow.setPluckType(PluckType::WhiteStatic);
    baseAndBow.setConstFeed(0.3f);
    baseAndBow.trigger(60.f, 1.f);
    baseAndBow.scheduleExcitation({0.f, 60000.f, ExcitationType::Bow, 0.4f, {}}); // spans past settleSamples
    for (size_t i = 0; i < settleSamples; ++i)
    {
        std::ignore = baseAndBow.step();
    }
    float baseAndBowPeak = 0.f;
    for (int i = 0; i < 2000; ++i)
    {
        baseAndBowPeak = std::max(baseAndBowPeak, std::abs(baseAndBow.step()));
    }

    EXPECT_GT(baseAndBowPeak, baseOnlyPeak);
}

TEST(KarplusStrongVoice, secondScheduleWhileOneIsActiveQueuesRatherThanPreempting)
{
    TestVoice voice{kSampleRate};
    voice.trigger(60.f, 1.f);
    const auto originalDamper = voice.damperFactor();

    voice.scheduleExcitation({0.f, 500.f, ExcitationType::Bow, 0.4f, {}}); // 500 ms window
    std::ignore = voice.step();
    voice.scheduleExcitation({0.f, 50.f, ExcitationType::PalmMute, 0.6f, {}}); // must queue, not preempt

    // Still well inside Bow's window: PalmMute must not have started yet.
    for (int i = 0; i < 1000; ++i)
    {
        std::ignore = voice.step();
        EXPECT_FLOAT_EQ(voice.damperFactor(), originalDamper);
    }
}

TEST(KarplusStrongVoice, queuedTechniquePromotesAfterCurrentOneEnds)
{
    TestVoice voice{kSampleRate};
    voice.trigger(60.f, 1.f);
    const auto originalDamper = voice.damperFactor();

    voice.scheduleExcitation({0.f, 100.f, ExcitationType::Bow, 0.4f, {}}); // 100 ms window
    std::ignore = voice.step();
    voice.scheduleExcitation({0.f, 500.f, ExcitationType::PalmMute, 0.6f, {}}); // queued behind Bow

    bool sawRaisedDamper = false;
    for (int i = 0; i < 10000; ++i)
    {
        std::ignore = voice.step();
        if (voice.damperFactor() > originalDamper + 1e-4f)
        {
            sawRaisedDamper = true;
            break;
        }
    }
    EXPECT_TRUE(sawRaisedDamper);
}

TEST(KarplusStrongVoice, queuedStartMsIsMeasuredFromPromotionNotFromScheduleCall)
{
    TestVoice voice{kSampleRate};
    voice.trigger(60.f, 1.f);
    const auto originalDamper = voice.damperFactor();

    constexpr size_t bowWindowSamples = 4800;     // 100 ms at 48 kHz
    constexpr size_t palmMuteDelaySamples = 2400; // 50 ms at 48 kHz

    voice.scheduleExcitation({0.f, 100.f, ExcitationType::Bow, 0.4f, {}});
    std::ignore = voice.step();
    voice.scheduleExcitation({50.f, 500.f, ExcitationType::PalmMute, 0.6f, {}}); // queued

    // Right as Bow's window closes, PalmMute must not have started yet - its own 50 ms delay
    // only begins once it is promoted to current, not from this scheduling call.
    for (size_t i = 0; i < bowWindowSamples; ++i)
    {
        std::ignore = voice.step();
    }
    EXPECT_FLOAT_EQ(voice.damperFactor(), originalDamper);

    bool sawRaisedDamper = false;
    for (size_t i = 0; i < palmMuteDelaySamples * 2; ++i)
    {
        std::ignore = voice.step();
        if (voice.damperFactor() > originalDamper + 1e-4f)
        {
            sawRaisedDamper = true;
            break;
        }
    }
    EXPECT_TRUE(sawRaisedDamper);
}

}
