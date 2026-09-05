#include <array>
#include <cmath>
#include <format>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <limits>
#include <vector>

#include "Analysis/ZeroCrossings.h"
#include "Audio/AudioBuffer.h"
#include "impl/MorphexsynthImpl.h"

namespace
{
constexpr float kSampleRate = 48000.f;
constexpr size_t kBlockSize = 16;
using Impl = MorphexsynthImpl<kBlockSize>;

std::array<uint8_t, 3> noteOn(const int channel, const int note, const int velocity)
{
    return {static_cast<uint8_t>(0x90 | channel), static_cast<uint8_t>(note), static_cast<uint8_t>(velocity)};
}

std::array<uint8_t, 3> noteOff(const int channel, const int note)
{
    return {static_cast<uint8_t>(0x80 | channel), static_cast<uint8_t>(note), 0x40};
}

std::array<uint8_t, 3> pitchBend(const int channel, const int centeredValue)
{
    const int wire = centeredValue + 8192;
    return {static_cast<uint8_t>(0xE0 | channel), static_cast<uint8_t>(wire & 0x7F),
            static_cast<uint8_t>((wire >> 7) & 0x7F)};
}

// For an engine that isn't the fixture's own `impl` (a test needing several independent
// engines); the fixture's renderBlocks()/renderPastRelease() cover the common case.
void renderBlocksOn(Impl& target, const int count)
{
    AbacDsp::AudioBuffer<2, kBlockSize> in{};
    AbacDsp::AudioBuffer<2, kBlockSize> out{};
    for (int i = 0; i < count; ++i)
    {
        target.processBlock(in, out);
    }
}

// Average zero-crossing period (samples) of an engine's left channel over the next
// sampleCount samples, as a proxy for pitch - lower is higher-pitched.
float measurePeriod(Impl& target, const size_t sampleCount = 4096)
{
    std::vector<float> left(sampleCount);
    AbacDsp::AudioBuffer<2, kBlockSize> in{};
    AbacDsp::AudioBuffer<2, kBlockSize> out{};
    for (size_t written = 0; written < sampleCount; written += kBlockSize)
    {
        target.processBlock(in, out);
        for (size_t i = 0; i < kBlockSize; ++i)
        {
            left[written + i] = out(i, 0);
        }
    }
    return AbacDsp::periodLengthByZeroCrossingAverage(left.data(), left.size(), true);
}
}

/**
 * @brief Shared fixture: a fully-configured, audible voice (explicit oscillator level,
 * wide-open filter, a real ADSR, X wired to PitchBend) so a test never accidentally relies
 * on the stub script's silent default. Also collects the repeated "run N blocks and look at
 * the result" shapes used across these tests, so each test states its own intent (settle,
 * check finite/bounded, check silence, measure loudness) instead of re-deriving the loop.
 */
class MorphexsynthTest : public ::testing::Test
{
  protected:
    static constexpr int kChannel{0};
    static constexpr int kNote{60};
    static constexpr int kVelocity{100};
    static constexpr float kAttackMs{2.f};
    static constexpr float kDecayMs{2.f};
    static constexpr float kSustainLevel{1.f};
    static constexpr float kReleaseMs{500.f};

    Impl impl{kSampleRate};

    void SetUp() override
    {
        ASSERT_TRUE(configureAudibleVoice(impl));
    }

    // The fixture's own default script, exposed so a test needing a separate engine
    // instance (rather than `impl`) can still start from the exact same configuration.
    [[nodiscard]] static bool configureAudibleVoice(Impl& target)
    {
        return target.setScript(
            std::format("function OnStart()\n"
                        "    SetOscillator(0, {{ waveform = 0, level = 1.0, pitchFactor = 1.0 }})\n"
                        "    SetFilter({{ cutoff = 127, resonance = 0, type = \"LP4\" }})\n"
                        "    SetAmpEnvelope({{ attackMs = {}, decayMs = {}, sustainLevel = {}, releaseMs = {} }})\n"
                        "    SetCtrlSlot(0, {{ source = 0, curve = 2, target = 1, valueType = 1, depth = 1 }})\n"
                        "end\n",
                        kAttackMs, kDecayMs, kSustainLevel, kReleaseMs));
    }

