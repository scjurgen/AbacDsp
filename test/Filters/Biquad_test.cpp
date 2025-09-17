
#include "Filters/Biquad.h"

#include "gtest/gtest.h"
#include "gmock/gmock.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>

namespace AbacDsp::Test
{

constexpr double maxDeltaDb = 0.1; // N.B.: be hard on the quality of Biquads filters, acceptable would be even +-0.5 dB

constexpr double StrongQvalue{10.0};         // absurdly high
constexpr double StandardQValue{0.70710678}; // standard value for -3dB at cutoff
constexpr double WeakQValue(0.2);            // absurdly low


void renderWithSineWave(std::vector<float>& target, const double sampleRate, const double frequency)
{
    double phase = 0.0;
    const double advance = frequency / sampleRate;

    for (size_t frameIdx = 0; frameIdx < target.size(); ++frameIdx)
    {
        target[frameIdx] = static_cast<float>(sin(phase * M_PI * 2));
        phase += advance;
        if (phase > 1.0)
        {
            phase -= 1.0;
        }
    }
}


class TestBiquadFilterSet
{
  public:
    TestBiquadFilterSet(const BiquadFilterType ft_, const double gain_)
        : type(ft_)
        , gain(gain_)
    {
    }

    BiquadFilterType type;
    double gain;
};

static std::array<TestBiquadFilterSet, 10> filterTestSet{{
    {BiquadFilterType::LowPass, 0.0},
    {BiquadFilterType::HighPass, 0.0},
    {BiquadFilterType::BandPass, 0.0},
    {BiquadFilterType::Peak, 6.0},
    {BiquadFilterType::Peak, -6.0},
    {BiquadFilterType::LoShelf, 6.0},
    {BiquadFilterType::LoShelf, -6.0},
    {BiquadFilterType::HiShelf, 6.0},
    {BiquadFilterType::HiShelf, -6.0},
    {BiquadFilterType::AllPass, 0.0},
}};

using FilterCutOffFrequency = double;
using TestFrequency = double;
using TestQFactor = double;

class DspBiquadFilterTests : public testing::TestWithParam<TestBiquadFilterSet>
{
  public:
};

class WrapperOfSpecializedBiquads
{
  public:
    void setType(BiquadFilterType type)
    {
        m_currentFilterType = type;
    }

    void computeCoefficients(BiquadFilterType type, const float sampleRate, const float frequency, const float Q,
                             const float peakGain)
    {
        m_sampleRate = sampleRate;
        m_currentFilterType = type;
        switch (m_currentFilterType)
        {
            case BiquadFilterType::LowPass:
                lp.computeCoefficients(sampleRate, frequency, Q, peakGain);
                break;
            case BiquadFilterType::HighPass:
                hp.computeCoefficients(sampleRate, frequency, Q, peakGain);
                break;
            case BiquadFilterType::BandPass:
                bp.computeCoefficients(sampleRate, frequency, Q, peakGain);
                break;
            case BiquadFilterType::Notch:
                notch.computeCoefficients(sampleRate, frequency, Q, peakGain);
                break;
            case BiquadFilterType::Peak:
                peak.computeCoefficients(sampleRate, frequency, Q, peakGain);
                break;
            case BiquadFilterType::LoShelf:
                ls.computeCoefficients(sampleRate, frequency, Q, peakGain);
                break;
            case BiquadFilterType::HiShelf:
                hs.computeCoefficients(sampleRate, frequency, Q, peakGain);
                break;
            case BiquadFilterType::AllPass:
                ap.computeCoefficients(sampleRate, frequency, Q, peakGain);
                break;

            default:
                break;
        }
    }

