
#include <tuple>

#include "gtest/gtest.h"

#include "Delays/MultiTapDelay.h"

namespace AbacDsp::Test
{
TEST(MultiTapDelayTest, simpleFeedAndEat)
{
    MultiTapDelay<1000, 1> sut{};
    sut.setTapDelay(0, 100);
    sut.write(1.f);
    std::ignore = sut.readTap(0);
    for (size_t i = 0; i < 99; ++i)
    {
        sut.write(0.f);
        EXPECT_EQ(sut.readTap(0), 0.f) << "failed at " << i;
    }
    sut.write(0.f);
    EXPECT_GT(sut.readTap(0), 0.99f);
}

TEST(MultiTapDelayTest, zeroDelay)
{
    MultiTapDelay<100, 1> sut{};
    sut.setTapDelay(0, 0);
    sut.write(1.f);
    EXPECT_EQ(sut.readTap(0), 1.f);
    for (size_t i = 0; i < 4; ++i)
    {
        sut.write(0.f);
        EXPECT_EQ(sut.readTap(0), 0.f);
    }
}

TEST(MultiTapDelayTest, oneDelay)
{
    MultiTapDelay<10, 1> sut{};
    sut.setTapDelay(0, 1);
    sut.write(1.f);
    EXPECT_EQ(sut.readTap(0), 0.f);
    sut.write(0.f);
    EXPECT_EQ(sut.readTap(0), 1.f);
    for (size_t i = 0; i < 4; ++i)
    {
        sut.write(0.f);
        EXPECT_EQ(sut.readTap(0), 0.f);
    }
}

TEST(MultiTapDelayTest, independentTapsReadTheSameBufferAtDifferentOffsets)
{
    MultiTapDelay<1000, 2> sut{};
    sut.setTapDelay(0, 1);
    sut.setTapDelay(1, 3);

    sut.write(1.f);
    EXPECT_EQ(sut.readTap(0), 0.f);
    EXPECT_EQ(sut.readTap(1), 0.f);

    sut.write(0.f);
    EXPECT_EQ(sut.readTap(0), 1.f) << "tap 0 sees the impulse one sample later";
    EXPECT_EQ(sut.readTap(1), 0.f);

    sut.write(0.f);
    EXPECT_EQ(sut.readTap(0), 0.f);
    EXPECT_EQ(sut.readTap(1), 0.f);

    sut.write(0.f);
    EXPECT_EQ(sut.readTap(0), 0.f);
    EXPECT_EQ(sut.readTap(1), 1.f) << "tap 1 sees the impulse three samples later";
}
}
