
#include "Filters/OnePoleFilter.h"

#include "gtest/gtest.h"

#include <algorithm>
#include <array>
#include <cmath>

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

//
// TEST(DspOnePoleFilterTest, MultiChannelLowPassMatchTheoreticalMagnitudes)
// {
//     constexpr size_t NumChannels = 2;
//     float sampleRate = 48000.f;
//
//     for (size_t cf = 100; cf <= 6400; cf *= 4) // cutoff
//     {
//         for (size_t hz = 50; hz <= 12800; hz *= 2) // test frequency
//         {
//             AbacDsp::MultiChannelOnePoleFilter<AbacDsp::OnePoleFilterCharacteristic::LowPass, NumChannels> sut{
//                 sampleRate};
//             sut.setCutoff(static_cast<float>(cf));
//             std::vector<float> wave(4000 * NumChannels);
//             NaiveDsp::Generator<NaiveDsp::Wave::Sine> sineWave{sampleRate, static_cast<float>(hz)};
//             sineWave.render(wave.begin(), wave.end(), NumChannels);
//             sut.processBlock(wave.data(), wave.size() / NumChannels);
//
//             // Check each channel
//             for (size_t channel = 0; channel < NumChannels; ++channel)
//             {
//                 std::vector<float> channelData(wave.size() / NumChannels);
//                 for (size_t i = 0; i < channelData.size(); ++i)
//                 {
//                     channelData[i] = wave[i * NumChannels + channel];
//                 }
//
//                 const auto [minV, maxV] =
//                     std::minmax_element(channelData.begin() + channelData.size() / 2, channelData.end());
//                 auto maxValue = std::max(std::abs(*minV), std::abs(*maxV));
//                 auto db = std::log10(std::abs(maxValue)) * 20.0f;
//
//                 auto magnitude = sut.magnitude(static_cast<float>(hz));
//                 auto expectedDb = std::log10(magnitude) * 20.0;
//                 auto maxDt = 1.2f;
//                 EXPECT_NEAR(db, expectedDb, maxDt);
//             }
//         }
//     }
// }
//
// // You can add similar tests for MultiChannel HighPass and AllPass if needed
//
// TEST(DspOnePoleFilterTest, MultiChannelIndependence)
// {
//     constexpr size_t NumChannels = 2;
//     float sampleRate = 48000.f;
//     float cutoff = 1000.f;
//
//     AbacDsp::MultiChannelOnePoleFilter<AbacDsp::OnePoleFilterCharacteristic::LowPass, NumChannels> sut{sampleRate};
//     sut.setCutoff(cutoff);
//
//     std::vector<float> input(1000 * NumChannels, 0.0f);
//     // Set channel 0 to all 1's and channel 1 to all -1's
//     for (size_t i = 0; i < input.size(); i += NumChannels)
//     {
//         input[i] = 1.0f;
//         input[i + 1] = -1.0f;
//     }
//
//     sut.processBlock(input.data(), input.size() / NumChannels);
//
//     // Check that the channels remain independent
//     for (size_t i = 0; i < input.size(); i += NumChannels)
//     {
//         EXPECT_GT(input[i], 0.0f);
//         EXPECT_LT(input[i + 1], 0.0f);
//     }
// }
}