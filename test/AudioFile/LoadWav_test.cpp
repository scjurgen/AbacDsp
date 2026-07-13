#include "AudioFile/LoadWav.h"
#include "AudioFile/SaveWav.h"
#include "gtest/gtest.h"

namespace AbacDsp::Test
{

TEST(LoadWav, roundTripsMonoSamples)
{
    const std::vector<float> original{1.f, 0.5f, 0.f, -0.5f, -1.f, 0.25f, -0.25f};
    std::vector<uint8_t> wavBytes;
    AudioUtility::SaveWav::saveMonoToMemory(wavBytes, original, 48000.f);

    const auto loaded = AudioUtility::LoadWav::loadMonoFromMemory(wavBytes);

    ASSERT_EQ(loaded.size(), original.size());
    for (size_t i = 0; i < original.size(); ++i)
    {
        EXPECT_NEAR(loaded[i], original[i], 1E-3f) << "failed at sample " << i;
    }
}

TEST(LoadWav, roundTripsStereoSamples)
{
    const std::vector<float> left{1.f, 0.5f, 0.f, -0.5f};
    const std::vector<float> right{-1.f, -0.5f, 0.f, 0.5f};
    std::vector<uint8_t> wavBytes;
    AudioUtility::SaveWav::saveStereoToMemory(wavBytes, left, right, 44100.f);

    const auto [loadedLeft, loadedRight] = AudioUtility::LoadWav::loadStereoFromMemory(wavBytes);

    ASSERT_EQ(loadedLeft.size(), left.size());
    ASSERT_EQ(loadedRight.size(), right.size());
    for (size_t i = 0; i < left.size(); ++i)
    {
        EXPECT_NEAR(loadedLeft[i], left[i], 1E-3f) << "failed at sample " << i;
        EXPECT_NEAR(loadedRight[i], right[i], 1E-3f) << "failed at sample " << i;
    }
}

TEST(LoadWav, emptyMemoryHasNoChannels)
{
    const std::vector<uint8_t> empty;
    const auto loaded = AudioUtility::LoadWav::loadMonoFromMemory(empty);
    EXPECT_TRUE(loaded.empty());
}

}
