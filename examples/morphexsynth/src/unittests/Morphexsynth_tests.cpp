#include <array>
#include <cmath>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

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

void renderBlocks(Impl& impl, const int count)
{
    AbacDsp::AudioBuffer<2, kBlockSize> in{};
    AbacDsp::AudioBuffer<2, kBlockSize> out{};
    for (int i = 0; i < count; ++i)
    {
        impl.processBlock(in, out);
    }
}
}

TEST(Morphexsynth, silentWithoutAnyNoteOn)
{
    Impl impl{kSampleRate};
    AbacDsp::AudioBuffer<2, kBlockSize> in{};
    AbacDsp::AudioBuffer<2, kBlockSize> out{};
    impl.processBlock(in, out);
    for (size_t i = 0; i < kBlockSize; ++i)
    {
        EXPECT_FLOAT_EQ(out(i, 0), 0.f);
        EXPECT_FLOAT_EQ(out(i, 1), 0.f);
    }
}

TEST(Morphexsynth, noteOnProducesFiniteBoundedOutput)
{
    Impl impl{kSampleRate};
    const auto on = noteOn(1, 60, 100);
    impl.processMidi(on.data());

    AbacDsp::AudioBuffer<2, kBlockSize> in{};
    AbacDsp::AudioBuffer<2, kBlockSize> out{};
    bool sawNonZero = false;
    for (int block = 0; block < 20; ++block)
    {
        impl.processBlock(in, out);
        for (size_t i = 0; i < kBlockSize; ++i)
        {
            ASSERT_TRUE(std::isfinite(out(i, 0)));
            ASSERT_TRUE(std::isfinite(out(i, 1)));
            ASSERT_LE(std::abs(out(i, 0)), 8.f);
            sawNonZero = sawNonZero || out(i, 0) != 0.f;
        }
    }
    EXPECT_TRUE(sawNonZero);
}

TEST(Morphexsynth, mpeZoneGatingRejectsChannelsOutsideMasterOrMemberRange)
{
    Impl impl{kSampleRate};
    impl.setMpeMasterChannel(1);
    impl.setMpeRangeLowerChannel(2);
    impl.setMpeRangeUpperChannel(4);

    const auto onOutsideZone = noteOn(4, 60, 100); // channel index 4 = MIDI channel 5, outside [2..4]
    impl.processMidi(onOutsideZone.data());
    renderBlocks(impl, 5);

    AbacDsp::AudioBuffer<2, kBlockSize> in{};
    AbacDsp::AudioBuffer<2, kBlockSize> out{};
    impl.processBlock(in, out);
    for (size_t i = 0; i < kBlockSize; ++i)
    {
        EXPECT_FLOAT_EQ(out(i, 0), 0.f);
    }
}

TEST(Morphexsynth, mpeZoneGatingAcceptsChannelsInsideMemberRange)
{
    Impl impl{kSampleRate};
    impl.setMpeMasterChannel(1);
    impl.setMpeRangeLowerChannel(2);
    impl.setMpeRangeUpperChannel(4);

    const auto on = noteOn(2, 60, 100); // channel index 2 = MIDI channel 3, inside [2..4]
    impl.processMidi(on.data());

    AbacDsp::AudioBuffer<2, kBlockSize> in{};
    AbacDsp::AudioBuffer<2, kBlockSize> out{};
    bool sawNonZero = false;
    for (int block = 0; block < 20; ++block)
    {
        impl.processBlock(in, out);
        for (size_t i = 0; i < kBlockSize; ++i)
        {
            sawNonZero = sawNonZero || out(i, 0) != 0.f;
        }
    }
    EXPECT_TRUE(sawNonZero);
}

TEST(Morphexsynth, perChannelPitchBendOnlyAffectsThatChannelsVoice)
{
    Impl impl{kSampleRate};
    const auto onCh0 = noteOn(0, 60, 100);
    const auto onCh1 = noteOn(1, 72, 100);
    impl.processMidi(onCh0.data());
    impl.processMidi(onCh1.data());
    renderBlocks(impl, 2);

    // Only channel 1's voice should react; this only checks that sending a large bend on one
    // channel doesn't crash or destabilize the mix - exact per-voice pitch isn't observable here.
    const auto bend = pitchBend(1, 8000);
    impl.processMidi(bend.data());

    AbacDsp::AudioBuffer<2, kBlockSize> in{};
    AbacDsp::AudioBuffer<2, kBlockSize> out{};
    for (int block = 0; block < 20; ++block)
    {
        impl.processBlock(in, out);
        for (size_t i = 0; i < kBlockSize; ++i)
        {
            ASSERT_TRUE(std::isfinite(out(i, 0)));
        }
    }
}

