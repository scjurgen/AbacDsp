#include <gtest/gtest.h>

#include "impl/LooperPartController.h"

namespace
{
constexpr size_t kBlock = 16;
constexpr float kSampleRate = 5120.f;
using Bank = AbacDsp::LoopPartBank<kBlock>;
using Buffer = AbacDsp::AudioBuffer<2, kBlock>;
using Controller = LooperPartController<kBlock>;

// Bundles the plain local storage LooperTimingController needs, mirroring
// how LooperImpl wires it -- see LooperTimingController.h's constructor.
struct TimingFixture
{
    AbacDsp::BeatSequencer seq{kSampleRate};
    std::array<AbacDsp::MeterTimeline, AbacDsp::kMaxLoopParts> meterTimelines{};
    std::array<size_t, AbacDsp::kMaxLoopParts> finalizedBarCounts{};
    size_t activePartIndex{0};
    float appliedBpm{120.f};
    bool eighthNoteUnit{false};
    int appliedTimeSignature{LooperTimingController::kDefaultTimeSignature};
    bool countingIn{false};
    int countInBarsOffset{0};
    uint64_t countInEndTickAbs{0};
    bool suppressNextClick{false};
    LooperTimingController timing{seq,
                                  meterTimelines,
                                  finalizedBarCounts,
                                  activePartIndex,
                                  appliedBpm,
                                  eighthNoteUnit,
                                  appliedTimeSignature,
                                  countingIn,
                                  countInBarsOffset,
                                  countInEndTickAbs,
                                  suppressNextClick,
                                  kSampleRate};

    TimingFixture()
    {
        seq.setBpm(appliedBpm);
    }
};

void feedConstant(AbacDsp::LoopRecorder<kBlock>& rec, const size_t frames, const float value)
{
    size_t done = 0;
    while (done < frames)
    {
        Buffer in{};
        Buffer out{};
        for (size_t i = 0; i < kBlock; ++i)
        {
            in(i, 0) = value;
            in(i, 1) = value;
        }
        rec.processBlock(in, out);
        done += kBlock;
    }
}

void recordInto(AbacDsp::LoopRecorder<kBlock>& rec, const size_t frames, const float value)
{
    rec.beginRecord();
    feedConstant(rec, frames, value);
    rec.stopRecordFree();
    rec.setFadeFrames(0);
}
}

TEST(LooperPartControllerTest, SwitchToEmptyPartIsRefused)
{
    TimingFixture fx;
    Bank bank(kSampleRate, 1.f);
    recordInto(bank.part(0), kBlock, 1.f);
    bank.part(0).play();
    Controller controller(bank, fx.timing, fx.activePartIndex, 8);

    EXPECT_FALSE(controller.requestSwitch(1));
    EXPECT_EQ(fx.activePartIndex, 0u);
}

TEST(LooperPartControllerTest, SwitchToSameActivePartIsRefused)
{
    TimingFixture fx;
    Bank bank(kSampleRate, 1.f);
    recordInto(bank.part(0), kBlock, 1.f);
    bank.part(0).play();
    Controller controller(bank, fx.timing, fx.activePartIndex, 8);

    EXPECT_FALSE(controller.requestSwitch(0));
}

TEST(LooperPartControllerTest, SwitchCommitsImmediatelyWhenActivePartIsNotPlaying)
{
    TimingFixture fx;
    Bank bank(kSampleRate, 1.f);
    recordInto(bank.part(0), kBlock, 1.f);
    recordInto(bank.part(1), kBlock, 2.f);
    // part 0 left Playing by recordInto()->stopRecordFree(); stop it so it's
    // not audible, matching "nothing to wait for".
    bank.part(0).stop();
    Controller controller(bank, fx.timing, fx.activePartIndex, 8);

    EXPECT_TRUE(controller.requestSwitch(1));
    EXPECT_EQ(fx.activePartIndex, 1u);
    EXPECT_FALSE(controller.isSwitchPending());
    EXPECT_FALSE(controller.isCrossfading());
}

