
#include "Filters/Biquad.h"

#include "gtest/gtest.h"
#include "gmock/gmock.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>

static constexpr double maxDeltaDb =
    0.05; // N.B.: be hard on the quality of Biquads filters, acceptable would be even +-0.5 dB

static constexpr double StrongQvalue{20.0f};                    // absurdly high
static constexpr double StandardQValue{0.70710678118654752440}; // standard value for -3dB at cutoff
static constexpr double WeakQValue(0.2);                        // absurdly low


inline void renderWithSineWave(std::vector<float>& target, const double sampleRate, const double frequency)
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
    TestBiquadFilterSet(const AbacDsp::BiquadFilterType ft_, const double gain_)
        : type(ft_)
          , gain(gain_)
    {
    }

    AbacDsp::BiquadFilterType type;
    double gain;
};

static std::array<TestBiquadFilterSet, 9> filterTestSet{{{AbacDsp::BiquadFilterType::LowPass, 0.0},
                                                         {AbacDsp::BiquadFilterType::HighPass, 0.0},
                                                         {AbacDsp::BiquadFilterType::BandPass, 0.0},
                                                         {AbacDsp::BiquadFilterType::Peak, 6.0},
                                                         {AbacDsp::BiquadFilterType::Peak, -6.0},
                                                         {AbacDsp::BiquadFilterType::LoShelf, 6.0},
                                                         {AbacDsp::BiquadFilterType::LoShelf, -6.0},
                                                         {AbacDsp::BiquadFilterType::HiShelf, 6.0},
                                                         {AbacDsp::BiquadFilterType::HiShelf, -6.0}}};

using FilterCutOffFrequency = double;
using TestFrequency = double;
using TestQFactor = double;

class DspBiquadFilterTests
    : public testing::TestWithParam<std::tuple<TestBiquadFilterSet, TestQFactor, FilterCutOffFrequency, TestFrequency>>
{
public:
};

class WrapperOfSpecializedBiquads
{
public:
    void setType(AbacDsp::BiquadFilterType type)
    {
        m_currentFilterType = type;
    }

    void computeCoefficients(AbacDsp::BiquadFilterType type, const float sampleRate, const float frequency,
                             const float Q, const float peakGain)
    {
        m_sampleRate = sampleRate;
        m_currentFilterType = type;
        switch (m_currentFilterType)
        {
            case AbacDsp::BiquadFilterType::LowPass:
                lp.computeCoefficients(sampleRate, frequency, Q, peakGain);
                break;
            case AbacDsp::BiquadFilterType::HighPass:
                hp.computeCoefficients(sampleRate, frequency, Q, peakGain);
                break;
            case AbacDsp::BiquadFilterType::BandPass:
                bp.computeCoefficients(sampleRate, frequency, Q, peakGain);
                break;
            case AbacDsp::BiquadFilterType::Notch:
                notch.computeCoefficients(sampleRate, frequency, Q, peakGain);
                break;
            case AbacDsp::BiquadFilterType::Peak:
                peak.computeCoefficients(sampleRate, frequency, Q, peakGain);
                break;
            case AbacDsp::BiquadFilterType::LoShelf:
                ls.computeCoefficients(sampleRate, frequency, Q, peakGain);
                break;
            case AbacDsp::BiquadFilterType::HiShelf:
                hs.computeCoefficients(sampleRate, frequency, Q, peakGain);
                break;
            default:
                break;
        }
    }

