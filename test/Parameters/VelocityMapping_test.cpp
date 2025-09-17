#include <gtest/gtest.h>
#include <cmath>

#include "Parameters/VelocityMapping.h"

namespace AbacDsp::Test
{

class VelocityMappingTest : public ::testing::Test
{
  protected:
    static constexpr float kTolerance = 1e-5f;

    static float dbToLinear(const float db)
    {
        return std::pow(10.0f, db / 20.0f);
    }
};

TEST_F(VelocityMappingTest, GetMethodWithSpecificDynamicRanges)
{
    const float dr_32db = dbToLinear(-32.0f);
    EXPECT_NEAR(VelocityMapping::get(0, dr_32db), 0.02512f, kTolerance);
    EXPECT_NEAR(VelocityMapping::get(1, dr_32db), 0.02658f, kTolerance);
    EXPECT_NEAR(VelocityMapping::get(64, dr_32db), 0.27363f, kTolerance);
    EXPECT_NEAR(VelocityMapping::get(127, dr_32db), 1.0f, kTolerance);

    const float dr_6db = dbToLinear(-6.0f);
    EXPECT_NEAR(VelocityMapping::get(0, dr_6db), 0.50119f, kTolerance);
    EXPECT_NEAR(VelocityMapping::get(1, dr_6db), 0.50426f, kTolerance);
    EXPECT_NEAR(VelocityMapping::get(64, dr_6db), 0.72409f, kTolerance);
    EXPECT_NEAR(VelocityMapping::get(127, dr_6db), 1.0f, kTolerance);

    const float dr_0db = dbToLinear(0.0f);
    EXPECT_NEAR(VelocityMapping::get(0, dr_0db), 1.0f, kTolerance);
    EXPECT_NEAR(VelocityMapping::get(64, dr_0db), 1.0f, kTolerance);
    EXPECT_NEAR(VelocityMapping::get(127, dr_0db), 1.0f, kTolerance);
}

TEST_F(VelocityMappingTest, GetNormalizedMethodWithSpecificDynamicRanges)
{
    const float dr_32db = dbToLinear(-32.0f);
    EXPECT_NEAR(VelocityMapping::getNormalized(0.0f, dr_32db), 0.025119f, kTolerance);
    EXPECT_NEAR(VelocityMapping::getNormalized(0.5f, dr_32db), 0.270128f, kTolerance);
    EXPECT_NEAR(VelocityMapping::getNormalized(1.0f, dr_32db), 1.0f, kTolerance);

    const float dr_6db = dbToLinear(-6.0f);
    EXPECT_NEAR(VelocityMapping::getNormalized(0.0f, dr_6db), 0.501187f, kTolerance);
    EXPECT_NEAR(VelocityMapping::getNormalized(0.5f, dr_6db), 0.72213f, kTolerance);
    EXPECT_NEAR(VelocityMapping::getNormalized(1.0f, dr_6db), 1.0f, kTolerance);

    const float dr_0db = dbToLinear(0.0f);
    EXPECT_NEAR(VelocityMapping::getNormalized(0.0f, dr_0db), 1.0f, kTolerance);
    EXPECT_NEAR(VelocityMapping::getNormalized(0.5f, dr_0db), 1.0f, kTolerance);
    EXPECT_NEAR(VelocityMapping::getNormalized(1.0f, dr_0db), 1.0f, kTolerance);
}

TEST_F(VelocityMappingTest, VelocityRangeClamping)
{
    const float dr_6db = dbToLinear(-6.0f);

    EXPECT_NEAR(VelocityMapping::get(0, dr_6db), .501187f, kTolerance);
    EXPECT_NEAR(VelocityMapping::get(127, dr_6db), 1.0f, kTolerance);

    EXPECT_GE(VelocityMapping::get(0, dr_6db), 0.0f);
    EXPECT_LE(VelocityMapping::get(128, dr_6db), 1.0f);
}

TEST_F(VelocityMappingTest, MonotonicBehavior)
{
    const float dr_6db = dbToLinear(-6.0f);

    float prev_result = VelocityMapping::get(0, dr_6db);
    for (uint8_t velocity = 1; velocity <= 127; ++velocity)
    {
        float current_result = VelocityMapping::get(velocity, dr_6db);
        EXPECT_GE(current_result, prev_result) << "Non-monotonic behavior at velocity " << static_cast<int>(velocity);
        prev_result = current_result;
    }
}

TEST_F(VelocityMappingTest, NormalizedInputRangeClamping)
{
    const float dr_6db = dbToLinear(-6.0f);

    EXPECT_GE(VelocityMapping::getNormalized(0.0f, dr_6db), 0.0f);
    EXPECT_LE(VelocityMapping::getNormalized(1.0f, dr_6db), 1.0f);

    EXPECT_NO_THROW(VelocityMapping::getNormalized(-0.1f, dr_6db));
    EXPECT_NO_THROW(VelocityMapping::getNormalized(1.1f, dr_6db));
}

TEST_F(VelocityMappingTest, ZeroDynamicRange)
{
    constexpr float dr_0db = 1.0f;

    EXPECT_NEAR(VelocityMapping::get(0, dr_0db), 1.0f, kTolerance);
    EXPECT_NEAR(VelocityMapping::get(64, dr_0db), 1.0f, kTolerance);
    EXPECT_NEAR(VelocityMapping::get(127, dr_0db), 1.0f, kTolerance);

    EXPECT_NEAR(VelocityMapping::getNormalized(0.0f, dr_0db), 1.0f, kTolerance);
    EXPECT_NEAR(VelocityMapping::getNormalized(0.5f, dr_0db), 1.0f, kTolerance);
    EXPECT_NEAR(VelocityMapping::getNormalized(1.0f, dr_0db), 1.0f, kTolerance);
}

}