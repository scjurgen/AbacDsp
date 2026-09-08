#include <algorithm>
#include <cmath>
#include <format>
#include <fstream>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <limits>
#include <sstream>

#include "Audio/AudioBuffer.h"
#include "impl/AmbientPadImpl.h"

namespace
{
constexpr float kSampleRate = 48000.f;
constexpr size_t kBlockSize = 16;
using Impl = AmbientPadImpl<kBlockSize>;

void renderBlocksOn(Impl& target, const int count)
{
    AbacDsp::AudioBuffer<2, kBlockSize> in{};
    AbacDsp::AudioBuffer<2, kBlockSize> out{};
    for (int i = 0; i < count; ++i)
    {
        target.processBlock(in, out);
    }
}

std::string readFile(const std::string& path)
{
    std::ifstream file(path);
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}
}

/**
 * @brief Shared fixture: an Impl already loaded with a script that plays channel 1 on
 * OnStart, so most tests can render straight away instead of re-deriving trigger setup.
 */
class AmbientpadTest : public ::testing::Test
{
  protected:
    static constexpr int kNote{69};
    static constexpr int kVelocity{100};

    Impl impl{kSampleRate};

    void SetUp() override
    {
        ASSERT_TRUE(configurePlayingVoice(impl));
    }

    [[nodiscard]] static bool configurePlayingVoice(Impl& target)
    {
        return target.setScript(std::format("function OnStart()\n"
                                            "    NoteOn(1, {}, {})\n"
                                            "end\n",
                                            kNote, kVelocity));
    }

    void renderBlocks(const int count)
    {
        renderBlocksOn(impl, count);
    }

    [[nodiscard]] AbacDsp::AudioBuffer<2, kBlockSize> processOneBlock()
    {
        AbacDsp::AudioBuffer<2, kBlockSize> in{};
        AbacDsp::AudioBuffer<2, kBlockSize> out{};
        impl.processBlock(in, out);
        return out;
    }

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
};

TEST_F(AmbientpadTest, silentWithoutPlayOrScriptedNoteOn)
{
    Impl freshImpl{kSampleRate};
    AbacDsp::AudioBuffer<2, kBlockSize> in{};
    AbacDsp::AudioBuffer<2, kBlockSize> out{};
    freshImpl.processBlock(in, out);
    for (size_t i = 0; i < kBlockSize; ++i)
    {
        EXPECT_FLOAT_EQ(out(i, 0), 0.f);
        EXPECT_FLOAT_EQ(out(i, 1), 0.f);
    }
}

TEST_F(AmbientpadTest, scriptedNoteOnProducesFiniteBoundedAudibleOutput)
{
    // Bloom's default attack (~2.6 s at the default 0.3) needs a few hundred blocks at
    // kBlockSize=16 to become clearly audible; render enough to get well past it.
    EXPECT_TRUE(expectFiniteAndBoundedTrackingNonZero(2000, 8.f));
}

TEST_F(AmbientpadTest, manualPlaySwitchGatesChannelOneDirectly)
{
    Impl freshImpl{kSampleRate};
    freshImpl.setNote(kNote);
    freshImpl.setPlay(true);
    renderBlocksOn(freshImpl, 2000);
    AbacDsp::AudioBuffer<2, kBlockSize> in{};
    AbacDsp::AudioBuffer<2, kBlockSize> out{};
    bool sawNonZero = false;
    for (int b = 0; b < 50; ++b)
    {
        freshImpl.processBlock(in, out);
        for (size_t i = 0; i < kBlockSize; ++i)
        {
            ASSERT_TRUE(std::isfinite(out(i, 0)));
            sawNonZero = sawNonZero || out(i, 0) != 0.f;
        }
    }
    EXPECT_TRUE(sawNonZero);
}

TEST_F(AmbientpadTest, setNoteRepitchesAPlayingVoiceLive)
{
    Impl freshImpl{kSampleRate};
    freshImpl.setNote(kNote);
    freshImpl.setPlay(true);
    renderBlocksOn(freshImpl, 2000);

    AbacDsp::AudioBuffer<2, kBlockSize> in{};
    AbacDsp::AudioBuffer<2, kBlockSize> out{};
    bool sawNonZero = false;
    for (int b = 0; b < 50; ++b)
    {
        freshImpl.setNote(static_cast<float>(kNote) + static_cast<float>(b) * 0.2f);
        freshImpl.processBlock(in, out);
        for (size_t i = 0; i < kBlockSize; ++i)
        {
            ASSERT_TRUE(std::isfinite(out(i, 0)));
            sawNonZero = sawNonZero || out(i, 0) != 0.f;
        }
    }
    EXPECT_TRUE(sawNonZero);
}