    void processBlock(const float* in, float* outBuffer, size_t numSamples)
    {
        switch (m_currentFilterType)
        {
            case AbacDsp::BiquadFilterType::LowPass:
                lp.processBlock(in, outBuffer, numSamples);
                break;
            case AbacDsp::BiquadFilterType::HighPass:
                hp.processBlock(in, outBuffer, numSamples);
                break;
            case AbacDsp::BiquadFilterType::BandPass:
                bp.processBlock(in, outBuffer, numSamples);
                break;
            case AbacDsp::BiquadFilterType::Notch:
                notch.processBlock(in, outBuffer, numSamples);
                break;
            case AbacDsp::BiquadFilterType::Peak:
                peak.processBlock(in, outBuffer, numSamples);
                break;
            case AbacDsp::BiquadFilterType::LoShelf:
                ls.processBlock(in, outBuffer, numSamples);
                break;
            case AbacDsp::BiquadFilterType::HiShelf:
                hs.processBlock(in, outBuffer, numSamples);
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
            case AbacDsp::BiquadFilterType::LowPass:
                return lp.magnitudeInDb(ratio);
            case AbacDsp::BiquadFilterType::HighPass:
                return hp.magnitudeInDb(ratio);
            case AbacDsp::BiquadFilterType::BandPass:
                return bp.magnitudeInDb(ratio);
            case AbacDsp::BiquadFilterType::Notch:
                return notch.magnitudeInDb(ratio);
            case AbacDsp::BiquadFilterType::Peak:
                return peak.magnitudeInDb(ratio);
            case AbacDsp::BiquadFilterType::LoShelf:
                return ls.magnitudeInDb(ratio);
            case AbacDsp::BiquadFilterType::HiShelf:
                return hs.magnitudeInDb(ratio);
            default:
                return 0;
        }
    }

    float m_sampleRate{48000.f};
    AbacDsp::BiquadFilterType m_currentFilterType;
    AbacDsp::Biquad<AbacDsp::BiquadFilterType::LowPass> lp;
    AbacDsp::Biquad<AbacDsp::BiquadFilterType::HighPass> hp;
    AbacDsp::Biquad<AbacDsp::BiquadFilterType::BandPass> bp;
    AbacDsp::Biquad<AbacDsp::BiquadFilterType::Notch> notch;
    AbacDsp::Biquad<AbacDsp::BiquadFilterType::Peak> peak;
    AbacDsp::Biquad<AbacDsp::BiquadFilterType::LoShelf> ls;
    AbacDsp::Biquad<AbacDsp::BiquadFilterType::HiShelf> hs;
};

