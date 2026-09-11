#include <cmath>

#include "gtest/gtest.h"

#include "Synthesizer/AmbientSynthVoice.h"

namespace AbacDsp::Test
{

namespace
{
constexpr float kSampleRate = 48000.f;

class AmbientSynthVoiceTest : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        voice = std::make_unique<AmbientSynthVoice>(kSampleRate, waveShaperTables);
    }

    WaveShaperTableStore waveShaperTables;
    std::unique_ptr<AmbientSynthVoice> voice;
};

void assertFiniteAndBounded(const std::array<float, 512>& block)
{
    for (const auto sample : block)
    {
        ASSERT_TRUE(std::isfinite(sample));
        ASSERT_LE(std::abs(sample), 8.f);
    }
}
}

TEST_F(AmbientSynthVoiceTest, triggerProducesFiniteBoundedOutput)
{
    voice->triggerVoice(69, 100);
    std::array<float, 512> mono{};
    voice->processBlock(mono.data(), mono.size());
    assertFiniteAndBounded(mono);
}

TEST_F(AmbientSynthVoiceTest, isPlayingReflectsEnvelopeLifecycle)
{
    EXPECT_FALSE(voice->isPlaying());
    voice->triggerVoice(69, 100);
    EXPECT_TRUE(voice->isPlaying());

    voice->stopVoice();
    std::array<float, 512> mono{};
    for (int block = 0; block < 400; ++block)
    {
        voice->processBlock(mono.data(), mono.size());
    }
    EXPECT_FALSE(voice->isPlaying());
}

TEST_F(AmbientSynthVoiceTest, sweepingMaterialAndCutoffStaysFiniteAndBounded)
{
    voice->triggerVoice(69, 100);
    std::array<float, 512> mono{};
    for (int step = 0; step <= 20; ++step)
    {
        const auto position = static_cast<float>(step) / 20.f;
        voice->setMaterial(position);
        voice->setCutoff(1.f - position);
        voice->processBlock(mono.data(), mono.size());
        assertFiniteAndBounded(mono);
    }
}

TEST_F(AmbientSynthVoiceTest, materialRangeAtExtremesStaysFiniteAndBounded)
{
    voice->setMaterialRange(1.f);
    voice->triggerVoice(69, 100);
    std::array<float, 512> mono{};
    for (const auto center : {0.f, 0.5f, 1.f})
    {
        voice->setMaterial(center);
        for (int block = 0; block < 20; ++block)
        {
            voice->processBlock(mono.data(), mono.size());
            assertFiniteAndBounded(mono);
        }
    }
}

TEST_F(AmbientSynthVoiceTest, modulationRangesAtMaximumStayFiniteAndBounded)
{
    voice->setCutoffOuRange(48.f);
    voice->setResonanceRange(1.f);
    voice->setBreathOuRange(10.f);
    voice->setPitchOuRange(100.f);
    voice->setCutoffLfo(60.f, 48.f, 0.f);
    voice->setResonanceLfo(60.f, 5.f, 0.f);
    voice->setMaterialLfo(60.f, 1.f, 0.f);
    voice->setPitchLfo(60.f, 100.f, 0.f);
    voice->setBreathLfo(60.f, 10.f, 0.f);
    voice->setDriftLfo(60.f, 100.f, 0.f);
    voice->triggerVoice(69, 100);
    std::array<float, 512> mono{};
    for (int block = 0; block < 40; ++block)
    {
        voice->processBlock(mono.data(), mono.size());
        assertFiniteAndBounded(mono);
    }
}

TEST_F(AmbientSynthVoiceTest, filterTypeSwitchesDiscretelyWithoutBlending)
{
    voice->triggerVoice(69, 100);
    std::array<float, 512> mono{};
    voice->processBlock(mono.data(), mono.size());

    for (const auto type : {FilterType::LP4, FilterType::LP2, FilterType::LP1Notch, FilterType::Notch, FilterType::BP2,
                            FilterType::HP1LP3, FilterType::AP4})
    {
        voice->setFilterType(type);
        voice->processBlock(mono.data(), mono.size());
        assertFiniteAndBounded(mono);
        EXPECT_EQ(voice->snapshot().filterType, type);
    }
}

TEST_F(AmbientSynthVoiceTest, distortionStagePassesThroughFiniteOutput)
{
    voice->setDistortion(1);
    voice->triggerVoice(69, 100);
    std::array<float, 512> mono{};
    for (int block = 0; block < 10; ++block)
    {
        voice->processBlock(mono.data(), mono.size());
        assertFiniteAndBounded(mono);
    }
}

