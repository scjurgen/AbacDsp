
#include "Delays/NaiveDelay.h"

#include "gtest/gtest.h"
#include "gmock/gmock.h"

namespace AbacDsp::Test
{
TEST(NaiveDelayTest, simpleFeedAndEat)
{
    NaiveDelay<1000> sut{};
    sut.setSize(100);
    sut.step(1);
    for (size_t i = 0; i < 99; ++i)
    {
        EXPECT_EQ(sut.step(0), 0.f) << "failed at " << i;
    }
    EXPECT_GT(sut.step(0), 0.99f);
}
TEST(NaiveDelayTest, zeroDelay)
{
    NaiveDelay<100> sut{};
    sut.setSize(0);
    EXPECT_EQ(sut.step(1), 1);
    EXPECT_EQ(sut.step(0), 0);
    EXPECT_EQ(sut.step(0), 0);
    EXPECT_EQ(sut.step(0), 0);
    EXPECT_EQ(sut.step(0), 0);
}
TEST(NaiveDelayTest, oneDelay)
{
    NaiveDelay<10> sut{};
    sut.setSize(1);
    EXPECT_EQ(sut.step(1), 0);
    EXPECT_EQ(sut.step(0), 1);
    EXPECT_EQ(sut.step(0), 0);
    EXPECT_EQ(sut.step(0), 0);
    EXPECT_EQ(sut.step(0), 0);
    EXPECT_EQ(sut.step(0), 0);
}
}