#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "Parameters/LinearParameter.h"

namespace AbacDsp::Test
{
class LinearParameterTest : public ::testing::Test
{
  protected:
    LinearParameter sut;

    void SetUp() override
    {
        sut.setMin(0.0f);
        sut.setMax(100.0f);
        sut.setSampleRate(96000.0f);
        sut.setMoniker("TestParam");
        sut.setTransitionTime(1.0f); // 1 second transition time
    }
};

TEST_F(LinearParameterTest, InitialValue)
{
    EXPECT_FLOAT_EQ(0.0f, sut.getValue());
}

TEST_F(LinearParameterTest, SetValue)
{
    sut.setValue(50.0f);
    for (int i = 0; i < 96000; ++i)
    {
        sut.tick();
    }
    EXPECT_FLOAT_EQ(50.0f, sut.getValue());
}

TEST_F(LinearParameterTest, SetValueOutOfRange)
{
    sut.setValue(150.0f);
    for (int i = 0; i < 96000; ++i)
    {
        sut.tick();
    }
    EXPECT_FLOAT_EQ(100.0f, sut.getValue());

    sut.setValue(-10.0f);
    for (int i = 0; i < 96000; ++i)
    {
        sut.tick();
    }
    EXPECT_FLOAT_EQ(0.0f, sut.getValue());
}

TEST_F(LinearParameterTest, Smoothing)
{
    sut.setValue(100.0f);
    float expectedStep = 100.0f / (96000.0f * 1.0f);
    for (int i = 0; i < 100; ++i)
    {
        sut.tick();
    }
    EXPECT_NEAR(100 * expectedStep, sut.getValue(), 0.001f);
}

TEST_F(LinearParameterTest, SmoothingWithHalf)
{
    sut.setTransitionTime(1.f);
    sut.forceValue(99.f);
    sut.setValue(100.0f - 0.0005f);
    for (int i = 0; i < 1000; ++i)
    {
        sut.tick();
    }
    for (int i = 0; i < 100; ++i)
    {
        sut.tick();
    }
    EXPECT_NEAR(100.f - 0.0005f, sut.getValue(), 0.001f);
}

TEST_F(LinearParameterTest, Moniker)
{
    EXPECT_EQ("TestParam", sut.getMoniker());

    sut.setMoniker("NewMoniker");
    EXPECT_EQ("NewMoniker", sut.getMoniker());
}

class LinearParameterTransitionTest : public LinearParameterTest, public ::testing::WithParamInterface<float>
{
};

TEST_P(LinearParameterTransitionTest, TransitionTime)
{
    float transitionTime = GetParam();
    sut.setTransitionTime(transitionTime);
    sut.setValue(100.0f);

    int numSamples = static_cast<int>(96000.0f * transitionTime);
    for (int i = 0; i < numSamples; ++i)
    {
        sut.tick();
    }

    EXPECT_NEAR(100.0f, sut.getValue(), 0.1f);
}

INSTANTIATE_TEST_SUITE_P(TransitionTimes, LinearParameterTransitionTest, ::testing::Values(0.5f, 1.0f, 2.0f));

class LinearSmoothingParameterTest : public ::testing::Test
{
  protected:
    LinearSmoothingParameter<4> sut;
};

TEST_F(LinearSmoothingParameterTest, InitialValueIsZero)
{
    EXPECT_FLOAT_EQ(0.0f, sut.getValue(0));
}

TEST_F(LinearSmoothingParameterTest, SetAndGetMoniker)
{
    sut.setMoniker("test");
    EXPECT_EQ("test", sut.getMoniker());
}

TEST_F(LinearSmoothingParameterTest, ForceValueClamps)
{
    sut.setMin(-1.0f);
    sut.setMax(1.0f);
    sut.forceValue(2.0f);
    EXPECT_FLOAT_EQ(1.0f, sut.getValue(0));
    sut.forceValue(-2.0f);
    EXPECT_FLOAT_EQ(-1.0f, sut.getValue(0));
}

TEST_F(LinearSmoothingParameterTest, SetValueSmooths)
{
    sut.setValue(1.0f);
    EXPECT_FLOAT_EQ(0.25f, sut.getValue(0));
    EXPECT_FLOAT_EQ(0.5f, sut.getValue(1));
    EXPECT_FLOAT_EQ(0.75f, sut.getValue(2));
    EXPECT_FLOAT_EQ(1.0f, sut.getValue(3));
}

TEST_F(LinearSmoothingParameterTest, SetValueClamps)
{
    sut.setMin(0.0f);
    sut.setMax(1.0f);
    sut.setValue(2.0f);
    EXPECT_FLOAT_EQ(1.0f, sut.getValue(3));
    sut.setValue(-1.0f);
    EXPECT_FLOAT_EQ(0.0f, sut.getValue(3));
}

TEST_F(LinearSmoothingParameterTest, SetValueNoChangeDoesNotSmooth)
{
    sut.setValue(1.0f);
    sut.setValue(1.0f);
    EXPECT_FLOAT_EQ(1.0f, sut.getValue(0));
    EXPECT_FLOAT_EQ(1.0f, sut.getValue(1));
    EXPECT_FLOAT_EQ(1.0f, sut.getValue(2));
    EXPECT_FLOAT_EQ(1.0f, sut.getValue(3));
}
}