    void processBlock(const float* in, float* outBuffer, size_t numSamples)
    {
        switch (m_currentFilterType)
        {
            case BiquadFilterType::LowPass:
                lp.processBlock(in, outBuffer, numSamples);
                break;
            case BiquadFilterType::HighPass:
                hp.processBlock(in, outBuffer, numSamples);
                break;
            case BiquadFilterType::BandPass:
                bp.processBlock(in, outBuffer, numSamples);
                break;
            case BiquadFilterType::Notch:
                notch.processBlock(in, outBuffer, numSamples);
                break;
            case BiquadFilterType::Peak:
                peak.processBlock(in, outBuffer, numSamples);
                break;
            case BiquadFilterType::LoShelf:
                ls.processBlock(in, outBuffer, numSamples);
                break;
            case BiquadFilterType::HiShelf:
                hs.processBlock(in, outBuffer, numSamples);
                break;
            case BiquadFilterType::AllPass:
                ap.processBlock(in, outBuffer, numSamples);
                break;
            default:
                break;
        }
    }

    [[nodiscard]] float magnitudeInDb(const float hz) const
    {
        const auto ratio = hz / m_sampleRate;
        switch (m_currentFilterType)
        {
            case BiquadFilterType::LowPass:
                return lp.magnitudeInDb(ratio);
            case BiquadFilterType::HighPass:
                return hp.magnitudeInDb(ratio);
            case BiquadFilterType::BandPass:
                return bp.magnitudeInDb(ratio);
            case BiquadFilterType::Notch:
                return notch.magnitudeInDb(ratio);
            case BiquadFilterType::Peak:
                return peak.magnitudeInDb(ratio);
            case BiquadFilterType::LoShelf:
                return ls.magnitudeInDb(ratio);
            case BiquadFilterType::HiShelf:
                return hs.magnitudeInDb(ratio);
            case BiquadFilterType::AllPass:
                return ap.magnitudeInDb(ratio);
            default:
                return 0;
        }
    }

    float m_sampleRate{48000.f};
    BiquadFilterType m_currentFilterType;
    Biquad<BiquadFilterType::LowPass> lp;
    Biquad<BiquadFilterType::HighPass> hp;
    Biquad<BiquadFilterType::BandPass> bp;
    Biquad<BiquadFilterType::Notch> notch;
    Biquad<BiquadFilterType::Peak> peak;
    Biquad<BiquadFilterType::LoShelf> ls;
    Biquad<BiquadFilterType::HiShelf> hs;
    Biquad<BiquadFilterType::AllPass> ap;
};

TEST_P(DspBiquadFilterTests, allMagnitudesMono)
{
    const auto ts = GetParam();
    std::vector<float> qvalues{StandardQValue, StrongQvalue, WeakQValue};
    std::vector<float> cfValues{125.f, 2000.f};
    std::vector<float> hzValues{25.f, 200.f, 800.f, 6400.f};

    for (auto qFactor : qvalues)
    {
        for (auto cf : cfValues)
        {
            for (auto hz : hzValues)
            {
                WrapperOfSpecializedBiquads sut;

                constexpr auto sampleRate{48000.0};

                sut.computeCoefficients(ts.type, sampleRate, cf, qFactor, ts.gain);

                std::vector<float> wave{};
                if (qFactor > 0.8)
                {
                    wave.resize(22000); // strong q needs more time to settle due to resonance
                }
                else
                {
                    wave.resize(5000);
                }
                renderWithSineWave(wave, sampleRate, hz);
                sut.processBlock(wave.data(), wave.data(), wave.size());
                const auto [minV, maxV] = std::minmax_element(wave.begin() + wave.size() / 2, wave.end());
                const auto maxValue = std::max(std::abs(*minV), std::abs(*maxV));

                const auto db = std::log10(maxValue) * 20.0f;

                const auto expectedDb = sut.magnitudeInDb(hz);
                EXPECT_NEAR(db, expectedDb, maxDeltaDb) << "failed with cf:" << cf << "\tf:" << hz << "\tQ:" << qFactor;
            }
        }
    }
}


class WrapperOfSpecializedBiquadsStereo
{
  public:
    void setType(BiquadFilterType type) {}