TEST(Morphexsynth, voiceStealingPicksOldestWhenAllVoicesAreBusy)
{
    Impl impl{kSampleRate};
    for (int i = 0; i < static_cast<int>(Impl::kMaxVoices) + 3; ++i)
    {
        const auto on = noteOn(0, 40 + i, 100);
        impl.processMidi(on.data());
        renderBlocks(impl, 1);
    }

    AbacDsp::AudioBuffer<2, kBlockSize> in{};
    AbacDsp::AudioBuffer<2, kBlockSize> out{};
    for (int block = 0; block < 20; ++block)
    {
        impl.processBlock(in, out);
        for (size_t i = 0; i < kBlockSize; ++i)
        {
            ASSERT_TRUE(std::isfinite(out(i, 0)));
            ASSERT_LE(std::abs(out(i, 0)), 16.f);
        }
    }
}

TEST(Morphexsynth, noteOffSilencesTheVoiceEventually)
{
    Impl impl{kSampleRate};
    impl.setCutoff(72.f);
    const auto on = noteOn(0, 60, 100);
    impl.processMidi(on.data());
    renderBlocks(impl, 5);

    const auto off = noteOff(0, 60);
    impl.processMidi(off.data());

    AbacDsp::AudioBuffer<2, kBlockSize> in{};
    AbacDsp::AudioBuffer<2, kBlockSize> out{};
    for (int block = 0; block < 4000; ++block)
    {
        impl.processBlock(in, out);
    }
    for (size_t i = 0; i < kBlockSize; ++i)
    {
        EXPECT_NEAR(out(i, 0), 0.f, 1e-3f);
    }
}

TEST(Morphexsynth, sustainPedalDefersNoteOffUntilReleased)
{
    Impl impl{kSampleRate};
    const auto on = noteOn(0, 60, 100);
    impl.processMidi(on.data());
    renderBlocks(impl, 5);

    const std::array<uint8_t, 3> sustainOn{0xB0, 64, 127};
    impl.processMidi(sustainOn.data());

    const auto off = noteOff(0, 60);
    impl.processMidi(off.data());
    renderBlocks(impl, 4000);

    AbacDsp::AudioBuffer<2, kBlockSize> in{};
    AbacDsp::AudioBuffer<2, kBlockSize> out{};
    impl.processBlock(in, out);
    bool stillSounding = false;
    for (size_t i = 0; i < kBlockSize; ++i)
    {
        stillSounding = stillSounding || out(i, 0) != 0.f;
    }
    EXPECT_TRUE(stillSounding) << "note-off under a held sustain pedal must be deferred";

    const std::array<uint8_t, 3> sustainOff{0xB0, 64, 0};
    impl.processMidi(sustainOff.data());
    renderBlocks(impl, 4000);

    impl.processBlock(in, out);
    for (size_t i = 0; i < kBlockSize; ++i)
    {
        EXPECT_NEAR(out(i, 0), 0.f, 1e-3f) << "releasing the pedal must finally silence the note";
    }
}

TEST(Morphexsynth, oscillatorSustainsADecayingToneNotJustAnInitialClick)
{
    Impl impl{kSampleRate};
    // Explicit oscillator level, filter wide open (high cutoff, no resonance/MPE/LFO/contour
    // routing to it), decay-only envelope: isolates the oscillator+envelope path from the
    // filter (reported symptom: sound only with resonance up - oscillator output suspected).
    ASSERT_TRUE(
        impl.setScript("function OnStart()\n"
                       "    SetOscillator(0, { waveform = 2, level = 1.0, pitchFactor = 1.0 })\n"
                       "    SetFilter({ cutoff = 127, resonance = 0, type = \"LP4\" })\n"
                       "    SetAmpEnvelope({ attackMs = 0, decayMs = 1000, sustainLevel = 0.5, releaseMs = 100 })\n"
                       "end\n"));

    const auto on = noteOn(0, 60, 127);
    impl.processMidi(on.data());

    AbacDsp::AudioBuffer<2, kBlockSize> in{};
    AbacDsp::AudioBuffer<2, kBlockSize> out{};

    constexpr size_t kBlocksPerMs = static_cast<size_t>(kSampleRate) / 1000 / kBlockSize;
    static_assert(kBlocksPerMs * 1000 * kBlockSize == static_cast<size_t>(kSampleRate),
                  "kSampleRate must divide evenly for the ms-based windows below");

    auto renderSilently = [&](const size_t blocks)
    {
        for (size_t b = 0; b < blocks; ++b)
        {
            impl.processBlock(in, out);
        }
    };
    auto measureRms = [&](const size_t blocks)
    {
        double sumSquares = 0.0;
        size_t count = 0;
        for (size_t b = 0; b < blocks; ++b)
        {
            impl.processBlock(in, out);
            for (size_t i = 0; i < kBlockSize; ++i)
            {
                sumSquares += static_cast<double>(out(i, 0)) * static_cast<double>(out(i, 0));
                ++count;
            }
        }
        return static_cast<float>(std::sqrt(sumSquares / static_cast<double>(count)));
    };

    const float earlyRms = measureRms(10 * kBlocksPerMs); // 0-10ms: right after trigger
    EXPECT_GT(earlyRms, 0.05f) << "note-on should produce an immediately audible tone";

    renderSilently(490 * kBlocksPerMs);                 // advance to the 500ms mark
    const float midRms = measureRms(10 * kBlocksPerMs); // 500-510ms: well inside the 1s decay
    EXPECT_GT(midRms, 0.3f * earlyRms)
        << "the tone must still be sounding mid-decay, not have already died out to a brief click "
        << "(early=" << earlyRms << ", mid=" << midRms << ")";

    renderSilently(590 * kBlocksPerMs);                     // advance to the 1100ms mark, past the 1s decay
    const float sustainRms = measureRms(10 * kBlocksPerMs); // 1100-1110ms: in sustain
    EXPECT_GT(sustainRms, 0.3f * earlyRms) << "sustain must still be clearly audible";

    const auto off = noteOff(0, 60);
    impl.processMidi(off.data());
    renderSilently(200 * kBlocksPerMs); // past the 100ms release

    const float afterReleaseRms = measureRms(10 * kBlocksPerMs);
    EXPECT_LT(afterReleaseRms, 0.01f) << "release must have finished silencing the note by now";
}

