#include <algorithm>
#include <array>
#include <cmath>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <string_view>
#include <vector>

#include "Filters/PoleMixingFilter.h"
namespace AbacDsp::Test
{
class FilterTestFixture : public ::testing::Test
{
  protected:
    static constexpr float cfStepMultiplier = 2.f;
    static constexpr float SampleRate = 48000.0f;
    static constexpr size_t BlockSize = 4000;
    static constexpr float InputDb = -40.0f;

    void runMagnitudeTest(size_t index, float resonance, float maxDbError)
    {
        const float inputAmplitude = std::pow(10.0f, InputDb / 20.0f);
        FourStageFilterTheoretical<float> theoretical(SampleRate, {0, 0, 0, 0, 1});
        const auto config = poleMixingList[index];
        {
            for (float hz = 50; hz <= 10000.f; hz *= cfStepMultiplier)
            {
                float dbErrorSum{0.f};
                size_t countSums = 0;
                theoretical.setCoefficients(config.cf);
                for (float cf = 50.f; cf <= 10000.f; cf *= cfStepMultiplier)
                {
                    const float magnitude = static_cast<float>(
                        theoretical.magnitudeBP(Filter1Pole4StageSmooth::adaptResonanceFrequency(cf), hz, resonance));
                    float expectedDb = std::log10(magnitude) * 20.0f;
                    if (expectedDb < -30)
                    {
                        continue;
                    }
                    // ignore if the expectedDb is very low
                    Filter1Pole4StageSmooth filter{SampleRate};
                    filter.setFilterCoefficients(config.cf);
                    filter.setCutoffFrequency(cf);
                    filter.setResonance(resonance);

                    std::vector<float> wave(BlockSize);
                    for (size_t i = 0; i < BlockSize; ++i)
                    {
                        wave[i] = inputAmplitude *
                                  std::sin(2 * std::numbers::pi_v<float> * hz * static_cast<float>(i) / SampleRate);
                    }

                    filter.processBlock(wave.data(), wave.data(), wave.size());

                    const size_t half = wave.size() / 2;
                    auto minmax = std::minmax_element(wave.begin() + half, wave.end());
                    float maxAbs = std::max(std::abs(*minmax.first), std::abs(*minmax.second));
                    float db = std::log10(maxAbs / inputAmplitude) * 20.0f;
                    // adapt db for filter types
                    dbErrorSum += std::abs(db - expectedDb);
                    countSums++;
                }
                if (countSums)
                {
                    EXPECT_NEAR(dbErrorSum / countSums, 0, maxDbError);
                }
            }
        }
    }
};

TEST_F(FilterTestFixture, Resonance0p0)
{
    constexpr float resonance{0.f};
    constexpr float maxDbError{0.1f};
    runMagnitudeTest(findFilterIndex("LP4"), resonance, maxDbError);
    runMagnitudeTest(findFilterIndex("BP4"), resonance, maxDbError);
    runMagnitudeTest(findFilterIndex("HP4"), resonance, maxDbError);
    runMagnitudeTest(findFilterIndex("AP4"), resonance, maxDbError);
}

TEST_F(FilterTestFixture, Resonance1)
{
    constexpr float resonance{1.f};
    constexpr float maxDbError{0.5f};
    runMagnitudeTest(findFilterIndex("LP4"), resonance, maxDbError);
    runMagnitudeTest(findFilterIndex("HP4"), resonance, maxDbError);
}

TEST_F(FilterTestFixture, Resonance2)
{
    constexpr float resonance{2.f};
    constexpr float maxDbError{1.5f};
    runMagnitudeTest(findFilterIndex("LP4"), resonance, maxDbError);
    runMagnitudeTest(findFilterIndex("BP4"), resonance, maxDbError);
}

TEST_F(FilterTestFixture, Resonance3p5)
{
    constexpr float resonance{3.5f};
    constexpr float maxDbError{3.5f};
    runMagnitudeTest(findFilterIndex("LP4"), resonance, maxDbError);
    runMagnitudeTest(findFilterIndex("AP4"), resonance, maxDbError);
}

TEST(PoleMixingFilterTests, magnitudeFunction)
{
    constexpr float sampleRate{48000.f};
    FourStageFilterTheoretical<float> sut(sampleRate, {0, 0, 0, 0, 1});
    for (float f = 250; f < 5000; f *= 1.2f)
    {
        constexpr float cutoff{1000.f};
        const float analog = 20.0f * std::log10(sut.magnitude(cutoff, f, 0.f));
        const float digital = 20.0f * std::log10(sut.magnitudeBP(cutoff, f, 0.f));
        EXPECT_NEAR(analog, digital, 1.f) << "failed at " << f;
    }
    // account for differences at higher frequencies
    for (float f = 5000; f < 12000; f *= 1.2f)
    {
        constexpr float cutoff{1000.f};
        const float analog = 20.0f * std::log10(sut.magnitude(cutoff, f, 0.f));
        const float digital = 20.0f * std::log10(sut.magnitudeBP(cutoff, f, 0.f));
        EXPECT_NEAR(analog, digital, 3.f) << "failed at " << f;
    }
}

TEST(PoleMixingFilterTests, theoreticalPhaseAndBandpassMagnitudeAreFinite)
{
    FourStageFilterTheoretical<float> sut{48000.f, poleMixingList[findFilterIndex("LP4")].cf};
    float phase = 0.0f;
    sut.phase(0.5f, 0.3f, phase);
    EXPECT_TRUE(std::isfinite(phase));
    EXPECT_GE(phase, -std::numbers::pi_v<float>);
    EXPECT_LE(phase, std::numbers::pi_v<float>);

    const float pole = std::exp(-2.0f * std::numbers::pi_v<float> * 1000.f / 48000.f);
    EXPECT_TRUE(std::isfinite(sut.magnitudeBP2(pole, 0.25f, 0.3f)));
}

TEST(PoleMixingFilterTests, findFilterIndexResolvesNamesAndThrowsOnUnknown)
{
    EXPECT_EQ(poleMixingList[findFilterIndex("LP4")].name, "LP4");
    EXPECT_THROW(static_cast<void>(findFilterIndex("no such preset")), std::out_of_range);
}

// --- FixedFourStageFilter (atan saturation, fixed integer weights) ---

TEST(FixedFourStageFilterTests, SmallSignalLowpassPassesDc)
{
    Lp24Smooth sut{48000.f};
    float out = 0.0f;
    for (int i = 0; i < 4000; ++i)
    {
        out = sut.step(0.001f); // tiny input keeps atan saturation in its linear region
    }
    EXPECT_NEAR(out, 0.001f, 1e-4f);
    EXPECT_GT(sut.currentFactor(), 0.0f);
    EXPECT_LT(sut.currentFactor(), 1.0f);
}

TEST(FixedFourStageFilterTests, ResonanceAndAdaptGain)
{
    Lp24Smooth sut{48000.f};
    EXPECT_FLOAT_EQ(sut.correctGain(), 1.0f);
    sut.setAdaptGain(2.0f);
    sut.setResonance(0.5f);
    EXPECT_FLOAT_EQ(sut.correctGain(), 3.0f);
    sut.setAdaptGain(20.0f); // clamped to the 10.0 ceiling
    sut.setResonance(0.5f);
    EXPECT_FLOAT_EQ(sut.correctGain(), 10.0f);
    EXPECT_TRUE(std::isfinite(sut.step(0.2f)));
}

TEST(FixedFourStageFilterTests, WarpTableCoversEverySupportedSampleRate)
{
    for (const float sampleRate : {22050.f, 44100.f, 48000.f, 96000.f, 192000.f, 384000.f})
    {
        Lp24Smooth sut{sampleRate}; // constructor warps the default 1 kHz cutoff for this rate
        EXPECT_GT(sut.currentFactor(), 0.0f);
        EXPECT_LT(sut.currentFactor(), 1.0f);
    }
}

TEST(FixedFourStageFilterTests, SameCutoffIsANoOp)
{
    Lp24Smooth sut{48000.f};
    const float pole = sut.currentFactor();
    EXPECT_FLOAT_EQ(sut.setCutoff(1000.f), 0.0f); // constructor already set 1 kHz
    EXPECT_FLOAT_EQ(sut.currentFactor(), pole);
}

TEST(FixedFourStageFilterTests, StepCountSmoothingGlidesThenSettles)
{
    Lp24Smooth sut{48000.f};
    sut.setSmoothingSteps(64);
    sut.setCutoff(2000.f);

    std::array<float, 32> block{};
    block.fill(0.001f);
    sut.processBlock(block.data(), block.data(), block.size()); // 32 < 64: partial glide
    EXPECT_TRUE(std::isfinite(block[0]));

    std::array<float, 64> rest{};
    rest.fill(0.001f);
    sut.processBlock(rest.data(), rest.data(), rest.size()); // remaining 32 steps finish, then steady state
    EXPECT_TRUE(std::isfinite(rest.back()));

    Lp24Smooth reference{48000.f};
    reference.setCutoff(2000.f); // no smoothing: pole jumps straight to target
    EXPECT_NEAR(sut.currentFactor(), reference.currentFactor(), 1e-6f);
}

TEST(FixedFourStageFilterTests, ProcessBlockInplaceMatchesStep)
{
    Lp24Smooth blockFilter{48000.f};
    Lp24Smooth stepFilter{48000.f};
    std::array<float, 16> buffer{};
    for (size_t i = 0; i < buffer.size(); ++i)
    {
        buffer[i] = 0.01f * static_cast<float>((i % 2) == 0 ? 1 : -1);
    }
    std::array<float, 16> expected{};
    for (size_t i = 0; i < buffer.size(); ++i)
    {
        expected[i] = stepFilter.step(buffer[i]);
    }
    blockFilter.processBlockInplace(buffer.data(), buffer.size());
    for (size_t i = 0; i < buffer.size(); ++i)
    {
        EXPECT_FLOAT_EQ(buffer[i], expected[i]);
    }
    blockFilter.reset();
    EXPECT_FLOAT_EQ(blockFilter.step(0.0f), 0.0f);
}

// --- ResonanceFrequencyModifier ---

TEST(ResonanceFrequencyModifierTests, CompensatesResonanceAboveThreshold)
{
    ResonanceFrequencyModifier sut{48000.f}; // threshold is sampleRate * 0.125 = 6000 Hz
    sut.setUserResonance(0.5f);

    sut.setFrequency(3000.f); // below threshold: resonance passes through unchanged
    EXPECT_FLOAT_EQ(sut.getCurrentResonance(), 0.5f);
    EXPECT_FLOAT_EQ(sut.getUserResonance(), 0.5f);
    EXPECT_FLOAT_EQ(sut.getCurrentFrequency(), 3000.f);

    sut.setFrequency(12000.f); // above threshold: scaled by 6000 / frequency
    EXPECT_FLOAT_EQ(sut.getCurrentResonance(), 0.5f * 6000.f / 12000.f);
}

// --- FourStageOnePoleFilterNoResonance ---

TEST(FourStageOnePoleFilterNoResonanceTests, LowpassHasUnityDcGain)
{
    FourStageOnePoleFilterNoResonance<0, 0, 0, 0, 1> sut{48000.f};
    sut.setCutoff(1000.f);
    float out = 0.0f;
    for (int i = 0; i < 4000; ++i)
    {
        out = sut.singleStep(1.0f);
    }
    EXPECT_NEAR(out, 1.0f, 1e-3f);

    std::array<float, 8> in{};
    in.fill(1.0f);
    std::array<float, 8> target{};
    sut.processBlock(in.data(), target.data(), in.size());
    EXPECT_TRUE(std::isfinite(target.back()));
    sut.reset();
    EXPECT_FLOAT_EQ(sut.singleStep(0.0f), 0.0f);
}

// --- Filter1Pole4StageSmooth setters not reached by the magnitude sweep ---

TEST(Filter1Pole4StageSmoothTests, CleanCutoffSmoothingAndResonanceAdaption)
{
    Filter1Pole4StageSmooth sut{48000.f};
    sut.setFilterCoefficients(poleMixingList[findFilterIndex("LP4")].cf);
    sut.setCutoffFrequencyClean(1000.f); // bypasses the resonance-frequency adaption
    sut.setParameterSmoothTimeMs(5.0f);
    sut.setResonance(0.5f);

    std::array<float, 64> block{};
    block.fill(0.01f);
    sut.processBlock(block.data(), block.data(), block.size());
    EXPECT_TRUE(std::isfinite(block.back()));
    sut.reset();
    EXPECT_FLOAT_EQ(sut.step(0.0f), 0.0f);

    // both branches of the resonance-frequency adaption (threshold at 2800 Hz)
    for (const float hz : {1000.f, 5000.f})
    {
        const float adapted = Filter1Pole4StageSmooth::adaptResonanceFrequency(hz);
        EXPECT_GE(adapted, 10.f);
        EXPECT_LE(adapted, 22000.f);
    }
}
}