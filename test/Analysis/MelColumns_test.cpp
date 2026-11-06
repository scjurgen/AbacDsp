#include <algorithm>
#include <chrono>
#include <cmath>
#include <gtest/gtest.h>
#include <numbers>
#include <thread>
#include <vector>

#include "Analysis/MelColumns.h"

namespace AbacDsp::Test
{
namespace
{
constexpr float sampleRate{48000.f};

MelColumns::Settings smallRing()
{
    MelColumns::Settings settings;
    settings.columns = 64;
    return settings;
}

/// Feeds `seconds` of the signal in blocks of one hop, giving the worker time to keep up, and collects every column
/// that arrives until `expected` columns are in or a generous deadline passes.
template <typename Signal>
std::vector<float> feedAndCollect(MelColumns& mel, Signal signal, const size_t expected)
{
    const size_t hop = mel.fftLength() / 4;
    std::vector<float> collected;
    std::vector<float> buffer(static_cast<size_t>(mel.bands()) * mel.columns());
    size_t index{0};
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while (collected.size() < expected * mel.bands() && std::chrono::steady_clock::now() < deadline)
    {
        std::vector<float> block(hop);
        for (auto& sample : block)
        {
            sample = signal(index++);
        }
        mel.processBlock(block);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        const size_t got = mel.pull(buffer);
        collected.insert(collected.end(), buffer.begin(),
                         buffer.begin() + static_cast<std::ptrdiff_t>(got * mel.bands()));
    }
    return collected;
}

float sineAt(const size_t n, const float hz, const float amplitude)
{
    return amplitude * std::sin(2.f * std::numbers::pi_v<float> * hz * static_cast<float>(n) / sampleRate);
}

/// The loudest band of the last collected column.
size_t loudestBand(const std::vector<float>& columns, const unsigned bands)
{
    const auto last = columns.end() - static_cast<std::ptrdiff_t>(bands);
    return static_cast<size_t>(std::max_element(last, columns.end()) - last);
}

float peakOfLastColumn(const std::vector<float>& columns, const unsigned bands)
{
    return *std::max_element(columns.end() - static_cast<std::ptrdiff_t>(bands), columns.end());
}
} // namespace

TEST(MelColumns, reportsItsGeometry)
{
    MelColumns mel{sampleRate, smallRing()};
    EXPECT_EQ(64u, mel.bands());
    EXPECT_EQ(64u, mel.columns());
    EXPECT_NEAR(512.f / sampleRate, mel.columnSeconds(), 1e-6f);
}

TEST(MelColumns, bandCentresRiseFromTheLowestToTheHighestFrequency)
{
    MelColumns mel{sampleRate, smallRing()};
    for (unsigned band = 1; band < mel.bands(); ++band)
    {
        EXPECT_GT(mel.bandCentreHz(band), mel.bandCentreHz(band - 1));
    }
    EXPECT_GT(mel.bandCentreHz(0), 30.f);
    EXPECT_LT(mel.bandCentreHz(mel.bands() - 1), 20000.f);
}

TEST(MelColumns, silenceIsBelowTheFloorInEveryBand)
{
    MelColumns mel{sampleRate, smallRing()};
    const auto columns = feedAndCollect(mel, [](size_t) { return 0.f; }, 6);
    ASSERT_GE(columns.size(), 6u * mel.bands());
    EXPECT_FLOAT_EQ(0.f, *std::max_element(columns.begin(), columns.end()));
}

TEST(MelColumns, aSinePeaksInTheBandOfItsFrequency)
{
    for (const float hz : {250.f, 1000.f, 4000.f, 12000.f})
    {
        MelColumns mel{sampleRate, smallRing()};
        const auto columns = feedAndCollect(mel, [hz](const size_t n) { return sineAt(n, hz, 1.f); }, 8);
        ASSERT_GE(columns.size(), 8u * mel.bands()) << hz;
        const auto band = static_cast<unsigned>(loudestBand(columns, mel.bands()));
        EXPECT_NEAR(hz, mel.bandCentreHz(band), hz * 0.12f) << hz;
        EXPECT_GT(peakOfLastColumn(columns, mel.bands()), 0.9f) << hz << ": within 9 dB of full scale";
    }
}

TEST(MelColumns, TwentyDbLessIsTwentyDbLessOnTheScale)
{
    MelColumns::Settings settings = smallRing();
    const auto peak = [&](const float amplitude)
    {
        MelColumns mel{sampleRate, settings};
        const auto columns =
            feedAndCollect(mel, [amplitude](const size_t n) { return sineAt(n, 1000.f, amplitude); }, 8);
        return peakOfLastColumn(columns, mel.bands());
    };
    const float full = peak(1.f);
    const float tenth = peak(0.1f);
    const float expected = 20.f / (settings.ceilDb - settings.floorDb);
    EXPECT_NEAR(expected, full - tenth, 0.01f);
}

TEST(MelColumns, BroadbandNoiseLightsEveryBandIncludingTheNarrowLowOnes)
{
    MelColumns mel{sampleRate, smallRing()};
    uint32_t state{12345};
    const auto noise = [&state](size_t)
    {
        state = state * 1664525u + 1013904223u;
        return static_cast<float>(state >> 8) / static_cast<float>(1u << 23) - 1.f;
    };
    const auto columns = feedAndCollect(mel, noise, 8);
    ASSERT_GE(columns.size(), 8u * mel.bands());
    const auto last = columns.end() - static_cast<std::ptrdiff_t>(mel.bands());
    for (auto it = last; it != columns.end(); ++it)
    {
        EXPECT_GT(*it, 0.3f) << "band " << (it - last);
    }
}

TEST(MelColumns, aSecondPullReturnsNothingNew)
{
    MelColumns mel{sampleRate, smallRing()};
    const auto columns = feedAndCollect(mel, [](const size_t n) { return sineAt(n, 500.f, 0.5f); }, 5);
    ASSERT_GE(columns.size(), 5u * mel.bands());
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    std::vector<float> buffer(mel.bands() * 4);
    (void) mel.pull(buffer);
    EXPECT_EQ(0u, mel.pull(buffer));
}

TEST(MelColumns, aSlowReaderKeepsTheNewestColumnsAndNeverMoreThanTheRingAllows)
{
    MelColumns::Settings settings;
    settings.columns = 8;
    MelColumns mel{sampleRate, settings};
    const size_t hop = mel.fftLength() / 4;
    for (size_t block = 0; block < 40; ++block)
    {
        std::vector<float> samples(hop);
        for (size_t i = 0; i < hop; ++i)
        {
            samples[i] = sineAt(block * hop + i, 1000.f, 0.5f);
        }
        mel.processBlock(samples);
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    std::vector<float> buffer(static_cast<size_t>(mel.bands()) * 16);
    const size_t got = mel.pull(buffer);
    EXPECT_GT(got, 0u);
    EXPECT_LE(got, 6u);
}

TEST(MelColumns, anOutputBufferForOneColumnReceivesTheNewestColumn)
{
    MelColumns mel{sampleRate, smallRing()};
    const size_t hop = mel.fftLength() / 4;
    for (size_t block = 0; block < 12; ++block)
    {
        std::vector<float> samples(hop);
        for (size_t i = 0; i < hop; ++i)
        {
            samples[i] = sineAt(block * hop + i, 1000.f, 0.5f);
        }
        mel.processBlock(samples);
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    std::vector<float> one(mel.bands());
    EXPECT_EQ(1u, mel.pull(one));
    EXPECT_GT(*std::max_element(one.begin(), one.end()), 0.8f);
}

} // namespace AbacDsp::Test
