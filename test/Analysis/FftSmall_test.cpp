#include "gtest/gtest.h"
#include "Analysis/FftMisc.h"

#include <cmath>
#include <numbers>
#include <vector>

namespace AbacDsp::Test
{

constexpr auto kEpsilon{1e-4f};

static std::vector<float> makeSine(size_t N, float freq, float sampleRate)
{
    std::vector<float> sig(N);
    std::ranges::generate(
        sig, [n = 0, freq, sampleRate]() mutable
        { return std::sin(2.f * std::numbers::pi_v<float> * freq * static_cast<float>(n++) / sampleRate); });
    return sig;
}

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
        EXPECT_NEAR(magnitude[i], testData.m_expectedMagnitudes[i], kEpsilon)
            << "Window: " << testData.m_windowName << ", Index: " << i;
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
        EXPECT_NEAR(legacyMagnitude[i], newMagnitude[i], kEpsilon);
}

TEST_F(BasicFFTTest, LegacyInterfaceWithHann)
{
    std::vector<float> legacyMagnitude;
    std::vector<float> newMagnitude;

    BasicFFT::realDataToMagnitude(m_testSignal, legacyMagnitude, false);
    BasicFFT::realDataToMagnitude<float, FftHannWindow>(m_testSignal, newMagnitude);

    ASSERT_EQ(legacyMagnitude.size(), newMagnitude.size());
    for (size_t i = 0; i < legacyMagnitude.size(); ++i)
        EXPECT_NEAR(legacyMagnitude[i], newMagnitude[i], kEpsilon);
}

