
#include <algorithm>
#include <array>
#include <cmath>

#include "gtest/gtest.h"

#include "Filters/OnePoleFilter.h"
#include "NaiveGenerators/Generator.h"
namespace AbacDsp::Test
{


template <OnePoleFilterCharacteristic Characteristic>
void testFilterPolarity(const float expected)
{
    OnePoleFilter<Characteristic> sut{48000.f};
    sut.setCutoff(static_cast<float>(1000.f));
    const auto result = sut.step(1);
    EXPECT_NEAR(result, expected, 1E-5f);
}

TEST(DspOnePoleFilterTest, polarityAllPass)
{
    testFilterPolarity<OnePoleFilterCharacteristic::HighPass>(0.938653f);
    testFilterPolarity<OnePoleFilterCharacteristic::LowPass>(0.122694f);
    testFilterPolarity<OnePoleFilterCharacteristic::HighPassLeaky>(0.877306f);
    // phase inversion!
    testFilterPolarity<OnePoleFilterCharacteristic::AllPass>(-0.876976f);
}


template <OnePoleFilterCharacteristic Characteristic>
void testFilterMagnitude(float sampleRate)
{
    for (float cf = 50; cf <= 16000; cf *= 1.2f)
    {
        // Allow wider max dB error at higher cutoffs,
        // since digital/analog deviation and fast-wave settling errors both grow with cf/sampleRate.

        const auto maxDt = 12 * (cf / sampleRate);

        for (float hz = 50; hz <= 16000; hz *= 1.2f)
        {
            AbacDsp::OnePoleFilter<Characteristic, false> sut{sampleRate};
            sut.setCutoff(static_cast<float>(cf));
            std::vector<float> wave(4000);
            Generator<Wave::Sine> sineWave{sampleRate, hz};
            sineWave.render(wave.begin(), wave.end());
            sut.processBlock(wave.data(), wave.data(), wave.size());

            // pick values from within to account for settling values
            const auto halfSize = wave.size() / 2;
            const auto [minV, maxV] = std::minmax_element(wave.begin() + halfSize, wave.end());
            const auto maxValue = std::max(std::abs(*minV), std::abs(*maxV));
            const auto db = std::log10(std::abs(maxValue)) * 20.0f;
            const auto magnitude = sut.magnitude(static_cast<float>(hz));
            const auto expectedDb = std::log10(magnitude) * 20.0;
            EXPECT_NEAR(db, expectedDb, maxDt) << "fail with cf:" << cf << " and f:" << hz;
        }
    }
}

TEST(DspOnePoleFilterTest, LowPassMatchTheoreticalMagnitudes)
{
    testFilterMagnitude<OnePoleFilterCharacteristic::LowPass>(48000.f);
}

TEST(DspOnePoleFilterTest, HighPassMatchTheoreticalMagnitudes)
{
    testFilterMagnitude<OnePoleFilterCharacteristic::HighPass>(48000.f);
}

TEST(DspOnePoleFilterTest, HighPassLeakyMatchTheoreticalMagnitudes)
{
    testFilterMagnitude<OnePoleFilterCharacteristic::HighPassLeaky>(48000.f);
}

TEST(DspOnePoleFilterTest, AllPassMatchTheoreticalMagnitudes)
{
    testFilterMagnitude<OnePoleFilterCharacteristic::AllPass>(48000.f);
}

// --- Mono edge branches ---

TEST(DspOnePoleFilterTest, CutoffAtOrAboveNyquistZeroesFeedback)
{
    OnePoleFilter<OnePoleFilterCharacteristic::LowPass> sut{48000.f};
    sut.setCutoff(24000.f); // == sampleRate / 2: feedback forced to 0, low-pass becomes a pass-through
    EXPECT_FLOAT_EQ(sut.step(0.5f), 0.5f);
    sut.setCutoff(30000.f); // above Nyquist: same branch
    EXPECT_FLOAT_EQ(sut.step(-0.25f), -0.25f);
}

template <OnePoleFilterCharacteristic Characteristic>
void exerciseCutoffAboveNyquist()
{
    OnePoleFilter<Characteristic> sut{48000.f, 1000.f};
    sut.setCutoff(30000.f); // above Nyquist: feedback branch taken for every characteristic
    EXPECT_TRUE(std::isfinite(sut.step(0.5f)));
}

TEST(DspOnePoleFilterTest, CutoffAboveNyquistHandledForAllCharacteristics)
{
    exerciseCutoffAboveNyquist<OnePoleFilterCharacteristic::LowPass>();
    exerciseCutoffAboveNyquist<OnePoleFilterCharacteristic::HighPass>();
    exerciseCutoffAboveNyquist<OnePoleFilterCharacteristic::HighPassLeaky>();
    exerciseCutoffAboveNyquist<OnePoleFilterCharacteristic::AllPass>();
}

TEST(DspOnePoleFilterStereoTest, CutoffAboveNyquistHandled)
{
    OnePoleFilterStereo<OnePoleFilterCharacteristic::LowPass> sut{48000.f, 30000.f};
    float outLeft = 0.0f;
    float outRight = 0.0f;
    sut.stepStereo(0.5f, -0.5f, outLeft, outRight);
    EXPECT_TRUE(std::isfinite(outLeft));
    EXPECT_TRUE(std::isfinite(outRight));
}

TEST(DspMultiChannelOnePoleFilterTest, CutoffAboveNyquistHandled)
{
    constexpr size_t numChannels = 2;
    MultiChannelOnePoleFilter<OnePoleFilterCharacteristic::LowPass, numChannels> sut{48000.f, 30000.f};
    std::array<float, numChannels> frame{0.5f, -0.5f};
    sut.step(frame.data());
    EXPECT_TRUE(std::isfinite(frame[0]));
    EXPECT_TRUE(std::isfinite(frame[1]));
}

TEST(DspOnePoleFilterTest, InPlaceAndCopyBlockAgree)
{
    OnePoleFilter<OnePoleFilterCharacteristic::LowPass> inPlaceFilter{48000.f, 1000.f};
    OnePoleFilter<OnePoleFilterCharacteristic::LowPass> copyFilter{48000.f, 1000.f};
    std::array<float, 8> input{1.f, -1.f, 1.f, -1.f, 1.f, -1.f, 1.f, -1.f};

    std::array<float, 8> copied{};
    copyFilter.processBlock(input.data(), copied.data(), input.size());

    std::array<float, 8> inPlace = input;
    inPlaceFilter.processBlock(inPlace.data(), inPlace.size());

    for (size_t i = 0; i < input.size(); ++i)
    {
        EXPECT_FLOAT_EQ(inPlace[i], copied[i]);
    }
}

TEST(DspOnePoleFilterTest, InPlaceBlockLeavesBufferUntouchedWhenFeedbackZero)
{
    OnePoleFilter<OnePoleFilterCharacteristic::LowPass> sut{48000.f};
    sut.setCutoff(24000.f); // feedback 0 <= 1E-8: early return
    std::array<float, 4> buffer{0.1f, 0.2f, 0.3f, 0.4f};
    const auto original = buffer;
    sut.processBlock(buffer.data(), buffer.size());
    EXPECT_EQ(buffer, original);
}

template <OnePoleFilterCharacteristic Characteristic>
void exerciseCopyBlockZeroFeedback()
{
    OnePoleFilter<Characteristic> sut{48000.f};
    sut.setFeedback(0.0f);
    std::array<float, 4> input{0.1f, 0.2f, 0.3f, 0.4f};
    std::array<float, 4> output{};
    sut.processBlock(input.data(), output.data(), input.size());
    EXPECT_EQ(output, input);
}

TEST(DspOnePoleFilterTest, CopyBlockCopiesInputWhenFeedbackZero)
{
    exerciseCopyBlockZeroFeedback<OnePoleFilterCharacteristic::LowPass>();
    exerciseCopyBlockZeroFeedback<OnePoleFilterCharacteristic::HighPass>();
    exerciseCopyBlockZeroFeedback<OnePoleFilterCharacteristic::HighPassLeaky>();
    exerciseCopyBlockZeroFeedback<OnePoleFilterCharacteristic::AllPass>();
}

template <OnePoleFilterCharacteristic Characteristic>
void exerciseMonoClamp()
{
    OnePoleFilter<Characteristic, true> sut{48000.f, 100.f};
    float out = 0.0f;
    for (int i = 0; i < 4000; ++i)
    {
        out = sut.step((i % 2) == 0 ? 1.0e6f : -1.0e6f);
    }
    EXPECT_TRUE(std::isfinite(out));
}

TEST(DspOnePoleFilterTest, ClampKeepsStateFiniteForAllCharacteristics)
{
    exerciseMonoClamp<OnePoleFilterCharacteristic::LowPass>();
    exerciseMonoClamp<OnePoleFilterCharacteristic::HighPass>();
    exerciseMonoClamp<OnePoleFilterCharacteristic::HighPassLeaky>();
    exerciseMonoClamp<OnePoleFilterCharacteristic::AllPass>();
}

// --- Stereo version (AllPass, LowPass, HighPass; no HighPassLeaky) ---

template <OnePoleFilterCharacteristic Characteristic>
void exerciseStereoBlockOverloadsAgree()
{
    OnePoleFilterStereo<Characteristic> inPlaceFilter{48000.f, 1000.f};
    OnePoleFilterStereo<Characteristic> copyFilter{48000.f, 1000.f};

    std::array<float, 8> left{};
    std::array<float, 8> right{};
    left.fill(1.0f);
    right.fill(-1.0f);
    auto leftIn = left;
    auto rightIn = right;

    inPlaceFilter.processBlock(left.data(), right.data(), left.size());

    std::array<float, 8> leftOut{};
    std::array<float, 8> rightOut{};
    copyFilter.processBlock(leftIn.data(), rightIn.data(), leftOut.data(), rightOut.data(), leftIn.size());

    for (size_t i = 0; i < left.size(); ++i)
    {
        EXPECT_FLOAT_EQ(left[i], leftOut[i]);
        EXPECT_FLOAT_EQ(right[i], rightOut[i]);
        EXPECT_TRUE(std::isfinite(left[i]));
    }
}

TEST(DspOnePoleFilterStereoTest, BlockOverloadsAgree)
{
    exerciseStereoBlockOverloadsAgree<OnePoleFilterCharacteristic::LowPass>();
    exerciseStereoBlockOverloadsAgree<OnePoleFilterCharacteristic::HighPass>();
    exerciseStereoBlockOverloadsAgree<OnePoleFilterCharacteristic::AllPass>();
}

TEST(DspOnePoleFilterStereoTest, ChannelsRemainIndependent)
{
    OnePoleFilterStereo<OnePoleFilterCharacteristic::LowPass> sut{48000.f, 1000.f};
    std::array<float, 2000> left{};
    std::array<float, 2000> right{};
    left.fill(1.0f);
    right.fill(-1.0f);
    sut.processBlock(left.data(), right.data(), left.size());
    EXPECT_GT(left.back(), 0.5f);
    EXPECT_LT(right.back(), -0.5f);
}

template <OnePoleFilterCharacteristic Characteristic>
void exerciseStereoClamp()
{
    OnePoleFilterStereo<Characteristic, true> sut{48000.f, 100.f};
    float outLeft = 0.0f;
    float outRight = 0.0f;
    for (int i = 0; i < 4000; ++i)
    {
        sut.stepStereo(1.0e6f, -1.0e6f, outLeft, outRight);
    }
    EXPECT_TRUE(std::isfinite(outLeft));
    EXPECT_TRUE(std::isfinite(outRight));
}

TEST(DspOnePoleFilterStereoTest, ClampKeepsStateFinite)
{
    exerciseStereoClamp<OnePoleFilterCharacteristic::LowPass>();
    exerciseStereoClamp<OnePoleFilterCharacteristic::HighPass>();
    exerciseStereoClamp<OnePoleFilterCharacteristic::AllPass>();
}

// --- Arbitrary channel count version ---

template <OnePoleFilterCharacteristic Characteristic>
void exerciseMultiChannelBlockOverloadsAgree()
{
    constexpr size_t numChannels = 2;
    MultiChannelOnePoleFilter<Characteristic, numChannels> inPlaceFilter{48000.f, 1000.f};
    MultiChannelOnePoleFilter<Characteristic, numChannels> copyFilter{48000.f, 1000.f};

    constexpr size_t numFrames = 8;
    std::array<float, numChannels * numFrames> input{};
    for (size_t i = 0; i < input.size(); ++i)
    {
        input[i] = (i % 2) == 0 ? 1.0f : -1.0f;
    }

    auto inPlace = input;
    inPlaceFilter.processBlock(inPlace.data(), numFrames);

    std::array<float, numChannels * numFrames> output{};
    copyFilter.processBlock(input.data(), output.data(), numFrames);

    for (size_t i = 0; i < input.size(); ++i)
    {
        EXPECT_FLOAT_EQ(inPlace[i], output[i]);
        EXPECT_TRUE(std::isfinite(output[i]));
    }
}

TEST(DspMultiChannelOnePoleFilterTest, BlockOverloadsAgree)
{
    exerciseMultiChannelBlockOverloadsAgree<OnePoleFilterCharacteristic::LowPass>();
    exerciseMultiChannelBlockOverloadsAgree<OnePoleFilterCharacteristic::HighPass>();
    exerciseMultiChannelBlockOverloadsAgree<OnePoleFilterCharacteristic::AllPass>();
}

template <OnePoleFilterCharacteristic Characteristic>
void exerciseMultiChannelZeroFeedback()
{
    constexpr size_t numChannels = 2;
    constexpr size_t numFrames = 4;
    MultiChannelOnePoleFilter<Characteristic, numChannels> sut{48000.f};
    sut.setFeedback(0.0f);

    std::array<float, numChannels * numFrames> input{};
    for (size_t i = 0; i < input.size(); ++i)
    {
        input[i] = 0.1f * static_cast<float>(i + 1);
    }

    std::array<float, numChannels * numFrames> output{};
    sut.processBlock(input.data(), output.data(), numFrames); // fdbk == 0: copy path
    EXPECT_EQ(output, input);

    auto inPlace = input;
    sut.processBlock(inPlace.data(), numFrames); // |fdbk| <= 1E-8: early return
    EXPECT_EQ(inPlace, input);
}

TEST(DspMultiChannelOnePoleFilterTest, BlocksPassThroughWhenFeedbackZero)
{
    exerciseMultiChannelZeroFeedback<OnePoleFilterCharacteristic::LowPass>();
    exerciseMultiChannelZeroFeedback<OnePoleFilterCharacteristic::HighPass>();
    exerciseMultiChannelZeroFeedback<OnePoleFilterCharacteristic::AllPass>();
}

TEST(DspMultiChannelOnePoleFilterTest, ClampKeepsStateBounded)
{
    constexpr size_t numChannels = 2;
    MultiChannelOnePoleFilter<OnePoleFilterCharacteristic::LowPass, numChannels, true> sut{48000.f, 100.f};
    std::array<float, numChannels> frame{1.0e6f, -1.0e6f};
    for (int i = 0; i < 4000; ++i)
    {
        frame = {1.0e6f, -1.0e6f};
        sut.step(frame.data());
    }
    EXPECT_LE(frame[0], 1.0f);
    EXPECT_GE(frame[0], -1.0f);
    EXPECT_LE(frame[1], 1.0f);
    EXPECT_GE(frame[1], -1.0f);
}
}