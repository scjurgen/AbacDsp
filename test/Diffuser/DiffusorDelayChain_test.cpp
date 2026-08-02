#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <gtest/gtest.h>
#include <numeric>

#include "Diffuser/DiffusorDelayChain.h"
#include "Numbers/Convert.h"

namespace AbacDsp::Test
{

constexpr size_t kBlockSize{16};
constexpr float kSampleRate{48000.f};

class DiffuserDelayChainTest : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        m_sut.resetDiffuser(6, 0.5f, 0.46f, 0.7f, 7.f, skipSmoothing);
    }

    [[nodiscard]] float blockEnergy(const std::array<float, kBlockSize>& block) const
    {
        return std::accumulate(block.begin(), block.end(), 0.f,
                               [](const float acc, const float v) { return acc + v * v; });
    }

    DiffuserDelayChain<24000, 24> m_sut{kSampleRate, kBlockSize};
};

TEST_F(DiffuserDelayChainTest, zeroFeedbackPassesInputThrough)
{
    m_sut.setFeedback(0.f);
    std::array<float, kBlockSize> in{};
    in[0] = 1.f;
    in[5] = -0.5f;
    std::array<float, kBlockSize> out{};
    m_sut.processBlock(in.data(), out.data(), kBlockSize);
    for (size_t i = 0; i < kBlockSize; ++i)
    {
        EXPECT_FLOAT_EQ(out[i], in[i]);
    }
}

TEST_F(DiffuserDelayChainTest, impulseResponseSpreadsAndDecays)
{
    std::array<float, kBlockSize> in{};
    in[0] = 1.f;
    std::array<float, kBlockSize> out{};
    m_sut.processBlock(in.data(), out.data(), kBlockSize);

    float earlyEnergy = blockEnergy(out);
    std::array<float, kBlockSize> silence{};
    for (size_t block = 0; block < 200; ++block)
    {
        m_sut.processBlock(silence.data(), out.data(), kBlockSize);
        earlyEnergy += blockEnergy(out);
    }
    EXPECT_GT(earlyEnergy, 0.f);

    float lateEnergy = 0.f;
    for (size_t block = 0; block < 60000 / kBlockSize; ++block)
    {
        m_sut.processBlock(silence.data(), out.data(), kBlockSize);
        lateEnergy = blockEnergy(out);
    }
    EXPECT_LT(lateEnergy, earlyEnergy * 0.01f);
    EXPECT_TRUE(std::isfinite(lateEnergy));
}

TEST_F(DiffuserDelayChainTest, elementCountReportsScheduledValue)
{
    EXPECT_EQ(m_sut.elements(), 6u);
    m_sut.setElements(12);
    EXPECT_EQ(m_sut.elements(), 12u);
}

TEST_F(DiffuserDelayChainTest, changingElementsWhileProcessingStaysFinite)
{
    std::array<float, kBlockSize> in{};
    std::array<float, kBlockSize> out{};
    in[0] = 1.f;
    m_sut.processBlock(in.data(), out.data(), kBlockSize);
    in[0] = 0.f;

    m_sut.setElements(12);
    for (size_t block = 0; block < 48000 / kBlockSize; ++block)
    {
        m_sut.processBlock(in.data(), out.data(), kBlockSize);
    }
    m_sut.setElements(3);
    for (size_t block = 0; block < 48000 / kBlockSize; ++block)
    {
        m_sut.processBlock(in.data(), out.data(), kBlockSize);
    }
    for (const auto v : out)
    {
        EXPECT_TRUE(std::isfinite(v));
    }
}

TEST_F(DiffuserDelayChainTest, parameterChangesStayFinite)
{
    m_sut.setBulge(6, 0.8f);
    m_sut.setBottomSize(2.f);
    m_sut.setTopSize(50.f);
    m_sut.setDamper(8000.f);
    m_sut.setAllPassFirstCutoff(300.f);
    m_sut.setAllPassLastCutoff(3000.f);
    m_sut.setModulationDepth(0.5f);
    m_sut.setModulationSpeed(1.f);

    std::array<float, kBlockSize> in{};
    std::array<float, kBlockSize> out{};
    in[0] = 1.f;
    for (size_t block = 0; block < 48000 / kBlockSize; ++block)
    {
        m_sut.processBlock(in.data(), out.data(), kBlockSize);
        in[0] = 0.f;
    }
    for (const auto v : out)
    {
        EXPECT_TRUE(std::isfinite(v));
    }
}

