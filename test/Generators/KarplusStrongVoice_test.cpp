#include <cmath>
#include <vector>

#include "gtest/gtest.h"

#include "Analysis/ZeroCrossings.h"
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

}
