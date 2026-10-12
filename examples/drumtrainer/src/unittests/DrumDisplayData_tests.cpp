#include <gtest/gtest.h>
#include <vector>

#include "impl/DrumDisplayData.h"

using namespace DrumDisplay;

namespace
{
std::vector<float> drain(const DisplayHitRing& ring, uint32_t& readIndex)
{
    std::vector<float> positions;
    ring.read(readIndex, [&](const DisplayHit& hit) { positions.push_back(hit.patternBeats); });
    return positions;
}
}

TEST(PatternPositionTest, PackAndUnpackRoundTrip)
{
    const PatternPosition position{0.625f, 12345, true};
    const auto unpacked = PatternPosition::unpack(position.pack());
    EXPECT_FLOAT_EQ(unpacked.phase, 0.625f);
    EXPECT_EQ(unpacked.revolution, 12345u);
    EXPECT_TRUE(unpacked.running);
    EXPECT_FALSE(PatternPosition::unpack(PatternPosition{0.f, 7, false}.pack()).running);
}

TEST(DisplayHitRingTest, ReaderSeesHitsInOrderOnce)
{
    DisplayHitRing ring;
    uint32_t readIndex = 0;
    ring.push({1.f, 0, DrumInput::PlayedVoice::Kick, 100});
    ring.push({2.f, 0, DrumInput::PlayedVoice::Snare, 100});
    EXPECT_EQ(drain(ring, readIndex), (std::vector<float>{1.f, 2.f}));
    EXPECT_TRUE(drain(ring, readIndex).empty());
    ring.push({3.f, 0, DrumInput::PlayedVoice::Kick, 100});
    EXPECT_EQ(drain(ring, readIndex), (std::vector<float>{3.f}));
}

TEST(DisplayHitRingTest, AReaderTooFarBehindSkipsToTheNewest)
{
    DisplayHitRing ring;
    uint32_t readIndex = 0;
    for (uint32_t i = 0; i < DisplayHitRing::kCapacity + 5; ++i)
    {
        ring.push({static_cast<float>(i), 0, DrumInput::PlayedVoice::Kick, 100});
    }
    EXPECT_TRUE(drain(ring, readIndex).empty());
    ring.push({999.f, 0, DrumInput::PlayedVoice::Kick, 100});
    EXPECT_EQ(drain(ring, readIndex), (std::vector<float>{999.f}));
}

TEST(DisplayHitRingTest, AReaderOfAnEarlierRingReplaysNothingStale)
{
    DisplayHitRing ring;
    uint32_t readIndex = 500;
    EXPECT_TRUE(drain(ring, readIndex).empty());
    ring.push({1.f, 0, DrumInput::PlayedVoice::Kick, 100});
    EXPECT_EQ(drain(ring, readIndex), (std::vector<float>{1.f}));
}
