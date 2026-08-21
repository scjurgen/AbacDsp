#include <atomic>
#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include "Sampler/LoopFile.h"
#include "Sampler/SequencePattern.h"
#include "Sampler/SequencerEngine.h"
#include "Sampler/SliceLibrary.h"

// ADL hooks LoopStorageService needs to (de)serialize AbacDsp::LoopMetadata /
// SequencePattern -- mirrors LooperImpl.h, which normally supplies these
// ahead of including LoopStorageService.h.
namespace AbacDsp
{
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(LoopMetadata, version, bpm, bars, beats)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(SequenceEvent, stepPosition, track, sliceIndex, gain, pitchRatio,
                                                reverse, randomizeSlice, timingOffsetFrames, humanizeAmountFrames)
}

namespace nlohmann
{
template <>
struct adl_serializer<AbacDsp::SequencePattern>
{
    static void to_json(json& j, const AbacDsp::SequencePattern& p)
    {
        j = json{{"lengthBars", p.lengthBars()},
                 {"beatsPerBar", p.beatsPerBar()},
                 {"stepsPerBeat", p.stepsPerBeat()},
                 {"events", p.events()}};
    }

    static AbacDsp::SequencePattern from_json(const json& j)
    {
        AbacDsp::SequencePattern pattern(j.at("lengthBars").get<size_t>(), j.at("beatsPerBar").get<size_t>(),
                                         j.at("stepsPerBeat").get<size_t>());
        for (const auto& eventJson : j.at("events"))
        {
            pattern.addEvent(eventJson.get<AbacDsp::SequenceEvent>());
        }
        return pattern;
    }
};
}

#include "impl/CaptureRing.h"
#include "impl/FreezeService.h"
#include "impl/LoopStorageService.h"
#include "impl/LooperPartController.h"
#include "impl/LooperTransportController.h"
#include "impl/PartBankResizeService.h"

namespace
{
constexpr size_t kBlock = 16;
constexpr float kSampleRate = 5120.f;
using Bank = AbacDsp::LoopPartBank<kBlock>;
using Buffer = AbacDsp::AudioBuffer<2, kBlock>;
using Controller = LooperPartController<kBlock>;
using Transport = LooperTransportController<kBlock>;

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

// Bundles the plain local storage LooperTransportController needs, mirroring
// LooperImpl's own wiring. freeRecord defaults true so startFreshRecording()
// takes the simple unquantized path (no bar-lock/pre-roll machinery to fake).
struct TransportFixture
{
    Bank& bank;
    LooperTimingController& timing;
    CaptureRing<kBlock> captureRing{kSampleRate};
    FreezeService<kBlock> freezeService;
    LoopStorageService<kBlock> loopStorage;
    PartBankResizeService<kBlock> resizeService;
    AbacDsp::SliceLibrary sliceLibrary{1024};
    AbacDsp::SequencePattern pattern{1, 4, 4};
    AbacDsp::SequencerEngine<> sequencer{kSampleRate};

    bool armed{false};
    bool countingIn{false};
    bool autoStopArmed{false};
    size_t autoStopBarTarget{0};
    bool pendingStop{false};
    uint64_t pendingStopTickAbs{0};
    size_t pendingStopLoopLength{0};
    bool barLockedTake{false};
    long startOffset{0};
    uint64_t tickAbs{0};
    size_t takeBarIndex{0};
    bool suppressNextBarIndexIncrement{false};
    bool sequencerPlaying{false};
    int appliedTimeSignature{LooperTimingController::kDefaultTimeSignature};
    int pendingTimeSignature{LooperTimingController::kDefaultTimeSignature};
    uint64_t absPos{0};

    bool freeRecord{true};
    int countInBars{0};
    int recordBars{0};
    bool autoStopEnabled{false};

    std::atomic<bool> clearPulse{false};
    std::atomic<bool> recordPulse{false};
    std::atomic<bool> playPulse{false};
    std::atomic<bool> overdubPulse{false};
    std::atomic<bool> freezePulse{false};
    std::atomic<bool> seqPlayPulse{false};
    std::atomic<bool> clearSeqPulse{false};
    std::atomic<bool> threshRecReq{false};
    std::atomic<bool> undoPulse{false};
    std::atomic<bool> mixDownPulse{false};

