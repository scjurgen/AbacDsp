
#include "Filters/OnePoleFilter.h"

#include "gtest/gtest.h"

#include <algorithm>
#include <array>
#include <cmath>

#include "NaiveGenerators/Sine.h"

template <AbacadDsp::OnePoleFilterCharacteristic Characteristic>
void testFilterMagnitude(float sampleRate)
{
    //for (float cf = 50; cf <= 12800; cf *= 1.2f) // cutoff
    {
        float cf = 200;
        for (float hz = 50; hz <= 12800; hz *= 1.2f) // test frequency
        {
            AbacadDsp::OnePoleFilter<Characteristic, false> sut{sampleRate};
            sut.setCutoff(static_cast<float>(cf));
            std::vector<float> wave(4000);
            SineWave sineWave{sampleRate, static_cast<float>(hz)};
            sineWave.render(wave.begin(), wave.end());
            sut.processBlock(wave.data(), wave.data(), wave.size());

            // pick values from within to account for settling values
            const auto [minV, maxV] = std::minmax_element(wave.begin() + wave.size() / 2, wave.end());
            auto maxValue = std::max(std::abs(*minV), std::abs(*maxV));
            auto db = std::log10(std::abs(maxValue)) * 20.0f;

            auto magnitude = sut.magnitude(static_cast<float>(hz));
            auto expectedDb = std::log10(magnitude) * 20.0;
            auto maxDt = 3.1f;
            // std::cout << cf << "\t" << hz << "\t" << db << "\t" << expectedDb << "\n";
            EXPECT_NEAR(db, expectedDb, maxDt) << "fail with cf:" << cf << " and f:" << hz;
        }
    }
}

template <AbacadDsp::OnePoleFilterCharacteristic Characteristic>
void find3(float sampleRate)
{
    float lastHz = 20;
    float lastFdbk = 1.f;
    for (float hz = lastHz; hz <= 24020; hz *= std::pow(2, 1.f / 12.f)) // test frequency
    {
        std::cout << log(hz) / log(2) << "\t";
        for (float fdbk = lastFdbk; fdbk > 0.54; fdbk = fdbk * 0.99999f)
        {
            AbacadDsp::OnePoleFilter<Characteristic, false> sut{sampleRate};
            // sut.setCutoff(static_cast<float>(cf));
            sut.setFeedback(fdbk);
            std::vector<float> wave(8000);
            SineWave sineWave{sampleRate, static_cast<float>(hz)};
            sineWave.render(wave.begin(), wave.end());
            sut.processBlock(wave.data(), wave.data(), wave.size());

            // pick values from within to account for settling values
            const auto [minV, maxV] = std::minmax_element(wave.begin() + wave.size() / 2, wave.end());
            auto maxValue = std::max(std::abs(*minV), std::abs(*maxV));
            auto db = std::log10(std::abs(maxValue)) * 20.0f;
            auto magnitude = sut.magnitude(static_cast<float>(hz));
            if (db < -3.0)
            {
                std::cout << sut.feedback() << std::endl;
                lastFdbk = fdbk;
                break;
            }
            // EXPECT_NEAR(db, expectedDb, maxDt) << "fail with cf:" << cf << " and f:" << hz;
        }
    }
}


TEST(DspOnePoleFilterTest, LowPassMatchTheoreticalMagnitudes)
{
    testFilterMagnitude<AbacadDsp::OnePoleFilterCharacteristic::LowPass>(48000.f);
}

TEST(DspOnePoleFilterTest, HighPassCheck)
{
    // find3<AbacadDsp::OnePoleFilterCharacteristic::HighPass>(48000.f);
}

TEST(DspOnePoleFilterTest, HighPassMatchTheoreticalMagnitudes)
{
    testFilterMagnitude<AbacadDsp::OnePoleFilterCharacteristic::HighPass>(48000.f);
}

TEST(DspOnePoleFilterTest, AllPassMatchTheoreticalMagnitudes)
{
    testFilterMagnitude<AbacadDsp::OnePoleFilterCharacteristic::AllPass>(48000.f);
}

TEST(DspOnePoleFilterTest, MultiChannelLowPassMatchTheoreticalMagnitudes)
{
    constexpr size_t NumChannels = 2;
    float sampleRate = 48000.f;

    for (size_t cf = 100; cf <= 6400; cf *= 4) // cutoff
    {
        for (size_t hz = 50; hz <= 12800; hz *= 2) // test frequency
        {
            AbacadDsp::MultiChannelOnePoleFilter<AbacadDsp::OnePoleFilterCharacteristic::LowPass, false, NumChannels>
                sut{sampleRate};
            sut.setCutoff(static_cast<float>(cf));
            std::vector<float> wave(4000 * NumChannels);
            SineWave sineWave{sampleRate, static_cast<float>(hz)};
            sineWave.render(wave.begin(), wave.end(), NumChannels);
            sut.processBlock(wave.data(), wave.size() / NumChannels);

            // Check each channel
            for (size_t channel = 0; channel < NumChannels; ++channel)
            {
                std::vector<float> channelData(wave.size() / NumChannels);
                for (size_t i = 0; i < channelData.size(); ++i)
                {
                    channelData[i] = wave[i * NumChannels + channel];
                }

                const auto [minV, maxV] =
                    std::minmax_element(channelData.begin() + channelData.size() / 2, channelData.end());
                auto maxValue = std::max(std::abs(*minV), std::abs(*maxV));
                auto db = std::log10(std::abs(maxValue)) * 20.0f;

                auto magnitude = sut.magnitude(static_cast<float>(hz));
                auto expectedDb = std::log10(magnitude) * 20.0;
                auto maxDt = 1.2f;
                EXPECT_NEAR(db, expectedDb, maxDt);
            }
        }
    }
}

// You can add similar tests for MultiChannel HighPass and AllPass if needed

TEST(DspOnePoleFilterTest, MultiChannelIndependence)
{
    constexpr size_t NumChannels = 2;
    float sampleRate = 48000.f;
    float cutoff = 1000.f;

    AbacadDsp::MultiChannelOnePoleFilter<AbacadDsp::OnePoleFilterCharacteristic::LowPass, false, NumChannels> sut{
        sampleRate};
    sut.setCutoff(cutoff);

    std::vector<float> input(1000 * NumChannels, 0.0f);
    // Set channel 0 to all 1's and channel 1 to all -1's
    for (size_t i = 0; i < input.size(); i += NumChannels)
    {
        input[i] = 1.0f;
        input[i + 1] = -1.0f;
    }

    sut.processBlock(input.data(), input.size() / NumChannels);

    // Check that the channels remain independent
    for (size_t i = 0; i < input.size(); i += NumChannels)
    {
        EXPECT_GT(input[i], 0.0f);
        EXPECT_LT(input[i + 1], 0.0f);
    }
}
