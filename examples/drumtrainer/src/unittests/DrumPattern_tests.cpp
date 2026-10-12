#include <gtest/gtest.h>

#include "impl/DrumPattern.h"

using namespace DrumPattern;

TEST(InstrumentTableTest, IndexMatchesEnumAndLuaName)
{
    EXPECT_EQ(luaName(Instrument::Kick), "Kick");
    EXPECT_EQ(luaName(Instrument::HihatOpen), "HihatOpen");
    EXPECT_EQ(luaName(Instrument::TomFloor), "TomFloor");
    EXPECT_EQ(kInstrumentNames.size(), 8u);
}

TEST(InstrumentTableTest, InstrumentFromIndexRejectsOutOfRange)
{
    EXPECT_EQ(instrumentFromIndex(0), Instrument::Kick);
    EXPECT_EQ(instrumentFromIndex(7), Instrument::TomFloor);
    EXPECT_FALSE(instrumentFromIndex(-1).has_value());
    EXPECT_FALSE(instrumentFromIndex(static_cast<long long>(kInstrumentNames.size())).has_value());
}

TEST(InstrumentTableTest, ClosedAndOpenHihatShareALimb)
{
    EXPECT_TRUE(sharesALimb(Instrument::HihatClosed, Instrument::HihatOpen));
    EXPECT_TRUE(sharesALimb(Instrument::Snare, Instrument::Snare));
    EXPECT_FALSE(sharesALimb(Instrument::Kick, Instrument::HihatClosed));
    EXPECT_FALSE(sharesALimb(Instrument::TomHigh, Instrument::TomLow));
}

TEST(TripleBufferTest, NothingAcquiredBeforeFirstPublish)
{
    TripleBuffer<int> buffer;
    EXPECT_EQ(buffer.acquireNewest(), nullptr);
}

TEST(TripleBufferTest, ConsumerSeesNewestValueOnly)
{
    TripleBuffer<int> buffer;
    buffer.backSlot() = 1;
    buffer.publish();
    buffer.backSlot() = 2;
    buffer.publish();
    const int* newest = buffer.acquireNewest();
    ASSERT_NE(newest, nullptr);
    EXPECT_EQ(*newest, 2);
    EXPECT_EQ(buffer.acquireNewest(), nullptr);
}

TEST(TripleBufferTest, ProducerNeverOverwritesTheValueTheConsumerHolds)
{
    TripleBuffer<int> buffer;
    buffer.backSlot() = 10;
    buffer.publish();
    const int* held = buffer.acquireNewest();
    ASSERT_NE(held, nullptr);
    for (int i = 11; i < 20; ++i)
    {
        buffer.backSlot() = i;
        buffer.publish();
        EXPECT_EQ(*held, 10);
    }
}
