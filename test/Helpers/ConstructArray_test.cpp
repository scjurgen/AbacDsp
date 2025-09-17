#include <gtest/gtest.h>

#include "Helpers/ConstructArray.h"

#include <string>
#include <memory>

namespace AbacDsp
{

class TestClass
{
  public:
    explicit TestClass(const float sampleRate)
        : m_sampleRate(sampleRate)
    {
    }
    explicit TestClass(const float sampleRate, const int channels)
        : m_sampleRate(sampleRate)
        , m_channels(channels)
    {
    }

    float getSampleRate() const
    {
        return m_sampleRate;
    }
    int getChannels() const
    {
        return m_channels;
    }

  private:
    float m_sampleRate = 48000.0f;
    int m_channels = 1;
};

class SimpleClass
{
  public:
    SimpleClass() = default;
    explicit SimpleClass(const int value)
        : m_value(value)
    {
    }

    [[nodiscard]] int getValue() const
    {
        return m_value;
    }

  private:
    int m_value = 42;
};


class ConstructArrayTest : public ::testing::Test
{
  protected:
    void SetUp() override {}

    void TearDown() override {}

    static constexpr float kSampleRate = 48000.0f;
    static constexpr int kChannels = 2;
};

TEST_F(ConstructArrayTest, ConstructArrayWithSingleParameter)
{
    auto testArray = constructArray<TestClass, 4>(kSampleRate);

    EXPECT_EQ(testArray.size(), 4);

    for (const auto& element : testArray)
    {
        EXPECT_EQ(element.getSampleRate(), kSampleRate);
        EXPECT_EQ(element.getChannels(), 1); // default value
    }
}

TEST_F(ConstructArrayTest, ConstructArrayWithMultipleParameters)
{
    auto testArray = constructArray<TestClass, 3>(kSampleRate, kChannels);

    EXPECT_EQ(testArray.size(), 3);

    for (const auto& element : testArray)
    {
        EXPECT_EQ(element.getSampleRate(), kSampleRate);
        EXPECT_EQ(element.getChannels(), kChannels);
    }
}

TEST_F(ConstructArrayTest, ConstructArrayWithNoParameters)
{
    auto testArray = constructArray<SimpleClass, 5>();

    EXPECT_EQ(testArray.size(), 5);

    for (const auto& element : testArray)
    {
        EXPECT_EQ(element.getValue(), 42);
    }
}

TEST_F(ConstructArrayTest, ConstructArrayBuiltInTypes)
{
    auto intArray = constructArray<int, 3>(99);

    EXPECT_EQ(intArray.size(), 3);

    for (const auto& element : intArray)
    {
        EXPECT_EQ(element, 99);
    }
}

TEST_F(ConstructArrayTest, ConstructArrayStrings)
{
    std::string testStr = "Hello World";
    auto stringArray = constructArray<std::string, 2>(testStr);

    EXPECT_EQ(stringArray.size(), 2);

    for (const auto& element : stringArray)
    {
        EXPECT_EQ(element, testStr);
    }
}

template <std::size_t NumChannels>
class FooBank
{
  public:
    explicit FooBank(const float sampleRate)
        : m_foo(constructArray<TestClass, NumChannels>(sampleRate))
    {
    }

    const std::array<TestClass, NumChannels>& getFoos() const
    {
        return m_foo;
    }

  private:
    std::array<TestClass, NumChannels> m_foo;
};

TEST_F(ConstructArrayTest, TemplateUsageInClass)
{
    FooBank<4> fooBank(kSampleRate);

    const auto& foos = fooBank.getFoos();
    EXPECT_EQ(foos.size(), 4);

    for (const auto& foo : foos)
    {
        EXPECT_EQ(foo.getSampleRate(), kSampleRate);
    }
}

TEST_F(ConstructArrayTest, ConstructArraySizeZero)
{
    // Test with size 0 (should compile and work)
    auto emptyArray = constructArray<int, 0>(42);

    EXPECT_EQ(emptyArray.size(), 0);
    EXPECT_TRUE(emptyArray.empty());
}

TEST_F(ConstructArrayTest, ConstructArraySizeOne)
{
    auto singleArray = constructArray<TestClass, 1>(kSampleRate);

    EXPECT_EQ(singleArray.size(), 1);
    EXPECT_EQ(singleArray[0].getSampleRate(), kSampleRate);
}

class ConstructArrayParameterizedTest : public ::testing::TestWithParam<std::size_t>
{
  protected:
    static constexpr float kSampleRate = 48000.0f;
};

TEST_P(ConstructArrayParameterizedTest, VariousSizes)
{
    std::size_t arraySize = GetParam();

    if (arraySize == 1)
    {
        auto arr = constructArray<SimpleClass, 1>();
        EXPECT_EQ(arr.size(), 1);
    }
    else if (arraySize == 2)
    {
        auto arr = constructArray<SimpleClass, 2>();
        EXPECT_EQ(arr.size(), 2);
    }
    else if (arraySize == 4)
    {
        auto arr = constructArray<SimpleClass, 4>();
        EXPECT_EQ(arr.size(), 4);
    }
    else if (arraySize == 8)
    {
        auto arr = constructArray<SimpleClass, 8>();
        EXPECT_EQ(arr.size(), 8);
    }
}

INSTANTIATE_TEST_SUITE_P(SizeTests, ConstructArrayParameterizedTest, ::testing::Values(1, 2, 4, 8));


TEST_F(ConstructArrayTest, AudioProcessingScenario)
{
    constexpr std::size_t kNumChannels = 8;
    constexpr float kSampleRate = 48000.0f;

    class AudioProcessor
    {
      public:
        explicit AudioProcessor(const float sampleRate)
            : m_sampleRate(sampleRate)
            , m_isInitialized(true)
        {
        }

        [[nodiscard]] bool isInitialized() const
        {
            return m_isInitialized;
        }
        [[nodiscard]] float getSampleRate() const
        {
            return m_sampleRate;
        }

      private:
        float m_sampleRate;
        bool m_isInitialized;
    };

    const auto processorArray = constructArray<AudioProcessor, kNumChannels>(kSampleRate);

    EXPECT_EQ(processorArray.size(), kNumChannels);

    for (const auto& processor : processorArray)
    {
        EXPECT_TRUE(processor.isInitialized());
        EXPECT_EQ(processor.getSampleRate(), kSampleRate);
    }
}

TEST_F(ConstructArrayTest, ArgumentForwarding)
{
    class ForwardingTestClass
    {
      public:
        ForwardingTestClass(const std::string& str, int value)
            : m_string(str)
            , m_value(value)
        {
        }

        const std::string& getString() const
        {
            return m_string;
        }
        int getValue() const
        {
            return m_value;
        }

      private:
        std::string m_string;
        int m_value;
    };

    std::string testString = "test_string";
    int testValue = 123;

    auto testArray = constructArray<ForwardingTestClass, 3>(testString, testValue);

    EXPECT_EQ(testArray.size(), 3);

    for (const auto& element : testArray)
    {
        EXPECT_EQ(element.getString(), testString);
        EXPECT_EQ(element.getValue(), testValue);
    }
}
}