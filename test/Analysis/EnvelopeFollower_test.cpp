#include "gtest/gtest.h"
#include "Analysis/EnvelopeFollower.h"

#include <vector>
#include <cmath>
#include <numeric>
#include <algorithm>

namespace AbacDsp::Test
{

constexpr float kTolerance = 1e-5f;
constexpr float kSampleRate = 48000.0f;
constexpr size_t kWindowSize = 10;


TEST(RmsFollowerTest, MultipleValuesFeed)
{
    RmsFollower sut{kWindowSize};
    const std::vector<float> buffer = {1.0f, 0.5f, -0.5f, 0.25f};

    sut.feed(buffer.data(), buffer.size());

    constexpr float sumSquares = 1.0f + 0.25f + 0.25f + 0.0625f;
    constexpr auto expectedMs = sumSquares / static_cast<float>(kWindowSize);
    const auto expectedRms = std::sqrt(expectedMs);

    EXPECT_NEAR(sut.getRms(), expectedRms, kTolerance);
    EXPECT_NEAR(sut.getMs(), expectedMs, kTolerance);
}

TEST(RmsFollowerTest, FillWindowCompletelyWithOnes)
{
    RmsFollower sut{kWindowSize};
    const std::vector<float> buffer(kWindowSize, 1.0f);

    sut.feed(buffer.data(), buffer.size());

    EXPECT_NEAR(sut.getRms(), 1.0f, kTolerance);
    EXPECT_NEAR(sut.getMs(), 1.0f, kTolerance);
}

TEST(RmsFollowerTest, OverfillWindowWithSlidingEffect)
{
    RmsFollower sut{kWindowSize};
    const std::vector<float> ones(kWindowSize, 1.0f);
    sut.feed(ones.data(), ones.size());

    EXPECT_NEAR(sut.getRms(), 1.0f, kTolerance);

    const std::vector<float> zeros(kWindowSize, 0.0f);
    sut.feed(zeros.data(), zeros.size());

    EXPECT_NEAR(sut.getRms(), 0.0f, kTolerance) << "sliding window should have cleared all ones";
    EXPECT_NEAR(sut.getMs(), 0.0f, kTolerance);
}

TEST(RmsFollowerTest, WindowSizeChange)
{
    RmsFollower sut{kWindowSize};
    const std::vector<float> ones(kWindowSize, 1.0f);
    sut.feed(ones.data(), ones.size());

    constexpr size_t newWindowSize = 5;
    sut.setWindowSize(newWindowSize);

    EXPECT_NEAR(sut.getRms(), 1.0f, kTolerance);

    const std::vector<float> mixed = {0.5f, -0.5f};
    sut.feed(mixed.data(), mixed.size());

    constexpr auto expectedMs = (3.0f + 0.25f + 0.25f) / static_cast<float>(newWindowSize);
    EXPECT_NEAR(sut.getMs(), expectedMs, kTolerance);
}

TEST(RmsFollowerTest, MinimumWindowSize)
{
    RmsFollower sut{kWindowSize};
    sut.setWindowSize(0);

    constexpr auto testValue = 2.0f;
    const std::vector<float> buffer = {testValue};
    sut.feed(buffer.data(), buffer.size());

    EXPECT_NEAR(sut.getRms(), testValue, kTolerance) << "minimum window size should be 1";
    EXPECT_NEAR(sut.getMs(), testValue * testValue, kTolerance);
}

TEST(RmsFollowerTest, LargeBufferProcessing)
{
    RmsFollower sut{kWindowSize};
    constexpr size_t bufferSize = kWindowSize * 3;
    std::vector<float> buffer(bufferSize);
    std::iota(buffer.begin(), buffer.end(), 1.0f);

    sut.feed(buffer.data(), buffer.size());

    float sumSquares = 0.0f;
    for (size_t i = bufferSize - kWindowSize; i < bufferSize; ++i)
    {
        sumSquares += buffer[i] * buffer[i];
    }
    const auto expectedMs = sumSquares / static_cast<float>(kWindowSize);
    const auto expectedRms = std::sqrt(expectedMs);

    EXPECT_NEAR(sut.getRms(), expectedRms, kTolerance);
    EXPECT_NEAR(sut.getMs(), expectedMs, kTolerance);
}

TEST(RmsFollowerTest, NegativeValuesSquaredCorrectly)
{
    RmsFollower sut{kWindowSize};
    const std::vector<float> buffer = {-1.0f, -2.0f, -3.0f};

    sut.feed(buffer.data(), buffer.size());

    constexpr auto expectedMs = (1.0f + 4.0f + 9.0f) / static_cast<float>(kWindowSize);
    const auto expectedRms = std::sqrt(expectedMs);

    EXPECT_NEAR(sut.getRms(), expectedRms, kTolerance);
    EXPECT_NEAR(sut.getMs(), expectedMs, kTolerance);
}

TEST(PeakEnvelopeFollowerTest, SingleStepPositiveValue)
{
    PeakEnvelopeFollower<60> sut{kSampleRate};
    sut.setAttackInMsecs(1.0f);
    sut.setReleaseInMsecs(100.0f);

    constexpr auto inputValue = 0.5f;
    const auto result = sut.step(inputValue);

    EXPECT_GT(result, 0.0f);
    EXPECT_LE(result, inputValue) << "envelope should not exceed input value";
    EXPECT_NEAR(sut.getEnvelope(), result, kTolerance);
}

TEST(PeakEnvelopeFollowerTest, SingleStepNegativeValue)
{
    PeakEnvelopeFollower<60> sut{kSampleRate};
    sut.setAttackInMsecs(1.0f);
    sut.setReleaseInMsecs(100.0f);

    constexpr auto inputValue = -0.5f;
    const auto result = sut.step(inputValue);

    EXPECT_GT(result, 0.0f) << "envelope should be positive for negative input";
    EXPECT_LE(result, std::abs(inputValue));
    EXPECT_NEAR(sut.getEnvelope(), result, kTolerance);
}

TEST(PeakEnvelopeFollowerTest, AttackBehavior)
{
    PeakEnvelopeFollower<60> sut{kSampleRate};
    sut.setAttackInMsecs(1.0f);
    sut.setReleaseInMsecs(100.0f);

    constexpr auto lowValue = 0.1f;
    constexpr auto highValue = 0.9f;

    sut.step(lowValue);
    const auto envelopeAfterLow = sut.getEnvelope();

    const auto envelopeAfterHigh = sut.step(highValue);

    EXPECT_GT(envelopeAfterHigh, envelopeAfterLow) << "attack should increase envelope quickly";
    EXPECT_GT(envelopeAfterHigh, lowValue);
}

TEST(PeakEnvelopeFollowerTest, ReleaseBehavior)
{
    PeakEnvelopeFollower<60> sut{kSampleRate};
    sut.setAttackInMsecs(1.0f);
    sut.setReleaseInMsecs(100.0f);

    constexpr auto highValue = 0.9f;
    constexpr auto lowValue = 0.1f;

    sut.step(highValue);
    const auto envelopeAfterHigh = sut.getEnvelope();

    const auto envelopeAfterLow = sut.step(lowValue);

    EXPECT_LT(envelopeAfterLow, envelopeAfterHigh) << "release should decrease envelope";
    EXPECT_GT(envelopeAfterLow, lowValue) << "release should be slower than attack";
}

TEST(PeakEnvelopeFollowerTest, MultipleStepsConvergence)
{
    PeakEnvelopeFollower<60> sut{kSampleRate};
    sut.setAttackInMsecs(1.0f);
    sut.setReleaseInMsecs(100.0f);

    constexpr auto targetValue = 0.7f;
    float previousEnvelope = 0.0f;

    for (int i = 0; i < 100; ++i)
    {
        const auto currentEnvelope = sut.step(targetValue);
        EXPECT_GE(currentEnvelope, previousEnvelope) << "envelope should increase towards target";
        previousEnvelope = currentEnvelope;
    }

    EXPECT_GT(sut.getEnvelope(), targetValue * 0.8f) << "should converge close to target";
}

TEST(PeakEnvelopeFollowerTest, BlockAnalyze)
{
    PeakEnvelopeFollower<60> sut{kSampleRate};
    sut.setAttackInMsecs(1.0f);
    sut.setReleaseInMsecs(100.0f);

    const std::vector<float> source = {0.1f, 0.5f, -0.8f, 0.3f, 0.0f};
    std::vector<float> target(source.size());

    sut.blockAnalyze(source.data(), target.data(), source.size());

    for (const auto value : target)
    {
        EXPECT_GE(value, 0.0f) << "envelope values must be non-negative";
    }

    EXPECT_GT(target[1], target[0]) << "0.5 > 0.1 should increase envelope";
    EXPECT_GT(target[2], target[1]) << "fast attack should handle 0.8 > 0.5 jump";
}

TEST(PeakEnvelopeFollowerTest, AttackTimeComparison)
{
    PeakEnvelopeFollower<60> sutFast{kSampleRate};
    sutFast.setAttackInMsecs(0.1f);

    PeakEnvelopeFollower<60> sutSlow{kSampleRate};
    sutSlow.setAttackInMsecs(10.0f);

    constexpr auto inputValue = 1.0f;
    const auto fastResult = sutFast.step(inputValue);
    const auto slowResult = sutSlow.step(inputValue);

    EXPECT_GT(fastResult, slowResult) << "fast attack should reach higher envelope in single step";
}

TEST(PeakEnvelopeFollowerTest, ReleaseTimeComparison)
{
    PeakEnvelopeFollower<60> sutFast{kSampleRate};
    sutFast.setAttackInMsecs(0.0f);
    sutFast.setReleaseInMsecs(1.0f);

    PeakEnvelopeFollower<60> sutSlow{kSampleRate};
    sutSlow.setAttackInMsecs(0.0f);
    sutSlow.setReleaseInMsecs(1000.0f);

    constexpr auto highValue = 1.0f;
    constexpr auto lowValue = 0.1f;

    sutFast.step(highValue);
    sutSlow.step(highValue);

    const auto fastResult = sutFast.step(lowValue);
    const auto slowResult = sutSlow.step(lowValue);

    EXPECT_LT(fastResult, slowResult) << "fast release should drop more towards low value";
}

TEST(PeakEnvelopeFollowerTest, DifferentDbRangeTemplate)
{
    PeakEnvelopeFollower<40> sut40db{kSampleRate};
    sut40db.setAttackInMsecs(10.0f);
    sut40db.setReleaseInMsecs(10.0f);

    PeakEnvelopeFollower<80> sut80db{kSampleRate};
    sut80db.setAttackInMsecs(10.0f);
    sut80db.setReleaseInMsecs(10.0f);

    constexpr auto inputValue = 0.5f;
    const auto result40 = sut40db.step(inputValue);
    const auto result80 = sut80db.step(inputValue);

    EXPECT_GE(result40, 0.0f);
    EXPECT_GE(result80, 0.0f);
}

TEST(PeakEnvelopeFollowerTest, ZeroInputHandling)
{
    PeakEnvelopeFollower<60> sut{kSampleRate};
    sut.setAttackInMsecs(1.0f);
    sut.setReleaseInMsecs(100.0f);

    sut.step(0.5f);
    EXPECT_GT(sut.getEnvelope(), 0.0f);

    auto previousEnvelope = sut.getEnvelope();
    for (int i = 0; i < 10; ++i)
    {
        const auto currentEnvelope = sut.step(0.0f);
        EXPECT_LE(currentEnvelope, previousEnvelope) << "envelope should decrease during release";
        EXPECT_GE(currentEnvelope, 0.0f) << "envelope must remain non-negative";
        previousEnvelope = currentEnvelope;
    }
}

TEST(PeakEnvelopeFollowerTest, ConsistencyBetweenStepAndBlockAnalyze)
{
    PeakEnvelopeFollower<60> sutBlock{kSampleRate};
    sutBlock.setAttackInMsecs(1.0f);
    sutBlock.setReleaseInMsecs(100.0f);

    PeakEnvelopeFollower<60> sutStep{kSampleRate};
    sutStep.setAttackInMsecs(1.0f);
    sutStep.setReleaseInMsecs(100.0f);

    const std::vector source = {0.1f, 0.3f, 0.7f};
    std::vector<float> blockResults(source.size());

    sutBlock.blockAnalyze(source.data(), blockResults.data(), source.size());

    std::vector<float> stepResults;
    stepResults.reserve(source.size());
    for (const auto value : source)
    {
        stepResults.push_back(sutStep.step(value));
    }

    for (size_t i = 0; i < source.size(); ++i)
    {
        EXPECT_NEAR(blockResults[i], stepResults[i], kTolerance)
            << "step and blockAnalyze must produce identical results at index " << i;
    }
}
}