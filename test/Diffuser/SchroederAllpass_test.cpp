#include <gtest/gtest.h>

#include "Diffuser/SchroederAllpass.h"

namespace AbacDsp::Test
{

class SchroederAllpassTest : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        m_sut.setGain(0.5f);
        m_sut.setSize(23);
    }

    void expectZeroOutputs(const size_t count)
    {
        for (size_t i = 0; i < count; ++i)
        {
            EXPECT_FLOAT_EQ(m_sut.step(0.f), 0.f);
        }
    }
    SchroederAllPass<1000, 16> m_sut;
};

TEST_F(SchroederAllpassTest, checkNoDecay)
{
    m_sut.setGain(0.f);
    m_sut.setSize(1);
    // causes latency of 1
    EXPECT_FLOAT_EQ(m_sut.step(1.f), 0);
    EXPECT_FLOAT_EQ(m_sut.step(0.f), 1);
    for (size_t i = 0; i < 10; ++i)
    {
        EXPECT_FLOAT_EQ(m_sut.step(0.f), 0);
    }
}

TEST_F(SchroederAllpassTest, checkDecayHalf)
{
    const std::vector<float> expectedValues = {-0.5f, 0.75f, 0.375f, 0.1875f, 0.09375f};

    EXPECT_FLOAT_EQ(m_sut.step(1.f), expectedValues[0]);

    for (size_t i = 1; i < expectedValues.size(); ++i)
    {
        expectZeroOutputs(22);
        EXPECT_FLOAT_EQ(m_sut.step(0.f), expectedValues[i]);
    }
}


TEST_F(SchroederAllpassTest, checkDecaySlow)
{
    const std::vector<float> expectedValues = {
        -0.75f,    0.4375f,    0.328125f,  0.246094f,  0.18457f,   0.138428f,
        0.103821f, 0.0778656f, 0.0583992f, 0.0437994f, 0.0328496f,
    };
    m_sut.setGain(0.75f);
    EXPECT_FLOAT_EQ(m_sut.step(1.f), expectedValues[0]);

    for (size_t i = 1; i < expectedValues.size(); ++i)
    {
        expectZeroOutputs(22);
        EXPECT_NEAR(m_sut.step(0.f), expectedValues[i], 1E-6f);
    }
}

class SchroederAllPassSoftTransitionTest : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        m_sut.setGain(0.5f);
        m_sut.setSize(23);
        expectZeroOutputs(10000);
    }

    void expectZeroOutputs(const size_t count)
    {
        for (size_t i = 0; i < count; ++i)
        {
            EXPECT_FLOAT_EQ(m_sut.step(0.f), 0.f) << "failed at " << i;
        }
    }
    SchroederAllPassSoftTransition<1000, 16> m_sut{48000.f};
};

TEST_F(SchroederAllPassSoftTransitionTest, checkDecayHalf)
{
    const std::vector<float> expectedValues = {-0.5f, 0.75f, 0.375f, 0.1875f, 0.09375f};

    EXPECT_FLOAT_EQ(m_sut.step(1.f), expectedValues[0]);

    for (size_t i = 1; i < expectedValues.size(); ++i)
    {
        expectZeroOutputs(22);
        EXPECT_FLOAT_EQ(m_sut.step(0.f), expectedValues[i]);
    }
}

TEST_F(SchroederAllPassSoftTransitionTest, checkDecaySlow)
{
    const std::vector<float> expectedValues = {
        -0.75f,    0.4375f,    0.328125f,  0.246094f,  0.18457f,   0.138428f,
        0.103821f, 0.0778656f, 0.0583992f, 0.0437994f, 0.0328496f,
    };
    m_sut.setGain(0.75f);
    EXPECT_FLOAT_EQ(m_sut.step(1.f), expectedValues[0]);

    for (size_t i = 1; i < expectedValues.size(); ++i)
    {
        expectZeroOutputs(22);
        EXPECT_NEAR(m_sut.step(0.f), expectedValues[i], 1E-6f);
    }
}

TEST_F(SchroederAllPassSoftTransitionTest, checkNoDecay)
{
    m_sut.setGain(0.f);
    m_sut.setSize(1);
    expectZeroOutputs(256); // it takes last size * 2 steps to settle but at least 256
    // causes latency of 1
    EXPECT_FLOAT_EQ(m_sut.step(1.f), 0);
    EXPECT_FLOAT_EQ(m_sut.step(0.f), 1);
    for (size_t i = 0; i < 10; ++i)
    {
        EXPECT_FLOAT_EQ(m_sut.step(0.f), 0);
    }
}

}