TEST_F(BasicFFTTest, DefaultWindowIsFlatTop)
{
    std::vector<float> defaultMagnitude;
    std::vector<float> flatTopMagnitude;

    BasicFFT::realDataToMagnitude(m_testSignal, defaultMagnitude);
    BasicFFT::realDataToMagnitude<float, FftFlatTopWindow>(m_testSignal, flatTopMagnitude);

    ASSERT_EQ(defaultMagnitude.size(), flatTopMagnitude.size());
    for (size_t i = 0; i < defaultMagnitude.size(); ++i)
        EXPECT_NEAR(defaultMagnitude[i], flatTopMagnitude[i], kEpsilon);
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

// FFTResponse

TEST(FFTResponseTest, GenerateNoiseSignalSize)
{
    EXPECT_EQ(FFTResponse::generateNoiseSignal(1024).size(), 1024u);
}

TEST(FFTResponseTest, GenerateNoiseSignalRange)
{
    for (float v : FFTResponse::generateNoiseSignal(1024))
    {
        EXPECT_GE(v, -1.f);
        EXPECT_LE(v, 1.f);
    }
}

TEST(FFTResponseTest, GenerateNoiseSignalDeterministic)
{
    EXPECT_EQ(FFTResponse::generateNoiseSignal(256), FFTResponse::generateNoiseSignal(256));
}
TEST(FFTResponseTest, AnalyseOutputSize)
{
    const auto buf = FFTResponse::generateNoiseSignal(8192);
    const auto result = FFTResponse::analyse<1, 0>(buf, 512);
    EXPECT_EQ(result.size(), 512u / 2 - 1);
}

TEST(FFTResponseTest, AnalyseNormalized)
{
    const auto buf = FFTResponse::generateNoiseSignal(8192);
    const auto result = FFTResponse::analyse<1, 0>(buf, 512);
    EXPECT_NEAR(*std::max_element(result.begin(), result.end()), 1.f, kEpsilon);
}

TEST(FFTResponseTest, AnalyseInterleavedChannelOffset)
{
    const auto mono = FFTResponse::generateNoiseSignal(4096);
    std::vector stereo(mono.size() * 2, 0.f);
    for (size_t i = 0; i < mono.size(); ++i)
    {
        stereo[i * 2] = mono[i];
    }

    const auto ch1 = FFTResponse::analyse<2, 1>(stereo, 512);
    for (float v : ch1)
    {
        EXPECT_NEAR(v, 0.f, 1e-6f);
    }
}

TEST(FFTResponseTest, ProcessFrequenciesBasic)
{
    const std::vector freq = {0.f, 100.f, 200.f, 300.f, 0.f};
    const std::vector bins = {0.f, 0.2f, 0.8f, 0.5f, 0.f};
    const auto slice = FFTResponse::processFrequencies(freq, bins, 50.f);
    EXPECT_FALSE(slice.bins.empty());
    EXPECT_NEAR(slice.maxValue, 0.8f, 1e-5f);
    EXPECT_NEAR(slice.minValue, 0.2f, 1e-5f);
    EXPECT_NEAR(slice.frequencies.front(), 100.f, 1e-5f);
}

TEST(FFTResponseTest, ProcessFrequenciesEmptyWhenAllBelowMin)
{
    const std::vector<float> freq = {10.f, 20.f, 30.f};
    const std::vector<float> bins = {0.1f, 0.2f, 0.3f};
    EXPECT_TRUE(FFTResponse::processFrequencies(freq, bins, 50.f).bins.empty());
}

TEST(FFTResponseTest, AnalyseByOctaveBinsNormalized)
{
    const auto buf = FFTResponse::generateNoiseSignal(16384);
    const auto result = FFTResponse::analyseByOctaveBins<1, 0>(buf, 1024, 48000.f, 3);
    EXPECT_FALSE(result.bins.empty());
    EXPECT_NEAR(result.maxValue, 1.f, kEpsilon);
}

TEST(FFTResponseTest, AnalyseByOctaveBinsStartFreqFilters)
{
    const auto buf = FFTResponse::generateNoiseSignal(16384);
    const auto full = FFTResponse::analyseByOctaveBins<1, 0>(buf, 1024, 48000.f, 3);
    const auto trimmed = FFTResponse::analyseByOctaveBins<1, 0>(buf, 1024, 48000.f, 3, 1000.f);
    EXPECT_LT(trimmed.bins.size(), full.bins.size());
    if (!trimmed.frequencies.empty())
        EXPECT_GT(trimmed.frequencies.front(), 1000.f);
}

TEST(FFTResponseTest, AnalyseByNoteBinsSize)
{
    const auto buf = FFTResponse::generateNoiseSignal(16384);
    const auto result = FFTResponse::analyseByNoteBins<1, 0>(buf, 1024, 48000.f);
    EXPECT_EQ(result.size(), 12u);
}

TEST(FFTResponseTest, AnalyseByNoteBinsNormalized)
{
    const auto buf = FFTResponse::generateNoiseSignal(16384);
    const auto result = FFTResponse::analyseByNoteBins<1, 0>(buf, 1024, 48000.f);
    EXPECT_NEAR(*std::max_element(result.begin(), result.end()), 1.f, kEpsilon);
}

// KissFft

TEST(KissFftTest, DCInput)
{
    constexpr size_t N = 32;
    KissFft<float> fwd(N, false);
    std::vector<std::complex<float>> in(N, {1.f, 0.f}), out(N);
    fwd.compute(in.data(), out.data());
    EXPECT_NEAR(out[0].real(), static_cast<float>(N), 1e-3f);
    for (size_t k = 1; k < N; ++k)
        EXPECT_NEAR(std::abs(out[k]), 0.f, 1e-3f);
}

TEST(KissFftTest, ForwardInverseRoundTrip)
{
    constexpr size_t N = 64;
    KissFft<float> fwd(N, false);
    KissFft<float> inv(N, true);
    const auto src = makeSine(N, 440.f, static_cast<float>(N));
    std::vector<std::complex<float>> in(N), mid(N), out(N);
    for (size_t i = 0; i < N; ++i)
        in[i] = {src[i], 0.f};
    fwd.compute(in.data(), mid.data());
    inv.compute(mid.data(), out.data());
    for (size_t i = 0; i < N; ++i)
        EXPECT_NEAR(out[i].real() / static_cast<float>(N), src[i], kEpsilon);
}

TEST(KissFftTest, Resize)
{
    KissFft<float> fwd(32, false);
    fwd.resize(64);
    constexpr size_t N = 64;
    std::vector<std::complex<float>> in(N, {1.f, 0.f}), out(N);
    fwd.compute(in.data(), out.data());
    EXPECT_NEAR(out[0].real(), static_cast<float>(N), 1e-3f);
}

TEST(KissFftTest, MixedRadixRoundTrip)
{
    constexpr size_t N = 48;
    KissFft<float> fwd(N, false);
    KissFft<float> inv(N, true);
    std::vector<std::complex<float>> in(N), mid(N), out(N);
    for (size_t i = 0; i < N; ++i)
        in[i] = {static_cast<float>(i), 0.f};
    fwd.compute(in.data(), mid.data());
    inv.compute(mid.data(), out.data());
    for (size_t i = 0; i < N; ++i)
        EXPECT_NEAR(out[i].real() / static_cast<float>(N), in[i].real(), 1e-3f);
}


class HannWindowMagnitudesFftTest : public ::testing::Test
{
  protected:
    static constexpr size_t kN = 512;
    HannWindowMagnitudesFft m_fft{kN};
    std::vector<float> m_dst = std::vector<float>(kN / 2);
    ;
};

TEST_F(HannWindowMagnitudesFftTest, PeakBinMatchesBasicFFT)
{
    const auto src = makeSine(kN, 1000.f, 48000.f);
    m_fft.compute(src, m_dst);
    std::vector<float> ref;
    BasicFFT::realDataToMagnitude<float, FftHannWindow>(src, ref);
    EXPECT_EQ(std::distance(m_dst.begin(), std::max_element(m_dst.begin(), m_dst.end())),
              std::distance(ref.begin(), std::max_element(ref.begin(), ref.end())));
}

TEST_F(HannWindowMagnitudesFftTest, ResizeAndRecompute)
{
    constexpr size_t newN = 256;
    m_fft.resize(newN);
    std::vector<float> dst(newN / 2);
    m_fft.compute(makeSine(newN, 1000.f, 48000.f), dst);
    EXPECT_EQ(dst.size(), newN / 2);
}

// WindowedMagnitudesFft

TEST(WindowedMagnitudesFftTest, HannWindowPeakBin)
{
    constexpr size_t N = 512;
    constexpr float sr = 48000.f, freq = 1000.f;
    HannWindowedMagnitudesFft<N> fft;
    std::vector<float> dst(N / 2);
    fft.compute(makeSine(N, freq, sr), dst);
    const size_t expected = static_cast<size_t>(std::round(freq / sr * static_cast<float>(N)));
    EXPECT_EQ(static_cast<size_t>(std::distance(dst.begin(), std::max_element(dst.begin(), dst.end()))), expected);
}

TEST(WindowedMagnitudesFftTest, BlackmanWindowPeakBin)
{
    constexpr size_t N = 512;
    constexpr float sr = 48000.f, freq = 1000.f;
    BlackmanWindowedMagnitudesFft<N> fft;
    std::vector<float> dst(N / 2);
    fft.compute(makeSine(N, freq, sr), dst);
    const size_t expected = static_cast<size_t>(std::round(freq / sr * static_cast<float>(N)));
    EXPECT_EQ(static_cast<size_t>(std::distance(dst.begin(), std::max_element(dst.begin(), dst.end()))), expected);
}

} // namespace AbacDsp::Test
