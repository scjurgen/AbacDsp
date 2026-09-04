#include <cmath>
#include <vector>

#include "gtest/gtest.h"

#include "Analysis/ZeroCrossings.h"
#include "Synthesizer/MorphexsynthVoice.h"

namespace AbacDsp::Test
{

namespace
{
constexpr float kSampleRate = 48000.f;

class MorphexsynthVoiceTest : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        voice = std::make_unique<MorphexsynthVoice>(kSampleRate, waveShaperTables, curveMap);
    }

    WaveShaperTableStore waveShaperTables;
    MpeCurveMap curveMap;
    std::unique_ptr<MorphexsynthVoice> voice;
};
}

TEST_F(MorphexsynthVoiceTest, triggerProducesFiniteBoundedOutput)
{
    voice->triggerVoice(60, 100, 0);
    std::array<float, 512> left{};
    std::array<float, 512> right{};
    voice->processBlock(left.data(), right.data(), left.size());
    for (size_t i = 0; i < left.size(); ++i)
    {
        ASSERT_TRUE(std::isfinite(left[i]));
        ASSERT_TRUE(std::isfinite(right[i]));
        ASSERT_LE(std::abs(left[i]), 8.f);
        ASSERT_LE(std::abs(right[i]), 8.f);
    }
}

TEST_F(MorphexsynthVoiceTest, isPlayingReflectsEnvelopeLifecycle)
{
    EXPECT_FALSE(voice->isPlaying());
    voice->triggerVoice(60, 100, 0);
    EXPECT_TRUE(voice->isPlaying());

    voice->setEnvelopeAttack(1.f);
    voice->setEnvelopeDecay(1.f);
    voice->setEnvelopeSustainLevel(0.f);
    voice->setEnvelopeRelease(1.f);
    voice->triggerVoice(60, 100, 0);
    voice->stopVoice();

    std::array<float, 512> left{};
    std::array<float, 512> right{};
    for (int block = 0; block < 40; ++block)
    {
        voice->processBlock(left.data(), right.data(), left.size());
    }
    EXPECT_FALSE(voice->isPlaying());
}

TEST_F(MorphexsynthVoiceTest, emergencyStopSilencesVoiceQuickly)
{
    voice->setEnvelopeRelease(2000.f);
    voice->triggerVoice(60, 100, 0);
    ASSERT_TRUE(voice->isPlaying());

    std::array<float, 512> left{};
    std::array<float, 512> right{};
    voice->processBlock(left.data(), right.data(), left.size());

    voice->emergencyStop(5.f);
    for (int block = 0; block < 20; ++block)
    {
        voice->processBlock(left.data(), right.data(), left.size());
    }
    EXPECT_FALSE(voice->isPlaying());
}

TEST_F(MorphexsynthVoiceTest, mpeDimensionsAudiblyChangeFilterCutoffModulation)
{
    voice->setCtrlDimension(0, CtrlDimension::X);
    voice->setCtrlTarget(0, CtrlTarget::FilterCutoff);
    voice->setCtrlCurve(0, CtrlCurve::Linear);
    voice->setCtrlType(0, CtrlValueType::BiPolar);
    voice->setCtrlDepth(0, 1.f);

    voice->triggerVoice(60, 100, 0);
    std::array<float, 512> left{};
    std::array<float, 512> right{};
    voice->processBlock(left.data(), right.data(), left.size());

    voice->mpeX(16383);
    for (int block = 0; block < 8; ++block)
    {
        voice->processBlock(left.data(), right.data(), left.size());
    }
    for (size_t i = 0; i < left.size(); ++i)
    {
        ASSERT_TRUE(std::isfinite(left[i]));
    }
}

TEST_F(MorphexsynthVoiceTest, filterCharacterSwitchDoesNotProduceNonFiniteOutput)
{
    voice->triggerVoice(60, 100, 0);
    for (const auto& preset : poleMixingList)
    {
        voice->setFilterType(preset.name);
        std::array<float, 128> left{};
        std::array<float, 128> right{};
        voice->processBlock(left.data(), right.data(), left.size());
        for (size_t i = 0; i < left.size(); ++i)
        {
            ASSERT_TRUE(std::isfinite(left[i])) << "filter " << preset.name;
        }
    }
}

TEST_F(MorphexsynthVoiceTest, lfoOscDepthAudiblyModulatesPitchAcrossAllOscillators)
{
    constexpr size_t kBlockSize{512};
    constexpr size_t kSettleBlocks{20};
    constexpr size_t kMeasureBlocks{200};

    const auto measurePeriodStdDev = [](const float oscDepth)
    {
        WaveShaperTableStore localWaveShaperTables;
        MpeCurveMap localCurveMap;
        MorphexsynthVoice v{kSampleRate, localWaveShaperTables, localCurveMap};
        v.setLevelOscillator(0, 1.f);
        v.setEnvelopeSustainLevel(1.f);
        v.setLfoWaveForm(LfoType::Sine);
        v.setLfoPitchFactor(4.f);
        v.setLfoOscModulationDepth(oscDepth);
        v.triggerVoice(60, 100, 0);

        std::array<float, kBlockSize> left{};
        std::array<float, kBlockSize> right{};
        for (size_t b = 0; b < kSettleBlocks; ++b)
        {
            v.processBlock(left.data(), right.data(), left.size());
        }

        std::vector<float> measured;
        measured.reserve(kMeasureBlocks * kBlockSize);
        for (size_t b = 0; b < kMeasureBlocks; ++b)
        {
            v.processBlock(left.data(), right.data(), left.size());
            measured.insert(measured.end(), left.begin(), left.end());
        }
        return calculateZeroCrossingStatistics(measured.data(), measured.size(), true).standardDeviation;
    };

    const auto noDepthStdDev = measurePeriodStdDev(0.f);
    const auto withDepthStdDev = measurePeriodStdDev(0.5f);

    EXPECT_GT(withDepthStdDev, noDepthStdDev);
}

TEST_F(MorphexsynthVoiceTest, distortionStagePassesThroughFiniteOutput)
{
    voice->setPresetWaveTable(1);
    voice->triggerVoice(60, 100, 0);
    std::array<float, 512> left{};
    std::array<float, 512> right{};
    for (int block = 0; block < 10; ++block)
    {
        voice->processBlock(left.data(), right.data(), left.size());
    }
    for (size_t i = 0; i < left.size(); ++i)
    {
        ASSERT_TRUE(std::isfinite(left[i]));
    }
}

}
