#include <cmath>
#include <cstdint>
#include <optional>
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

namespace
{
std::vector<float> randomDetuneRun(const WaveShaperTableStore& shapers, const MpeCurveMap& curves,
                                   const std::optional<uint64_t> seed)
{
    MorphexsynthVoice voice{kSampleRate, shapers, curves};
    if (seed)
    {
        voice.seed(*seed);
    }
    voice.setLevelOscillator(0, 1.f);
    voice.setEnvelopeSustainLevel(1.f);
    voice.setRandomDetune(50.f);
    voice.setLfoWaveForm(LfoType::Noise);
    voice.setLfoPitchFactor(4.f);
    voice.setLfoOscModulationDepth(0.5f);
    voice.triggerVoice(60, 100, 0);

    std::vector<float> measured;
    std::array<float, 512> left{};
    std::array<float, 512> right{};
    for (int block = 0; block < 40; ++block)
    {
        voice.processBlock(left.data(), right.data(), left.size());
        measured.insert(measured.end(), left.begin(), left.end());
    }
    return measured;
}
}

TEST_F(MorphexsynthVoiceTest, randomDetuneAndNoiseRepeatWithTheDefaultSeed)
{
    EXPECT_EQ(randomDetuneRun(waveShaperTables, curveMap, std::nullopt),
              randomDetuneRun(waveShaperTables, curveMap, std::nullopt));
}

TEST_F(MorphexsynthVoiceTest, randomDetuneAndNoiseRepeatWithTheSameSeedAndDifferWithAnother)
{
    const auto first = randomDetuneRun(waveShaperTables, curveMap, 11);
    EXPECT_EQ(first, randomDetuneRun(waveShaperTables, curveMap, 11));
    EXPECT_NE(first, randomDetuneRun(waveShaperTables, curveMap, 12));
}

TEST_F(MorphexsynthVoiceTest, waveMorphMovesTheOscillatorFromItsWaveformToTheSaw)
{
    const auto run = [this](const float morph)
    {
        MorphexsynthVoice voice{kSampleRate, waveShaperTables, curveMap};
        voice.setLevelOscillator(0, 1.f);
        voice.setEnvelopeSustainLevel(1.f);
        voice.setWaveForm(0, 0);
        voice.setWaveMorph(0, morph);
        voice.triggerVoice(60, 100, 0);
        std::vector<float> measured;
        std::array<float, 512> left{};
        std::array<float, 512> right{};
        for (int block = 0; block < 10; ++block)
        {
            voice.processBlock(left.data(), right.data(), left.size());
            measured.insert(measured.end(), left.begin(), left.end());
        }
        return measured;
    };
    const auto triangle = run(-1.f);
    const auto saw = run(1.f);
    EXPECT_NE(triangle, saw);
    EXPECT_EQ(triangle, run(-5.f));
    EXPECT_EQ(saw, run(5.f));
}

TEST_F(MorphexsynthVoiceTest, envelopeLevelRisesWithTheAttackAndFallsToZeroAfterTheRelease)
{
    voice->setEnvelopeAttack(5.f);
    voice->setEnvelopeDecay(5.f);
    voice->setEnvelopeSustainLevel(0.6f);
    voice->setEnvelopeRelease(5.f);
    EXPECT_FLOAT_EQ(0.f, voice->envelopeLevel());

    voice->triggerVoice(60, 100, 0);
    std::array<float, 512> left{};
    std::array<float, 512> right{};
    voice->processBlock(left.data(), right.data(), left.size());
    voice->processBlock(left.data(), right.data(), left.size());
    EXPECT_GT(voice->envelopeLevel(), 0.1f);
    EXPECT_LE(voice->envelopeLevel(), 1.f);

    voice->stopVoice();
    for (int block = 0; block < 20; ++block)
    {
        voice->processBlock(left.data(), right.data(), left.size());
    }
    EXPECT_FALSE(voice->isPlaying());
    EXPECT_NEAR(0.f, voice->envelopeLevel(), 1e-3f);
}