    void renderBlocks(const int count)
    {
        renderBlocksOn(impl, count);
    }

    // Longer than kReleaseMs, so a hung voice would still be clearly audible when checked.
    void renderPastRelease()
    {
        renderBlocks(static_cast<int>(2.f * kReleaseMs * kSampleRate / 1000.f / static_cast<float>(kBlockSize)));
    }

    [[nodiscard]] AbacDsp::AudioBuffer<2, kBlockSize> processOneBlock()
    {
        AbacDsp::AudioBuffer<2, kBlockSize> in{};
        AbacDsp::AudioBuffer<2, kBlockSize> out{};
        impl.processBlock(in, out);
        return out;
    }

    [[nodiscard]] bool sawNonZeroOverNextBlocks(const int count)
    {
        AbacDsp::AudioBuffer<2, kBlockSize> in{};
        AbacDsp::AudioBuffer<2, kBlockSize> out{};
        bool sawNonZero = false;
        for (int b = 0; b < count; ++b)
        {
            impl.processBlock(in, out);
            for (size_t i = 0; i < kBlockSize; ++i)
            {
                sawNonZero = sawNonZero || out(i, 0) != 0.f;
            }
        }
        return sawNonZero;
    }

    void expectFiniteOverNextBlocks(const int count, const float bound = std::numeric_limits<float>::infinity())
    {
        AbacDsp::AudioBuffer<2, kBlockSize> in{};
        AbacDsp::AudioBuffer<2, kBlockSize> out{};
        for (int b = 0; b < count; ++b)
        {
            impl.processBlock(in, out);
            for (size_t i = 0; i < kBlockSize; ++i)
            {
                ASSERT_TRUE(std::isfinite(out(i, 0)));
                ASSERT_TRUE(std::isfinite(out(i, 1)));
                ASSERT_LE(std::abs(out(i, 0)), bound);
            }
        }
    }

    // Same finite/bounded check as expectFiniteOverNextBlocks(), for a test that also wants
    // to know whether any sample was audible across the same pass.
    [[nodiscard]] bool expectFiniteAndBoundedTrackingNonZero(const int count,
                                                             const float bound = std::numeric_limits<float>::infinity())
    {
        AbacDsp::AudioBuffer<2, kBlockSize> in{};
        AbacDsp::AudioBuffer<2, kBlockSize> out{};
        bool sawNonZero = false;
        for (int b = 0; b < count; ++b)
        {
            impl.processBlock(in, out);
            for (size_t i = 0; i < kBlockSize; ++i)
            {
                EXPECT_TRUE(std::isfinite(out(i, 0)));
                EXPECT_TRUE(std::isfinite(out(i, 1)));
                EXPECT_LE(std::abs(out(i, 0)), bound);
                sawNonZero = sawNonZero || out(i, 0) != 0.f;
            }
        }
        return sawNonZero;
    }

    [[nodiscard]] float rmsOverNextBlocks(const int count)
    {
        double sumSquares = 0.0;
        size_t samples = 0;
        AbacDsp::AudioBuffer<2, kBlockSize> in{};
        AbacDsp::AudioBuffer<2, kBlockSize> out{};
        for (int b = 0; b < count; ++b)
        {
            impl.processBlock(in, out);
            for (size_t i = 0; i < kBlockSize; ++i)
            {
                sumSquares += static_cast<double>(out(i, 0)) * static_cast<double>(out(i, 0));
                ++samples;
            }
        }
        return static_cast<float>(std::sqrt(sumSquares / static_cast<double>(samples)));
    }
};

TEST_F(MorphexsynthTest, silentWithoutAnyNoteOn)
{
    const auto out = processOneBlock();
    for (size_t i = 0; i < kBlockSize; ++i)
    {
        EXPECT_FLOAT_EQ(out(i, 0), 0.f);
        EXPECT_FLOAT_EQ(out(i, 1), 0.f);
    }
}

TEST_F(MorphexsynthTest, noteOnProducesFiniteBoundedOutput)
{
    const auto on = noteOn(1, kNote, kVelocity);
    impl.processMidi(on.data());
    EXPECT_TRUE(expectFiniteAndBoundedTrackingNonZero(20, 8.f));
}

