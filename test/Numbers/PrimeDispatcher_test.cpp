#include "Numbers/PrimeDispatcher.h"
#include <gtest/gtest.h>
#include <vector>

namespace AbacDsp::Test
{
TEST(PrimeDispatcherTest, IsPrimeNumber)
{
    EXPECT_FALSE(isPrimeNumber(0));
    EXPECT_FALSE(isPrimeNumber(1));
    EXPECT_TRUE(isPrimeNumber(2));
    EXPECT_TRUE(isPrimeNumber(3));
    EXPECT_FALSE(isPrimeNumber(4));
    EXPECT_TRUE(isPrimeNumber(5));
    EXPECT_FALSE(isPrimeNumber(9));
    EXPECT_TRUE(isPrimeNumber(13));
    EXPECT_FALSE(isPrimeNumber(15));
    EXPECT_TRUE(isPrimeNumber(17));
    EXPECT_TRUE(isPrimeNumber(97));
    EXPECT_FALSE(isPrimeNumber(100));
}

TEST(PrimeDispatcherTest, GetUsefulPrime)
{
    EXPECT_EQ(getUsefulPrime<3>(0), 3);
    EXPECT_EQ(getUsefulPrime<3>(1), 3);
    EXPECT_EQ(getUsefulPrime<3>(2), 3);
    EXPECT_EQ(getUsefulPrime<3>(3), 3);
    EXPECT_EQ(getUsefulPrime<3>(11), 11);
    EXPECT_EQ(getUsefulPrime<3>(12), 13);
    EXPECT_EQ(getUsefulPrime<3>(97), 97);
    EXPECT_EQ(getUsefulPrime<7>(12), 13);
    EXPECT_EQ(getUsefulPrime<5>(100), 101);
}

TEST(PrimeDispatcherTest, ReturnOrderedPrimeTable)
{
    std::vector<size_t> values = {8, 9, 12, 11};
    std::vector<size_t> out(4);
    returnOrderedPrimeTable<3>(values.begin(), out.begin(), values.size());

    EXPECT_EQ(out[0], 11);
    EXPECT_EQ(out[1], 13);
    EXPECT_EQ(out[2], 17);
    EXPECT_EQ(out[3], 19);
}

TEST(PrimeDispatcherTest, GenerateUniquePrimeSet)
{
    std::vector<size_t> values = {5, 5, 7, 7};
    std::vector<size_t> out(4);
    generateUniquePrimeSet<3>(values.begin(), out.begin(), values.size());

    EXPECT_EQ(out[0], 5);
    EXPECT_EQ(out[1], 7);
    EXPECT_EQ(out[2], 11);
    EXPECT_EQ(out[3], 13);
}

TEST(PrimeDispatcherTest, EnsureUniqueDiscreteSize)
{
    size_t values[] = {11, 11, 13, 13};
    ensureUniqueDiscreteSize(values, 4);

    EXPECT_EQ(values[0], 11);
    EXPECT_EQ(values[1], 13);
    EXPECT_EQ(values[2], 17);
    EXPECT_EQ(values[3], 19);
}

TEST(PrimeDispatcherTest, LargeInputHandling)
{
    const size_t largeInput = 1'500'000'000;
    EXPECT_EQ(getUsefulPrime<3334242>(largeInput), largeInput);
}

}