TEST_F(AmbientSynthVoiceTest, zeroModulationRangesKeepCutoffAndPitchStable)
{
    voice->setCutoffOuRange(0.f);
    voice->setResonanceRange(0.f);
    voice->setMaterialRange(0.f);
    voice->setBreathOuRange(0.f);
    voice->setPitchOuRange(0.f);
    voice->triggerVoice(69, 100);
    std::array<float, 512> mono{};
    voice->processBlock(mono.data(), mono.size());
    const auto first = voice->snapshot();

    for (int block = 0; block < 200; ++block)
    {
        voice->processBlock(mono.data(), mono.size());
    }
    const auto later = voice->snapshot();

    EXPECT_NEAR(later.filterCutoffHz, first.filterCutoffHz, 1.f);
    EXPECT_NEAR(later.osc0Hz, first.osc0Hz, 0.01f);
    EXPECT_NEAR(later.osc1Hz, first.osc1Hz, 0.01f);
}

TEST_F(AmbientSynthVoiceTest, setNoteRepitchesLiveWithoutRetriggeringEnvelope)
{
    voice->triggerVoice(69, 100);
    std::array<float, 512> mono{};
    voice->processBlock(mono.data(), mono.size());
    ASSERT_TRUE(voice->isPlaying());
    const auto before = voice->snapshot();

    voice->setNote(81); // +12 semitones
    voice->processBlock(mono.data(), mono.size());
    assertFiniteAndBounded(mono);
    const auto after = voice->snapshot();

    EXPECT_TRUE(voice->isPlaying());
    EXPECT_NEAR(after.envelope, before.envelope, 0.05f);
    EXPECT_NEAR(after.osc0Hz / before.osc0Hz, 2.f, 0.05f);
}

TEST_F(AmbientSynthVoiceTest, setPitchGlidesGraduallyThenReachesTarget)
{
    voice->triggerVoice(69, 100);
    std::array<float, 512> mono{};
    voice->processBlock(mono.data(), mono.size());
    const auto before = voice->snapshot();

    constexpr float glideSeconds = 0.5f;
    voice->setPitch(81, 0.f, glideSeconds); // +12 semitones (x2 Hz), glided
    voice->processBlock(mono.data(), mono.size());
    assertFiniteAndBounded(mono);
    const auto justStarted = voice->snapshot();
    EXPECT_LT(justStarted.osc0Hz / before.osc0Hz, 1.5f); // nowhere near the target yet

    const auto blocksForGlide = static_cast<int>(glideSeconds * kSampleRate / static_cast<float>(mono.size())) + 5;
    for (int block = 0; block < blocksForGlide; ++block)
    {
        voice->processBlock(mono.data(), mono.size());
        assertFiniteAndBounded(mono);
    }
    const auto after = voice->snapshot();
    EXPECT_NEAR(after.osc0Hz / before.osc0Hz, 2.f, 0.05f);
}

TEST_F(AmbientSynthVoiceTest, setPitchCentsCombineWithNoteLikeSetNoteOnAWholeSemitone)
{
    voice->triggerVoice(69, 100);
    std::array<float, 512> mono{};
    voice->processBlock(mono.data(), mono.size());

    voice->setPitch(69, 100.f, 0.f); // +100 cents == +1 semitone, instant
    voice->processBlock(mono.data(), mono.size());
    const auto viaCents = voice->snapshot();

    voice = std::make_unique<AmbientSynthVoice>(kSampleRate, waveShaperTables);
    voice->triggerVoice(69, 100);
    voice->processBlock(mono.data(), mono.size());
    voice->setNote(70);
    voice->processBlock(mono.data(), mono.size());
    const auto viaNote = voice->snapshot();

    EXPECT_NEAR(viaCents.osc0Hz, viaNote.osc0Hz, 0.01f);
}

TEST_F(AmbientSynthVoiceTest, gainSmoothingReachesTargetWithoutDiscontinuity)
{
    voice->triggerVoice(69, 100);
    std::array<float, 512> mono{};
    voice->processBlock(mono.data(), mono.size());

    voice->setGain(-12.f);
    float previous = mono.back();
    for (int block = 0; block < 20; ++block)
    {
        voice->processBlock(mono.data(), mono.size());
        for (const auto sample : mono)
        {
            ASSERT_LE(std::abs(sample - previous), 1.f);
            previous = sample;
        }
    }
}

}