TEST(Morphexsynth, loadingANewScriptDoesNotInheritThePreviousScriptsOscillatorLevel)
{
    Impl impl{kSampleRate};
    ASSERT_TRUE(
        impl.setScript("function OnStart()\n"
                       "    SetOscillator(0, { waveform = 2, level = 1.0, pitchFactor = 1.0 })\n"
                       "    SetFilter({ cutoff = 127, resonance = 0, type = \"LP4\" })\n"
                       "    SetAmpEnvelope({ attackMs = 0, decayMs = 50, sustainLevel = 1.0, releaseMs = 50 })\n"
                       "end\n"));

    // A second script that never calls SetOscillator: if the first script's level leaked
    // through, this note would still be audible.
    ASSERT_TRUE(impl.setScript("function OnStart()\nend\n"));

    const auto on = noteOn(0, 60, 127);
    impl.processMidi(on.data());

    AbacDsp::AudioBuffer<2, kBlockSize> in{};
    AbacDsp::AudioBuffer<2, kBlockSize> out{};
    for (int block = 0; block < 20; ++block)
    {
        impl.processBlock(in, out);
    }
    for (size_t i = 0; i < kBlockSize; ++i)
    {
        EXPECT_NEAR(out(i, 0), 0.f, 1e-3f);
    }
}

TEST(Morphexsynth, loadingANewScriptDoesNotInheritThePreviousScriptsMpeZone)
{
    Impl impl{kSampleRate};
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
    AbacDsp::AudioBuffer<2, kBlockSize> in{};
    AbacDsp::AudioBuffer<2, kBlockSize> out{};
    const auto on = noteOn(9, 60, 127);
    impl.processMidi(on.data());
    for (int block = 0; block < 20; ++block)
    {
        impl.processBlock(in, out);
    }
    bool sawNonZero = false;
    for (size_t i = 0; i < kBlockSize; ++i)
    {
        sawNonZero = sawNonZero || out(i, 0) != 0.f;
    }
    EXPECT_TRUE(sawNonZero);
}

TEST(Morphexsynth, onNoteOnScriptCustomizationAppliesToTheTriggeringNoteItself)
{
    Impl impl{kSampleRate};
    ASSERT_TRUE(impl.setScript("function OnNoteOn(channel, note, velocity)\n"
                               "    SetAmpEnvelope({ attackMs = 0, decayMs = 0, sustainLevel = 0, releaseMs = 0 })\n"
                               "end\n"));

    const auto on = noteOn(0, 60, 127);
    impl.processMidi(on.data());

    AbacDsp::AudioBuffer<2, kBlockSize> in{};
    AbacDsp::AudioBuffer<2, kBlockSize> out{};
    // A 0ms/0ms/0-sustain envelope collapses to silence almost immediately - but only if
    // OnNoteOn's SetAmpEnvelope() took effect before this note triggered, not one block
    // late (see MorphexsynthImpl::processMidi's note-on ordering comment).
    for (int block = 0; block < 5; ++block)
    {
        impl.processBlock(in, out);
    }
    for (size_t i = 0; i < kBlockSize; ++i)
    {
        EXPECT_NEAR(out(i, 0), 0.f, 0.05f);
    }
}
