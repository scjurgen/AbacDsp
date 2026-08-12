#include <gtest/gtest.h>

#include "Generators/KarplusStrongEnsemble.h"
#include "impl/DroneScriptEngine.h"
#include "impl/DroneSequencer.h"

namespace
{
constexpr float kSampleRate{48000.f};
constexpr size_t kMaxStringLength{10000};
constexpr size_t kMaxVoices{4};
using TestSequencer = DroneSequencer<kMaxStringLength, kMaxVoices>;
using TestEnsemble = AbacDsp::KarplusStrongEnsemble<kMaxVoices, kMaxStringLength>;

[[nodiscard]] size_t msToSamples(const float ms)
{
    return static_cast<size_t>(ms * kSampleRate * 0.001f);
}
}

TEST(DroneSequencer, NoRequestsWhileNotPlaying)
{
    TestSequencer seq(kSampleRate);
    TestEnsemble ensemble(kSampleRate);
    DroneScriptEngine script;
    seq.setIntervalMs(50.f);

    for (int i = 0; i < 100000; ++i)
    {
        seq.step(ensemble, script);
    }
    EXPECT_EQ(seq.nominalPositionSamples(), 0);
    for (size_t v = 0; v < kMaxVoices; ++v)
    {
        EXPECT_FALSE(ensemble.voice(v).isActive());
    }
}

TEST(DroneSequencer, NominalGridAdvancesByExactIntervalRegardlessOfJitter)
{
    TestSequencer seq(kSampleRate);
    TestEnsemble ensemble(kSampleRate);
    DroneScriptEngine script;
    seq.setIntervalMs(25.f); // 1200 samples at 48 kHz
    seq.setHumanizeTiming(80.f);
    seq.setPlaying(true);

    constexpr int64_t kIntervalSamples{1200};
    // Jitter perturbs when a request actually fires - under 80% humanize a fixed
    // kIntervalSamples-sized stepping window can contain two firings or none, so watch
    // for the actual nominal-position transitions instead of assuming one per window.
    int64_t previous = seq.nominalPositionSamples();
    int ticksObserved = 0;
    for (int64_t i = 0; i < 2'000'000 && ticksObserved < 50; ++i)
    {
        seq.step(ensemble, script);
        const auto current = seq.nominalPositionSamples();
        if (current != previous)
        {
            ++ticksObserved;
            EXPECT_EQ(current, ticksObserved * kIntervalSamples) << "tick " << ticksObserved;
            previous = current;
        }
    }
    EXPECT_EQ(ticksObserved, 50);
}