// Regression test for a bug where scaleDiffuser() fed the offset-perturbed per-element sizes
// through generateUniquePrimeSet, which sorts by value before assigning primes back
// positionally: once a spread offset flipped two neighboring elements' relative order, their
// assigned delay lengths swapped between elements instead of drifting by the offset. Element 0
// always gets a positive offset (invertParity=false), so its size must never drop below its
// spread=0 baseline, however large the spread -- the old bug could make it drop far below by
// handing it element 1's (much smaller, offset-shrunk) target instead of its own.
TEST(DiffuserDelayChainSizeSpreadTest, positivelyOffsetElementNeverShrinksBelowBaseline)
{
    constexpr size_t kBlockSize{16};
    constexpr float kSampleRate{48000.f};
    DiffuserDelayChain<24000, 24> chain{kSampleRate, kBlockSize};
    // Bottom/top close together so two elements pack tightly, making it easy for a modest
    // spread to exceed half the gap between them and flip their natural ordering.
    chain.resetDiffuser(2, 0.3f, 0.f, 0.5f, 0.55f, skipSmoothing);

    const auto before = chain.getElementSizesInSamples();
    ASSERT_LT(before[0], before[1]);
    const auto gap = before[1] - before[0];

    const auto spreadSamples = static_cast<float>(gap) * 5.f;
    const auto spreadMeters = Convert::samplesToMeters(spreadSamples, kSampleRate);
    chain.setSizeSpread(spreadMeters, false);

    // Size changes ramp in smoothly rather than jumping instantly; let them settle before
    // reading the result back.
    std::array<float, kBlockSize> in{};
    std::array<float, kBlockSize> out{};
    for (size_t block = 0; block < 48000 / kBlockSize; ++block)
    {
        chain.processBlock(in.data(), out.data(), kBlockSize);
    }

    const auto after = chain.getElementSizesInSamples();
    EXPECT_GE(after[0], before[0]);
}

TEST_F(DiffuserDelayChainTest, levelSinkStaysNullSafeByDefault)
{
    std::array<float, kBlockSize> in{};
    in[0] = 1.f;
    std::array<float, kBlockSize> out{};
    m_sut.processBlock(in.data(), out.data(), kBlockSize);
    for (const auto v : out)
    {
        EXPECT_TRUE(std::isfinite(v));
    }
}

TEST_F(DiffuserDelayChainTest, levelSinkBin0ReflectsInputPeak)
{
    std::array<std::atomic<float>, 25> sink{};
    m_sut.setLevelMeterSink(&sink);
    std::array<float, kBlockSize> in{};
    std::fill(in.begin(), in.end(), 0.5f);
    std::array<float, kBlockSize> out{};
    m_sut.processBlock(in.data(), out.data(), kBlockSize);
    EXPECT_NEAR(sink[0].load(), 0.5f, 1E-6f);
}

TEST_F(DiffuserDelayChainTest, levelSinkLastActiveBinTracksSustainedSignal)
{
    std::array<std::atomic<float>, 25> sink{};
    m_sut.setLevelMeterSink(&sink);
    std::array<float, kBlockSize> in{};
    std::fill(in.begin(), in.end(), 1.f);
    std::array<float, kBlockSize> out{};
    for (size_t block = 0; block < 20; ++block)
    {
        m_sut.processBlock(in.data(), out.data(), kBlockSize);
    }
    EXPECT_GT(sink[6].load(), 0.01f);
    EXPECT_TRUE(std::isfinite(sink[6].load()));
}

TEST_F(DiffuserDelayChainTest, levelSinkTracksInstantaneousPeakPerBlock)
{
    std::array<std::atomic<float>, 25> sink{};
    m_sut.setLevelMeterSink(&sink);
    std::array<float, kBlockSize> in{};
    std::fill(in.begin(), in.end(), 1.f);
    std::array<float, kBlockSize> out{};
    m_sut.processBlock(in.data(), out.data(), kBlockSize);
    EXPECT_NEAR(sink[0].load(), 1.f, 1E-6f);

    // No ballistic smoothing at this layer anymore: a silent block reads back as silence
    // immediately, rather than decaying gradually.
    std::array<float, kBlockSize> silence{};
    m_sut.processBlock(silence.data(), out.data(), kBlockSize);
    EXPECT_NEAR(sink[0].load(), 0.f, 1E-6f);
}

