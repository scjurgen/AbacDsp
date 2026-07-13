#include <gtest/gtest.h>
#include <vector>

#include "Analysis/Spectrogram.h"

namespace AbacDsp::Test
{

TEST(Spectrogram, melSpectroGramProcessesBlockWithoutCrashing)
{
    MelSpectroGram spec;
    const std::vector<float> block(2048, 0.1f);
    spec.processBlock(block);

    const auto imageSet = spec.getImageSet();
    EXPECT_EQ(imageSet.size(), imageSet.width * imageSet.fftHalfLength);
    EXPECT_NE(imageSet.data, nullptr);
}

TEST(SimpleSpectrogram, processesBlockWithoutCrashing)
{
    SimpleSpectrogram spec;
    spec.setSampleRate(48000.f);
    const std::vector<float> block(4096, 0.1f);
    spec.processBlock(block);

    const auto imageSet = spec.getImageSet();
    EXPECT_EQ(imageSet.size(), imageSet.width * imageSet.height);
    EXPECT_NE(imageSet.data, nullptr);
}

TEST(FloatingHorizonFFTImage, processesBlockWithoutCrashing)
{
    FloatingHorizonFFTImage spec;
    const std::vector<float> block(4096, 0.1f);
    spec.processBlock(block);

    const auto imageSet = spec.getImageSet();
    EXPECT_EQ(imageSet.size(), imageSet.width * imageSet.height);
    EXPECT_NE(imageSet.data, nullptr);
}

}
