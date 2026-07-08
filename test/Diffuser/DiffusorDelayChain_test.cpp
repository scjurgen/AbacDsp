#include <gtest/gtest.h>

#include "Diffuser/DiffusorDelayChain.h"

#include <array>
#include <cmath>
#include <numeric>

namespace AbacDsp::Test
{

constexpr size_t kBlockSize{16};
constexpr float kSampleRate{48000.f};

class DiffuserDelayChainTest : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        m_sut.resetDiffuser(6, 0.5f, 0.46f, 100.f, 1000.f, skipSmoothing);
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
    m_sut.setBottomSize(200.f);
    m_sut.setTopSize(5000.f);
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

}