    void computeCoefficients(BiquadFilterType type, const float sampleRate, const float frequency, const float Q,
                             const float peakGain)
    {
        m_sampleRate = sampleRate;
        m_currentFilterType = type;
        switch (m_currentFilterType)
        {
            case BiquadFilterType::LowPass:
                lp.computeCoefficients(sampleRate, frequency, Q, peakGain);
                break;
            case BiquadFilterType::HighPass:
                hp.computeCoefficients(sampleRate, frequency, Q, peakGain);
                break;
            case BiquadFilterType::BandPass:
                bp.computeCoefficients(sampleRate, frequency, Q, peakGain);
                break;
            case BiquadFilterType::Notch:
                notch.computeCoefficients(sampleRate, frequency, Q, peakGain);
                break;
            case BiquadFilterType::Peak:
                peak.computeCoefficients(sampleRate, frequency, Q, peakGain);
                break;
            case BiquadFilterType::LoShelf:
                ls.computeCoefficients(sampleRate, frequency, Q, peakGain);
                break;
            case BiquadFilterType::HiShelf:
                hs.computeCoefficients(sampleRate, frequency, Q, peakGain);
                break;
            case BiquadFilterType::AllPass:
                ap.computeCoefficients(sampleRate, frequency, Q, peakGain);
                break;
            default:
                break;
        }
    }

    void processBlock(const float* left, const float* right, float* outLeft, float* outRight, size_t numSamples)
    {
        switch (m_currentFilterType)
        {
            case BiquadFilterType::LowPass:
                lp.processBlock(left, right, outLeft, outRight, numSamples);
                break;
            case BiquadFilterType::HighPass:
                hp.processBlock(left, right, outLeft, outRight, numSamples);
                break;
            case BiquadFilterType::BandPass:
                bp.processBlock(left, right, outLeft, outRight, numSamples);
                break;
            case BiquadFilterType::Notch:
                notch.processBlock(left, right, outLeft, outRight, numSamples);
                break;
            case BiquadFilterType::Peak:
                peak.processBlock(left, right, outLeft, outRight, numSamples);
                break;
            case BiquadFilterType::LoShelf:
                ls.processBlock(left, right, outLeft, outRight, numSamples);
                break;
            case BiquadFilterType::HiShelf:
                hs.processBlock(left, right, outLeft, outRight, numSamples);
                break;
            case BiquadFilterType::AllPass:
                ap.processBlock(left, right, outLeft, outRight, numSamples);
                break;
            default:
                break;
        }
    }

    [[nodiscard]] float magnitudeInDb(const float hz) const
    {
        auto ratio = hz / m_sampleRate;
        switch (m_currentFilterType)
        {
            case BiquadFilterType::LowPass:
                return lp.magnitudeInDb(ratio);
            case BiquadFilterType::HighPass:
                return hp.magnitudeInDb(ratio);
            case BiquadFilterType::BandPass:
                return bp.magnitudeInDb(ratio);
            case BiquadFilterType::Notch:
                return notch.magnitudeInDb(ratio);
            case BiquadFilterType::Peak:
                return peak.magnitudeInDb(ratio);
            case BiquadFilterType::LoShelf:
                return ls.magnitudeInDb(ratio);
            case BiquadFilterType::HiShelf:
                return hs.magnitudeInDb(ratio);
            case BiquadFilterType::AllPass:
                return ap.magnitudeInDb(ratio);
            default:
                return 0;
        }
    }

    float m_sampleRate{48000.f};

