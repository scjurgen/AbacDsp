
#include <gtest/gtest.h>

#include "Audio/FixedSizeProcessor.h"

#include <vector>

template <typename T>
class MockAudioBuffer
{
public:
    MockAudioBuffer(size_t numChannels, size_t numSamples)
        : m_numChannels(numChannels)
          , m_numSamples(numSamples)
          , m_data(numChannels * numSamples)
    {
    }

    [[nodiscard]] size_t getNumChannels() const
    {
        return m_numChannels;
    }

    [[nodiscard]] size_t getNumSamples() const
    {
        return m_numSamples;
    }

    T* getWritePointer(const size_t channel)
    {
        return &m_data[channel * m_numSamples];
    }

    [[nodiscard]] const T* getReadPointer(const size_t channel) const
    {
        return &m_data[channel * m_numSamples];
    }

    const T* plainData()
    {
        return m_data.data();
    }

private:
    size_t m_numChannels;
    size_t m_numSamples;
    std::vector<T> m_data;
};

class FixedSizeProcessorTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
    }

    void TearDown() override
    {
    }
};

TEST_F(FixedSizeProcessorTest, HandlesPartialBigBlock)
{
    constexpr size_t NumChannels{1};
    constexpr size_t NumFixFrames{3};
    size_t processorInternalState = 0;
    auto passThroughProcessor = [&processorInternalState](const AudioBuffer<NumChannels, NumFixFrames>& input,
                                                          AudioBuffer<NumChannels, NumFixFrames>& output)
    {
        for (size_t frame = 0; frame < input.numFrames(); ++frame)
        {
            for (size_t channel = 0; channel < input.numChannels(); ++channel)
            {
                output(frame, channel) = input(frame, channel) + static_cast<float>(processorInternalState);
                processorInternalState++;
            }
        }
    };

    FixedSizeProcessor<NumChannels, NumFixFrames, MockAudioBuffer<float>> sut(passThroughProcessor);

    MockAudioBuffer<float> buffer(NumChannels, 7);

    for (size_t channel = 0; channel < NumChannels; ++channel)
    {
        auto* channelData = buffer.getWritePointer(static_cast<int>(channel));
        for (size_t sample = 0; sample < buffer.getNumSamples(); ++sample)
        {
            channelData[sample] = static_cast<float>(sample + 1);
        }
    }

    sut.processBlock(buffer);
    MockAudioBuffer<float> expectedBuffer(NumChannels, 7);
    expectedBuffer.getWritePointer(0)[0] = 0;
    expectedBuffer.getWritePointer(0)[1] = 0;
    expectedBuffer.getWritePointer(0)[2] = 0;
    expectedBuffer.getWritePointer(0)[3] = 1;
    expectedBuffer.getWritePointer(0)[4] = 3;
    expectedBuffer.getWritePointer(0)[5] = 5;
    expectedBuffer.getWritePointer(0)[6] = 7;

    for (size_t channel = 0; channel < NumChannels; ++channel)
    {
        auto* channelData = buffer.getReadPointer(static_cast<int>(channel));
        auto* expectedData = expectedBuffer.getReadPointer(static_cast<int>(channel));
        for (size_t sample = 0; sample < buffer.getNumSamples(); ++sample)
        {
            EXPECT_FLOAT_EQ(channelData[sample], expectedData[sample]);
        }
    }
    for (size_t channel = 0; channel < NumChannels; ++channel)
    {
        auto* channelData = buffer.getWritePointer(static_cast<int>(channel));
        for (size_t sample = 0; sample < buffer.getNumSamples(); ++sample)
        {
            channelData[sample] = static_cast<float>(sample + 1);
        }
    }
    sut.processBlock(buffer);
    expectedBuffer.getWritePointer(0)[0] = 9;
    expectedBuffer.getWritePointer(0)[1] = 11;
    expectedBuffer.getWritePointer(0)[2] = 13;
    expectedBuffer.getWritePointer(0)[3] = 8; // buffersize processed + 1
    expectedBuffer.getWritePointer(0)[4] = 10;
    expectedBuffer.getWritePointer(0)[5] = 12;
    expectedBuffer.getWritePointer(0)[6] = 14;

    for (size_t channel = 0; channel < NumChannels; ++channel)
    {
        auto* channelData = buffer.getReadPointer(static_cast<int>(channel));
        auto* expectedData = expectedBuffer.getReadPointer(static_cast<int>(channel));
        for (size_t sample = 0; sample < buffer.getNumSamples(); ++sample)
        {
            EXPECT_FLOAT_EQ(channelData[sample], expectedData[sample]);
        }
    }
}

TEST_F(FixedSizeProcessorTest, ProcessesCorrectly)
{
    constexpr size_t NumChannels{2};
    constexpr size_t NumFixFrames{16};
    float gain = 0.5f;
    auto gainProcessor =
        [gain](const AudioBuffer<NumChannels, NumFixFrames>& input, AudioBuffer<NumChannels, NumFixFrames>& output)
    {
        for (size_t frame = 0; frame < input.numFrames(); ++frame)
        {
            for (size_t channel = 0; channel < input.numChannels(); ++channel)
            {
                output(frame, channel) = input(frame, channel) * gain;
            }
        }
    };

    FixedSizeProcessor<NumChannels, NumFixFrames, MockAudioBuffer<float>> sut(gainProcessor);

    MockAudioBuffer<float> buffer(NumChannels, 32);
    for (size_t channel = 0; channel < buffer.getNumChannels(); ++channel)
    {
        auto* channelData = buffer.getWritePointer(channel);
        for (size_t sample = 0; sample < buffer.getNumSamples(); ++sample)
        {
            channelData[sample] = static_cast<float>(sample + 1) / static_cast<float>(buffer.getNumSamples());
        }
    }

    sut.processBlock(buffer);

    for (size_t channel = 0; channel < buffer.getNumChannels(); ++channel)
    {
        auto* channelData = buffer.getReadPointer(channel);
        for (size_t sample = 0; sample < buffer.getNumSamples() - NumFixFrames; ++sample)
        {
            const float expectedValue = static_cast<float>(sample + 1) / static_cast<float>(buffer.getNumSamples()) *
                                        gain;
            EXPECT_NEAR(channelData[sample + NumFixFrames], expectedValue, 1e-6f);
        }
    }
}