TEST_F(MorphexsynthTest, mpeZoneGatingRejectsChannelsOutsideMasterOrMemberRange)
{
    impl.setMpeMasterChannel(1);
    impl.setMpeRangeLowerChannel(2);
    impl.setMpeRangeUpperChannel(4);

    const auto onOutsideZone = noteOn(4, kNote, kVelocity); // channel index 4 = MIDI channel 5, outside [2..4]
    impl.processMidi(onOutsideZone.data());
    renderBlocks(5);

    const auto out = processOneBlock();
    for (size_t i = 0; i < kBlockSize; ++i)
    {
        EXPECT_FLOAT_EQ(out(i, 0), 0.f);
    }
}

TEST_F(MorphexsynthTest, mpeZoneGatingAcceptsChannelsInsideMemberRange)
{
    impl.setMpeMasterChannel(1);
    impl.setMpeRangeLowerChannel(2);
    impl.setMpeRangeUpperChannel(4);

    const auto on = noteOn(2, kNote, kVelocity); // channel index 2 = MIDI channel 3, inside [2..4]
    impl.processMidi(on.data());
    EXPECT_TRUE(sawNonZeroOverNextBlocks(20));
}

TEST_F(MorphexsynthTest, perChannelPitchBendOnlyAffectsThatChannelsVoice)
{
    const auto onCh0 = noteOn(0, kNote, kVelocity);
    const auto onCh1 = noteOn(1, 72, kVelocity);
    impl.processMidi(onCh0.data());
    impl.processMidi(onCh1.data());
    renderBlocks(2);

    // Only channel 1's voice should react; this only checks that sending a large bend on one
    // channel doesn't crash or destabilize the mix - exact per-voice pitch isn't observable here.
    const auto bend = pitchBend(1, 8000);
    impl.processMidi(bend.data());
    expectFiniteOverNextBlocks(20);
}

// Measured during attack+decay, not sustain, since each of these fresh voices hasn't
// settled yet; each case needs its own independent engine, so this bypasses `impl`.
TEST_F(MorphexsynthTest, PitchBendUpRaisesPitchAndDownLowersIt)
{
    const auto measureWithBend = [](const int centeredBendOrZeroForNone)
    {
        Impl localImpl{kSampleRate};
        EXPECT_TRUE(configureAudibleVoice(localImpl));
        const auto on = noteOn(0, 72, kVelocity);
        localImpl.processMidi(on.data());
        if (centeredBendOrZeroForNone != 0)
        {
            const auto bend = pitchBend(0, centeredBendOrZeroForNone);
            localImpl.processMidi(bend.data());
        }
        renderBlocksOn(localImpl, 5); // let the pitch-bend connector's smoothing settle
        return measurePeriod(localImpl, 384);
    };

    const auto restPeriod = measureWithBend(0);
    ASSERT_GT(restPeriod, 0.f);

    const auto upPeriod = measureWithBend(8191); // maximum upward bend
    ASSERT_GT(upPeriod, 0.f);
    EXPECT_LT(upPeriod, restPeriod) << "bending up should shorten the period (raise pitch)";

    const auto downPeriod = measureWithBend(-8192); // maximum downward bend
    ASSERT_GT(downPeriod, 0.f);
    EXPECT_GT(downPeriod, restPeriod) << "bending down should lengthen the period (lower pitch)";
}

TEST_F(MorphexsynthTest, RapidRetriggerBeforeAnyNoteOffStillFullyReleases)
{
    const auto on = noteOn(kChannel, kNote, kVelocity);
    impl.processMidi(on.data());
    renderBlocks(2);
    impl.processMidi(on.data()); // retrigger the same note before any note-off arrives
    renderBlocks(2);

    const auto off = noteOff(kChannel, kNote);
    impl.processMidi(off.data());
    renderPastRelease();

    EXPECT_LT(rmsOverNextBlocks(10), 1e-4f) << "a retriggered note must not leave a hanging voice after one note-off";
}

