#include "Numbers/BulgeControl.h"

#include <gtest/gtest.h>
#include <vector>

namespace AbacDsp::Test
{

class FillTableTest : public ::testing::TestWithParam<std::tuple<float, std::vector<int>>>
{
};

TEST_P(FillTableTest, ProducesExpectedValues)
{
    auto [bulge, expected] = GetParam();
    std::vector<int> result(5);

    Bulge::fillTable(result.begin(), 5, bulge, 1000, 2000);

    EXPECT_EQ(result, expected);
}

INSTANTIATE_TEST_SUITE_P(BulgeValues, FillTableTest,
                         ::testing::Values(std::make_tuple(-1.0f, std::vector<int>{1000, 1684, 1938, 1996, 2000}),
                                           std::make_tuple(-0.5f, std::vector<int>{1000, 1438, 1750, 1938, 2000}),
                                           std::make_tuple(-0.25f, std::vector<int>{1000, 1334, 1625, 1859, 2000}),
                                           std::make_tuple(0.0f, std::vector<int>{1000, 1250, 1500, 1750, 2000}),
                                           std::make_tuple(0.25f, std::vector<int>{1000, 1141, 1375, 1666, 2000}),
                                           std::make_tuple(0.5f, std::vector<int>{1000, 1063, 1250, 1563, 2000}),
                                           std::make_tuple(1.0f, std::vector<int>{1000, 1004, 1063, 1316, 2000})));

TEST(FillTableTest, HandlesInvertedMinMax)
{
    std::vector<int> result(5);
    Bulge::fillTable(result.begin(), 5, 0.0f, 2000, 1000);

    std::vector<int> expected{2000, 1750, 1500, 1250, 1000};
    EXPECT_EQ(result, expected);
}

TEST(FillTableTest, HandlesSingleItem)
{
    std::vector<int> result(1);
    Bulge::fillTable(result.begin(), 1, 0.5f, 100, 200);

    EXPECT_EQ(result[0], 100);
}

TEST(FillTableTest, HandlesZeroItems)
{
    std::vector<int> result;
    Bulge::fillTable(result.begin(), 0, 0.5f, 100, 200);

    EXPECT_TRUE(result.empty());
}

class FillNormalizedTableTest : public ::testing::TestWithParam<std::tuple<float, std::vector<float>>>
{
};

TEST_P(FillNormalizedTableTest, ProducesExpectedValues)
{
    auto [bulge, expected] = GetParam();
    std::vector<float> result(5);

    Bulge::fillNormalizedTable(result.begin(), 5, bulge);

    for (size_t i = 0; i < result.size(); ++i)
    {
        EXPECT_NEAR(result[i], expected[i], 0.0001f);
    }
}

INSTANTIATE_TEST_SUITE_P(
    BulgeValues, FillNormalizedTableTest,
    ::testing::Values(std::make_tuple(-1.0f, std::vector<float>{0.0f, 0.6836f, 0.9375f, 0.9961f, 1.0f}),
                      std::make_tuple(-0.5f, std::vector<float>{0.0f, 0.4375f, 0.75f, 0.9375f, 1.0f}),
                      std::make_tuple(0.0f, std::vector<float>{0.0f, 0.25f, 0.5f, 0.75f, 1.0f}),
                      std::make_tuple(0.5f, std::vector<float>{0.0f, 0.0625f, 0.25f, 0.5625f, 1.0f}),
                      std::make_tuple(1.0f, std::vector<float>{0.0f, 0.0039f, 0.0625f, 0.3164f, 1.0f})));

class GetValueTest : public ::testing::TestWithParam<std::tuple<float, float, float>>
{
};

TEST_P(GetValueTest, ProducesExpectedValue)
{
    auto [x, bulge, expected] = GetParam();

    float result = Bulge::getValue(x, bulge);

    EXPECT_NEAR(result, expected, 0.0001f);
}

INSTANTIATE_TEST_SUITE_P(InputOutputPairs, GetValueTest,
                         ::testing::Values(std::make_tuple(0.0f, -0.5f, 0.0f), std::make_tuple(0.25f, -0.5f, 0.4375f),
                                           std::make_tuple(0.5f, -0.5f, 0.75f), std::make_tuple(0.75f, -0.5f, 0.9375f),
                                           std::make_tuple(1.0f, -0.5f, 1.0f), std::make_tuple(0.0f, 0.0f, 0.0f),
                                           std::make_tuple(0.25f, 0.0f, 0.25f), std::make_tuple(0.5f, 0.0f, 0.5f),
                                           std::make_tuple(0.75f, 0.0f, 0.75f), std::make_tuple(1.0f, 0.0f, 1.0f),
                                           std::make_tuple(0.0f, 0.5f, 0.0f), std::make_tuple(0.25f, 0.5f, 0.0625f),
                                           std::make_tuple(0.5f, 0.5f, 0.25f), std::make_tuple(0.75f, 0.5f, 0.5625f),
                                           std::make_tuple(1.0f, 0.5f, 1.0f)));

}