    Transport transport;

    TransportFixture(Bank& bankRef, TimingFixture& fx)
        : bank(bankRef)
        , timing(fx.timing)
        , freezeService(bankRef)
        , loopStorage(bankRef, fx.seq, sliceLibrary, pattern, fx.meterTimelines, fx.appliedBpm, fx.eighthNoteUnit,
                      kSampleRate)
        , resizeService(bankRef, kSampleRate)
        , transport(Transport::Deps{
              .bank = bankRef,
              .seq = fx.seq,
              .timing = fx.timing,
              .captureRing = captureRing,
              .freezeService = freezeService,
              .loopStorage = loopStorage,
              .resizeService = resizeService,
              .sliceLibrary = sliceLibrary,
              .pattern = pattern,
              .sequencer = sequencer,
              .armed = armed,
              .countingIn = countingIn,
              .autoStopArmed = autoStopArmed,
              .autoStopBarTarget = autoStopBarTarget,
              .pendingStop = pendingStop,
              .pendingStopTickAbs = pendingStopTickAbs,
              .pendingStopLoopLength = pendingStopLoopLength,
              .barLockedTake = barLockedTake,
              .startOffset = startOffset,
              .tickAbs = tickAbs,
              .takeBarIndex = takeBarIndex,
              .suppressNextBarIndexIncrement = suppressNextBarIndexIncrement,
              .sequencerPlaying = sequencerPlaying,
              .appliedTimeSignature = appliedTimeSignature,
              .pendingTimeSignature = pendingTimeSignature,
              .absPos = absPos,
              .freeRecord = freeRecord,
              .countInBars = countInBars,
              .recordBars = recordBars,
              .autoStopEnabled = autoStopEnabled,
              .clearPulse = clearPulse,
              .recordPulse = recordPulse,
              .playPulse = playPulse,
              .overdubPulse = overdubPulse,
              .freezePulse = freezePulse,
              .seqPlayPulse = seqPlayPulse,
              .clearSeqPulse = clearSeqPulse,
              .threshRecReq = threshRecReq,
              .undoPulse = undoPulse,
              .mixDownPulse = mixDownPulse,
              .requestSpectrogramRegen = [] {},
              .tryRedirectRecordIntoSelectedPart = [] { return false; },
          })
    {
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
    TransportFixture tx(bank, fx);
    Controller controller(bank, fx.timing, tx.transport, fx.activePartIndex, 8);

    EXPECT_FALSE(controller.requestSwitch(1));
    EXPECT_EQ(fx.activePartIndex, 0u);
}

TEST(LooperPartControllerTest, SwitchToSameActivePartIsRefused)
{
    TimingFixture fx;
    Bank bank(kSampleRate, 1.f);
    recordInto(bank.part(0), kBlock, 1.f);
    bank.part(0).play();
    TransportFixture tx(bank, fx);
    Controller controller(bank, fx.timing, tx.transport, fx.activePartIndex, 8);

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
    TransportFixture tx(bank, fx);
    Controller controller(bank, fx.timing, tx.transport, fx.activePartIndex, 8);

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
    TransportFixture tx(bank, fx);
    Controller controller(bank, fx.timing, tx.transport, fx.activePartIndex, 8);

    EXPECT_TRUE(controller.requestSwitch(1));
    EXPECT_TRUE(controller.isSwitchPending());
    EXPECT_EQ(fx.activePartIndex, 0u) << "must not switch before part 0's own loop wraps";

    Buffer in{};
    Buffer out{};
    controller.processBlock(in, out); // part 0's 1-block loop wraps within this call
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
    TransportFixture tx(bank, fx);
    Controller controller(bank, fx.timing, tx.transport, fx.activePartIndex, 8);

    ASSERT_TRUE(controller.requestSwitch(1));
    EXPECT_FALSE(controller.requestSwitch(2));
}

TEST(LooperPartControllerTest, SwitchRefusedWhileActivePartIsRecording)
{
    TimingFixture fx;
    Bank bank(kSampleRate, 1.f);
    recordInto(bank.part(1), kBlock, 2.f);
    bank.part(0).beginRecord();
    TransportFixture tx(bank, fx);
    Controller controller(bank, fx.timing, tx.transport, fx.activePartIndex, 8);

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
    TransportFixture tx(bank, fx);
    Controller controller(bank, fx.timing, tx.transport, fx.activePartIndex, kFade);

    ASSERT_TRUE(controller.requestSwitch(1));
    Buffer in{};
    Buffer discard{};
    for (int i = 0; i < 4; ++i)
    {
        controller.processBlock(in, discard); // part 0's 4-block loop finishing its own cycle
    }
    ASSERT_TRUE(controller.isCrossfading());

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
    TransportFixture tx(bank, fx);
    Controller controller(bank, fx.timing, tx.transport, fx.activePartIndex, 4);

    ASSERT_TRUE(controller.requestSwitch(1));
    Buffer in{};
    Buffer out{};
    controller.processBlock(in, out); // part 0's 1-block loop wraps within this call
    EXPECT_TRUE(controller.isCrossfading());
    EXPECT_NE(bank.part(0).state(), AbacDsp::LooperState::Overdubbing);
}

TEST(LooperPartControllerTest, RequestRecordSwitchToEmptyPartIsAllowed)
{
    TimingFixture fx;
    Bank bank(kSampleRate, 1.f);
    recordInto(bank.part(0), kBlock, 1.f);
    bank.part(0).stop();
    TransportFixture tx(bank, fx);
    Controller controller(bank, fx.timing, tx.transport, fx.activePartIndex, 8);

    EXPECT_TRUE(controller.requestRecordSwitch(1));
    EXPECT_EQ(fx.activePartIndex, 1u);
    EXPECT_EQ(bank.part(1).state(), AbacDsp::LooperState::Recording);
}

TEST(LooperPartControllerTest, RequestRecordSwitchToActivePartIsRefused)
{
    TimingFixture fx;
    Bank bank(kSampleRate, 1.f);
    recordInto(bank.part(0), kBlock, 1.f);
    bank.part(0).play();
    TransportFixture tx(bank, fx);
    Controller controller(bank, fx.timing, tx.transport, fx.activePartIndex, 8);

    EXPECT_FALSE(controller.requestRecordSwitch(0));
}

TEST(LooperPartControllerTest, RequestRecordSwitchRefusedWhileActivePartIsRecording)
{
    TimingFixture fx;
    Bank bank(kSampleRate, 1.f);
    bank.part(0).beginRecord();
    TransportFixture tx(bank, fx);
    Controller controller(bank, fx.timing, tx.transport, fx.activePartIndex, 8);

    EXPECT_FALSE(controller.requestRecordSwitch(1));
}

// While the active part is audible, a record-switch mutes it first (crossfade
// to silence, same math as a play-switch) and only starts the fresh take once
// that fade completes -- verifying no click and no premature record start.
TEST(LooperPartControllerTest, RequestRecordSwitchWhilePlayingFadesToSilenceThenStartsRecording)
{
    TimingFixture fx;
    Bank bank(kSampleRate, 1.f);
    constexpr size_t kFade = 8;
    recordInto(bank.part(0), 4 * kBlock, 1.f);
    bank.part(0).play();
    TransportFixture tx(bank, fx);
    Controller controller(bank, fx.timing, tx.transport, fx.activePartIndex, kFade);

    ASSERT_TRUE(controller.requestRecordSwitch(1));
    EXPECT_TRUE(controller.isSwitchPending());
    Buffer discard{};
    for (int i = 0; i < 4; ++i)
    {
        controller.processBlock(discard, discard); // part 0's 4-block loop finishing its own cycle
    }
    ASSERT_TRUE(controller.isCrossfading());
    EXPECT_EQ(bank.part(1).state(), AbacDsp::LooperState::Empty) << "not triggered until the fade completes";

    Buffer in{};
    Buffer out{};
    controller.processBlock(in, out);

    EXPECT_FALSE(controller.isCrossfading()) << "an 8-frame fade completes within the first 16-frame block";
    EXPECT_EQ(bank.part(0).state(), AbacDsp::LooperState::Stopped);
    EXPECT_EQ(bank.part(1).state(), AbacDsp::LooperState::Recording);
}