TEST_F(AmbientpadTest, noteOffEventuallySilencesTheVoice)
{
    // Play(false) reaches the same voice NoteOn(1,...) triggered; reloading a second
    // script for NoteOff would reset every voice via resetVoicesToDefaults() first.
    ASSERT_TRUE(impl.setScript(std::format("function OnStart()\n"
                                           "    SetBloom(0)\n"
                                           "    NoteOn(1, {}, {})\n"
                                           "end\n",
                                           kNote, kVelocity)));
    renderBlocks(20);
    impl.setPlay(false);

    // -100 dB "off" is dbToGain(-100), a tiny nonzero gain, not literally zero, so the
    // reverb can leave a genuine (inaudible) tail - check near-silent, not bit-exact.
    AbacDsp::AudioBuffer<2, kBlockSize> in{};
    AbacDsp::AudioBuffer<2, kBlockSize> out{};
    for (int b = 0; b < 4000; ++b)
    {
        impl.processBlock(in, out);
    }
    for (int b = 0; b < 5; ++b)
    {
        impl.processBlock(in, out);
        for (size_t i = 0; i < kBlockSize; ++i)
        {
            EXPECT_LT(std::abs(out(i, 0)), 1e-4f);
        }
    }
}

TEST_F(AmbientpadTest, sweepingMusicalIntentControlsStaysFiniteAndBounded)
{
    for (int step = 0; step <= 10; ++step)
    {
        const auto v = static_cast<float>(step) / 10.f;
        impl.setMaterial(v);
        impl.setLight(1.f - v);
        impl.setMotion(v);
        impl.setBreath(v);
        impl.setStability(1.f - v);
        impl.setBloom(v);
        (void) expectFiniteAndBoundedTrackingNonZero(4, 8.f);
    }
}

TEST_F(AmbientpadTest, scriptedSetPitchGlidesChannelOneLive)
{
    ASSERT_TRUE(impl.setScript(std::format("function OnStart()\n"
                                           "    NoteOn(1, {}, {})\n"
                                           "    SetPitch(1, {}, 0, 0.5)\n"
                                           "end\n",
                                           kNote, kVelocity, kNote + 12)));
    EXPECT_TRUE(expectFiniteAndBoundedTrackingNonZero(2000, 8.f));
}

TEST_F(AmbientpadTest, secondOscillatorCanBeRetunedViaScript)
{
    ASSERT_TRUE(
        impl.setScript(std::format("function OnStart()\n"
                                   "    SetOscillator(1, 1, {{ waveform = 0, level = 0.8, height = 12, cents = 0 }})\n"
                                   "    NoteOn(1, {}, {})\n"
                                   "end\n",
                                   kNote, kVelocity)));
    EXPECT_TRUE(expectFiniteAndBoundedTrackingNonZero(2000, 8.f));
}

TEST_F(AmbientpadTest, loadingANewScriptResetsVoicesToDefaults)
{
    ASSERT_TRUE(impl.setScript(std::format("function OnStart()\n"
                                           "    SetGain(1, -20)\n"
                                           "    NoteOn(1, {}, {})\n"
                                           "end\n",
                                           kNote, kVelocity)));
    renderBlocks(200);

    ASSERT_TRUE(impl.setScript(std::format("function OnStart()\n"
                                           "    NoteOn(1, {}, {})\n"
                                           "end\n",
                                           kNote, kVelocity)));
    EXPECT_TRUE(expectFiniteAndBoundedTrackingNonZero(2000, 8.f));
}