TEST_F(DiffuserDelayChainTest, levelSinkBinsBeyondActiveCountSitAtFloor)
{
    std::array<std::atomic<float>, 25> sink{};
    m_sut.setLevelMeterSink(&sink);
    std::array<float, kBlockSize> in{};
    in[0] = 1.f;
    std::array<float, kBlockSize> out{};
    m_sut.processBlock(in.data(), out.data(), kBlockSize);

    for (size_t bin = 7; bin <= 24; ++bin)
    {
        EXPECT_EQ(sink[bin].load(), 0.f);
    }
}

// A constant-DC input settles to a bit-exact steady output (delaying a constant is still that
// same constant, so modulation has no effect on it), which would make a "does it change"
// check pass trivially once things settle. Feed a slowly moving signal instead so a real,
// continuously live meter is expected to keep changing no matter how long it has been running.
[[nodiscard]] std::array<float, kBlockSize> slowlyVaryingBlock(const size_t block) noexcept
{
    std::array<float, kBlockSize> blockData{};
    std::fill(blockData.begin(), blockData.end(), 0.7f + 0.3f * std::sin(static_cast<float>(block) * 0.05f));
    return blockData;
}

TEST_F(DiffuserDelayChainTest, levelSinkKeepsUpdatingWhileElementCountGrows)
{
    std::array<std::atomic<float>, 25> sink{};
    m_sut.setLevelMeterSink(&sink);
    std::array<float, kBlockSize> out{};
    for (size_t block = 0; block < 20; ++block)
    {
        m_sut.processBlock(slowlyVaryingBlock(block).data(), out.data(), kBlockSize);
    }
    const float readingBeforeFade = sink[3].load();
    ASSERT_GT(readingBeforeFade, 0.f);

    // 6 -> 12 elements enters the crossfade path (increaseNumElements) for many blocks (fade
    // length is clamped to at least 12000 samples), so the loop below stays inside the fade.
    m_sut.setElements(12);
    bool bin3Changed = false;
    for (size_t block = 20; block < 70 && !bin3Changed; ++block)
    {
        m_sut.processBlock(slowlyVaryingBlock(block).data(), out.data(), kBlockSize);
        bin3Changed = sink[3].load() != readingBeforeFade;
    }
    EXPECT_TRUE(bin3Changed);
}

TEST_F(DiffuserDelayChainTest, levelSinkKeepsUpdatingWhileElementCountShrinks)
{
    std::array<std::atomic<float>, 25> sink{};
    m_sut.setLevelMeterSink(&sink);
    std::array<float, kBlockSize> out{};
    m_sut.setElements(12);
    size_t block = 0;
    for (; block < 48000 / kBlockSize; ++block)
    {
        m_sut.processBlock(slowlyVaryingBlock(block).data(), out.data(), kBlockSize);
    }
    ASSERT_EQ(m_sut.elements(), 12u);
    const float readingBeforeFade = sink[3].load();

    // 12 -> 3 elements enters the crossfade path (decreaseNumElements); bin 3 stays within the
    // settled (new, shorter) portion of the chain for the whole transition.
    m_sut.setElements(3);
    bool bin3Changed = false;
    for (size_t endBlock = block + 50; block < endBlock && !bin3Changed; ++block)
    {
        m_sut.processBlock(slowlyVaryingBlock(block).data(), out.data(), kBlockSize);
        bin3Changed = sink[3].load() != readingBeforeFade;
    }
    EXPECT_TRUE(bin3Changed);
}