TEST(LooperPartControllerTest, SwitchWhilePlayingQueuesUntilBarBoundary)
{
    TimingFixture fx;
    Bank bank(kSampleRate, 1.f);
    recordInto(bank.part(0), kBlock, 1.f);
    recordInto(bank.part(1), kBlock, 2.f);
    bank.part(0).play();
    Controller controller(bank, fx.timing, fx.activePartIndex, 8);

    EXPECT_TRUE(controller.requestSwitch(1));
    EXPECT_TRUE(controller.isSwitchPending());
    EXPECT_EQ(fx.activePartIndex, 0u) << "must not switch before the bar boundary";

    controller.onBarBoundary();
    EXPECT_FALSE(controller.isSwitchPending());
    EXPECT_EQ(fx.activePartIndex, 1u);
    EXPECT_TRUE(controller.isCrossfading());
}

TEST(LooperPartControllerTest, SecondRequestIsRefusedWhilePending)
{
    TimingFixture fx;
    Bank bank(kSampleRate, 1.f);
    recordInto(bank.part(0), kBlock, 1.f);
    recordInto(bank.part(1), kBlock, 2.f);
    recordInto(bank.part(2), kBlock, 3.f);
    bank.part(0).play();
    Controller controller(bank, fx.timing, fx.activePartIndex, 8);

    ASSERT_TRUE(controller.requestSwitch(1));
    EXPECT_FALSE(controller.requestSwitch(2));
}

TEST(LooperPartControllerTest, SwitchRefusedWhileActivePartIsRecording)
{
    TimingFixture fx;
    Bank bank(kSampleRate, 1.f);
    recordInto(bank.part(1), kBlock, 2.f);
    bank.part(0).beginRecord();
    Controller controller(bank, fx.timing, fx.activePartIndex, 8);

    EXPECT_FALSE(controller.requestSwitch(1));
}

TEST(LooperPartControllerTest, CrossfadeRampsFromOutgoingToIncomingThenStopsOutgoing)
{
    TimingFixture fx;
    Bank bank(kSampleRate, 1.f);
    constexpr size_t kFade = 8;
    recordInto(bank.part(0), 4 * kBlock, 1.f);
    recordInto(bank.part(1), 4 * kBlock, 5.f);
    bank.part(0).play();
    Controller controller(bank, fx.timing, fx.activePartIndex, kFade);

    ASSERT_TRUE(controller.requestSwitch(1));
    controller.onBarBoundary();
    ASSERT_TRUE(controller.isCrossfading());

    Buffer in{};
    Buffer first{};
    controller.processBlock(in, first);
    // Linear ramp over kFade samples: pure outgoing at 0, pure incoming by
    // kFade, halfway (both at 0.5) at the midpoint.
    EXPECT_NEAR(first(0, 0), 1.f, 1e-4f);
    EXPECT_NEAR(first(kFade / 2, 0), 0.5f * 1.f + 0.5f * 5.f, 1e-4f);

    EXPECT_FALSE(controller.isCrossfading()) << "an 8-frame fade completes within the first 16-frame block";
    EXPECT_EQ(bank.part(0).state(), AbacDsp::LooperState::Stopped);

    Buffer second{};
    controller.processBlock(in, second);

    Buffer settled{};
    controller.processBlock(in, settled);
    for (size_t i = 0; i < kBlock; ++i)
    {
        EXPECT_NEAR(settled(i, 0), 5.f, 1e-3f) << "frame " << i;
    }
}

TEST(LooperPartControllerTest, SwitchWhileOverdubbingEndsOverdubAndCrossfades)
{
    TimingFixture fx;
    Bank bank(kSampleRate, 1.f);
    recordInto(bank.part(0), kBlock, 1.f);
    recordInto(bank.part(1), kBlock, 2.f);
    bank.part(0).play();
    bank.part(0).beginOverdub();
    ASSERT_EQ(bank.part(0).state(), AbacDsp::LooperState::Overdubbing);
    Controller controller(bank, fx.timing, fx.activePartIndex, 4);

    ASSERT_TRUE(controller.requestSwitch(1));
    controller.onBarBoundary();
    EXPECT_TRUE(controller.isCrossfading());
    EXPECT_NE(bank.part(0).state(), AbacDsp::LooperState::Overdubbing);
}
