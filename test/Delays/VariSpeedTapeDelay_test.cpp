#include "gtest/gtest.h"

#include <cmath>
#include "Delays/VariSpeedTapeDelay.h"

TEST(UpsampleSincTest, testConversion)
{
    constexpr size_t OverSample{8};
    constexpr size_t Slices{3};
    AbacDsp::UpSample<OverSample, Slices> sut{48000.f};
    std::vector signal = {1.f, -1.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f};
    for (size_t j = 0; j < signal.size(); ++j)
    {
        sut.feed(signal[j]);
        for (size_t i = 0; i < OverSample; ++i)
        {
            const auto f = static_cast<float>(i);
            std::cout << j * OverSample + i << "\t" << sut.readDelayed<4>(16 - f) << std::endl;
        }
    }
}


class TestableTapeDelay : public AbacDsp::VariSpeedTapeDelay<100>
{
public:
    using AbacDsp::VariSpeedTapeDelay<100>::VariSpeedTapeDelay; // inherit constructor

    [[nodiscard]] const std::vector<float>& buffer() const
    {
        return m_buffer;
    }
};

TEST(VariSpeedTapeDelayTests, advance1WithFrac0p25)
{
    constexpr float epsilon{1E-6f};
    TestableTapeDelay sut{48000.f};
    sut.setGlobalAdvance(0.25f);
    EXPECT_FLOAT_EQ(sut.step(0.f), 0.f);
    sut.setGlobalAdvance(1.f);
    sut.step(1.f);
    sut.step(-1.f);
    for (size_t j = 0; j < 50; ++j)
    {
        sut.step(0.f);
    }

    EXPECT_FLOAT_EQ(sut.buffer()[10], 0.f);
    EXPECT_GT(sut.buffer()[13], 0.8f);
    EXPECT_LT(sut.buffer()[14], -.7f);
    EXPECT_NEAR(sut.buffer()[16], 0.f, 1E-3f);
    EXPECT_NEAR(sut.buffer()[17], 0.f, 1E-3f);
}

//
// TEST(VariSpeedTapeDelayTests, advance1)
// {
//     constexpr float epsilon{1E-6f};
//     TestableTapeDelay sut{48000.f};
//     sut.setGlobalAdvance(1.f);
//     // sut.setFeedFactor(1.f);
//     // sut.setHoldFactor(0.f);
//     // sut.setFeedbackFactor(0.f);
//     EXPECT_FLOAT_EQ(sut.step(1.f), 0.f);
//     EXPECT_FLOAT_EQ(sut.step(-1.f), 0.f);
//     EXPECT_FLOAT_EQ(sut.step(0.f), 0.f);
//     EXPECT_FLOAT_EQ(sut.step(0.f), 0.f);
//     EXPECT_FLOAT_EQ(sut.step(0.f), 0.f);
//     EXPECT_FLOAT_EQ(sut.step(0.f), 0.f);
//     EXPECT_FLOAT_EQ(sut.step(0.f), 0.f);
//     EXPECT_FLOAT_EQ(sut.buffer()[10], 0.f);
//     EXPECT_GT(sut.buffer()[13], 0.8f);
//     EXPECT_LT(sut.buffer()[14], -.7f);
//     EXPECT_NEAR(sut.buffer()[16], 0.f, 1E-3f);
//     EXPECT_NEAR(sut.buffer()[17], 0.f, 1E-3f);
// }
//
// TEST(VariSpeedTapeDelayTests, advance3)
// {
//     constexpr float epsilon{1E-6f};
//     TestableTapeDelay sut{48000.f};
//     sut.setGlobalAdvance(3.f);
//     // sut.setFeedFactor(1.f);
//     // sut.setHoldFactor(0.f);
//     // sut.setFeedbackFactor(0.f);
//     EXPECT_FLOAT_EQ(sut.step(1.f), 0.f);
//     EXPECT_FLOAT_EQ(sut.step(-1.f), 0.f);
//     EXPECT_FLOAT_EQ(sut.step(0.f), 0.f);
//     EXPECT_FLOAT_EQ(sut.step(0.f), 0.f);
//     EXPECT_FLOAT_EQ(sut.step(0.f), 0.f);
// }
//
//
// TEST(VariSpeedTapeDelayTests, advance05)
// {
//     constexpr float epsilon{1E-6f};
//     TestableTapeDelay sut{48000.f};
//     sut.setGlobalAdvance(.5f);
//     // sut.setFeedFactor(1.f);
//     // sut.setHoldFactor(0.f);
//     // sut.setFeedbackFactor(0.f);
//     EXPECT_FLOAT_EQ(sut.step(1.f), 0.f);
//     EXPECT_FLOAT_EQ(sut.step(-1.f), 0.f);
//     EXPECT_FLOAT_EQ(sut.step(0.f), 0.f);
//     EXPECT_FLOAT_EQ(sut.step(0.f), 0.f);
//     EXPECT_FLOAT_EQ(sut.step(0.f), 0.f);
// }
//
//
// TEST(VariSpeedTapeDelayTests, AdvanceTransition099To101)
// {
//     constexpr float epsilon{1E-4f};
//     TestableTapeDelay sut{48000.f};
//     // sut.setFeedFactor(1.f);
//     // sut.setHoldFactor(0.f);
//     // sut.setFeedbackFactor(0.f);
//     sut.setGlobalAdvance(0.89f);
//     for (int i = 0; i < 10; ++i)
//     {
//         sut.step(1.f);
//     }
//     sut.setGlobalAdvance(1.12f);
//     for (int i = 0; i < 12; ++i)
//     {
//         sut.step(2.f);
//     }
//     sut.setGlobalAdvance(0.87f);
//     for (int i = 0; i < 12; ++i)
//     {
//         sut.step(3.f);
//     }
//     for (size_t i = 12; i < 32; ++i)
//     {
//         EXPECT_GT(sut.buffer()[i], 0.98f) << "Buffer mismatch at i=" << i;
//     }
// }