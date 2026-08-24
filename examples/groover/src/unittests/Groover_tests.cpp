#include <chrono>
#include <gtest/gtest.h>
#include <thread>

#include "Audio/AudioBuffer.h"
#include "impl/GrooverImpl.h"

// Deliberately does not depend on real (gitignored, user-supplied) sample/MIDI
// content: the default groove path resolves to an empty-but-valid program when
// the real directories aren't present, enough to exercise buffering mechanics.
namespace
{
constexpr size_t kBlock = 16;
constexpr float kSampleRate = 1000.f;
constexpr float kBpm = 120.f;

using Groover = GrooverImpl<kBlock>;
using Buffer = AbacDsp::AudioBuffer<2, kBlock>;

[[nodiscard]] bool waitUntilBurstReady(Groover& groover)
{
    const Buffer in{};
    Buffer out{};
    for (int i = 0; i < 2000; ++i)
    {
        groover.processBlock(in, out);
        if (groover.hasBurstAudioForTest())
        {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    return false;
}
}

TEST(GrooverTest, PlayPressCopiesBurstWithoutSynchronousFillThatBlock)
{
    Groover groover(kSampleRate);
    ASSERT_TRUE(waitUntilBurstReady(groover));

    const Buffer in{};
    Buffer out{};
    groover.setBpm(kBpm);
    groover.setPlay(true);
    groover.processBlock(in, out); // consumes the pulse -> togglePlay() -> burst copy

    // A 1 s burst is far larger than one quarter note's lookahead here, so
    // framesAhead() must already reflect it - the fill loop's own
    // advanceSample() path did not need to run to catch up.
    const auto samplesPerBeat = static_cast<size_t>(kSampleRate * 60.f / kBpm);
    EXPECT_GT(groover.loopBufferFramesAheadForTest(), samplesPerBeat);
}

TEST(GrooverTest, HostSyncPpqJumpFlushesLoopBuffer)
{
    Groover groover(kSampleRate);
    ASSERT_TRUE(waitUntilBurstReady(groover));

    groover.setBpm(kBpm);
    const Buffer in{};
    Buffer out{};
    groover.setPlay(true);
    groover.processBlock(in, out); // burst copied (Host Sync not yet on)

    const auto beforeJump = groover.loopBufferFramesAheadForTest();
    ASSERT_GT(beforeJump, 0u);

    // Enable Host Sync mid-playback with a ppqPosition far from wherever the
    // (burst-primed, tempo-only) internal tick position currently sits - a
    // real "user turns Host Sync on while already playing" scenario.
    groover.setHostSync(true);
    EffectBase::HostTransport transport;
    transport.isPlaying = true;
    transport.bpm = static_cast<double>(kBpm);
    transport.ppqPosition = 50.0;
    transport.updateCount = 1;
    groover.setHostTransport(transport);
    groover.processBlock(in, out);

    // The jump must flush the buffer (a substantial drop from the ~1 s burst
    // level); the fill step must already have topped it back up.
    const auto afterJump = groover.loopBufferFramesAheadForTest();
    EXPECT_LT(afterJump, beforeJump / 2);
    EXPECT_GT(afterJump, 0u);
}