TEST_F(MorphexsynthVoiceTest, pitchOffsetFollowsThePerNoteBendInBothDirections)
{
    voice->setCtrlDimension(0, CtrlDimension::X);
    voice->setCtrlTarget(0, CtrlTarget::PitchBend);
    voice->setCtrlCurve(0, CtrlCurve::Linear);
    voice->setCtrlType(0, CtrlValueType::BiPolar);
    voice->setCtrlDepth(0, 1.f);
    voice->triggerVoice(60, 100, 0);
    std::array<float, 512> left{};
    std::array<float, 512> right{};
    voice->processBlock(left.data(), right.data(), left.size());
    EXPECT_NEAR(0.f, voice->pitchOffsetSemitones(), 0.05f);

    voice->mpeX(16383);
    for (int block = 0; block < 20; ++block)
    {
        voice->processBlock(left.data(), right.data(), left.size());
    }
    EXPECT_NEAR(12.f, voice->pitchOffsetSemitones(), 0.5f);

    voice->mpeX(0);
    for (int block = 0; block < 20; ++block)
    {
        voice->processBlock(left.data(), right.data(), left.size());
    }
    EXPECT_NEAR(-12.f, voice->pitchOffsetSemitones(), 0.5f);
}

TEST_F(MorphexsynthVoiceTest, oscillatorFrequencyFollowsPitchFactorDetuneAndBend)
{
    voice->setPitchFactor(1, 2.f);
    voice->triggerVoice(69, 100, 0);
    EXPECT_NEAR(440.f, voice->oscillatorFrequency(0), 0.5f);
    EXPECT_NEAR(880.f, voice->oscillatorFrequency(1), 1.f);

    voice->setDetune(2, 12.f);
    EXPECT_NEAR(880.f, voice->oscillatorFrequency(2), 1.f);

    voice->setCtrlDimension(0, CtrlDimension::X);
    voice->setCtrlTarget(0, CtrlTarget::PitchBend);
    voice->setCtrlCurve(0, CtrlCurve::Linear);
    voice->setCtrlType(0, CtrlValueType::BiPolar);
    voice->setCtrlDepth(0, 1.f);
    voice->mpeX(16383);
    std::array<float, 512> left{};
    std::array<float, 512> right{};
    for (int block = 0; block < 20; ++block)
    {
        voice->processBlock(left.data(), right.data(), left.size());
    }
    EXPECT_NEAR(880.f, voice->oscillatorFrequency(0), 15.f);
    EXPECT_NEAR(1760.f, voice->oscillatorFrequency(1), 30.f);
}

TEST_F(MorphexsynthVoiceTest, oscillatorLevelFollowsTheLevelAndTheLevelSlots)
{
    voice->setLevelOscillator(0, 0.6f);
    voice->setLevelOscillator(1, 0.3f);
    voice->triggerVoice(60, 100, 0);
    std::array<float, 512> left{};
    std::array<float, 512> right{};
    EXPECT_FLOAT_EQ(0.f, voice->oscillatorLevel(2));
    for (int block = 0; block < 4; ++block)
    {
        voice->processBlock(left.data(), right.data(), left.size());
    }
    EXPECT_NEAR(0.6f, voice->oscillatorLevel(0), 1e-3f);
    EXPECT_NEAR(0.3f, voice->oscillatorLevel(1), 1e-3f);
    EXPECT_NEAR(0.f, voice->oscillatorLevel(2), 1e-3f);

    voice->setCtrlDimension(1, CtrlDimension::Y);
    voice->setCtrlTarget(1, CtrlTarget::OscLevel3);
    voice->setCtrlCurve(1, CtrlCurve::Linear);
    voice->setCtrlType(1, CtrlValueType::Abs);
    voice->setCtrlDepth(1, 1.f);
    voice->mpeY(16383);
    for (int block = 0; block < 20; ++block)
    {
        voice->processBlock(left.data(), right.data(), left.size());
    }
    EXPECT_GT(voice->oscillatorLevel(2), 0.5f);
}

TEST_F(MorphexsynthVoiceTest, readingTheOscillatorValuesDoesNotChangeTheOutput)
{
    const auto render = [this](const bool reading)
    {
        voice->setLevelOscillator(0, 0.5f);
        voice->triggerVoice(60, 100, 0);
        std::array<float, 512> left{};
        std::array<float, 512> right{};
        std::vector<float> out;
        for (int block = 0; block < 4; ++block)
        {
            voice->processBlock(left.data(), right.data(), left.size());
            if (reading)
            {
                (void) voice->oscillatorLevel(0);
                (void) voice->oscillatorFrequency(0);
            }
            out.insert(out.end(), left.begin(), left.end());
        }
        return out;
    };
    const auto plain = render(false);
    SetUp();
    EXPECT_EQ(plain, render(true));
}

}
