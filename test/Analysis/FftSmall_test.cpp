#include "gtest/gtest.h"
#include "Analysis/FftSmall.h"

namespace AbacDsp::Test
{

constexpr auto kEpsilon{1e-4f};

struct WindowTestData
{
    std::string m_windowName;
    std::function<void(const std::vector<float>&, std::vector<float>&)> m_fftFunction;
    std::vector<float> m_expectedMagnitudes;
};

class BasicFFTParameterizedTest : public ::testing::TestWithParam<WindowTestData>
{
  protected:
    void SetUp() override
    {
        // Create test signal - same as original tests
        m_testSignal.resize(32, 0.0f);
        m_testSignal[5] = 0.5f;
        m_testSignal[6] = 1.0f;
        m_testSignal[7] = 0.5f;
    }

    std::vector<float> m_testSignal;
};

TEST_P(BasicFFTParameterizedTest, WindowMagnitudes)
{
    const auto& testData = GetParam();
    std::vector<float> magnitude;

    testData.m_fftFunction(m_testSignal, magnitude);

    ASSERT_EQ(magnitude.size(), 16);
    for (size_t i = 0; i < magnitude.size(); ++i)
    {
        EXPECT_NEAR(magnitude[i], testData.m_expectedMagnitudes[i], kEpsilon)
            << "Window: " << testData.m_windowName << ", Index: " << i;
    }
}

INSTANTIATE_TEST_SUITE_P(
    AllWindows, BasicFFTParameterizedTest,
    ::testing::Values(
        WindowTestData{"HannWindow",
                       [](const std::vector<float>& in, std::vector<float>& out)
                       { BasicFFT::realDataToMagnitude<float, FftHannWindow>(in, out); },
                       {0.656249f, 0.650171f, 0.63217f, 0.602935f, 0.563589f, 0.515638f, 0.460919f, 0.401525f,
                        0.339722f, 0.27786f, 0.218274f, 0.163174f, 0.114527f, 0.0738796f, 0.0420634f, 0.0186227f}},
        WindowTestData{"FlatTopWindow",
                       [](const std::vector<float>& in, std::vector<float>& out)
                       { BasicFFT::realDataToMagnitude<float, FftFlatTopWindow>(in, out); },
                       {0.579226f, 0.574045f, 0.558698f, 0.533775f, 0.500229f, 0.459344f, 0.412683f, 0.362025f,
                        0.309299f, 0.256502f, 0.205613f, 0.158512f, 0.116877f, 0.0820985f, 0.0552406f, 0.0373709f}},
        WindowTestData{"BlackmanWindow",
                       [](const std::vector<float>& in, std::vector<float>& out)
                       { BasicFFT::realDataToMagnitude<float, FftBlackmanWindow>(in, out); },
                       {0.379767f, 0.376313f, 0.366083f, 0.349468f, 0.327104f, 0.299846f, 0.268735f, 0.234956f,
                        0.19979f, 0.164565f, 0.130592f, 0.0991037f, 0.0711803f, 0.0476475f, 0.0289495f, 0.0151654f}},
        WindowTestData{"RectangularWindow",
                       [](const std::vector<float>& in, std::vector<float>& out)
                       { BasicFFT::realDataToMagnitude<float, FftRectangularWindow>(in, out); },
                       {2.0f, 1.98079f, 1.92388f, 1.83147f, 1.70711f, 1.55557f, 1.38268f, 1.19509f, 1.0f, 0.80491f,
                        0.617317f, 0.44443f, 0.292893f, 0.16853f, 0.0761205f, 0.0192147f}}),
    [](const ::testing::TestParamInfo<WindowTestData>& info) { return info.param.m_windowName; });


class BasicFFTTest : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        m_testSignal.resize(32, 0.0f);
        m_testSignal[5] = 0.5f;
        m_testSignal[6] = 1.0f;
        m_testSignal[7] = 0.5f;
    }

    std::vector<float> m_testSignal;
};

TEST_F(BasicFFTTest, LegacyInterfaceWithFlatTop)
{
    std::vector<float> legacyMagnitude;
    std::vector<float> newMagnitude;

    BasicFFT::realDataToMagnitude(m_testSignal, legacyMagnitude, true);
    BasicFFT::realDataToMagnitude<float, FftFlatTopWindow>(m_testSignal, newMagnitude);

    ASSERT_EQ(legacyMagnitude.size(), newMagnitude.size());
    for (size_t i = 0; i < legacyMagnitude.size(); ++i)
    {
        EXPECT_NEAR(legacyMagnitude[i], newMagnitude[i], kEpsilon);
    }
}

TEST_F(BasicFFTTest, LegacyInterfaceWithHann)
{
    std::vector<float> legacyMagnitude;
    std::vector<float> newMagnitude;

    BasicFFT::realDataToMagnitude(m_testSignal, legacyMagnitude, false);
    BasicFFT::realDataToMagnitude<float, FftHannWindow>(m_testSignal, newMagnitude);

    ASSERT_EQ(legacyMagnitude.size(), newMagnitude.size());
    for (size_t i = 0; i < legacyMagnitude.size(); ++i)
    {
        EXPECT_NEAR(legacyMagnitude[i], newMagnitude[i], kEpsilon);
    }
}

TEST_F(BasicFFTTest, DefaultWindowIsFlatTop)
{
    std::vector<float> defaultMagnitude;
    std::vector<float> flatTopMagnitude;

    BasicFFT::realDataToMagnitude(m_testSignal, defaultMagnitude);
    BasicFFT::realDataToMagnitude<float, FftFlatTopWindow>(m_testSignal, flatTopMagnitude);

    ASSERT_EQ(defaultMagnitude.size(), flatTopMagnitude.size());
    for (size_t i = 0; i < defaultMagnitude.size(); ++i)
    {
        EXPECT_NEAR(defaultMagnitude[i], flatTopMagnitude[i], kEpsilon);
    }
}

TEST_F(BasicFFTTest, InvalidSizeHandling)
{
    std::vector<float> oddSizeSignal(31, 1.0f);
    std::vector<float> magnitude;

    testing::internal::CaptureStderr();
    BasicFFT::realDataToMagnitude(oddSizeSignal, magnitude);
    const std::string error_output = testing::internal::GetCapturedStderr();

    EXPECT_TRUE(error_output.find("array size must be multiple of 2^n") != std::string::npos);
    EXPECT_TRUE(magnitude.empty());
}

TEST_F(BasicFFTTest, EmptyInputHandling)
{
    std::vector<float> emptySignal;
    std::vector<float> magnitude;

    testing::internal::CaptureStderr();
    BasicFFT::realDataToMagnitude(emptySignal, magnitude);
    const std::string error_output = testing::internal::GetCapturedStderr();

    EXPECT_TRUE(error_output.find("array size must be multiple of 2^n") != std::string::npos);
    EXPECT_TRUE(magnitude.empty());
}

}