TEST_F(MorphexsynthTest, voiceStealingPicksOldestWhenAllVoicesAreBusy)
{
    for (int i = 0; i < static_cast<int>(Impl::kMaxVoices) + 3; ++i)
    {
        const auto on = noteOn(kChannel, 40 + i, kVelocity);
        impl.processMidi(on.data());
        renderBlocks(1);
    }
    expectFiniteOverNextBlocks(20, 16.f);
}

TEST_F(MorphexsynthTest, noteOffSilencesTheVoiceEventually)
{
    impl.setCutoff(72.f);
    const auto on = noteOn(kChannel, kNote, kVelocity);
    impl.processMidi(on.data());
    renderBlocks(5);

    const auto off = noteOff(kChannel, kNote);
    impl.processMidi(off.data());
    renderPastRelease();

    EXPECT_LT(rmsOverNextBlocks(10), 1e-3f);
}

TEST_F(MorphexsynthTest, sustainPedalDefersNoteOffUntilReleased)
{
    const auto on = noteOn(kChannel, kNote, kVelocity);
    impl.processMidi(on.data());
    renderBlocks(5);

    const std::array<uint8_t, 3> sustainOn{0xB0, 64, 127};
    impl.processMidi(sustainOn.data());

    const auto off = noteOff(kChannel, kNote);
    impl.processMidi(off.data());
    renderPastRelease();
    EXPECT_TRUE(sawNonZeroOverNextBlocks(1)) << "note-off under a held sustain pedal must be deferred";

    const std::array<uint8_t, 3> sustainOff{0xB0, 64, 0};
    impl.processMidi(sustainOff.data());
    renderPastRelease();

    EXPECT_LT(rmsOverNextBlocks(10), 1e-3f) << "releasing the pedal must finally silence the note";
}

TEST_F(MorphexsynthTest, oscillatorSustainsADecayingToneNotJustAnInitialClick)
{
    // Explicit oscillator level, filter wide open (high cutoff, no resonance/MPE/LFO/contour
    // routing to it), decay-only envelope: isolates the oscillator+envelope path from the
    // filter (reported symptom: sound only with resonance up - oscillator output suspected).
    ASSERT_TRUE(
        impl.setScript("function OnStart()\n"
                       "    SetOscillator(0, { waveform = 2, level = 1.0, pitchFactor = 1.0 })\n"
                       "    SetFilter({ cutoff = 127, resonance = 0, type = \"LP4\" })\n"
                       "    SetAmpEnvelope({ attackMs = 0, decayMs = 1000, sustainLevel = 0.5, releaseMs = 100 })\n"
                       "end\n"));

    const auto on = noteOn(kChannel, kNote, 127);
    impl.processMidi(on.data());

    constexpr size_t kBlocksPerMs = static_cast<size_t>(kSampleRate) / 1000 / kBlockSize;
    static_assert(kBlocksPerMs * 1000 * kBlockSize == static_cast<size_t>(kSampleRate),
                  "kSampleRate must divide evenly for the ms-based windows below");

    const float earlyRms = rmsOverNextBlocks(static_cast<int>(10 * kBlocksPerMs)); // 0-10ms: right after trigger
    EXPECT_GT(earlyRms, 0.05f) << "note-on should produce an immediately audible tone";

    renderBlocks(static_cast<int>(490 * kBlocksPerMs));                          // advance to the 500ms mark
    const float midRms = rmsOverNextBlocks(static_cast<int>(10 * kBlocksPerMs)); // 500-510ms: inside the 1s decay
    EXPECT_GT(midRms, 0.3f * earlyRms)
        << "the tone must still be sounding mid-decay, not have already died out to a brief click "
        << "(early=" << earlyRms << ", mid=" << midRms << ")";

    renderBlocks(static_cast<int>(590 * kBlocksPerMs)); // advance to the 1100ms mark, past the 1s decay
    const float sustainRms = rmsOverNextBlocks(static_cast<int>(10 * kBlocksPerMs)); // 1100-1110ms: in sustain
    EXPECT_GT(sustainRms, 0.3f * earlyRms) << "sustain must still be clearly audible";

    const auto off = noteOff(kChannel, kNote);
    impl.processMidi(off.data());
    renderBlocks(static_cast<int>(200 * kBlocksPerMs)); // past the 100ms release

    const float afterReleaseRms = rmsOverNextBlocks(static_cast<int>(10 * kBlocksPerMs));
    EXPECT_LT(afterReleaseRms, 0.01f) << "release must have finished silencing the note by now";
}

