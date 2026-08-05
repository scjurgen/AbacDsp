#include <algorithm>
#include <cmath>
#include <vector>

#include "gtest/gtest.h"

#include "Filters/Sinc/SincFilter.h"

namespace AbacDsp::Test
{

namespace
{
size_t widthFromFloatDivision(size_t coeffCount, size_t increment, float maxRatio)
{
    return static_cast<size_t>(
        3 * std::lrint(static_cast<float>(coeffCount) / static_cast<float>(increment) * maxRatio) + 1);
}

// Deliberately reproduces the pre-fix truncated-integer-division formula, to
// show it would have given a different (wrong) buffer size.
size_t widthFromTruncatedDivision(size_t coeffCount, size_t increment, float maxRatio)
{
    // NOLINTNEXTLINE(bugprone-integer-division)
    return static_cast<size_t>(3 * std::lrint(static_cast<float>(coeffCount / increment) * maxRatio) + 1);
}
}

// Regression test: getBufferSize() used to divide m_coeffs.size() by m_increment
// as integers, truncating before the cast to float. Real kernel tables (e.g.
// sinc_69_768.h) are never a whole multiple of their increment, so the
// truncated width came out smaller than the correct one.
TEST(SincFilterTest, getBufferSizeUsesFloatDivisionNotTruncated)
{
    constexpr size_t increment = 768;
    constexpr size_t coeffCount = 110709; // half-length of sinc_69_768.h, not a multiple of increment
    constexpr float maxRatio = 128.f;     // SrConvertMaxRatio
    constexpr size_t channels = 2;

    const SincFilter::InitParam param{increment, std::vector<float>(coeffCount, 0.f)};
    const SincFilter sut{param};

    const auto correctWidth = widthFromFloatDivision(coeffCount, increment, maxRatio);
    const auto truncatedWidth = widthFromTruncatedDivision(coeffCount, increment, maxRatio);
    ASSERT_NE(correctWidth, truncatedWidth) << "test parameters no longer exercise the truncation";

    const auto expectedBufferSize = 1 + channels * (1 + std::max<size_t>(correctWidth, 4096));
    const auto buggyBufferSize = 1 + channels * (1 + std::max<size_t>(truncatedWidth, 4096));

    EXPECT_EQ(sut.getBufferSize(maxRatio, channels), expectedBufferSize);
    EXPECT_NE(sut.getBufferSize(maxRatio, channels), buggyBufferSize);
}

TEST(SincFilterTest, getBufferSizeAppliesFloorBelowIt)
{
    constexpr size_t increment = 128;
    constexpr size_t coeffCount = 1251; // half-length of sinc_4.h, not a multiple of increment
    constexpr float maxRatio = 16.f;    // SrPullConvertMaxRatio, small enough to stay under the floor
    constexpr size_t channels = 2;

    const SincFilter::InitParam param{increment, std::vector<float>(coeffCount, 0.f)};
    const SincFilter sut{param};

    const auto width = widthFromFloatDivision(coeffCount, increment, maxRatio);
    ASSERT_LT(width, 4096u) << "test parameters no longer stay under the floor";

    EXPECT_EQ(sut.getBufferSize(maxRatio, channels), 1 + channels * (1 + 4096));
}

}
