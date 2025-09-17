#include <gtest/gtest.h>
#include <cmath>
#include <random>

#include "Reverbs/HadamardFeed.h"

#include "Reverbs/Hadamard4.h"
#include "Reverbs/Hadamard8.h"
#include "Reverbs/Hadamard16.h"
#include "Reverbs/Hadamard32.h"

#include "Reverbs/HadamardWalsh4.h"
#include "Reverbs/HadamardWalsh8.h"
#include "Reverbs/HadamardWalsh16.h"
#include "Reverbs/HadamardWalsh32.h"

namespace AbacDsp::Test
{
TEST(HadamardFeedTest, checkSumAndSingle)
{
    struct TestSet
    {
        unsigned order;
        std::vector<float> sums;
    };
    std::vector<TestSet> expected{
        {4, {4, 0, 0, 0}},
        {8, {8, 0, 0, 0, 0, 0, 0, 0}},
        {12, {-10, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2}},
        {16, {16, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
        {20, {-2, 2, 2, 2, 2, 6, 2, 2, -2, 6, 6, -10, 6, 2, 6, -6, 2, -6, 2, -2}},
        {24, {-4, 4, 4, 0, 12, 0, 4, 4, 0, 4, 0, -4, 4, -4, 0, 0, 12, -4, 4, 4, -8, -4, 0, -4}},
        {28, {-6, 6, -2, -10, -2, 6, 2, -2, 2, -2, 2, 2, 6, 10, -2, 2, -6, 10, -2, 2, 10, 6, 6, -2, -6, -6, 2, 2}},
        {32, {32, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
    };
    for (size_t i = 0; i < expected.size(); ++i)
    {
        auto order = expected[i].order;
        const std::vector inVec(order, 1.f);
        std::vector sumVec(order, 1.f);

        hadamardFeed(order, inVec.data(), sumVec.data());
        float sumSum = 0;
        for (unsigned r = 0; r < order; ++r)
        {
            sumSum += sumVec[r];
            EXPECT_FLOAT_EQ(sumVec[r], expected[i].sums[r]);
        }
        EXPECT_EQ(sumSum, order);
    }
}

TEST(HadamardFeedTest, singleValue)
{
    for (unsigned order = 4; order <= 32; order += 4)
    {
        std::vector inVec(order, 0.f);
        std::vector sumVec(order, 0.f);
        inVec[0] = 1.f;

        hadamardFeed(order, inVec.data(), sumVec.data());
        float sumSum = 0;
        for (unsigned i = 0; i < order; ++i)
        {
            sumSum += sumVec[i];
            EXPECT_FLOAT_EQ(sumVec[i], 1.f);
        }
        EXPECT_EQ(sumSum, order);
    }
}

TEST(HadamardFeedTest, randomValuesMaximalDeterminant)
{
    std::minstd_rand generator(4242);

    std::uniform_real_distribution<float> dist(-1.f, 1.f);
    for (size_t runs = 0; runs < 1000; ++runs)
    {
        for (unsigned order = 4; order <= 32; order += 4)
        {
            std::vector inVec(order, 0.f);
            std::vector sumVec(order, 0.f);
            for (size_t r = 0; r < order; ++r)
            {
                inVec[r] = dist(generator);
            }
            hadamardFeed(order, inVec.data(), sumVec.data());
            float sumSum = 0;
            for (unsigned i = 0; i < order; ++i)
            {
                sumSum += sumVec[i];
            }
            EXPECT_LT(sumSum, order);
        }
    }
}

TEST(HadamardFeedTest, hadmardWalch4SimdCompare)
{
    std::minstd_rand generator(4242);

    std::uniform_real_distribution dist(-1.f, 1.f);
    for (size_t runs = 0; runs < 1000; ++runs)
    {
        std::array<float, 4> inVec{};
        std::array<float, 4> sumVec{};
        std::array<float, 4> sumVecSimd{};
        std::array<float, 4> sumVecWalsh{};
        std::array<float, 4> sumVecWalshSimd{};
        for (size_t r = 0; r < 4; ++r)
        {
            inVec[r] = dist(generator);
        }
        hadamardFeed4(inVec, sumVec);
        hadamardFeed4_simd(inVec.data(), sumVecSimd.data());
        hadamardWalsh4(inVec, sumVecWalsh);
        hadamardWalsh4_simd(inVec, sumVecWalshSimd);
        for (unsigned i = 0; i < 4; ++i)
        {
            EXPECT_NEAR(sumVec[i], sumVecSimd[i], 1E-5f);
            EXPECT_NEAR(sumVec[i], sumVecWalsh[i], 1E-5f);
            EXPECT_NEAR(sumVec[i], sumVecWalshSimd[i], 1E-5f);
        }
    }
}


TEST(HadamardFeedTest, hadmardWalsh8Compare)
{
    std::minstd_rand generator(4242);

    std::uniform_real_distribution dist(-1.f, 1.f);
    for (size_t runs = 0; runs < 1000; ++runs)
    {
        std::array<float, 8> inVec{};
        std::array<float, 8> sumVec{};
        std::array<float, 8> sumVecSimd{};
        std::array<float, 8> sumVecWalsh{};
        std::array<float, 8> sumVecWalshSimd{};
        for (size_t r = 0; r < 8; ++r)
        {
            inVec[r] = dist(generator);
        }
        hadamardFeed8(inVec, sumVec);
        hadamardFeed8_simd(inVec.data(), sumVecSimd.data());
        hadamardWalsh8(inVec, sumVecWalsh);
        hadamardWalsh8_simd(inVec, sumVecWalshSimd);
        for (unsigned i = 0; i < 8; ++i)
        {
            EXPECT_NEAR(sumVec[i], sumVecSimd[i], 1E-5f);
            EXPECT_NEAR(sumVec[i], sumVecWalsh[i], 1E-5f);
            EXPECT_NEAR(sumVec[i], sumVecWalshSimd[i], 1E-5f);
        }
    }
}


TEST(HadamardFeedTest, hadmardWalsh16Compare)
{
    std::minstd_rand generator(4242);

    std::uniform_real_distribution dist(-1.f, 1.f);
    for (size_t runs = 0; runs < 1000; ++runs)
    {
        std::array<float, 16> inVec{};
        std::array<float, 16> sumVec{};
        std::array<float, 16> sumVecSimd{};
        std::array<float, 16> sumVecWalsh{};
        std::array<float, 16> sumVecWalshSimd{};
        for (size_t r = 0; r < 16; ++r)
        {
            inVec[r] = dist(generator);
        }
        hadamardFeed16(inVec, sumVec);
        hadamardFeed16_simd(inVec.data(), sumVecSimd.data());
        hadamardWalsh16(inVec, sumVecWalsh);
        hadamardWalsh16_simd(inVec, sumVecWalshSimd);
        for (unsigned i = 0; i < 16; ++i)
        {
            EXPECT_NEAR(sumVec[i], sumVecSimd[i], 1E-5f);
            EXPECT_NEAR(sumVec[i], sumVecWalsh[i], 1E-5f);
            EXPECT_NEAR(sumVec[i], sumVecWalshSimd[i], 1E-5f);
        }
    }
}

TEST(HadamardFeedTest, hadmardWalsh32Compare)
{
    std::minstd_rand generator(4242);

    std::uniform_real_distribution dist(-1.f, 1.f);
    for (size_t runs = 0; runs < 1000; ++runs)
    {
        std::array<float, 32> inVec{};
        std::array<float, 32> sumVec{};
        std::array<float, 32> sumVecSimd{};
        std::array<float, 32> sumVecWalsh{};
        std::array<float, 32> sumVecWalshSimd{};
        for (size_t r = 0; r < 32; ++r)
        {
            inVec[r] = dist(generator);
        }
        hadamardFeed32(inVec, sumVec);
        hadamardFeed32_simd(inVec.data(), sumVecSimd.data());
        hadamardWalsh32(inVec, sumVecWalsh);
        hadamardWalsh32_simd(inVec, sumVecWalshSimd);
        for (unsigned i = 0; i < 16; ++i)
        {
            EXPECT_NEAR(sumVec[i], sumVecSimd[i], 1E-5f);
            EXPECT_NEAR(sumVec[i], sumVecWalsh[i], 1E-5f);
            EXPECT_NEAR(sumVec[i], sumVecWalshSimd[i], 1E-5f);
        }
    }
}

}