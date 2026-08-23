#include <gtest/gtest.h>
#include <set>
#include <sstream>
#include <vector>

#include "Sampler/GrooveDrumPlayer.h"

namespace AbacDsp::test
{

namespace
{
constexpr float kSampleRate = 1000.f; // fade in ms maps 1:1 to frames

[[nodiscard]] std::vector<float> makeConstLoop(const size_t frames, const float l, const float r)
{
    std::vector<float> loop(frames * 2, 0.f);
    for (size_t f = 0; f < frames; ++f)
    {
        loop[f * 2] = l;
        loop[f * 2 + 1] = r;
    }
    return loop;
}
}

TEST(GrooveDrumPlayerTest, TriggerAtTickZeroFiresOnFirstSample)
{
    const auto loop = makeConstLoop(20, 1.f, 1.f);
    SliceLibrary library(20);
    library.extractTrack(loop, std::vector<Slice>{{0, 20}});

    GrooveProgram program{{{0, 0, 1.f}}, 960, 960};

    GrooveDrumPlayer player(kSampleRate);
    player.setFadeMs(1.f);
    player.setLibrary(&library);
    player.setGroove(&program);

    EXPECT_EQ(player.activeVoiceCount(), 0u);
    static_cast<void>(player.advanceSample(100));
    EXPECT_EQ(player.activeVoiceCount(), 1u);
}

TEST(GrooveDrumPlayerTest, TriggerFiresAtExactTickOffsetNotBeforeOrAfter)
{
    const auto loop = makeConstLoop(20, 1.f, 1.f);
    SliceLibrary library(20);
    library.extractTrack(loop, std::vector<Slice>{{0, 20}});

    // 9.6 ticks/sample; tick 485 (off an exact multiple, clear of double
    // rounding at the boundary) first crosses at k=51: 50*9.6=480<485<489.6.
    GrooveProgram program{{{485, 0, 1.f}}, 960, 960};

    GrooveDrumPlayer player(kSampleRate);
    player.setFadeMs(1.f);
    player.setLibrary(&library);
    player.setGroove(&program);

    for (int i = 0; i < 50; ++i)
    {
        static_cast<void>(player.advanceSample(100));
    }
    EXPECT_EQ(player.activeVoiceCount(), 0u) << "must not fire before its exact tick";

    static_cast<void>(player.advanceSample(100));
    EXPECT_EQ(player.activeVoiceCount(), 1u) << "must fire on the sample that crosses its tick";
}

TEST(GrooveDrumPlayerTest, LoopWrapsAndRetriggersOnItsOwnPeriodIndependentOfCaller)
{
    const auto loop = makeConstLoop(5, 1.f, 1.f); // short slice: fully finishes well before a wrap
    SliceLibrary library(5);
    library.extractTrack(loop, std::vector<Slice>{{0, 5}});

    // ticksPerQuarterNote == samplesPerBeat -> exactly 1 tick/sample; loop wraps every 50 samples.
    GrooveProgram program{{{0, 0, 1.f}}, 50, 1};

    GrooveDrumPlayer player(kSampleRate);
    player.setFadeMs(1.f);
    player.setLibrary(&library);
    player.setGroove(&program);

    size_t risingEdges = 0;
    bool wasActive = false;
    for (int i = 0; i < 205; ++i)
    {
        static_cast<void>(player.advanceSample(1));
        const bool active = player.activeVoiceCount() > 0;
        if (active && !wasActive)
        {
            ++risingEdges;
        }
        wasActive = active;
    }
    EXPECT_EQ(risingEdges, 5u); // fires at sample 1, 51, 101, 151, 201
}

TEST(GrooveDrumPlayerTest, RoundRobinVariesSliceAcrossRepeatedTriggers)
{
    // Slices differ only by length; identify the picked slice by how many
    // samples the voice stays active.
    constexpr size_t kSliceCount = 8;
    constexpr size_t kLengthStep = 5;

    std::vector<float> longLoop;
    std::vector<Slice> slices;
    size_t offset = 0;
    for (size_t i = 0; i < kSliceCount; ++i)
    {
        const size_t length = (i + 1) * kLengthStep;
        const auto piece = makeConstLoop(length, 1.f, 1.f);
        longLoop.insert(longLoop.end(), piece.begin(), piece.end());
        slices.push_back({offset, length});
        offset += length;
    }
    SliceLibrary library(offset);
    library.extractTrack(longLoop, slices);

    GrooveProgram program{{{0, 0, 1.f}}, 1000, 1};

    GrooveDrumPlayer player(kSampleRate);
    player.setFadeMs(1.f);
    player.setLibrary(&library);
    player.setGroove(&program);

    std::set<size_t> observedLengths;
    for (int trial = 0; trial < 60; ++trial)
    {
        player.resetPosition();
        size_t framesActive = 0;
        bool wasActive = false;
        for (int i = 0; i < static_cast<int>(kSliceCount * kLengthStep) + 5; ++i)
        {
            static_cast<void>(player.advanceSample(1));
            const bool active = player.activeVoiceCount() > 0;
            if (active)
            {
                ++framesActive;
                wasActive = true;
            }
            else if (wasActive)
            {
                // The call that just ran still rendered the voice's final frame
                // before deactivating it internally, so that frame counts too.
                ++framesActive;
                break;
            }
        }
        observedLengths.insert(framesActive);
        EXPECT_EQ(framesActive % kLengthStep, 0u);
    }
    EXPECT_GT(observedLengths.size(), 1u) << "expected slice selection to vary across repeated triggers";
}

TEST(GrooveDrumPlayerTest, GainIsAppliedLinearlyWithNoPeakNormalization)
{
    const auto loop = makeConstLoop(64, 9.f, 9.f); // recorded peak = 9
    SliceLibrary library(64);
    library.extractTrack(loop, std::vector<Slice>{{0, 64}});

    GrooveProgram program{{{0, 0, 0.5f}}, 1000, 1};

    GrooveDrumPlayer player(kSampleRate);
    player.setFadeMs(1.f); // 1-frame fade at 1000 Hz
    player.setLibrary(&library);
    player.setGroove(&program);

    std::array<float, 2> out{};
    for (int i = 0; i < 10; ++i)
    {
        out = player.advanceSample(1);
    }
    // No 1/peak normalization: raw 9 * event gain 0.5, not (9/9) * 0.5.
    EXPECT_NEAR(out[0], 4.5f, 1e-3f);
    EXPECT_NEAR(out[1], 4.5f, 1e-3f);
}

TEST(GrooveDrumPlayerTest, ResetPositionRetriggersImmediately)
{
    const auto loop = makeConstLoop(10, 1.f, 1.f);
    SliceLibrary library(10);
    library.extractTrack(loop, std::vector<Slice>{{0, 10}});

    GrooveProgram program{{{0, 0, 1.f}}, 1000, 1};

    GrooveDrumPlayer player(kSampleRate);
    player.setFadeMs(1.f);
    player.setLibrary(&library);
    player.setGroove(&program);

    for (int i = 0; i < 10; ++i)
    {
        static_cast<void>(player.advanceSample(1));
    }
    EXPECT_EQ(player.activeVoiceCount(), 0u) << "the 10-frame slice should have finished";

    player.resetPosition();
    static_cast<void>(player.advanceSample(1));
    EXPECT_EQ(player.activeVoiceCount(), 1u) << "resetPosition() should re-arm the tick-0 event";
}

TEST(GrooveDrumPlayerTest, TriggerLogUsesTrackNameWhenSetAndIndexOtherwise)
{
    const auto loop = makeConstLoop(4, 1.f, 1.f);
    SliceLibrary library(4);
    library.extractTrack(loop, std::vector<Slice>{{0, 4}});
    GrooveProgram program{{{0, 0, 1.f}}, 1000, 1};

    std::ostringstream captured;
    std::streambuf* originalCoutBuffer = std::cout.rdbuf(captured.rdbuf());

    {
        GrooveDrumPlayer player(kSampleRate);
        player.setLibrary(&library);
        player.setGroove(&program);
        static_cast<void>(player.advanceSample(1)); // no setTrackNames(): falls back to "0"
    }

    const std::vector<std::string> names{"bd"};
    {
        GrooveDrumPlayer player(kSampleRate);
        player.setLibrary(&library);
        player.setTrackNames(names);
        player.setGroove(&program);
        static_cast<void>(player.advanceSample(1));
    }

    std::cout.rdbuf(originalCoutBuffer);

    const std::string log = captured.str();
    EXPECT_NE(log.find("track      0"), std::string::npos);
    EXPECT_NE(log.find("track     bd"), std::string::npos);
}

TEST(GrooveDrumPlayerTest, MissingLibraryOrProgramProducesSilenceAndNoCrash)
{
    GrooveDrumPlayer player(kSampleRate);
    for (int i = 0; i < 10; ++i)
    {
        const auto out = player.advanceSample(100);
        EXPECT_FLOAT_EQ(out[0], 0.f);
        EXPECT_FLOAT_EQ(out[1], 0.f);
    }
    EXPECT_EQ(player.activeVoiceCount(), 0u);
}

}