TEST_P(DspBiquadFilterTests, allMagnitudesMono)
{
    const auto ts(std::get<0>(GetParam()));
    const auto qFactor = std::get<1>(GetParam());
    const auto cf = std::get<2>(GetParam());
    const auto hz = std::get<3>(GetParam());
    WrapperOfSpecializedBiquads sut;

    constexpr auto sampleRate{48000.0};

    sut.computeCoefficients(ts.type, sampleRate, cf, qFactor, ts.gain);

    std::vector<float> wave{};
    if (qFactor > 0.8)
    {
        wave.resize(48000); // strong q needs more time to settle due to resonance
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
    EXPECT_NEAR(db, expectedDb, maxDeltaDb);
}


class WrapperOfSpecializedBiquadsStereo
{
public:
    void setType(AbacDsp::BiquadFilterType type)
    {
    }

    void computeCoefficients(AbacDsp::BiquadFilterType type, const float sampleRate, const float frequency,
                             const float Q, const float peakGain)
    {
        m_sampleRate = sampleRate;
        m_currentFilterType = type;
        switch (m_currentFilterType)
        {
            case AbacDsp::BiquadFilterType::LowPass:
                lp.computeCoefficients(sampleRate, frequency, Q, peakGain);
                break;
            case AbacDsp::BiquadFilterType::HighPass:
                hp.computeCoefficients(sampleRate, frequency, Q, peakGain);
                break;
            case AbacDsp::BiquadFilterType::BandPass:
                bp.computeCoefficients(sampleRate, frequency, Q, peakGain);
                break;
            case AbacDsp::BiquadFilterType::Notch:
                notch.computeCoefficients(sampleRate, frequency, Q, peakGain);
                break;
            case AbacDsp::BiquadFilterType::Peak:
                peak.computeCoefficients(sampleRate, frequency, Q, peakGain);
                break;
            case AbacDsp::BiquadFilterType::LoShelf:
                ls.computeCoefficients(sampleRate, frequency, Q, peakGain);
                break;
            case AbacDsp::BiquadFilterType::HiShelf:
                hs.computeCoefficients(sampleRate, frequency, Q, peakGain);
                break;
            default:
                break;
        }
    }

    void processBlock(const float* left, const float* right, float* outLeft, float* outRight, size_t numSamples)
    {
        switch (m_currentFilterType)
        {
            case AbacDsp::BiquadFilterType::LowPass:
                lp.processBlock(left, right, outLeft, outRight, numSamples);
                break;
            case AbacDsp::BiquadFilterType::HighPass:
                hp.processBlock(left, right, outLeft, outRight, numSamples);
                break;
            case AbacDsp::BiquadFilterType::BandPass:
                bp.processBlock(left, right, outLeft, outRight, numSamples);
                break;
            case AbacDsp::BiquadFilterType::Notch:
                notch.processBlock(left, right, outLeft, outRight, numSamples);
                break;
            case AbacDsp::BiquadFilterType::Peak:
                peak.processBlock(left, right, outLeft, outRight, numSamples);
                break;
            case AbacDsp::BiquadFilterType::LoShelf:
                ls.processBlock(left, right, outLeft, outRight, numSamples);
                break;
            case AbacDsp::BiquadFilterType::HiShelf:
                hs.processBlock(left, right, outLeft, outRight, numSamples);
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
            case AbacDsp::BiquadFilterType::LowPass:
                return lp.magnitudeInDb(ratio);
            case AbacDsp::BiquadFilterType::HighPass:
                return hp.magnitudeInDb(ratio);
            case AbacDsp::BiquadFilterType::BandPass:
                return bp.magnitudeInDb(ratio);
            case AbacDsp::BiquadFilterType::Notch:
                return notch.magnitudeInDb(ratio);
            case AbacDsp::BiquadFilterType::Peak:
                return peak.magnitudeInDb(ratio);
            case AbacDsp::BiquadFilterType::LoShelf:
                return ls.magnitudeInDb(ratio);
            case AbacDsp::BiquadFilterType::HiShelf:
                return hs.magnitudeInDb(ratio);
            default:
                return 0;
        }
    }

    float m_sampleRate{48000.f};

    AbacDsp::BiquadFilterType m_currentFilterType;
    AbacDsp::BiquadStereo<AbacDsp::BiquadFilterType::LowPass> lp;
    AbacDsp::BiquadStereo<AbacDsp::BiquadFilterType::HighPass> hp;
    AbacDsp::BiquadStereo<AbacDsp::BiquadFilterType::BandPass> bp;
    AbacDsp::BiquadStereo<AbacDsp::BiquadFilterType::Notch> notch;
    AbacDsp::BiquadStereo<AbacDsp::BiquadFilterType::Peak> peak;
    AbacDsp::BiquadStereo<AbacDsp::BiquadFilterType::LoShelf> ls;
    AbacDsp::BiquadStereo<AbacDsp::BiquadFilterType::HiShelf> hs;
};

TEST_P(DspBiquadFilterTests, allMagnitudesStereo)
{
    const auto ts(std::get<0>(GetParam()));
    const auto qFactor = std::get<1>(GetParam());
    const auto cf = std::get<2>(GetParam());
    const auto hz = std::get<3>(GetParam());
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


INSTANTIATE_TEST_SUITE_P(DspBiquadMagnitudeTest, DspBiquadFilterTests,
                         ::testing::Combine(testing::ValuesIn(filterTestSet),
                             testing::Values(StandardQValue, StrongQvalue, WeakQValue),
                             testing::Values(125.f, 2000.f),
                             testing::Values(25.f, 200.f, 800.f, 6400.f)));


TEST(DspBiquadFilterTest, chebyshevFilterType1LowpassMono)
{
    constexpr auto sampleRate{48000.0f};

    AbacDsp::ChebyshevBiquad sut;
    sut.setSampleRate(sampleRate);

    for (size_t order = 1; order < 10; ++order)
    {
        sut.computeType1(order, 1000, 6, true);
        std::vector<float> wave(48000, 0);
        std::vector<float> out(48000, 0);
        renderWithSineWave(wave, sampleRate, 2000.f);
        sut.processBlock(wave.data(), out.data(), wave.size());
        {
            float expectedDb = sut.getMagnitudeInDb(2000.f);
            const auto [minV, maxV] = std::minmax_element(out.begin() + out.size() / 2, out.end());
            const auto maxValue = std::max(std::abs(*minV), std::abs(*maxV));
            const auto db = std::log10(maxValue) * 20.0f;
            EXPECT_LT(db, -10 * order);
        }
        for (float hz = 125.f / 4.f; hz < 20000; hz *= 2)
        {
            renderWithSineWave(wave, sampleRate, hz);
            sut.processBlock(wave.data(), out.data(), wave.size());
            {
                float expectedDb = sut.getMagnitudeInDb(hz);

                const auto [minV, maxV] = std::minmax_element(out.begin() + out.size() / 2, out.end());
                const auto maxValue = std::max(std::abs(*minV), std::abs(*maxV));
                const auto db = std::log10(maxValue) * 20.0f;
                if (expectedDb < -80)
                {
                    EXPECT_LT(db, -80);
                }
                else
                {
                    EXPECT_NEAR(db, expectedDb, 2.0f) << "left channel failure @" << hz << " order:" << order;
                }
                if (hz < 800)
                {
                    EXPECT_GT(db, -6.5); // stay inside ripple
                    EXPECT_LT(db, 0);
                }
            }
        }
    }
}

TEST(DspBiquadFilterTest, chebyshevFilterType1HighpassMono)
{
    for (size_t order = 1; order < 10; ++order)
    {
        constexpr auto sampleRate{48000.0f};
        AbacDsp::ChebyshevBiquad sut;
        sut.setSampleRate(sampleRate);
        sut.computeType1(order, 1000.f, 6, false);
        std::vector<float> wave(48000, 0);
        std::vector<float> out(48000, 0);
        renderWithSineWave(wave, sampleRate, 500.f);
        sut.processBlock(wave.data(), out.data(), wave.size());
        {
            float expectedDb = sut.getMagnitudeInDb(500.f);
            const auto [minV, maxV] = std::minmax_element(out.begin() + out.size() / 2, out.end());
            const auto maxValue = std::max(std::abs(*minV), std::abs(*maxV));
            const auto db = std::log10(maxValue) * 20.0f;
            EXPECT_LT(db, -10 * order);
        }
        for (float hz = 100; hz < 20000; hz *= 2)
        {
            renderWithSineWave(wave, sampleRate, hz);
            sut.processBlock(wave.data(), out.data(), wave.size());
            {
                float expectedDb = sut.getMagnitudeInDb(hz);

                const auto [minV, maxV] = std::minmax_element(out.begin() + out.size() / 2, out.end());
                const auto maxValue = std::max(std::abs(*minV), std::abs(*maxV));
                const auto db = std::log10(maxValue) * 20.0f;
                if (expectedDb < -80)
                {
                    EXPECT_LT(db, -80);
                }
                else
                {
                    EXPECT_NEAR(db, expectedDb, 2.0f) << "left channel failure @" << hz << " order: " << order;
                }
            }
        }
    }
}


TEST(DspBiquadFilterTest, chebyshevFilterType2LowpassMono)
{
    for (size_t order = 8; order < 10; ++order)
    {
        constexpr auto sampleRate{48000.0f};
        AbacDsp::ChebyshevBiquad sut;
        sut.setSampleRate(sampleRate);
        sut.computeType2(order, 1000, 3, true);

        std::vector<float> wave(48000, 0);
        std::vector<float> out(48000, 0);

        renderWithSineWave(wave, sampleRate, 2000.f);
        sut.processBlock(wave.data(), out.data(), wave.size());
        {
            float expectedDb = sut.getMagnitudeInDb(2000.f);
            const auto [minV, maxV] = std::minmax_element(out.begin() + out.size() / 2, out.end());
            const auto maxValue = std::max(std::abs(*minV), std::abs(*maxV));
            const auto db = std::log10(maxValue) * 20.0f;
            EXPECT_LT(db, -10 * order);
        }

        for (float hz = 10; hz < 20000; hz *= 2)
        {
            renderWithSineWave(wave, sampleRate, hz);
            sut.processBlock(wave.data(), out.data(), wave.size());
            {
                float expectedDb = sut.getMagnitudeInDb(hz);

                const auto [minV, maxV] = std::minmax_element(out.begin() + out.size() / 2, out.end());
                const auto maxValue = std::max(std::abs(*minV), std::abs(*maxV));
                const auto db = std::log10(maxValue) * 20.0f;
                if (expectedDb < -78)
                {
                    EXPECT_LT(db, -72); // allow 6dB noise floor
                }
                else
                {
                    EXPECT_NEAR(db, expectedDb, 2.0f) << "left channel failure @" << hz << " order: " << order;
                }
            }
        }
    }
}

TEST(DspBiquadFilterTest, chebyshevFilterType2HighpassMono)
{
    for (size_t order = 8; order < 10; ++order)
    {
        constexpr auto sampleRate{48000.0f};
        AbacDsp::ChebyshevBiquad sut;
        sut.setSampleRate(sampleRate);
        sut.computeType2(order, 1000, 3, false);


        std::vector<float> wave(48000, 0);
        std::vector<float> out(48000, 0);
        renderWithSineWave(wave, sampleRate, 500.f);
        sut.processBlock(wave.data(), out.data(), wave.size());
        {
            float expectedDb = sut.getMagnitudeInDb(500.f);
            const auto [minV, maxV] = std::minmax_element(out.begin() + out.size() / 2, out.end());
            const auto maxValue = std::max(std::abs(*minV), std::abs(*maxV));
            const auto db = std::log10(maxValue) * 20.0f;
            EXPECT_LT(db, -10 * order);
        }

        for (float hz = 10; hz < 20000; hz *= 2)
        {
            renderWithSineWave(wave, sampleRate, hz);
            sut.processBlock(wave.data(), out.data(), wave.size());
            {
                const float expectedDb = sut.getMagnitudeInDb(hz);

                const auto [minV, maxV] = std::minmax_element(out.begin() + out.size() / 2, out.end());
                const auto maxValue = std::max(std::abs(*minV), std::abs(*maxV));
                const auto db = std::log10(maxValue) * 20.0f;
                if (expectedDb < -78)
                {
                    EXPECT_LT(db, -72); // allow 6dB noise floor
                }
                else
                {
                    EXPECT_NEAR(db, expectedDb, 2.0f) << "left channel failure @" << hz << " order: " << order;
                }
            }
        }
    }
}


TEST(DspBiquadFilterTest, chebyshevFilterType1LowpassStereo)
{
    constexpr auto sampleRate{48000.0f};

    AbacDsp::ChebyshevBiquad sut;
    sut.setSampleRate(sampleRate);

    for (size_t order = 1; order < 10; ++order)
    {
        sut.computeType1(order, 1000, 6, true);
        std::vector<float> wave(48000, 0);
        std::vector<float> left(48000, 0);
        std::vector<float> right(48000, 0);
        renderWithSineWave(wave, sampleRate, 2000.f);
        sut.processBlockStereo(wave.data(), wave.data(), left.data(), right.data(), wave.size());
        {
            float expectedDb = sut.getMagnitudeInDb(2000.f);
            const auto [minV, maxV] = std::minmax_element(left.begin() + left.size() / 2, left.end());
            const auto maxValue = std::max(std::abs(*minV), std::abs(*maxV));
            const auto db = std::log10(maxValue) * 20.0f;
            EXPECT_LT(db, -10 * order);
        }
        for (float hz = 125.f / 4.f; hz < 20000; hz *= 2)
        {
            renderWithSineWave(wave, sampleRate, hz);
            sut.processBlockStereo(wave.data(), wave.data(), left.data(), right.data(), wave.size());
            {
                float expectedDb = sut.getMagnitudeInDb(hz);

                const auto [minV, maxV] = std::minmax_element(left.begin() + left.size() / 2, left.end());
                const auto maxValue = std::max(std::abs(*minV), std::abs(*maxV));
                const auto db = std::log10(maxValue) * 20.0f;
                if (expectedDb < -80)
                {
                    EXPECT_LT(db, -80);
                }
                else
                {
                    EXPECT_NEAR(db, expectedDb, 2.0f) << "left channel failure @" << hz;
                }
                if (hz < 800)
                {
                    EXPECT_GT(db, -6.5); // stay inside ripple
                    EXPECT_LT(db, 0);
                }
            }
        }
    }
}


TEST(DspBiquadFilterTest, chebyshevFilterType1HighpassStereo)
{
    for (size_t order = 1; order < 10; ++order)
    {
        constexpr auto sampleRate{48000.0f};
        AbacDsp::ChebyshevBiquad sut;
        sut.setSampleRate(sampleRate);
        sut.computeType1(order, 1000.f, 6, false);
        std::vector<float> wave(48000, 0);
        std::vector<float> left(48000, 0);
        std::vector<float> right(48000, 0);
        renderWithSineWave(wave, sampleRate, 500.f);
        sut.processBlockStereo(wave.data(), wave.data(), left.data(), right.data(), wave.size());
        {
            float expectedDb = sut.getMagnitudeInDb(500.f);
            const auto [minV, maxV] = std::minmax_element(left.begin() + left.size() / 2, left.end());
            const auto maxValue = std::max(std::abs(*minV), std::abs(*maxV));
            const auto db = std::log10(maxValue) * 20.0f;
            EXPECT_LT(db, -10 * order);
        }
        for (float hz = 100; hz < 20000; hz *= 2)
        {
            renderWithSineWave(wave, sampleRate, hz);
            sut.processBlockStereo(wave.data(), wave.data(), left.data(), right.data(), wave.size());
            {
                float expectedDb = sut.getMagnitudeInDb(hz);

                const auto [minV, maxV] = std::minmax_element(left.begin() + left.size() / 2, left.end());
                const auto maxValue = std::max(std::abs(*minV), std::abs(*maxV));
                const auto db = std::log10(maxValue) * 20.0f;
                if (expectedDb < -80) // ignore noise floor
                {
                    EXPECT_LT(db, -80);
                }
                else
                {
                    EXPECT_NEAR(db, expectedDb, 2.0f) << "left channel failure @" << hz << " order: " << order;
                }
            }
        }
    }
}

TEST(DspBiquadFilterTest, chebyshevFilterType2LowpassStereo)
{
    for (size_t order = 8; order < 10; ++order)
    {
        constexpr auto sampleRate{48000.0f};
        AbacDsp::ChebyshevBiquad sut;
        sut.setSampleRate(sampleRate);
        sut.computeType2(order, 1000, 3, true);
        std::vector<float> wave(48000, 0);
        std::vector<float> left(48000, 0);
        std::vector<float> right(48000, 0);
        renderWithSineWave(wave, sampleRate, 2000.f);
        sut.processBlockStereo(wave.data(), wave.data(), left.data(), right.data(), wave.size());
        {
            float expectedDb = sut.getMagnitudeInDb(2000.f);
            const auto [minV, maxV] = std::minmax_element(left.begin() + left.size() / 2, left.end());
            const auto maxValue = std::max(std::abs(*minV), std::abs(*maxV));
            const auto db = std::log10(maxValue) * 20.0f;
            EXPECT_LT(db, -10 * order);
        }

        for (float hz = 10; hz < 20000; hz *= 2)
        {
            renderWithSineWave(wave, sampleRate, hz);
            sut.processBlockStereo(wave.data(), wave.data(), left.data(), right.data(), wave.size());
            {
                const float expectedDb = sut.getMagnitudeInDb(hz);

                const auto [minV, maxV] = std::minmax_element(left.begin() + left.size() / 2, left.end());
                const auto maxValue = std::max(std::abs(*minV), std::abs(*maxV));
                const auto db = std::log10(maxValue) * 20.0f;
                if (expectedDb < -78)
                {
                    EXPECT_LT(db, -72); // allow 6dB noise floor
                }
                else
                {
                    EXPECT_NEAR(db, expectedDb, 2.0f) << "left channel failure @" << hz << " order: " << order;
                }
            }
        }
    }
}


TEST(DspBiquadFilterTest, chebyshevFilterType2HighpassStereo)
{
    for (size_t order = 8; order < 10; ++order)
    {
        constexpr auto sampleRate{48000.0f};
        AbacDsp::ChebyshevBiquad sut;
        sut.setSampleRate(sampleRate);
        sut.computeType2(order, 1000, 3, false);
        std::vector<float> wave(48000, 0);
        std::vector<float> left(48000, 0);
        std::vector<float> right(48000, 0);
        renderWithSineWave(wave, sampleRate, 500.f);
        sut.processBlockStereo(wave.data(), wave.data(), left.data(), right.data(), wave.size());
        {
            float expectedDb = sut.getMagnitudeInDb(500.f);
            const auto [minV, maxV] = std::minmax_element(left.begin() + left.size() / 2, left.end());
            const auto maxValue = std::max(std::abs(*minV), std::abs(*maxV));
            const auto db = std::log10(maxValue) * 20.0f;
            EXPECT_LT(db, -10 * order);
        }

        for (float hz = 10; hz < 20000; hz *= 2)
        {
            renderWithSineWave(wave, sampleRate, hz);
            sut.processBlockStereo(wave.data(), wave.data(), left.data(), right.data(), wave.size());
            {
                float expectedDb = sut.getMagnitudeInDb(hz);

                const auto [minV, maxV] = std::minmax_element(left.begin() + left.size() / 2, left.end());
                const auto maxValue = std::max(std::abs(*minV), std::abs(*maxV));
                const auto db = std::log10(maxValue) * 20.0f;
                if (expectedDb < -78)
                {
                    EXPECT_LT(db, -72); // allow 6dB noise floor
                }
                else
                {
                    EXPECT_NEAR(db, expectedDb, 2.0f) << "left channel failure @" << hz << " order: " << order;
                }
            }
        }
    }
}

/*
 * // fc=1000 ripple=3 high pass
std::array<std::array<float,5>, 5> coefficients={{
}}
// fc=1000 ripple=3 low pass
std::array<std::array<float,5>, 5> coefficients={{
}}
// fc=1000 ripple=3 high pass
std::array<std::array<float,5>, 5> coefficients={{
}}
 */

TEST(DspBiquadFilterTest, checkCoefficientsType1LowPass)
{
    // fc=1000 ripple=3 low pass
    const std::array<std::array<float, 5>, 5> coefficients = {{
        {0.0029727008659393f, 0.0059454017318785f, 0.0029727008659393f, -1.9795991182327f, 0.99639534950256f},
        {0.0034143801312894f, 0.0068287602625787f, 0.0034143801312894f, -1.975909113884f, 0.98956662416458f},
        {0.0021591528784484f, 0.0043183057568967f, 0.0021591528784484f, -1.9751399755478f, 0.98377656936646f},
        {0.00090879137860611f, 0.0018175827572122f, 0.00090879137860611f, -1.9759397506714f, 0.97957491874695f},
        {0.00013712796499021f, 0.00027425592998043f, 0.00013712796499021f, -1.9768172502518f, 0.97736573219299f},
    }};
    AbacDsp::ChebyshevBiquad sut;
    sut.computeType1(10, 1000, 3, true);
    EXPECT_EQ(sut.elements(), 5);
    for (size_t i = 0; i < sut.elements(); ++i)
    {
        auto cf = sut.getCoefficients(i);
        EXPECT_FLOAT_EQ(coefficients[i][0], cf.b0);
        EXPECT_FLOAT_EQ(coefficients[i][1], cf.b1);
        EXPECT_FLOAT_EQ(coefficients[i][2], cf.b2);
        EXPECT_FLOAT_EQ(coefficients[i][3], cf.a1);
        EXPECT_FLOAT_EQ(coefficients[i][4], cf.a2);
    }
}

TEST(DspBiquadFilterTest, checkCoefficientsType1HighPass)
{
    // fc=1000 ripple=3 low pass
    const std::array<std::array<float, 5>, 5> coefficients = {{
        {0.70357483625412f, -1.4071496725082f, 0.70357483625412f, -1.9789683818817f, 0.99633520841599f},
        {0.98821842670441f, -1.9764368534088f, 0.98821842670441f, -1.9658463001251f, 0.98702752590179f},
        {0.97599148750305f, -1.9519829750061f, 0.97599148750305f, -1.9354702234268f, 0.96849608421326f},
        {0.93601679801941f, -1.8720335960388f, 0.93601679801941f, -1.8344408273697f, 0.90962678194046f},
        {0.67223185300827f, -1.3444637060165f, 0.67223185300827f, -1.1655949354172f, 0.52333217859268f},
    }};
    AbacDsp::ChebyshevBiquad sut;
    sut.computeType1(10, 1000, 3, false);
    EXPECT_EQ(sut.elements(), 5);
    for (size_t i = 0; i < sut.elements(); ++i)
    {
        auto cf = sut.getCoefficients(i);
        EXPECT_FLOAT_EQ(coefficients[i][0], cf.b0);
        EXPECT_FLOAT_EQ(coefficients[i][1], cf.b1);
        EXPECT_FLOAT_EQ(coefficients[i][2], cf.b2);
        EXPECT_FLOAT_EQ(coefficients[i][3], cf.a1);
        EXPECT_FLOAT_EQ(coefficients[i][4], cf.a2);
    }
}

TEST(DspBiquadFilterTest, checkCoefficientsType2LowPass)
{
    // fc=1000 ripple=3 low pass
    const std::array<std::array<float, 5>, 5> coefficients = {{
        {0.99026596546173f, -1.9631650447845f, 0.99026596546173f, -1.9789681434631f, 0.99633502960205f},
        {0.98387670516968f, -1.9465719461441f, 0.98387658596039f, -1.9658459424973f, 0.98702734708786f},
        {0.9692223072052f, -1.9054185152054f, 0.9692223072052f, -1.9354699850082f, 0.96849608421326f},
        {0.92060536146164f, -1.7660242319107f, 0.92060530185699f, -1.8344397544861f, 0.90962624549866f},
        {0.59890443086624f, -0.84006577730179f, 0.59890443086624f, -1.1655919551849f, 0.52333503961563f},
    }};
    AbacDsp::ChebyshevBiquad sut;
    sut.computeType2(10, 1000, 3, true);
    EXPECT_EQ(sut.elements(), 5);
    for (size_t i = 0; i < sut.elements(); ++i)
    {
        auto cf = sut.getCoefficients(i);
        EXPECT_FLOAT_EQ(coefficients[i][0], cf.b0);
        EXPECT_FLOAT_EQ(coefficients[i][1], cf.b1);
        EXPECT_FLOAT_EQ(coefficients[i][2], cf.b2);
        EXPECT_FLOAT_EQ(coefficients[i][3], cf.a1);
        EXPECT_FLOAT_EQ(coefficients[i][4], cf.a2);
    }
}

TEST(DspBiquadFilterTest, checkCoefficientsType2HighPass)
{
    // fc=1000 ripple=3 low pass
    const std::array<std::array<float, 5>, 5> coefficients = {{
        {0.99816447496414f, -1.9796662330627f, 0.99816429615021f, -1.9795993566513f, 0.9963955283165f},
        {0.99474984407425f, -1.9759755134583f, 0.99474984407425f, -1.9759088754654f, 0.98956632614136f},
        {0.99185508489609f, -1.9752066135406f, 0.99185508489609f, -1.9751400947571f, 0.9837766289711f},
        {0.98975425958633f, -1.9760061502457f, 0.98975425958633f, -1.9759398698807f, 0.97957497835159f},
        {0.98864996433258f, -1.9768840074539f, 0.98864978551865f, -1.9768177270889f, 0.97736620903015f},
    }};
    AbacDsp::ChebyshevBiquad sut;
    sut.computeType2(10, 1000, 3, false);
    EXPECT_EQ(sut.elements(), 5);
    for (size_t i = 0; i < sut.elements(); ++i)
    {
        auto cf = sut.getCoefficients(i);
        EXPECT_FLOAT_EQ(coefficients[i][0], cf.b0);
        EXPECT_FLOAT_EQ(coefficients[i][1], cf.b1);
        EXPECT_FLOAT_EQ(coefficients[i][2], cf.b2);
        EXPECT_FLOAT_EQ(coefficients[i][3], cf.a1);
        EXPECT_FLOAT_EQ(coefficients[i][4], cf.a2);
    }
}

TEST(DISABLED_DspBiquadFilterTest, coefficientTables)
{
    AbacDsp::ChebyshevBiquad sut;
    sut.computeType1(10, 1000, 3, true);
    sut.printCoefficients();
    sut.computeType1(10, 1000, 3, false);
    sut.printCoefficients();
    sut.computeType2(10, 1000, 3, true);
    sut.printCoefficients();
    sut.computeType2(10, 1000, 3, false);
    sut.printCoefficients();
}