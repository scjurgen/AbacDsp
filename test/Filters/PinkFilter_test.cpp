#include "Filters/PinkFilter.h"
#include "Analysis/FftSmall.h"

#include "gtest/gtest.h"
#include "gmock/gmock.h"
namespace AbacDsp::Test
{
TEST(PinkFilterTest, staysInRange_FastPink)
{
    std::minstd_rand generator(4242);
    std::uniform_real_distribution<float> dist(-1.f, 1.f);
    AbacDsp::PinkFilter<> sut{};
    auto minValue = sut.step(dist(generator));
    auto maxValue = sut.step(dist(generator));
    for (size_t i = 0; i < 1'000'000; ++i)
    {
        const auto value = sut.step(dist(generator));
        maxValue = std::max(value, maxValue);
        minValue = std::min(value, minValue);
    }
    EXPECT_THAT(maxValue, ::testing::AllOf(::testing::Ge(0.3f), ::testing::Le(1.f)));
    EXPECT_THAT(minValue, ::testing::AllOf(::testing::Ge(-1.f), ::testing::Le(-0.3f)));
}

TEST(PinkFilterTest, plausibleSpectrum_FastPink)
{
    std::minstd_rand generator(4242);
    std::uniform_real_distribution<float> dist(-1.f, 1.f);
    AbacDsp::PinkFilter<> sut{};
    std::vector<float> sutBuffer(100000);
    std::generate(sutBuffer.begin(), sutBuffer.end(), [&]() mutable { return sut.step(dist(generator)); });
    constexpr size_t windowSize{128};
    const auto fftResult = FFTResponse::analyse<1, 0>(sutBuffer, windowSize);
    const std::vector<float> expected{
        0.f,       -2.05713f, -6.04291f, -8.45103f, -9.69294f, -10.4657f, -11.0895f, -11.7058f, -12.1761f,
        -12.536f,  -12.8592f, -13.1932f, -13.5425f, -13.8928f, -14.1284f, -14.3914f, -14.7005f, -15.0844f,
        -15.5118f, -15.8645f, -16.1236f, -16.382f,  -16.6037f, -16.765f,  -16.9764f, -17.2594f, -17.499f,
        -17.7181f, -17.9037f, -17.9941f, -18.0828f, -18.2727f, -18.4286f, -18.5392f, -18.6808f, -18.877f,
        -19.035f,  -19.1084f, -19.2124f, -19.3161f, -19.4365f, -19.5449f, -19.6184f, -19.6395f, -19.6964f,
        -19.8143f, -19.931f,  -19.9724f, -20.083f,  -20.2304f, -20.291f,  -20.2818f, -20.2856f, -20.3414f,
        -20.4215f, -20.497f,  -20.4922f, -20.445f,  -20.4046f, -20.3952f, -20.4277f, -20.4444f, -20.7211f,
    };

    for (size_t i = 1; i < fftResult.size(); ++i)
    {
        EXPECT_NEAR(std::log10(fftResult[i]) * 20 / expected[i], 1.f, 1E-4f) << "failed at index " << i;
    }
}

//-------------------------------------------------------------------------------
// Tests for the 7-pole “accurate” variant
//-------------------------------------------------------------------------------

TEST(PinkFilterTest, staysInRange_AccuratePink)
{
    std::minstd_rand generator(4242);
    std::uniform_real_distribution<float> dist(-1.f, 1.f);
    AbacDsp::PinkFilter<false> sut{};
    auto minValue = sut.step(dist(generator));
    auto maxValue = sut.step(dist(generator));
    for (size_t i = 0; i < 1'000'000; ++i)
    {
        const auto value = sut.step(dist(generator));
        maxValue = std::max(value, maxValue);
        minValue = std::min(value, minValue);
    }
    EXPECT_THAT(maxValue, ::testing::AllOf(::testing::Ge(0.3f), ::testing::Le(1.f)));
    EXPECT_THAT(minValue, ::testing::AllOf(::testing::Ge(-1.f), ::testing::Le(-0.3f)));
}

TEST(PinkFilterTest, plausibleSpectrum_AccuratePink)
{
    std::minstd_rand generator(4242);
    std::uniform_real_distribution<float> dist(-1.f, 1.f);
    AbacDsp::PinkFilter<false> sut{};
    std::vector<float> sutBuffer(100000);
    std::generate(sutBuffer.begin(), sutBuffer.end(), [&]() mutable { return sut.step(dist(generator)); });
    constexpr size_t windowSize{128};
    const auto fftResult = FFTResponse::analyse<1, 0>(sutBuffer, windowSize);
    const std::vector<float> expected{
        0.f,       -2.10431f, -6.1664f,  -8.50383f, -9.82274f, -10.7646f, -11.5466f, -12.2876f, -12.8418f,
        -13.2501f, -13.5912f, -13.9186f, -14.2433f, -14.5566f, -14.7441f, -14.9536f, -15.2036f, -15.5253f,
        -15.8813f, -16.1606f, -16.3451f, -16.5284f, -16.6748f, -16.7624f, -16.9034f, -17.1161f, -17.2831f,
        -17.4304f, -17.5477f, -17.5714f, -17.5957f, -17.7235f, -17.817f,  -17.8702f, -17.9567f, -18.0974f,
        -18.2013f, -18.2239f, -18.2804f, -18.3376f, -18.4139f, -18.4812f, -18.5145f, -18.4977f, -18.5197f,
        -18.6039f, -18.6882f, -18.7003f, -18.783f,  -18.9023f, -18.9387f, -18.9082f, -18.8916f, -18.9304f,
        -18.9936f, -19.052f,  -19.034f,  -18.9751f, -18.9252f, -18.9089f, -18.9367f, -18.9493f, -19.2237f,
    };

    for (size_t i = 1; i < fftResult.size(); ++i)
    {
        EXPECT_NEAR(std::log10(fftResult[i]) * 20 / expected[i], 1.f, 1E-4f) << "failed at index " << i;
    }
}
}