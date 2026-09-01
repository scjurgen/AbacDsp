#include "gtest/gtest.h"

#include "Synthesizer/MpeCurveMap.h"

namespace AbacDsp::Test
{

TEST(MpeCurveMap, linearCurveIsIdentity)
{
    const MpeCurveMap map;
    constexpr size_t linearCurve = 2;
    EXPECT_NEAR(map.get(linearCurve, 0), 0.0f, 1e-5f);
    EXPECT_NEAR(map.get(linearCurve, static_cast<int>(MpeCurveMap::kMapSize) - 1), 1.0f, 1e-5f);
    const auto mid = static_cast<int>(MpeCurveMap::kMapSize) / 2;
    const auto expected = static_cast<float>(mid) / static_cast<float>(MpeCurveMap::kMapSize - 1);
    EXPECT_NEAR(map.get(linearCurve, mid), expected, 1e-4f);
}

TEST(MpeCurveMap, squareCurveBowsBelowLinearInMidRange)
{
    const MpeCurveMap map;
    constexpr size_t linearCurve = 2;
    constexpr size_t squareCurve = 3;
    const auto mid = static_cast<int>(MpeCurveMap::kMapSize) / 2;
    EXPECT_LT(map.get(squareCurve, mid), map.get(linearCurve, mid));
}

TEST(MpeCurveMap, cubeRootCurveBowsAboveLinearInMidRange)
{
    const MpeCurveMap map;
    constexpr size_t linearCurve = 2;
    constexpr size_t cubeRootCurve = 0;
    const auto mid = static_cast<int>(MpeCurveMap::kMapSize) / 2;
    EXPECT_GT(map.get(cubeRootCurve, mid), map.get(linearCurve, mid));
}

TEST(MpeCurveMap, outOfRangePositionClampsInsteadOfIndexingOutOfBounds)
{
    const MpeCurveMap map;
    constexpr size_t linearCurve = 2;
    const auto lastValid = map.get(linearCurve, static_cast<int>(MpeCurveMap::kMapSize) - 1);

    EXPECT_FLOAT_EQ(map.get(linearCurve, static_cast<int>(MpeCurveMap::kMapSize)), lastValid);
    EXPECT_FLOAT_EQ(map.get(linearCurve, static_cast<int>(MpeCurveMap::kMapSize) + 1000), lastValid);
    EXPECT_FLOAT_EQ(map.get(linearCurve, -1), map.get(linearCurve, 0));
    EXPECT_FLOAT_EQ(map.get(linearCurve, -1000), map.get(linearCurve, 0));
}

TEST(MpeCurveMap, allCurvesAreMonotonicallyNonDecreasing)
{
    const MpeCurveMap map;
    for (size_t curve = 0; curve < MpeCurveMap::kNumCurves; ++curve)
    {
        float previous = map.get(curve, 0);
        for (int i = 1; i < static_cast<int>(MpeCurveMap::kMapSize); i += 37)
        {
            const auto value = map.get(curve, i);
            EXPECT_GE(value, previous) << "curve " << curve << " at position " << i;
            previous = value;
        }
    }
}

}
