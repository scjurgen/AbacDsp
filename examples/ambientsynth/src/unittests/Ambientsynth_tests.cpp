#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <format>
#include <fstream>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <limits>
#include <sstream>
#include <thread>

#include "Audio/AudioBuffer.h"
#include "impl/AmbientSynthImpl.h"

namespace
{
constexpr float kSampleRate = 48000.f;
constexpr size_t kBlockSize = 16;
using Impl = AmbientSynthImpl<kBlockSize>;

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
class AmbientsynthTest : public ::testing::Test
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

TEST_F(AmbientsynthTest, silentWithoutScriptedNoteOn)
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

TEST_F(AmbientsynthTest, scriptedNoteOnProducesFiniteBoundedAudibleOutput)
{
    // Bloom's default attack (~2.6 s at the default 0.3) needs a few hundred blocks at
    // kBlockSize=16 to become clearly audible; render enough to get well past it.
    EXPECT_TRUE(expectFiniteAndBoundedTrackingNonZero(2000, 8.f));
}

TEST_F(AmbientsynthTest, noteOffEventuallySilencesTheVoice)
{
    ASSERT_TRUE(impl.setScript(std::format("function OnStart()\n"
                                           "    SetBloom(0)\n"
                                           "    NoteOn(1, {}, {})\n"
                                           "end\n",
                                           kNote, kVelocity)));
    renderBlocks(20);
    ASSERT_TRUE(impl.setScript(std::format("function OnStart()\n"
                                           "    SetBloom(0)\n"
                                           "    NoteOff(1, {})\n"
                                           "end\n",
                                           kNote)));

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

TEST_F(AmbientsynthTest, sweepingMusicalIntentControlsStaysFiniteAndBounded)
{
    for (int step = 0; step <= 10; ++step)
    {
        const auto v = static_cast<float>(step) / 10.f;
        impl.setMaterial(v);
        impl.setCutoff(1.f - v);
        impl.setResonance(v);
        impl.setBloom(v);
        (void) expectFiniteAndBoundedTrackingNonZero(4, 8.f);
    }
}

TEST_F(AmbientsynthTest, everyFilterTypeStaysFiniteAndBounded)
{
    for (int type = 0; type < static_cast<int>(AmbientSynthScriptEngine::kNumFilterTypes); ++type)
    {
        impl.setFilterType(type);
        (void) expectFiniteAndBoundedTrackingNonZero(4, 8.f);
    }
}

TEST_F(AmbientsynthTest, scriptedRangeControlsReachTheVoicesAndStayBounded)
{
    ASSERT_TRUE(impl.setScript(std::format("function OnStart()\n"
                                           "    SetCutoffOuRange(24)\n"
                                           "    SetResonanceRange(1)\n"
                                           "    SetBreathOuRange(10)\n"
                                           "    SetPitchOuRange(100)\n"
                                           "    NoteOn(1, {}, {})\n"
                                           "end\n",
                                           kNote, kVelocity)));
    EXPECT_TRUE(expectFiniteAndBoundedTrackingNonZero(2000, 8.f));
}

TEST_F(AmbientsynthTest, perChannelLfoBindingsReachTheVoicesAndStayBounded)
{
    ASSERT_TRUE(impl.setScript(std::format("function OnStart()\n"
                                           "    SetCutoffLfo(1, 6, 24, 0)\n"
                                           "    SetMaterialLfo(1, 6, 0.5, 0)\n"
                                           "    SetResonanceLfo(1, 6, 2, 0)\n"
                                           "    SetPitchLfo(1, 6, 50, 0)\n"
                                           "    SetBreathLfo(1, 6, 5, 0)\n"
                                           "    SetDriftLfo(1, 6, 50, 0)\n"
                                           "    NoteOn(1, {}, {})\n"
                                           "end\n",
                                           kNote, kVelocity)));
    EXPECT_TRUE(expectFiniteAndBoundedTrackingNonZero(2000, 8.f));
}

TEST_F(AmbientsynthTest, scriptedSetPitchGlidesChannelOneLive)
{
    ASSERT_TRUE(impl.setScript(std::format("function OnStart()\n"
                                           "    NoteOn(1, {}, {})\n"
                                           "    SetPitch(1, {}, 0, 0.5)\n"
                                           "end\n",
                                           kNote, kVelocity, kNote + 12)));
    EXPECT_TRUE(expectFiniteAndBoundedTrackingNonZero(2000, 8.f));
}

TEST_F(AmbientsynthTest, secondOscillatorCanBeRetunedViaScript)
{
    ASSERT_TRUE(
        impl.setScript(std::format("function OnStart()\n"
                                   "    SetOscillator(1, 1, {{ waveform = 0, level = 0.8, height = 12, cents = 0 }})\n"
                                   "    NoteOn(1, {}, {})\n"
                                   "end\n",
                                   kNote, kVelocity)));
    EXPECT_TRUE(expectFiniteAndBoundedTrackingNonZero(2000, 8.f));
}

TEST_F(AmbientsynthTest, loadingANewScriptResetsVoicesToDefaults)
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

// Regression guard: setScript() (e.g. the editor's Apply button) rebuilds every voice in
// place while processBlock() (the audio thread) may be mid-flight on the same voices -
// AmbientSynthImpl::m_scriptMutex now serializes the two.
TEST_F(AmbientsynthTest, concurrentSetScriptAndProcessBlockDoesNotCrash)
{
    std::atomic<bool> stop{false};
    std::jthread scriptThread(
        [this, &stop]()
        {
            int i = 0;
            while (!stop.load(std::memory_order_relaxed))
            {
                const bool playsANote = (i++ % 2) == 0;
                impl.setScript(playsANote ? "function OnStart()\n    NoteOn(1, 69, 100)\nend\n"
                                          : "function OnStart()\nend\n");
            }
        });

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(300);
    while (std::chrono::steady_clock::now() < deadline)
    {
        renderBlocks(4);
    }
    stop.store(true, std::memory_order_relaxed);
}

TEST_F(AmbientsynthTest, playHarmonyVoiceLeadsFromCurrentlyPlayingChords)
{
    ASSERT_TRUE(impl.setScript("function OnStart()\n"
                               "    SetHarmonyHome(4)\n"
                               "    PlayHarmony({ semitones = {0, 4, 7} })\n"
                               "end\n"));
    EXPECT_TRUE(expectFiniteAndBoundedTrackingNonZero(200, 8.f));

    ASSERT_TRUE(impl.setScript("function OnStart()\n"
                               "    SetHarmonyHome(4)\n"
                               "    PlayHarmony({ semitones = {0, 4, 7} })\n"
                               "end\n"));
    renderBlocks(200);
    ASSERT_TRUE(impl.setScript("function OnStart()\n"
                               "    PlayHarmony({ semitones = {0, 3, 7} })\n"
                               "end\n"));
    EXPECT_TRUE(expectFiniteAndBoundedTrackingNonZero(2000, 8.f));

    // Only the target chord's absolute pitches (home note 64 plus {0, 3, 7}) should sound.
    for (size_t channel = 1; channel < Impl::kMaxVoices; ++channel)
    {
        if (!impl.voiceIsPlaying(channel))
        {
            continue;
        }
        const auto pitch = static_cast<int>(std::lround(impl.voicePitchSemitones(channel)));
        EXPECT_THAT(pitch, ::testing::AnyOf(64, 67, 71)) << "channel " << channel;
    }
}

TEST_F(AmbientsynthTest, harmonyGlideTimeSpeedsUpReachingTheTarget)
{
    // Both PlayHarmony calls happen within one script load (via Timer.After) - a second
    // setScript() call would reset every voice to silence first, defeating the point of
    // testing a glide *from* a currently-playing chord.
    ASSERT_TRUE(impl.setScript("function OnStart()\n"
                               "    SetHarmonyHome(4)\n"
                               "    SetHarmonyGlideTime(0.05)\n"
                               "    PlayHarmony({ semitones = {0, 4, 7} })\n"
                               "    Timer.After(5, function() PlayHarmony({ semitones = {1, 4, 7} }) end)\n"
                               "end\n"));

    // 500 blocks at kBlockSize=16 is ~167 ms - well past both the 5 ms timer delay and the
    // 0.05 s glide time.
    renderBlocks(500);

    bool sawGlidedChannel = false;
    for (size_t channel = 1; channel < Impl::kMaxVoices; ++channel)
    {
        if (!impl.voiceIsPlaying(channel))
        {
            continue;
        }
        sawGlidedChannel = sawGlidedChannel || std::abs(impl.voicePitchSemitones(channel) - 65.f) < 0.1f;
    }
    EXPECT_TRUE(sawGlidedChannel);
}

TEST_F(AmbientsynthTest, fractionalPlayHarmonyProducesADetunedPitch)
{
    ASSERT_TRUE(impl.setScript("function OnStart()\n"
                               "    SetHarmonyHome(4)\n"
                               "    PlayHarmony({ semitones = {0, 3.5, 7} })\n"
                               "end\n"));
    renderBlocks(50);

    bool sawFractionalPitch = false;
    for (size_t channel = 1; channel < Impl::kMaxVoices; ++channel)
    {
        if (!impl.voiceIsPlaying(channel))
        {
            continue;
        }
        sawFractionalPitch = sawFractionalPitch || std::abs(impl.voicePitchSemitones(channel) - 67.5f) < 0.01f;
    }
    EXPECT_TRUE(sawFractionalPitch);
}

TEST_F(AmbientsynthTest, pedalNoteExcludesTheLastChannelFromPlayHarmony)
{
    ASSERT_TRUE(impl.setScript("function OnStart()\n"
                               "    SetHarmonyHome(4)\n"
                               "    SetPedalNote(34)\n"
                               "    PlayHarmony({ semitones = {0, 4, 7} })\n"
                               "end\n"));
    renderBlocks(50);

    ASSERT_TRUE(impl.voiceIsPlaying(Impl::kMaxVoices));
    EXPECT_NEAR(impl.voicePitchSemitones(Impl::kMaxVoices), 34.f, 0.5f);
}

TEST(BaseScriptTest, whiteKeyNoteOnSelectsTheMatchingRegion)
{
    const auto source = readFile(std::string(AMBIENTSYNTH_BASE_SCRIPTS_DIR) + "/performance.lua");
    ASSERT_FALSE(source.empty());

    Impl impl{kSampleRate};
    ASSERT_TRUE(impl.setScript(source)) << impl.scriptError();

    // G4 (67) selects "Open suspended" (region 5), set 1: {-2, 0, 14, 5, 9} home-relative to
    // E4 (60 + pitch class 4 = 64) -> {62, 64, 78, 69, 73}.
    const std::array<uint8_t, 3> noteOn{0x90, 67, 100};
    impl.processMidi(noteOn.data());

    // Long enough for every cross-faded-out channel from the initial Drone chord to finish
    // releasing (~5.1 s at the default Bloom) and any glided channel to reach its target.
    renderBlocksOn(impl, 45000);

    std::array<int, 6> expected{52, 62, 64, 78, 69, 73};
    for (size_t channel = 1; channel < Impl::kMaxVoices; ++channel)
    {
        if (!impl.voiceIsPlaying(channel))
        {
            continue;
        }
        const auto pitch = static_cast<int>(std::lround(impl.voicePitchSemitones(channel)));
        EXPECT_THAT(pitch, ::testing::AnyOfArray(expected)) << "channel " << channel;
    }
}

TEST(BaseScriptTest, blackKeyNoteOnIsIgnoredForRegionSelection)
{
    const auto source = readFile(std::string(AMBIENTSYNTH_BASE_SCRIPTS_DIR) + "/performance.lua");
    ASSERT_FALSE(source.empty());

    Impl impl{kSampleRate};
    ASSERT_TRUE(impl.setScript(source)) << impl.scriptError();
    renderBlocksOn(impl, 5);

    std::array<float, Impl::kMaxVoices> before{};
    for (size_t channel = 1; channel <= Impl::kMaxVoices; ++channel)
    {
        before[channel - 1] = impl.voicePitchSemitones(channel);
    }

    const std::array<uint8_t, 3> blackKeyNoteOn{0x90, 61, 100}; // C#4, no region mapping
    impl.processMidi(blackKeyNoteOn.data());
    renderBlocksOn(impl, 5);

    for (size_t channel = 1; channel <= Impl::kMaxVoices; ++channel)
    {
        EXPECT_FLOAT_EQ(impl.voicePitchSemitones(channel), before[channel - 1]) << "channel " << channel;
    }
}

TEST(BaseScriptTest, pedalNoteOnBelowC4SetsPedalAndRevertsOnNoteOff)
{
    const auto source = readFile(std::string(AMBIENTSYNTH_BASE_SCRIPTS_DIR) + "/performance.lua");
    ASSERT_FALSE(source.empty());

    Impl impl{kSampleRate};
    ASSERT_TRUE(impl.setScript(source)) << impl.scriptError();

    const std::array<uint8_t, 3> pedalNoteOn{0x90, 30, 100};
    impl.processMidi(pedalNoteOn.data());
    renderBlocksOn(impl, 5);
    ASSERT_TRUE(impl.voiceIsPlaying(Impl::kMaxVoices));
    EXPECT_NEAR(impl.voicePitchSemitones(Impl::kMaxVoices), 30.f, 0.5f);

    const std::array<uint8_t, 3> pedalNoteOff{0x80, 30, 0};
    impl.processMidi(pedalNoteOff.data());
    renderBlocksOn(impl, 5);
    ASSERT_TRUE(impl.voiceIsPlaying(Impl::kMaxVoices));
    EXPECT_NEAR(impl.voicePitchSemitones(Impl::kMaxVoices), 40.f, 0.5f); // performance.lua's defaultPedalNote
}

class EveryBaseScriptTest : public ::testing::TestWithParam<std::string>
{
};

TEST_P(EveryBaseScriptTest, loadsAndProducesFiniteBoundedOutput)
{
    const auto source = readFile(std::string(AMBIENTSYNTH_BASE_SCRIPTS_DIR) + "/" + GetParam() + ".lua");
    ASSERT_FALSE(source.empty()) << GetParam();

    Impl impl{kSampleRate};
    ASSERT_TRUE(impl.setScript(source)) << impl.scriptError();
    ASSERT_FALSE(impl.hasScriptError()) << impl.scriptError();

    AbacDsp::AudioBuffer<2, kBlockSize> in{};
    AbacDsp::AudioBuffer<2, kBlockSize> out{};
    bool sawNonZero = false;
    for (int b = 0; b < 4000; ++b)
    {
        impl.processBlock(in, out);
        for (size_t i = 0; i < kBlockSize; ++i)
        {
            ASSERT_TRUE(std::isfinite(out(i, 0)));
            ASSERT_LE(std::abs(out(i, 0)), 8.f);
            sawNonZero = sawNonZero || out(i, 0) != 0.f;
        }
    }
    EXPECT_TRUE(sawNonZero);
}

INSTANTIATE_TEST_SUITE_P(EveryBaseScript, EveryBaseScriptTest, ::testing::Values("performance", "full-api-reference"),
                         [](const ::testing::TestParamInfo<std::string>& info)
                         {
                             std::string name = info.param;
                             std::replace(name.begin(), name.end(), '-', '_');
                             return name;
                         });

TEST(BaseScriptTest, playRegionHarmonyRealizesEveryRegionAndSetSafely)
{
    const auto source = readFile(std::string(AMBIENTSYNTH_BASE_SCRIPTS_DIR) + "/performance.lua");
    ASSERT_FALSE(source.empty());

    for (int region = 1; region <= 6; ++region)
    {
        for (int set = 1; set <= 3; ++set)
        {
            // Appending a fresh OnStart() after the base script's own works because a Lua
            // function definition is just an assignment to a global - the later one wins,
            // while PlayRegionHarmony/HarmonicRegions defined earlier stay intact.
            Impl impl{kSampleRate};
            ASSERT_TRUE(impl.setScript(source + std::format("\nfunction OnStart()\n"
                                                            "    SetHarmonyHome(4)\n"
                                                            "    PlayRegionHarmony({{ region = {}, set = {} }})\n"
                                                            "end\n",
                                                            region, set)))
                << impl.scriptError();

            AbacDsp::AudioBuffer<2, kBlockSize> in{};
            AbacDsp::AudioBuffer<2, kBlockSize> out{};
            for (int b = 0; b < 200; ++b)
            {
                impl.processBlock(in, out);
                for (size_t i = 0; i < kBlockSize; ++i)
                {
                    ASSERT_TRUE(std::isfinite(out(i, 0)));
                    ASSERT_LE(std::abs(out(i, 0)), 8.f) << "region " << region << " set " << set;
                }
            }
        }
    }
}