    BiquadFilterType m_currentFilterType;
    BiquadStereo<BiquadFilterType::LowPass> lp;
    BiquadStereo<BiquadFilterType::HighPass> hp;
    BiquadStereo<BiquadFilterType::BandPass> bp;
    BiquadStereo<BiquadFilterType::Notch> notch;
    BiquadStereo<BiquadFilterType::Peak> peak;
    BiquadStereo<BiquadFilterType::LoShelf> ls;
    BiquadStereo<BiquadFilterType::HiShelf> hs;
    BiquadStereo<BiquadFilterType::AllPass> ap;
};

TEST_P(DspBiquadFilterTests, allMagnitudesStereo)
{
    std::vector<float> qvalues{StandardQValue, StrongQvalue, WeakQValue};
    std::vector<float> cfValues{125.f, 2000.f};
    std::vector<float> hzValues{25.f, 200.f, 800.f, 6400.f};

    const auto ts = GetParam();
    for (auto qFactor : qvalues)
    {
        for (auto cf : cfValues)
        {
            for (auto hz : hzValues)
            {
                WrapperOfSpecializedBiquadsStereo sut;

                constexpr auto sampleRate{48000.0};

                sut.computeCoefficients(ts.type, sampleRate, cf, qFactor, ts.gain);

                std::vector<float> wave{};
                std::vector<float> left{};
                std::vector<float> right{};

                wave.resize(qFactor > 0.8 ? 48000 : 5000);  // strong q needs more time to settle due to resonance
                left.resize(qFactor > 0.8 ? 48000 : 5000);  // strong q needs more time to settle due to resonance
                right.resize(qFactor > 0.8 ? 48000 : 5000); // strong q needs more time to settle due to resonance

                renderWithSineWave(wave, sampleRate, hz);
                sut.processBlock(wave.data(), wave.data(), left.data(), right.data(), wave.size());


                const auto expectedDb = sut.magnitudeInDb(hz);
                {
                    const auto [minV, maxV] = std::minmax_element(left.begin() + left.size() / 2, left.end());
                    const auto maxValue = std::max(std::abs(*minV), std::abs(*maxV));
                    const auto db = std::log10(maxValue) * 20.0f;
                    EXPECT_NEAR(db, expectedDb, maxDeltaDb) << "left channel failure";
                }
                {
                    const auto [minV, maxV] = std::minmax_element(right.begin() + right.size() / 2, right.end());
                    const auto maxValue = std::max(std::abs(*minV), std::abs(*maxV));
                    const auto db = std::log10(maxValue) * 20.0f;
                    EXPECT_NEAR(db, expectedDb, maxDeltaDb) << "right channel failure";
                }
            }
        }
    }
}


INSTANTIATE_TEST_SUITE_P(DspBiquadMagnitudeTest, DspBiquadFilterTests, testing::ValuesIn(filterTestSet));

TEST(DspBiquadFilterTest, checkCoefficientsType1LowPass)
{
    constexpr float kEpsilon{1E-4f};
    // fc=1000 ripple=3 low pass
    const std::array<std::array<float, 5>, 5> coefficients = {{
        {0.0029727008659393f, 0.0059454017318785f, 0.0029727008659393f, -1.9795991182327f, 0.99639534950256f},
        {0.0034143801312894f, 0.0068287602625787f, 0.0034143801312894f, -1.975909113884f, 0.98956662416458f},
        {0.0021591528784484f, 0.0043183057568967f, 0.0021591528784484f, -1.9751399755478f, 0.98377656936646f},
        {0.00090879137860611f, 0.0018175827572122f, 0.00090879137860611f, -1.9759397506714f, 0.97957491874695f},
        {0.00013712796499021f, 0.00027425592998043f, 0.00013712796499021f, -1.9768172502518f, 0.97736573219299f},
    }};
    ChebyshevBiquad sut(48000.f);
    sut.computeType1(10, 1000, 3, true);
    EXPECT_EQ(sut.elements(), 5);
    for (size_t i = 0; i < sut.elements(); ++i)
    {
        auto cf = sut.getCoefficients(i);
        EXPECT_NEAR(coefficients[i][0], cf.b0, kEpsilon);
        EXPECT_NEAR(coefficients[i][1], cf.b1, kEpsilon);
        EXPECT_NEAR(coefficients[i][2], cf.b2, kEpsilon);
        EXPECT_NEAR(coefficients[i][3], cf.a1, kEpsilon);
        EXPECT_NEAR(coefficients[i][4], cf.a2, kEpsilon);
    }
}

TEST(DspBiquadFilterTest, checkCoefficientsType1HighPass)
{
    constexpr float kEpsilon{1E-4f};
    // fc=1000 ripple=3 low pass
    const std::array<std::array<float, 5>, 5> coefficients = {{
        {0.70357483625412f, -1.4071496725082f, 0.70357483625412f, -1.9789683818817f, 0.99633520841599f},
        {0.98821842670441f, -1.9764368534088f, 0.98821842670441f, -1.9658463001251f, 0.98702752590179f},
        {0.97599148750305f, -1.9519829750061f, 0.97599148750305f, -1.9354702234268f, 0.96849608421326f},
        {0.93601679801941f, -1.8720335960388f, 0.93601679801941f, -1.8344408273697f, 0.90962678194046f},
        {0.67223185300827f, -1.3444637060165f, 0.67223185300827f, -1.1655949354172f, 0.52333217859268f},
    }};
    ChebyshevBiquad sut(48000.f);
    sut.computeType1(10, 1000, 3, false);
    EXPECT_EQ(sut.elements(), 5);
    for (size_t i = 0; i < sut.elements(); ++i)
    {
        auto cf = sut.getCoefficients(i);
        EXPECT_NEAR(coefficients[i][0], cf.b0, kEpsilon);
        EXPECT_NEAR(coefficients[i][1], cf.b1, kEpsilon);
        EXPECT_NEAR(coefficients[i][2], cf.b2, kEpsilon);
        EXPECT_NEAR(coefficients[i][3], cf.a1, kEpsilon);
        EXPECT_NEAR(coefficients[i][4], cf.a2, kEpsilon);
    }
}

TEST(DspBiquadFilterTest, checkCoefficientsType2LowPass)
{
    constexpr float kEpsilon{1E-4f};
    // fc=1000 ripple=3 low pass
    const std::array<std::array<float, 5>, 5> coefficients = {{
        {0.99026596546173f, -1.9631650447845f, 0.99026596546173f, -1.9789681434631f, 0.99633502960205f},
        {0.98387670516968f, -1.9465719461441f, 0.98387658596039f, -1.9658459424973f, 0.98702734708786f},
        {0.9692223072052f, -1.9054185152054f, 0.9692223072052f, -1.9354699850082f, 0.96849608421326f},
        {0.92060536146164f, -1.7660242319107f, 0.92060530185699f, -1.8344397544861f, 0.90962624549866f},
        {0.59890443086624f, -0.84006577730179f, 0.59890443086624f, -1.1655919551849f, 0.52333503961563f},
    }};
    ChebyshevBiquad sut(48000.f);
    sut.computeType2(10, 1000, 3, true);
    EXPECT_EQ(sut.elements(), 5);
    for (size_t i = 0; i < sut.elements(); ++i)
    {
        auto cf = sut.getCoefficients(i);
        EXPECT_NEAR(coefficients[i][0], cf.b0, kEpsilon);
        EXPECT_NEAR(coefficients[i][1], cf.b1, kEpsilon);
        EXPECT_NEAR(coefficients[i][2], cf.b2, kEpsilon);
        EXPECT_NEAR(coefficients[i][3], cf.a1, kEpsilon);
        EXPECT_NEAR(coefficients[i][4], cf.a2, kEpsilon);
    }
}

TEST(DspBiquadFilterTest, checkCoefficientsType2HighPass)
{
    constexpr float kEpsilon{1E-4f};
    // fc=1000 ripple=3 low pass
    const std::array<std::array<float, 5>, 5> coefficients = {{
        {0.99816447496414f, -1.9796662330627f, 0.99816429615021f, -1.9795993566513f, 0.9963955283165f},
        {0.99474984407425f, -1.9759755134583f, 0.99474984407425f, -1.9759088754654f, 0.98956632614136f},
        {0.99185508489609f, -1.9752066135406f, 0.99185508489609f, -1.9751400947571f, 0.9837766289711f},
        {0.98975425958633f, -1.9760061502457f, 0.98975425958633f, -1.9759398698807f, 0.97957497835159f},
        {0.98864996433258f, -1.9768840074539f, 0.98864978551865f, -1.9768177270889f, 0.97736620903015f},
    }};
    ChebyshevBiquad sut(48000.f);
    sut.computeType2(10, 1000, 3, false);
    EXPECT_EQ(sut.elements(), 5);
    for (size_t i = 0; i < sut.elements(); ++i)
    {
        auto cf = sut.getCoefficients(i);
        EXPECT_NEAR(coefficients[i][0], cf.b0, kEpsilon);
        EXPECT_NEAR(coefficients[i][1], cf.b1, kEpsilon);
        EXPECT_NEAR(coefficients[i][2], cf.b2, kEpsilon);
        EXPECT_NEAR(coefficients[i][3], cf.a1, kEpsilon);
        EXPECT_NEAR(coefficients[i][4], cf.a2, kEpsilon);
    }
}

TEST(DspBiquadFilterTest, coefficientTables)
{
    GTEST_SKIP();
    ChebyshevBiquad sut(48000.f);
    sut.computeType1(10, 1000, 3, true);
    sut.printCoefficients();
    sut.computeType1(10, 1000, 3, false);
    sut.printCoefficients();
    sut.computeType2(10, 1000, 3, true);
    sut.printCoefficients();
    sut.computeType2(10, 1000, 3, false);
    sut.printCoefficients();
}


/*********** Chebyshev Filters **************/

namespace ChebyshevTestHelpers
{
constexpr auto kSampleRate = 48000.0f;
constexpr auto kBufferSize = 16000;
constexpr auto kRipple = 6.0f;
constexpr auto kFrequencyTolerance = 2.0f;
constexpr auto kNoiseFloorThreshold = -78.0f;
constexpr auto kNoiseFloorLimit = -72.0f;

struct TestBuffers
{
    std::vector<float> wave;
    std::vector<float> left;
    std::vector<float> right;
    std::vector<float> out;

