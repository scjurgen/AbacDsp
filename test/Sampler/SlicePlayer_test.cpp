#include <array>
#include <gtest/gtest.h>
#include <vector>

#include "Sampler/SlicePlayer.h"

namespace AbacDsp::test
{

namespace
{
constexpr size_t kBlock = 16;
constexpr float kSampleRate = 1000.f; // fade in ms maps 1:1 to frames
using Player = SlicePlayer<kBlock>;

struct Rendered
{
    std::vector<float> left;
    std::vector<float> right;
};

// Interleaved stereo loop where L = base + frame, R = -(base + frame).
[[nodiscard]] std::vector<float> makeRampLoop(const size_t frames, const float base = 0.f)
{
    std::vector<float> loop(frames * 2, 0.f);
    for (size_t f = 0; f < frames; ++f)
    {
        loop[f * 2] = base + static_cast<float>(f);
        loop[f * 2 + 1] = -(base + static_cast<float>(f));
    }
    return loop;
}

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

[[nodiscard]] Rendered render(Player& player, const size_t frames)
{
    Rendered out{};
    size_t done = 0;
    while (done < frames)
    {
        AudioBuffer<2, kBlock> block{};
        player.processBlock(block);
        for (size_t i = 0; i < kBlock; ++i)
        {
            out.left.push_back(block(i, 0));
            out.right.push_back(block(i, 1));
        }
        done += kBlock;
    }
    return out;
}
}

TEST(SlicePlayerTest, SilentWithoutTrigger)
{
    Player player{kSampleRate};
    const auto loop = makeConstLoop(64, 1.f, 0.5f);
    player.setLoop(loop, 64);
    const std::array<Slice, 1> slices{Slice{0, 64}};
    player.setSlices(slices);

    const auto out = render(player, 64);
    for (const float v : out.left)
    {
        EXPECT_FLOAT_EQ(v, 0.f);
    }
    EXPECT_EQ(player.activeVoiceCount(), 0u);
}

TEST(SlicePlayerTest, TriggerIgnoredWhenIndexOutOfRange)
{
    Player player{kSampleRate};
    const auto loop = makeConstLoop(64, 1.f, 0.5f);
    player.setLoop(loop, 64);
    const std::array<Slice, 1> slices{Slice{0, 64}};
    player.setSlices(slices);

    player.triggerSlice(5);
    EXPECT_EQ(player.activeVoiceCount(), 0u);
}

TEST(SlicePlayerTest, PlateauMatchesLoopContent)
{
    Player player{kSampleRate};
    player.setFadeMs(2.f);                     // 2 frames
    const auto loop = makeRampLoop(20, 100.f); // L = 100 + frame
    player.setLoop(loop, 20);
    const std::array<Slice, 1> slices{Slice{10, 10}}; // frames 10..19 -> L 110..119
    player.setSlices(slices);

    player.triggerSlice(0);
    const auto out = render(player, kBlock);
    // effectiveFade = min(2, 5) = 2; plateau where pos in [1, 8].
    for (size_t pos = 1; pos <= 8; ++pos)
    {
        EXPECT_FLOAT_EQ(out.left[pos], 100.f + 10.f + static_cast<float>(pos));
        EXPECT_FLOAT_EQ(out.right[pos], -(100.f + 10.f + static_cast<float>(pos)));
    }
}

TEST(SlicePlayerTest, EdgesFadeInAndOut)
{
    Player player{kSampleRate};
    player.setFadeMs(8.f); // 8 frames
    const auto loop = makeConstLoop(64, 1.f, 1.f);
    player.setLoop(loop, 64);
    const std::array<Slice, 1> slices{Slice{0, 64}};
    player.setSlices(slices);

    player.triggerSlice(0);
    const auto out = render(player, 64);

    // Fade-in ramps up over the first 8 frames.
    EXPECT_LT(out.left[0], out.left[1]);
    EXPECT_LT(out.left[1], out.left[7]);
    EXPECT_FLOAT_EQ(out.left[7], 1.f); // plateau reached
    // Fade-out ramps down to ~0 at the final frame.
    EXPECT_FLOAT_EQ(out.left[56], 1.f);
    EXPECT_GT(out.left[56], out.left[60]);
    EXPECT_LT(out.left[63], 0.2f);
    // Gains never exceed unity.
    for (const float v : out.left)
    {
        EXPECT_LE(v, 1.f + 1e-6f);
    }
}

TEST(SlicePlayerTest, PlayLengthTruncatesSlice)
{
    Player player{kSampleRate};
    player.setFadeMs(2.f);
    const auto loop = makeConstLoop(64, 1.f, 1.f);
    player.setLoop(loop, 64);
    const std::array<Slice, 1> slices{Slice{0, 64}};
    player.setSlices(slices);

    player.triggerSlice(0, 16); // cap to 16 frames
    const auto out = render(player, 64);
    // After 16 frames the voice has ended: silence thereafter.
    for (size_t i = 16; i < out.left.size(); ++i)
    {
        EXPECT_FLOAT_EQ(out.left[i], 0.f) << "frame " << i;
    }
    EXPECT_EQ(player.activeVoiceCount(), 0u);
}

TEST(SlicePlayerTest, VoiceEndsAfterPlayLength)
{
    Player player{kSampleRate};
    player.setFadeMs(2.f);
    const auto loop = makeConstLoop(64, 1.f, 1.f);
    player.setLoop(loop, 64);
    const std::array<Slice, 1> slices{Slice{0, 32}};
    player.setSlices(slices);

    player.triggerSlice(0);
    EXPECT_EQ(player.activeVoiceCount(), 1u);
    static_cast<void>(render(player, 32));
    EXPECT_EQ(player.activeVoiceCount(), 0u);
}

TEST(SlicePlayerTest, OverlappingSlicesSum)
{
    Player player{kSampleRate};
    player.setFadeMs(2.f);
    const auto loop = makeConstLoop(64, 1.f, 1.f);
    player.setLoop(loop, 64);
    const std::array<Slice, 2> slices{Slice{0, 64}, Slice{0, 64}};
    player.setSlices(slices);

    player.triggerSlice(0);
    player.triggerSlice(1);
    EXPECT_EQ(player.activeVoiceCount(), 2u);
    // In the overlapping plateau both voices contribute ~1 -> ~2.
    const auto out = render(player, 16);
    EXPECT_NEAR(out.left[8], 2.f, 1e-4f);
}

TEST(SlicePlayerTest, ResetSilencesVoices)
{
    Player player{kSampleRate};
    const auto loop = makeConstLoop(64, 1.f, 1.f);
    player.setLoop(loop, 64);
    const std::array<Slice, 1> slices{Slice{0, 64}};
    player.setSlices(slices);

    player.triggerSlice(0);
    ASSERT_EQ(player.activeVoiceCount(), 1u);
    player.reset();
    EXPECT_EQ(player.activeVoiceCount(), 0u);
}

TEST(SlicePlayerTest, VoiceStealingCapsPolyphony)
{
    Player player{kSampleRate};
    player.setFadeMs(2.f);
    const auto loop = makeConstLoop(64, 1.f, 1.f);
    player.setLoop(loop, 64);
    std::vector<Slice> slices(Player::kMaxVoices + 4, Slice{0, 64});
    player.setSlices(slices);

    for (size_t i = 0; i < Player::kMaxVoices + 4; ++i)
    {
        player.triggerSlice(i);
    }
    EXPECT_EQ(player.activeVoiceCount(), Player::kMaxVoices);
}

TEST(SlicePlayerTest, ShortSliceFadeDoesNotOverrun)
{
    Player player{kSampleRate};
    player.setFadeMs(8.f); // larger than half the slice
    const auto loop = makeConstLoop(64, 1.f, 1.f);
    player.setLoop(loop, 64);
    const std::array<Slice, 1> slices{Slice{0, 6}}; // playLen 6, effectiveFade clamped to 3
    player.setSlices(slices);

    player.triggerSlice(0);
    const auto out = render(player, kBlock);
    for (size_t i = 0; i < 6; ++i)
    {
        EXPECT_GE(out.left[i], 0.f);
        EXPECT_LE(out.left[i], 1.f + 1e-6f);
    }
    for (size_t i = 6; i < out.left.size(); ++i)
    {
        EXPECT_FLOAT_EQ(out.left[i], 0.f);
    }
}

}