TEST_F(DiffuserDelayChainTest, tapSpanZeroMatchesCurrentBehavior)
{
    DiffuserDelayChain<24000, 24> withoutTapSpan{kSampleRate, kBlockSize};
    DiffuserDelayChain<24000, 24> withTapSpanZero{kSampleRate, kBlockSize};
    withoutTapSpan.resetDiffuser(6, 0.5f, 0.46f, 0.7f, 7.f, skipSmoothing);
    withTapSpanZero.resetDiffuser(6, 0.5f, 0.46f, 0.7f, 7.f, skipSmoothing);
    withTapSpanZero.setTapSpan(0.f);

    std::array<float, kBlockSize> in{};
    in[0] = 1.f;
    std::array<float, kBlockSize> outA{};
    std::array<float, kBlockSize> outB{};
    for (size_t block = 0; block < 20; ++block)
    {
        withoutTapSpan.processBlock(in.data(), outA.data(), kBlockSize);
        withTapSpanZero.processBlock(in.data(), outB.data(), kBlockSize);
        in[0] = 0.f;
        for (size_t i = 0; i < kBlockSize; ++i)
        {
            EXPECT_FLOAT_EQ(outA[i], outB[i]);
        }
    }
}

// With a DC input, a stage's per-block peak equals its settled output value, so the level
// sink can stand in for a per-sample probe of an otherwise inaccessible intermediate tap.
TEST_F(DiffuserDelayChainTest, tapSpanOneHundredAveragesAllActiveElements)
{
    DiffuserDelayChain<24000, 24> reference{kSampleRate, kBlockSize};
    DiffuserDelayChain<24000, 24> tapped{kSampleRate, kBlockSize};
    reference.resetDiffuser(2, 0.3f, 0.46f, 0.7f, 7.f, skipSmoothing);
    tapped.resetDiffuser(2, 0.3f, 0.46f, 0.7f, 7.f, skipSmoothing);
    reference.setModulationDepth(0.f);
    tapped.setModulationDepth(0.f);
    tapped.setTapSpan(100.f);

    std::array<std::atomic<float>, 25> sink{};
    reference.setLevelMeterSink(&sink);

    std::array<float, kBlockSize> in{};
    std::fill(in.begin(), in.end(), 1.f);
    std::array<float, kBlockSize> refOut{};
    std::array<float, kBlockSize> tappedOut{};
    for (size_t block = 0; block < 6000; ++block)
    {
        reference.processBlock(in.data(), refOut.data(), kBlockSize);
        tapped.processBlock(in.data(), tappedOut.data(), kBlockSize);
    }

    const auto expected = (sink[1].load() + sink[2].load()) / std::sqrt(2.f);
    for (const auto v : tappedOut)
    {
        EXPECT_NEAR(v, expected, 5E-3f);
    }
}

TEST_F(DiffuserDelayChainTest, tapSpanStaysWithinActiveElementCountAfterShrinking)
{
    std::array<float, kBlockSize> in{};
    std::array<float, kBlockSize> out{};
    in[0] = 1.f;
    m_sut.setElements(3);
    for (size_t block = 0; block < 48000 / kBlockSize; ++block)
    {
        m_sut.processBlock(in.data(), out.data(), kBlockSize);
        in[0] = 0.f;
    }
    ASSERT_EQ(m_sut.elements(), 3u);

    m_sut.setTapSpan(100.f);
    in[0] = 1.f;
    for (size_t block = 0; block < 100; ++block)
    {
        m_sut.processBlock(in.data(), out.data(), kBlockSize);
        in[0] = 0.f;
        for (const auto v : out)
        {
            EXPECT_TRUE(std::isfinite(v));
        }
    }
}

TEST_F(DiffuserDelayChainTest, tapSpanStaysFiniteAcrossElementCountFade)
{
    m_sut.setTapSpan(50.f);
    std::array<float, kBlockSize> in{};
    std::array<float, kBlockSize> out{};
    in[0] = 1.f;
    m_sut.processBlock(in.data(), out.data(), kBlockSize);
    in[0] = 0.f;

    m_sut.setElements(12);
    for (size_t block = 0; block < 48000 / kBlockSize; ++block)
    {
        m_sut.processBlock(in.data(), out.data(), kBlockSize);
    }
    m_sut.setElements(3);
    for (size_t block = 0; block < 48000 / kBlockSize; ++block)
    {
        m_sut.processBlock(in.data(), out.data(), kBlockSize);
    }
    for (const auto v : out)
    {
        EXPECT_TRUE(std::isfinite(v));
    }
}
}