TEST_F(MorphexsynthTest, loadingANewScriptDoesNotInheritThePreviousScriptsOscillatorLevel)
{
    ASSERT_TRUE(
        impl.setScript("function OnStart()\n"
                       "    SetOscillator(0, { waveform = 2, level = 1.0, pitchFactor = 1.0 })\n"
                       "    SetFilter({ cutoff = 127, resonance = 0, type = \"LP4\" })\n"
                       "    SetAmpEnvelope({ attackMs = 0, decayMs = 50, sustainLevel = 1.0, releaseMs = 50 })\n"
                       "end\n"));

    // A second script that never calls SetOscillator: if the first script's level leaked
    // through, this note would still be audible.
    ASSERT_TRUE(impl.setScript("function OnStart()\nend\n"));

    const auto on = noteOn(kChannel, kNote, 127);
    impl.processMidi(on.data());
    renderBlocks(19);

    const auto out = processOneBlock();
    for (size_t i = 0; i < kBlockSize; ++i)
    {
        EXPECT_NEAR(out(i, 0), 0.f, 1e-3f);
    }
}

TEST_F(MorphexsynthTest, loadingANewScriptDoesNotInheritThePreviousScriptsMpeZone)
{
    ASSERT_TRUE(impl.setScript("function OnStart()\n"
                               "    SetMpeZone(5, 6, 8)\n"
                               "end\n"));

    // A second script that never calls SetMpeZone(): the zone must revert to the default.
    ASSERT_TRUE(
        impl.setScript("function OnStart()\n"
                       "    SetOscillator(0, { waveform = 2, level = 1.0 })\n"
                       "    SetAmpEnvelope({ attackMs = 0, decayMs = 50, sustainLevel = 1.0, releaseMs = 50 })\n"
                       "end\n"));

    // MIDI channel 10 was outside the first script's zone (master 5, members 6..8) but is
    // inside the restored default (master 1, members 2..16).
    const auto on = noteOn(9, kNote, 127);
    impl.processMidi(on.data());
    EXPECT_TRUE(sawNonZeroOverNextBlocks(20));
}

TEST_F(MorphexsynthTest, onNoteOnScriptCustomizationAppliesToTheTriggeringNoteItself)
{
    ASSERT_TRUE(impl.setScript("function OnNoteOn(channel, note, velocity)\n"
                               "    SetAmpEnvelope({ attackMs = 0, decayMs = 0, sustainLevel = 0, releaseMs = 0 })\n"
                               "end\n"));

    const auto on = noteOn(kChannel, kNote, 127);
    impl.processMidi(on.data());

    // A 0ms/0ms/0-sustain envelope collapses to silence almost immediately - but only if
    // OnNoteOn's SetAmpEnvelope() took effect before this note triggered, not one block
    // late (see MorphexsynthImpl::processMidi's note-on ordering comment).
    renderBlocks(4);
    const auto out = processOneBlock();
    for (size_t i = 0; i < kBlockSize; ++i)
    {
        EXPECT_NEAR(out(i, 0), 0.f, 0.05f);
    }
}

// A retrigger while the previous instance is still mid-release must not hang: its own
// note-off has to reach it specifically, not whichever voice matches first.
TEST_F(MorphexsynthTest, RetriggerDuringLongReleaseIsFullySilencedByItsOwnNoteOff)
{
    const auto on = noteOn(kChannel, kNote, kVelocity);
    const auto off = noteOff(kChannel, kNote);

    impl.processMidi(on.data());
    renderBlocks(5);
    impl.processMidi(off.data()); // first instance starts its long release
    impl.processMidi(on.data());  // retrigger the same note immediately
    renderBlocks(5);
    impl.processMidi(off.data()); // release the retriggered instance too
    renderPastRelease();

    EXPECT_LT(rmsOverNextBlocks(10), 1e-4f) << "the retriggered instance's own note-off must still silence it";
}