TEST(DroneSequencer, NegativeDelayFiresEarlierThanZeroDelayInTheSameRequest)
{
    TestSequencer seq(kSampleRate);
    TestEnsemble ensemble(kSampleRate);
    DroneScriptEngine script;
    ASSERT_TRUE(script.loadScript(R"(
        function NextNotes()
            return {
                { note = 60, velocity = 0.8, channel = 0, length = 0, delay = 0 },
                { note = 62, velocity = 0.8, channel = 1, length = 0, delay = -20 },
            }
        end
    )"));
    seq.setIntervalMs(200.f);
    seq.setPlaying(true);

    long earlySample = -1;
    long onBeatSample = -1;
    for (long i = 0; i < static_cast<long>(msToSamples(200.f)) && (earlySample < 0 || onBeatSample < 0); ++i)
    {
        seq.step(ensemble, script);
        if (earlySample < 0 && ensemble.voice(1).isActive())
        {
            earlySample = i;
        }
        if (onBeatSample < 0 && ensemble.voice(0).isActive())
        {
            onBeatSample = i;
        }
    }
    ASSERT_GE(earlySample, 0);
    ASSERT_GE(onBeatSample, 0);
    EXPECT_NEAR(onBeatSample - earlySample, static_cast<long>(msToSamples(20.f)), 2);
}

TEST(DroneSequencer, OutOfRangeChannelClampsToLastActiveVoiceInsteadOfCrashing)
{
    TestSequencer seq(kSampleRate);
    TestEnsemble ensemble(kSampleRate);
    DroneScriptEngine script;
    ASSERT_TRUE(script.loadScript(R"(
        function NextNotes()
            return { { note = 60, velocity = 0.8, channel = 99, length = 0, delay = 0 } }
        end
    )"));
    seq.setIntervalMs(50.f);
    seq.setPlaying(true);

    for (int i = 0; i < static_cast<int>(msToSamples(50.f)); ++i)
    {
        seq.step(ensemble, script);
    }
    for (size_t v = 0; v < kMaxVoices - 1; ++v)
    {
        EXPECT_FALSE(ensemble.voice(v).isActive()) << "voices beyond setVoices() must never be touched: " << v;
    }
    EXPECT_TRUE(ensemble.voice(kMaxVoices - 1).isActive());
}

TEST(DroneSequencer, AllNotesInOneRequestGetScheduled)
{
    TestSequencer seq(kSampleRate);
    TestEnsemble ensemble(kSampleRate);
    DroneScriptEngine script;
    ASSERT_TRUE(script.loadScript(R"(
        function NextNotes()
            return {
                { note = 60, velocity = 0.5, channel = 0, length = 0, delay = 0 },
                { note = 62, velocity = 0.5, channel = 1, length = 0, delay = 0 },
                { note = 64, velocity = 0.5, channel = 2, length = 0, delay = 0 },
                { note = 65, velocity = 0.5, channel = 3, length = 0, delay = 0 },
            }
        end
    )"));
    seq.setIntervalMs(50.f);
    seq.setPlaying(true);

    for (int i = 0; i < static_cast<int>(msToSamples(50.f)); ++i)
    {
        seq.step(ensemble, script);
    }
    for (size_t v = 0; v < kMaxVoices; ++v)
    {
        EXPECT_TRUE(ensemble.voice(v).isActive()) << "voice " << v;
    }
}

TEST(DroneSequencer, TransposeAndDetuneDoNotBreakTriggeringPipeline)
{
    // Matches this codebase's existing testing style for PluckSequencer: transpose and
    // detune are simple arithmetic applied before trigger()/bendInCents() - this checks
    // the pipeline still fires correctly with them active, not the resulting pitch (no
    // existing test in this suite measures Karplus-Strong output pitch directly either).
    TestSequencer seq(kSampleRate);
    TestEnsemble ensemble(kSampleRate);
    DroneScriptEngine script;
    seq.setIntervalMs(50.f);
    seq.setTranspose(12.f);
    seq.setDetuneCents(0, 25.f);
    seq.setPlaying(true);

    for (int i = 0; i < static_cast<int>(msToSamples(50.f)); ++i)
    {
        seq.step(ensemble, script);
    }
    EXPECT_TRUE(ensemble.voice(0).isActive());
}

TEST(DroneSequencer, PositiveLengthRunsWithoutFaultsAcrossManyTicks)
{
    TestSequencer seq(kSampleRate);
    TestEnsemble ensemble(kSampleRate);
    DroneScriptEngine script;
    ASSERT_TRUE(script.loadScript(R"(
        function NextNotes()
            return { { note = 60, velocity = 0.6, channel = 0, length = 15, delay = 0 } }
        end
    )"));
    seq.setIntervalMs(20.f);
    seq.setPlaying(true);

    for (int i = 0; i < static_cast<int>(msToSamples(20.f)) * 200; ++i)
    {
        seq.step(ensemble, script);
    }
    SUCCEED();
}

TEST(DroneSequencer, ScriptWithoutSlideFieldNeverSlides)
{
    TestSequencer seq(kSampleRate);
    TestEnsemble ensemble(kSampleRate);
    DroneScriptEngine script;
    ASSERT_TRUE(script.loadScript(R"(
        function NextNotes()
            return { { note = 60, velocity = 0.8, channel = 0, length = 0, delay = 0 } }
        end
    )"));
    seq.setIntervalMs(50.f);
    seq.setPlaying(true);

    for (int i = 0; i < static_cast<int>(msToSamples(50.f)); ++i)
    {
        seq.step(ensemble, script);
        EXPECT_FALSE(seq.isSliding(0));
    }
    EXPECT_TRUE(ensemble.voice(0).isActive()) << "sanity: the note itself still fired";
}

TEST(DroneSequencer, NonzeroSlideSetsIsSlidingRightAfterTrigger)
{
    TestSequencer seq(kSampleRate);
    TestEnsemble ensemble(kSampleRate);
    DroneScriptEngine script;
    ASSERT_TRUE(script.loadScript(R"(
        function NextNotes()
            return { { note = 60, velocity = 0.8, channel = 0, length = 0, delay = 0, slide = 7, slideTime = 50 } }
        end
    )"));
    seq.setIntervalMs(200.f);
    seq.setPlaying(true);

    bool sawSliding = false;
    for (int i = 0; i < static_cast<int>(msToSamples(200.f)); ++i)
    {
        seq.step(ensemble, script);
        sawSliding = sawSliding || seq.isSliding(0);
    }
    EXPECT_TRUE(sawSliding);
}

TEST(DroneSequencer, SlideFlagClearsOnceSlideTimeElapses)
{
    TestSequencer seq(kSampleRate);
    TestEnsemble ensemble(kSampleRate);
    DroneScriptEngine script;
    ASSERT_TRUE(script.loadScript(R"(
        function NextNotes()
            return { { note = 60, velocity = 0.8, channel = 0, length = 0, delay = 0, slide = 7, slideTime = 20 } }
        end
    )"));
    seq.setIntervalMs(500.f); // long enough that only the initial trigger fires in this window
    seq.setPlaying(true);

    // Past the trigger's fixed lookahead (30ms) plus the 20ms slide, comfortably short of
    // the next 500ms interval tick.
    for (int i = 0; i < static_cast<int>(msToSamples(100.f)); ++i)
    {
        seq.step(ensemble, script);
    }
    EXPECT_TRUE(ensemble.voice(0).isActive()) << "sanity: the note fired";
    EXPECT_FALSE(seq.isSliding(0)) << "slide should have resolved to the target pitch by now";
}

TEST(DroneSequencer, TwoChannelsSlideIndependently)
{
    TestSequencer seq(kSampleRate);
    TestEnsemble ensemble(kSampleRate);
    DroneScriptEngine script;
    ASSERT_TRUE(script.loadScript(R"(
        function NextNotes()
            return {
                { note = 60, velocity = 0.8, channel = 0, length = 0, delay = 0, slide = 7, slideTime = 50 },
                { note = 64, velocity = 0.8, channel = 1, length = 0, delay = 0 },
            }
        end
    )"));
    seq.setIntervalMs(200.f);
    seq.setPlaying(true);

    bool sawChannel0Sliding = false;
    bool sawChannel1Sliding = false;
    for (int i = 0; i < static_cast<int>(msToSamples(200.f)); ++i)
    {
        seq.step(ensemble, script);
        sawChannel0Sliding = sawChannel0Sliding || seq.isSliding(0);
        sawChannel1Sliding = sawChannel1Sliding || seq.isSliding(1);
    }
    EXPECT_TRUE(sawChannel0Sliding);
    EXPECT_FALSE(sawChannel1Sliding);
}

TEST(DroneSequencer, ZeroSlideTimeSnapsInsteadOfStickingSliding)
{
    TestSequencer seq(kSampleRate);
    TestEnsemble ensemble(kSampleRate);
    DroneScriptEngine script;
    ASSERT_TRUE(script.loadScript(R"(
        function NextNotes()
            return { { note = 60, velocity = 0.8, channel = 0, length = 0, delay = 0, slide = 5, slideTime = 0 } }
        end
    )"));
    seq.setIntervalMs(500.f);
    seq.setPlaying(true);

    for (int i = 0; i < static_cast<int>(msToSamples(100.f)); ++i)
    {
        seq.step(ensemble, script);
    }
    EXPECT_TRUE(ensemble.voice(0).isActive());
    EXPECT_FALSE(seq.isSliding(0)) << "slideTime=0 should snap instantly, not stay stuck sliding";
}
