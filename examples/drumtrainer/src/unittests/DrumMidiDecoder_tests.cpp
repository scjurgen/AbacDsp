#include <array>
#include <cstdint>
#include <gtest/gtest.h>
#include <utility>
#include <vector>

#include "impl/DrumMidiDecoder.h"

using namespace DrumInput;

namespace
{
constexpr float kSampleRate{48000.f};
constexpr uint8_t kNoteOnCh10{0x99};
constexpr uint8_t kCcCh10{0xB9};

struct Fixture
{
    explicit Fixture(const DrumMap& map = DrumMap{})
    {
        decoder.setMap(map, kSampleRate);
    }

    void send(const std::array<uint8_t, 3> bytes, const int64_t time)
    {
        decoder.process(bytes, time, [this](const DrumHit& hit) { hits.push_back(hit); });
    }

    void advance(const int64_t now)
    {
        decoder.advance(now, [this](const DrumHit& hit) { hits.push_back(hit); });
    }

    DrumMidiDecoder decoder;
    std::vector<DrumHit> hits;
};
}

TEST(DrumMidiDecoderTest, MappedNoteOnBecomesAHitImmediately)
{
    Fixture f;
    f.send({kNoteOnCh10, 74, 110}, 500);
    ASSERT_EQ(f.hits.size(), 1u);
    EXPECT_EQ(f.hits[0].voice, PlayedVoice::Kick);
    EXPECT_EQ(f.hits[0].velocity, 110);
    EXPECT_EQ(f.hits[0].time, 500);
}

TEST(DrumMidiDecoderTest, EveryDefaultPadMapsToItsVoice)
{
    Fixture f;
    const auto cases = std::to_array<std::pair<uint8_t, PlayedVoice>>({{60, PlayedVoice::Snare},
                                                                       {62, PlayedVoice::TomLow},
                                                                       {63, PlayedVoice::TomHigh},
                                                                       {70, PlayedVoice::Crash},
                                                                       {72, PlayedVoice::TomFloor},
                                                                       {74, PlayedVoice::Kick},
                                                                       {76, PlayedVoice::HihatPedal}});
    for (const auto& [note, voice] : cases)
    {
        f.hits.clear();
        f.send({kNoteOnCh10, note, 100}, 0);
        ASSERT_EQ(f.hits.size(), 1u) << int{note};
        EXPECT_EQ(f.hits[0].voice, voice) << int{note};
    }
}

TEST(DrumMidiDecoderTest, NonNoteOnMessagesAndUnmappedNotesAreIgnored)
{
    Fixture f;
    f.send({kNoteOnCh10, 61, 100}, 0);
    f.send({kNoteOnCh10, 60, 0}, 0);
    f.send({0x89, 60, 64}, 0);
    f.send({0xA9, 64, 90}, 0);
    f.send({kCcCh10, 7, 100}, 0);
    f.decoder.process(std::array<uint8_t, 2>{kNoteOnCh10, 60}, 0, [&](const DrumHit& hit) { f.hits.push_back(hit); });
    f.advance(10000);
    EXPECT_TRUE(f.hits.empty());
}

TEST(DrumMidiDecoderTest, InputChannelFiltersNotesAndPedal)
{
    DrumMap map{};
    map.inputChannel = 10;
    Fixture f{map};
    f.send({0x90, 60, 100}, 0);
    EXPECT_TRUE(f.hits.empty());
    f.send({0xB0, 4, 0}, 0);
    f.send({kNoteOnCh10, 64, 100}, 100);
    f.advance(10000);
    ASSERT_EQ(f.hits.size(), 1u);
    EXPECT_EQ(f.hits[0].voice, PlayedVoice::HihatClosed);
}

TEST(DrumMidiDecoderTest, HihatStickIsClassifiedByThePedalCc)
{
    Fixture f;
    f.send({kCcCh10, 4, 0}, 0);
    f.send({kNoteOnCh10, 64, 90}, 1000);
    EXPECT_TRUE(f.hits.empty());
    f.advance(2000);
    f.send({kCcCh10, 4, 123}, 3000);
    f.send({kNoteOnCh10, 64, 80}, 4000);
    f.send({kCcCh10, 4, 127}, 5000);
    f.send({kNoteOnCh10, 64, 70}, 6000);
    f.advance(7000);
    ASSERT_EQ(f.hits.size(), 3u);
    EXPECT_EQ(f.hits[0].voice, PlayedVoice::HihatOpen);
    EXPECT_EQ(f.hits[0].pedal, 0);
    EXPECT_EQ(f.hits[0].time, 1000);
    EXPECT_EQ(f.hits[1].voice, PlayedVoice::HihatHalfOpen);
    EXPECT_EQ(f.hits[2].voice, PlayedVoice::HihatClosed);
    EXPECT_EQ(f.hits[2].velocity, 70);
}

TEST(DrumMidiDecoderTest, OtherVoicesCarryThePedalValueInEffect)
{
    Fixture f;
    f.send({kCcCh10, 4, 10}, 0);
    f.send({kNoteOnCh10, 76, 100}, 100);
    ASSERT_EQ(f.hits.size(), 1u);
    EXPECT_EQ(f.hits[0].voice, PlayedVoice::HihatPedal);
    EXPECT_EQ(f.hits[0].pedal, 10);
}

TEST(DrumMidiDecoderTest, PedalCcNumberComesFromTheMap)
{
    DrumMap map{};
    map.hihat.pedalCc = 11;
    Fixture f{map};
    f.send({kCcCh10, 4, 0}, 0);
    f.send({kNoteOnCh10, 64, 100}, 100);
    f.advance(10000);
    ASSERT_EQ(f.hits.size(), 1u);
    EXPECT_EQ(f.hits[0].voice, PlayedVoice::HihatClosed);
    f.send({kCcCh10, 11, 0}, 20000);
    f.send({kNoteOnCh10, 64, 100}, 20100);
    f.advance(30000);
    ASSERT_EQ(f.hits.size(), 2u);
    EXPECT_EQ(f.hits[1].voice, PlayedVoice::HihatOpen);
}