    TestBuffers()
        : wave(kBufferSize, 0.0f)
        , left(kBufferSize, 0.0f)
        , right(kBufferSize, 0.0f)
        , out(kBufferSize, 0.0f)
    {
    }
};

float calculateDbFromBuffer(const std::vector<float>& buffer)
{
    const auto [minV, maxV] = std::minmax_element(buffer.begin() + buffer.size() / 2, buffer.end());
    const auto maxValue = std::max(std::abs(*minV), std::abs(*maxV));
    return std::log10(maxValue) * 20.0f;
}

void testSingleFrequency(ChebyshevBiquad& sut, TestBuffers& buffers, float frequency, bool isStereo, int order,
                         bool checkRipple = false)
{
    renderWithSineWave(buffers.wave, kSampleRate, frequency);

    const std::vector<float>* targetBuffer;

    if (isStereo)
    {
        sut.processBlockStereo(buffers.wave.data(), buffers.wave.data(), buffers.left.data(), buffers.right.data(),
                               buffers.wave.size());
        targetBuffer = &buffers.left;
    }
    else
    {
        sut.processBlock(buffers.wave.data(), buffers.out.data(), buffers.wave.size());
        targetBuffer = &buffers.out;
    }

    const float expectedDb = sut.getMagnitudeInDb(frequency);
    const float db = calculateDbFromBuffer(*targetBuffer);

    if (expectedDb < kNoiseFloorThreshold)
    {
        EXPECT_LT(db, kNoiseFloorLimit) << "left channel failure @" << frequency << " order:" << order;
    }
    else
    {
        EXPECT_NEAR(db, expectedDb, kFrequencyTolerance)
            << "left channel failure @" << frequency << "Hz order:" << order;
    }

    if (checkRipple && frequency < 800.0f)
    // check ripple only if in passband, in stopband it is not interesting at all
    {
        EXPECT_GT(db, -6.5f);
        EXPECT_LT(db, 0.0f);
    }
}

void testInitialFrequency(ChebyshevBiquad& sut, TestBuffers& buffers, float testFrequency, bool isStereo)
{
    renderWithSineWave(buffers.wave, kSampleRate, testFrequency);

    const std::vector<float>* targetBuffer;

    if (isStereo)
    {
        sut.processBlockStereo(buffers.wave.data(), buffers.wave.data(), buffers.left.data(), buffers.right.data(),
                               buffers.wave.size());
        targetBuffer = &buffers.left;
    }
    else
    {
        sut.processBlock(buffers.wave.data(), buffers.out.data(), buffers.wave.size());
        targetBuffer = &buffers.out;
    }

    const float expectedDb = sut.getMagnitudeInDb(testFrequency);
    const float db = calculateDbFromBuffer(*targetBuffer);

    EXPECT_NEAR(db, expectedDb, 1.0f);
}

template <typename FilterSetupFunc>
void runFilterTest(FilterSetupFunc setupFilter, bool isStereo, bool checkRipple, float initialTestFreq)
{
    for (int order = 1; order < 10; ++order)
    {
        ChebyshevBiquad sut(kSampleRate);
        setupFilter(sut, order);
        TestBuffers buffers;

        testInitialFrequency(sut, buffers, initialTestFreq, isStereo);

        for (float hz = 31.25f; hz < 20000; hz *= 2.0f)
        {
            testSingleFrequency(sut, buffers, hz, isStereo, order, checkRipple);
        }
    }
}
}

TEST(DspBiquadFilterTest, chebyshevFilterType1LowpassMono)
{
    using namespace ChebyshevTestHelpers;
    runFilterTest([](ChebyshevBiquad& sut, int order) { sut.computeType1(order, 1000, kRipple, true); }, false, true,
                  2000.0f);
}

TEST(DspBiquadFilterTest, chebyshevFilterType1HighpassMono)
{
    using namespace ChebyshevTestHelpers;
    runFilterTest([](ChebyshevBiquad& sut, int order) { sut.computeType1(order, 1000, kRipple, false); }, false, false,
                  500.0f);
}

TEST(DspBiquadFilterTest, chebyshevFilterType2LowpassMono)
{
    using namespace ChebyshevTestHelpers;
    runFilterTest([](ChebyshevBiquad& sut, int order) { sut.computeType2(order, 1000, kRipple, true); }, false, false,
                  2000.0f);
}

TEST(DspBiquadFilterTest, chebyshevFilterType2HighpassMono)
{
    using namespace ChebyshevTestHelpers;
    runFilterTest([](ChebyshevBiquad& sut, int order) { sut.computeType2(order, 1000, kRipple, false); }, false, false,
                  500.0f);
}

TEST(DspBiquadFilterTest, chebyshevFilterType1LowpassStereo)
{
    using namespace ChebyshevTestHelpers;
    runFilterTest([](ChebyshevBiquad& sut, int order) { sut.computeType1(order, 1000, kRipple, true); }, true, true,
                  2000.0f);
}

TEST(DspBiquadFilterTest, chebyshevFilterType1HighpassStereo)
{
    using namespace ChebyshevTestHelpers;
    runFilterTest([](ChebyshevBiquad& sut, int order) { sut.computeType1(order, 1000, kRipple, false); }, true, false,
                  500.0f);
}

TEST(DspBiquadFilterTest, chebyshevFilterType2LowpassStereo)
{
    using namespace ChebyshevTestHelpers;
    runFilterTest([](ChebyshevBiquad& sut, int order) { sut.computeType2(order, 1000, kRipple, true); }, true, false,
                  2000.0f);
}

TEST(DspBiquadFilterTest, chebyshevFilterType2HighpassStereo)
{
    using namespace ChebyshevTestHelpers;
    runFilterTest([](ChebyshevBiquad& sut, int order) { sut.computeType2(order, 1000, kRipple, false); }, true, false,
                  500.0f);
}
}