TEST_F(AmbientpadTest, harmonicOrganismRealizesTransitionsSafely)
{
    Impl freshImpl{kSampleRate};
    freshImpl.setHarmonyEnabled(true);

    // One dwell period plus margin, at the organism's own default seed - long enough to see at
    // least one real transition (see the Phase 5 robustness probe: it happens almost every cycle).
    const auto samplesNeeded = static_cast<size_t>((AbacDsp::HarmonicOrganism::kDwellSeconds + 1.f) * kSampleRate);
    const auto blocksNeeded = static_cast<int>(samplesNeeded / kBlockSize);

    AbacDsp::AudioBuffer<2, kBlockSize> in{};
    AbacDsp::AudioBuffer<2, kBlockSize> out{};
    for (int b = 0; b < blocksNeeded; ++b)
    {
        freshImpl.processBlock(in, out);
        for (size_t i = 0; i < kBlockSize; ++i)
        {
            ASSERT_TRUE(std::isfinite(out(i, 0)));
            ASSERT_TRUE(std::isfinite(out(i, 1)));
            ASSERT_LE(std::abs(out(i, 0)), 8.f);
        }
    }
    EXPECT_GE(freshImpl.harmonyTransitionCount(), 1u);

    // Regression guard: the realizer once used home-relative offsets as absolute MIDI notes
    // directly (e.g. offset 3 played as note 3, ~9.7 Hz) - finite/bounded above doesn't catch it.
    for (size_t channel = 1; channel <= Impl::kMaxVoices; ++channel)
    {
        if (!freshImpl.voiceIsPlaying(channel))
        {
            continue;
        }
        const auto pitch = freshImpl.voicePitchSemitones(channel);
        EXPECT_GE(pitch, 30.f) << "channel " << channel;
        EXPECT_LE(pitch, 110.f) << "channel " << channel;
    }
}

TEST_F(AmbientpadTest, harmonyLuaBindingsReachTheOrganism)
{
    ASSERT_TRUE(impl.setScript("function OnStart()\n"
                               "    SetHarmony(true)\n"
                               "    SetHarmonyHome(4)\n"
                               "    SetPedalChannels({10})\n"
                               "    Arrive()\n"
                               "end\n"));

    const auto samplesNeeded = static_cast<size_t>((AbacDsp::HarmonicOrganism::kDwellSeconds + 1.f) * kSampleRate);
    const auto blocksNeeded = static_cast<int>(samplesNeeded / kBlockSize);
    for (int b = 0; b < blocksNeeded; ++b)
    {
        const auto out = processOneBlock();
        for (size_t i = 0; i < kBlockSize; ++i)
        {
            ASSERT_TRUE(std::isfinite(out(i, 0)));
            ASSERT_LE(std::abs(out(i, 0)), 8.f);
        }
    }
    EXPECT_GE(impl.harmonyTransitionCount(), 1u);
}

class BaseScriptTest : public ::testing::TestWithParam<std::string>
{
};

TEST_P(BaseScriptTest, loadsAndProducesFiniteBoundedOutput)
{
    const auto source = readFile(std::string(AMBIENTPAD_BASE_SCRIPTS_DIR) + "/" + GetParam() + ".lua");
    ASSERT_FALSE(source.empty()) << GetParam();

    Impl impl{kSampleRate};
    ASSERT_TRUE(impl.setScript(source)) << impl.scriptError();
    ASSERT_FALSE(impl.hasScriptError()) << impl.scriptError();

    // A few seconds - enough to hear the pedal/initial voice and one Timer-scheduled gesture,
    // without waiting out a full 30s harmonic-organism dwell period.
    AbacDsp::AudioBuffer<2, kBlockSize> in{};
    AbacDsp::AudioBuffer<2, kBlockSize> out{};
    for (int b = 0; b < 20000; ++b)
    {
        impl.processBlock(in, out);
        for (size_t i = 0; i < kBlockSize; ++i)
        {
            ASSERT_TRUE(std::isfinite(out(i, 0)));
            ASSERT_LE(std::abs(out(i, 0)), 8.f);
        }
    }
}

TEST(BaseScriptBassOverrideTest, minorHomeChannel16EndsUpAtTheOverrideNoteNotHomeRegister)
{
    const auto source = readFile(std::string(AMBIENTPAD_BASE_SCRIPTS_DIR) + "/minor-home.lua");
    ASSERT_FALSE(source.empty());

    Impl impl{kSampleRate};
    ASSERT_TRUE(impl.setScript(source)) << impl.scriptError();

    renderBlocksOn(impl, static_cast<int>(0.5f * kSampleRate) / static_cast<int>(kBlockSize));

    ASSERT_TRUE(impl.voiceIsPlaying(16));
    EXPECT_NEAR(impl.voicePitchSemitones(16), 34.f, 0.5f);
}

INSTANTIATE_TEST_SUITE_P(EveryBaseScript, BaseScriptTest,
                         ::testing::Values("breathing-drone", "harmonic-scene", "minor-home", "major-light",
                                           "modal-warmth", "open-suspended", "chromatic-weather", "pedal-modulation"),
                         [](const ::testing::TestParamInfo<std::string>& info)
                         {
                             std::string name = info.param;
                             std::replace(name.begin(), name.end(), '-', '_');
                             return name;
                         });
