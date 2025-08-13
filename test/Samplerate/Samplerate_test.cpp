#include "gtest/gtest.h"

#include <cmath>

#include "Samplerate/sinc_4_128.h"
#include "Samplerate/VariReader.h"

template <size_t BufferSize, size_t NumChannels>
class VariReaderTest : public ::testing::Test
{
  protected:
    std::shared_ptr<AbacDsp::SincFilter> filter = std::make_shared<AbacDsp::SincFilter>(AbacDsp::init_4_128);
    AbacDsp::VariReader<BufferSize, NumChannels> reader{48000.f, filter};

    void fillRamp(size_t frames, float start = 0.0f, float step = 1.0f)
    {
        std::vector<float> input(frames);
        for (size_t i = 0; i < frames; ++i)
            input[i] = start + i * step;
        reader.process(input.data(), frames);
    }
};

using SampleRateConverterMono = VariReaderTest<1480000, 1>;
using SampleRateConverterStereo = VariReaderTest<1480000, 2>;

TEST_F(SampleRateConverterMono, CheckRatioOfProduction)
{
    constexpr size_t frames = 32;   // input frames
    constexpr size_t slices = 2000; // input frames
    std::vector<float> input(frames);
    for (size_t i = 0; i < 4; ++i)
    {
        input[i] = 1.f;
    }
    std::vector<float> zero(frames, 0.f);

    for (const auto ratio : {1.f, 1.2f, 0.8f, 3.3f, 2.f, 2.1f, 0.5f, 0.222f})
    {
        reader.setRatio(ratio);
        reader.process(input.data(), frames);
        for (size_t j = 0; j < slices - 1; ++j)
        {
            reader.process(zero.data(), frames);
        }
        size_t cntFrames = 0;
        while (!reader.endOfData())
        {
            reader.pull();
            cntFrames++;
        }
        const float cRatio = static_cast<float>(cntFrames) / static_cast<float>(frames * slices);
        EXPECT_NEAR(1.f, cRatio / ratio, 1E-3f);
    }
}

TEST_F(SampleRateConverterMono, checkDelta)
{
    constexpr size_t frames = 32; // input frames
    constexpr size_t slices = 10; // input frames

    std::vector input(frames, 0.f);
    std::vector zero(frames, 0.f);
    for (size_t i = 0; i < 4; ++i)
    {
        input[i] = 1.f;
    }

    struct TestValues
    {
        float ratio;
        size_t expectedCount;
    };

    const std::vector<TestValues> testValues{{1.0f, 22}, {1.2f, 27}, {0.8f, 22}, {3.3f, 75},  {4.0f, 90},
                                             {2.0f, 45}, {2.1f, 48}, {0.5f, 21}, {0.222f, 20}};

    for (const auto& [ratio, expectedCount] : testValues)
    {
        reader.setRatio(ratio);

        reader.process(zero.data(), frames);
        reader.process(zero.data(), frames);
        reader.process(input.data(), frames);

        for (size_t j = 0; j < slices - 1; ++j)
        {
            reader.process(zero.data(), frames);
        }
        size_t cntFrames = 0;
        while (!reader.endOfData())
        {
            if (const auto value = reader.pull(); std::abs(value) > 0.00001f)
            {
                ++cntFrames;
            }
        }
        EXPECT_EQ(expectedCount, cntFrames);
    }
}


TEST_F(SampleRateConverterStereo, checkDelta)
{
    constexpr size_t frames = 32; // input frames
    constexpr size_t slices = 10; // input frames

    std::vector input(frames * 2, 0.f);
    std::vector zero(frames * 2, 0.f);
    for (size_t i = 0; i < 8; ++i)
    {
        input[i] = 1.f;
    }

    struct TestValues
    {
        float ratio;
        size_t expectedCount;
    };
    // same values as Mono, expected will use value * 2
    const std::vector<TestValues> testValues{{1.0f, 22}, {1.2f, 27}, {0.8f, 22}, {3.3f, 75},  {4.0f, 90},
                                             {2.0f, 45}, {2.1f, 48}, {0.5f, 21}, {0.222f, 20}};

    for (const auto& [ratio, expectedCount] : testValues)
    {
        reader.setRatio(ratio);

        reader.process(zero.data(), frames);
        reader.process(zero.data(), frames);
        reader.process(input.data(), frames);

        for (size_t j = 0; j < slices - 1; ++j)
        {
            reader.process(zero.data(), frames);
        }

        size_t cntFrames = 0;
        while (!reader.endOfData())
        {
            if (const auto value = reader.pull(); std::abs(value) > 0.00001f)
            {
                ++cntFrames;
            }
        }

        EXPECT_EQ(expectedCount * 2, cntFrames);
    }
}
