
#include "gtest/gtest.h"
#include "gmock/gmock.h"

#include "AudioFile/SaveWav.h"

#include <cstring>

namespace AbacDsp::Test
{

TEST(AudioFileIO, monoWaveFile)
{
    std::vector<uint8_t> waveTarget;
    std::vector<float> someAudio{1.f, 0.1f, -1.f, -0.1f};
    AudioUtility::FileOut::saveMonoToMemory(waveTarget, someAudio, 48000.f);

    ASSERT_GE(waveTarget.size(), 44u);

    EXPECT_EQ(std::string(reinterpret_cast<const char*>(waveTarget.data()), 4), "RIFF");
    EXPECT_EQ(std::string(reinterpret_cast<const char*>(waveTarget.data() + 8), 4), "WAVE");
    EXPECT_EQ(std::string(reinterpret_cast<const char*>(waveTarget.data() + 12), 4), "fmt ");

    const uint32_t fmtChunkSize = *reinterpret_cast<const uint32_t*>(waveTarget.data() + 16);
    EXPECT_EQ(fmtChunkSize, 16u);

    const uint16_t audioFormat = *reinterpret_cast<const uint16_t*>(waveTarget.data() + 20);
    EXPECT_EQ(audioFormat, 1u);

    const uint16_t numChannels = *reinterpret_cast<const uint16_t*>(waveTarget.data() + 22);
    EXPECT_EQ(numChannels, 1u);

    const uint32_t sampleRate = *reinterpret_cast<const uint32_t*>(waveTarget.data() + 24);
    EXPECT_EQ(sampleRate, 48000u);

    const uint16_t bitsPerSample = *reinterpret_cast<const uint16_t*>(waveTarget.data() + 34);
    EXPECT_EQ(bitsPerSample, 24u);

    const uint32_t byteRate = *reinterpret_cast<const uint32_t*>(waveTarget.data() + 28);
    const uint32_t expectedByteRate = sampleRate * numChannels * bitsPerSample / 8;
    EXPECT_EQ(byteRate, expectedByteRate);

    const uint16_t blockAlign = *reinterpret_cast<const uint16_t*>(waveTarget.data() + 32);
    const uint16_t expectedBlockAlign = numChannels * bitsPerSample / 8;
    EXPECT_EQ(blockAlign, expectedBlockAlign);

    EXPECT_EQ(std::string(reinterpret_cast<const char*>(waveTarget.data() + 36), 4), "data");

    const uint32_t dataChunkSize = *reinterpret_cast<const uint32_t*>(waveTarget.data() + 40);
    const uint32_t expectedDataSize = someAudio.size() * bitsPerSample / 8;
    EXPECT_EQ(dataChunkSize, expectedDataSize);

    const uint32_t fileSize = *reinterpret_cast<const uint32_t*>(waveTarget.data() + 4);
    const uint32_t expectedFileSize = waveTarget.size() - 8;
    EXPECT_EQ(fileSize, expectedFileSize);

    const size_t expectedMinimumSize = 44 + expectedDataSize;
    EXPECT_GE(waveTarget.size(), expectedMinimumSize);
}

TEST(AudioFileIO, stereoWaveFile)
{
    std::vector<uint8_t> waveTarget;
    std::vector<float> leftAudio{1.f, 0.1f, -1.f, -0.1f};
    std::vector<float> rightAudio{0.5f, -0.5f, 0.8f, -0.8f};
    AudioUtility::FileOut::saveStereoToMemory(waveTarget, leftAudio, rightAudio, 48000.f);

    ASSERT_GE(waveTarget.size(), 44u);

    EXPECT_EQ(std::string(reinterpret_cast<const char*>(waveTarget.data()), 4), "RIFF");
    EXPECT_EQ(std::string(reinterpret_cast<const char*>(waveTarget.data() + 8), 4), "WAVE");
    EXPECT_EQ(std::string(reinterpret_cast<const char*>(waveTarget.data() + 12), 4), "fmt ");

    const uint32_t fmtChunkSize = *reinterpret_cast<const uint32_t*>(waveTarget.data() + 16);
    EXPECT_EQ(fmtChunkSize, 16u);

    const uint16_t audioFormat = *reinterpret_cast<const uint16_t*>(waveTarget.data() + 20);
    EXPECT_EQ(audioFormat, 1u);

    const uint16_t numChannels = *reinterpret_cast<const uint16_t*>(waveTarget.data() + 22);
    EXPECT_EQ(numChannels, 2u);

    const uint32_t sampleRate = *reinterpret_cast<const uint32_t*>(waveTarget.data() + 24);
    EXPECT_EQ(sampleRate, 48000u);

    const uint16_t bitsPerSample = *reinterpret_cast<const uint16_t*>(waveTarget.data() + 34);
    EXPECT_EQ(bitsPerSample, 24u);

    const uint32_t byteRate = *reinterpret_cast<const uint32_t*>(waveTarget.data() + 28);
    const uint32_t expectedByteRate = sampleRate * numChannels * bitsPerSample / 8;
    EXPECT_EQ(byteRate, expectedByteRate);

    const uint16_t blockAlign = *reinterpret_cast<const uint16_t*>(waveTarget.data() + 32);
    const uint16_t expectedBlockAlign = numChannels * bitsPerSample / 8;
    EXPECT_EQ(blockAlign, expectedBlockAlign);

    EXPECT_EQ(std::string(reinterpret_cast<const char*>(waveTarget.data() + 36), 4), "data");

    const uint32_t dataChunkSize = *reinterpret_cast<const uint32_t*>(waveTarget.data() + 40);
    const uint32_t expectedDataSize = leftAudio.size() * numChannels * bitsPerSample / 8;
    EXPECT_EQ(dataChunkSize, expectedDataSize);

    const uint32_t fileSize = *reinterpret_cast<const uint32_t*>(waveTarget.data() + 4);
    const uint32_t expectedFileSize = waveTarget.size() - 8;
    EXPECT_EQ(fileSize, expectedFileSize);

    const size_t expectedMinimumSize = 44 + expectedDataSize;
    EXPECT_GE(waveTarget.size(), expectedMinimumSize);
}

TEST(AudioFileIO, stereoChannelDataIntegrity)
{
    std::vector<uint8_t> waveTarget;
    std::vector<float> leftAudio{1.f, 0.f};
    std::vector<float> rightAudio{0.f, 1.f};
    AudioUtility::FileOut::saveStereoToMemory(waveTarget, leftAudio, rightAudio, 48000.f);

    ASSERT_GE(waveTarget.size(), 44u);

    const uint16_t numChannels = *reinterpret_cast<const uint16_t*>(waveTarget.data() + 22);
    EXPECT_EQ(numChannels, 2u);

    const uint16_t bitsPerSample = *reinterpret_cast<const uint16_t*>(waveTarget.data() + 34);
    const uint32_t dataChunkSize = *reinterpret_cast<const uint32_t*>(waveTarget.data() + 40);
    const uint32_t expectedDataSize = leftAudio.size() * numChannels * bitsPerSample / 8;
    EXPECT_EQ(dataChunkSize, expectedDataSize);

    const size_t dataOffset = 44;
    const size_t bytesPerSample = bitsPerSample / 8;
    const size_t frameSize = numChannels * bytesPerSample;

    EXPECT_EQ(waveTarget.size(), dataOffset + dataChunkSize);
    EXPECT_EQ(dataChunkSize % frameSize, 0u);
}

TEST(AudioFileIO, differentSampleRates)
{
    std::vector<uint8_t> monoTarget44k, stereoTarget96k;
    std::vector<float> monoAudio{1.f, -1.f};
    std::vector<float> leftAudio{0.5f, -0.5f};
    std::vector<float> rightAudio{-0.5f, 0.5f};
    AudioUtility::FileOut::saveMonoToMemory(monoTarget44k, monoAudio, 44100.f);
    AudioUtility::FileOut::saveStereoToMemory(stereoTarget96k, leftAudio, rightAudio, 96000.f);

    const uint32_t sampleRate44k = *reinterpret_cast<const uint32_t*>(monoTarget44k.data() + 24);
    EXPECT_EQ(sampleRate44k, 44100u);

    const uint32_t sampleRate96k = *reinterpret_cast<const uint32_t*>(stereoTarget96k.data() + 24);
    EXPECT_EQ(sampleRate96k, 96000u);

    const uint32_t byteRate44k = *reinterpret_cast<const uint32_t*>(monoTarget44k.data() + 28);
    const uint32_t expectedByteRate44k = 44100 * 1 * 24 / 8;
    EXPECT_EQ(byteRate44k, expectedByteRate44k);

    const uint32_t byteRate96k = *reinterpret_cast<const uint32_t*>(stereoTarget96k.data() + 28);
    const uint32_t expectedByteRate96k = 96000 * 2 * 24 / 8;
    EXPECT_EQ(byteRate96k, expectedByteRate96k);
}

TEST(AudioFileIO, emptyAudioHandling)
{
    std::vector<uint8_t> monoTarget, stereoTarget;
    std::vector<float> emptyAudio;

    AudioUtility::FileOut::saveMonoToMemory(monoTarget, emptyAudio, 48000.f);
    AudioUtility::FileOut::saveStereoToMemory(stereoTarget, emptyAudio, emptyAudio, 48000.f);

    ASSERT_GE(monoTarget.size(), 44u);
    ASSERT_GE(stereoTarget.size(), 44u);

    const uint32_t monoDataSize = *reinterpret_cast<const uint32_t*>(monoTarget.data() + 40);
    EXPECT_EQ(monoDataSize, 0u);

    const uint32_t stereoDataSize = *reinterpret_cast<const uint32_t*>(stereoTarget.data() + 40);
    EXPECT_EQ(stereoDataSize, 0u);

    const uint16_t monoChannels = *reinterpret_cast<const uint16_t*>(monoTarget.data() + 22);
    EXPECT_EQ(monoChannels, 1u);

    const uint16_t stereoChannels = *reinterpret_cast<const uint16_t*>(stereoTarget.data() + 22);
    EXPECT_EQ(stereoChannels, 2u);
}

}