#include <array>
#include <cstdint>
#include <gtest/gtest.h>
#include <span>
#include <vector>

#include "impl/MidiClickOut.h"

using AbacDsp::ClickAccent;
using DrumOutput::MidiClickOut;

namespace
{
constexpr float kSampleRate{48000.f};
constexpr int64_t kNoteLength{1440}; // 30 ms at 48 kHz

struct Sent
{
    std::array<uint8_t, 3> bytes{};
    int offset{0};
};

std::vector<Sent> drain(MidiClickOut& click, const int64_t blockStart)
{
    std::vector<Sent> sent;
    click.drain(blockStart, [&](const std::span<const uint8_t> bytes, const int offset)
                { sent.push_back({{bytes[0], bytes[1], bytes[2]}, offset}); });
    return sent;
}

MidiClickOut makeClick(const DrumInput::ClickSettings& settings = {})
{
    MidiClickOut click;
    click.configure(settings, kSampleRate);
    return click;
}
}

TEST(MidiClickOutTest, AccentsPickNoteAndVelocity)
{
    auto click = makeClick();
    click.trigger(ClickAccent::Downbeat, 10);
    click.trigger(ClickAccent::Beat, 20);
    click.trigger(ClickAccent::Sub, 30);
    click.trigger(ClickAccent::None, 40);
    std::vector<Sent> sent;
    for (const auto& event : drain(click, 0))
    {
        if ((event.bytes[0] & 0xF0) == 0x90)
        {
            sent.push_back(event);
        }
    }
    ASSERT_EQ(sent.size(), 3u);
    EXPECT_EQ(sent[0].bytes, (std::array<uint8_t, 3>{0x99, 58, MidiClickOut::kDownbeatVelocity}));
    EXPECT_EQ(sent[0].offset, 10);
    EXPECT_EQ(sent[1].bytes, (std::array<uint8_t, 3>{0x99, 59, MidiClickOut::kBeatVelocity}));
    EXPECT_EQ(sent[2].bytes, (std::array<uint8_t, 3>{0x99, 59, MidiClickOut::kSubVelocity}));
}

TEST(MidiClickOutTest, NoteOffFollowsAfterTheNoteLength)
{
    auto click = makeClick();
    click.trigger(ClickAccent::Downbeat, 100);
    static_cast<void>(drain(click, 0));
    click.advance(100 + kNoteLength);
    EXPECT_TRUE(drain(click, 0).empty());
    click.advance(100 + kNoteLength + 1);
    const auto sent = drain(click, 1000);
    ASSERT_EQ(sent.size(), 1u);
    EXPECT_EQ(sent[0].bytes, (std::array<uint8_t, 3>{0x89, 58, 0}));
    EXPECT_EQ(sent[0].offset, 100 + kNoteLength - 1000);
}

TEST(MidiClickOutTest, ARetriggeredNoteIsReleasedFirst)
{
    auto click = makeClick();
    click.trigger(ClickAccent::Beat, 0);
    click.trigger(ClickAccent::Sub, 500);
    const auto sent = drain(click, 0);
    ASSERT_EQ(sent.size(), 3u);
    EXPECT_EQ(sent[1].bytes[0], 0x89);
    EXPECT_EQ(sent[1].offset, 500);
    EXPECT_EQ(sent[2].bytes[0], 0x99);
    click.advance(10000);
    EXPECT_EQ(drain(click, 0).size(), 1u);
}

TEST(MidiClickOutTest, ChannelAndNotesComeFromTheSettings)
{
    auto click = makeClick({2, 36, 37});
    click.trigger(ClickAccent::Downbeat, 0);
    click.trigger(ClickAccent::Beat, 5000);
    click.advance(10000);
    const auto sent = drain(click, 0);
    ASSERT_EQ(sent.size(), 4u);
    EXPECT_EQ(sent[0].bytes, (std::array<uint8_t, 3>{0x91, 36, MidiClickOut::kDownbeatVelocity}));
    EXPECT_EQ(sent[1].bytes, (std::array<uint8_t, 3>{0x81, 36, 0}));
    EXPECT_EQ(sent[2].bytes[1], 37);
}

TEST(MidiClickOutTest, DrainSortsEventsByTime)
{
    auto click = makeClick();
    click.trigger(ClickAccent::Downbeat, 0);
    click.trigger(ClickAccent::Beat, 1000);
    click.advance(2000);
    const auto sent = drain(click, 0);
    ASSERT_EQ(sent.size(), 3u);
    EXPECT_EQ(sent[0].offset, 0);
    EXPECT_EQ(sent[1].offset, 1000);
    EXPECT_EQ(sent[2].offset, static_cast<int>(kNoteLength));
}
