#include <cmath>
#include <iostream>
#include <limits>
#include <numbers>
#include <string>
#include <vector>

#include "AudioFile/SaveWav.h"
#include "gtest/gtest.h"

#include "Analysis/FftMisc.h"
#include "Delays/PitchFadeWindowDelay.h"

namespace AbacDsp::Test
{

namespace
{
constexpr size_t MaxSize{4800};
constexpr float SampleRate{48000.f};

// 4800 samples hold a whole number of periods of e.g. 220 Hz, which would make any
// buffer-length jump land back on the same phase and hide the very defect under test.
constexpr float ProbeFreq{237.f};

struct Config
{
    float semitones{0.f};
    bool reverse{false};
    size_t window{MaxSize / 2};
    size_t fadeTime{MaxSize / 2};
    bool pitchSynchronous{false};
};

// A pitch shifted sine is still a sine, so its sample-to-sample step cannot exceed the
// slope of a sine at the shifted frequency. Anything beyond that is a discontinuity in
// the material, which is what a click is.
[[nodiscard]] float worstJump(const Config& cfg)
{
    PitchFadeWindowDelay<MaxSize> sut;
    sut.setSize(cfg.window);
    sut.setFadeTime(cfg.fadeTime);
    sut.setReverse(cfg.reverse);
    if (cfg.pitchSynchronous)
    {
        sut.enablePitchSynchronousMode(SampleRate);
    }
    sut.setPitch(cfg.semitones);

    constexpr size_t NumSamples{48000 * 2};
    std::vector<float> out(NumSamples);
    for (size_t i = 0; i < NumSamples; ++i)
    {
        const auto phase = 2.f * std::numbers::pi_v<float> * ProbeFreq * static_cast<float>(i) / SampleRate;
        out[i] = sut.step(std::sin(phase));
    }

    float worst = 0.f;
    // skip the startup transient: the buffer holds silence until it has been filled once
    for (size_t i = MaxSize + 1; i < NumSamples; ++i)
    {
        worst = std::max(worst, std::fabs(out[i] - out[i - 1]));
    }
    return worst;
}

/*
 * Energy above cutoffHz relative to total energy, in dB.
 *
 * The grain machine legitimately modulates the tone: the random per grain offset combs
 * it and the crossfade amplitude modulates it. Both put sidebands close to the carrier,
 * so a band well above it stays clean. A read head that fails to advance is a step
 * discontinuity, and its energy reaches Nyquist.
 *
 * This sums the band rather than taking its loudest bin: a click is concentrated in
 * time, so an FFT spreads it thinly over thousands of bins and no single bin stands out.
 */
[[nodiscard]] float highBandEnergyDb(const std::vector<float>& signal, const float cutoffHz)
{
    std::vector<float> magnitude;
    BasicFFT::realDataToMagnitude<float, FftHannWindow>(signal, magnitude);

    const auto binHz = SampleRate / static_cast<float>(signal.size());
    const auto firstHighBin = static_cast<size_t>(cutoffHz / binHz);

    double total{0.0};
    double high{0.0};
    for (size_t i = 0; i < magnitude.size(); ++i)
    {
        const auto energy = static_cast<double>(magnitude[i]) * static_cast<double>(magnitude[i]);
        total += energy;
        if (i >= firstHighBin)
        {
            high += energy;
        }
    }

    if (high <= 0.0 || total <= 0.0)
    {
        return -std::numeric_limits<float>::infinity();
    }
    return static_cast<float>(10.0 * std::log10(high / total));
}

constexpr size_t FftSize{16384};

// renders past the startup transient, in the geometry BlockProcPitch asks for
[[nodiscard]] std::vector<float> renderSettled(const float freq, const float semitones,
                                               const bool pitchSynchronous = false)
{
    PitchFadeWindowDelay<MaxSize> sut;
    sut.setSize(MaxSize / 2);
    sut.setFadeTime(MaxSize / 2);
    if (pitchSynchronous)
    {
        sut.enablePitchSynchronousMode(SampleRate);
    }
    sut.setPitch(semitones);

    constexpr size_t Skip{MaxSize * 2};
    std::vector<float> settled(FftSize);
    for (size_t i = 0; i < Skip + FftSize; ++i)
    {
        const auto phase = 2.f * std::numbers::pi_v<float> * freq * static_cast<float>(i) / SampleRate;
        const auto value = sut.step(std::sin(phase));
        if (i >= Skip)
        {
            settled[i - Skip] = value;
        }
    }
    return settled;
}

[[nodiscard]] float maxSineSlope(const float semitones)
{
    const auto ratio = std::pow(2.f, semitones / 12.f);
    return 2.f * std::numbers::pi_v<float> * ProbeFreq * std::max(ratio, 1.f) / SampleRate;
}

/*
 * Spurious-free dynamic range around the carrier, in dB: the largest bin outside a guard
 * band around the fundamental, relative to the fundamental peak itself. The grain machine
 * legitimately puts sidebands close to the carrier (the per grain placement combs it,
 * the crossfade amplitude modulates it), so this is exactly the metric that separates a
 * randomly jittered placement from a period-locked one - a lower (more negative) number
 * is cleaner.
 */
[[nodiscard]] float carrierSfdrDb(const std::vector<float>& signal, const float carrierHz)
{
    std::vector<float> magnitude;
    BasicFFT::realDataToMagnitude<float, FftHannWindow>(signal, magnitude);

    const auto binHz = SampleRate / static_cast<float>(signal.size());
    const auto carrierBin = static_cast<size_t>(std::lround(carrierHz / binHz));
    constexpr size_t GuardBins{5};
    const auto guardLow = carrierBin > GuardBins ? carrierBin - GuardBins : size_t{0};
    const auto guardHigh = std::min(carrierBin + GuardBins, magnitude.size() - 1);

    double peakEnergy{0.0};
    for (size_t i = guardLow; i <= guardHigh; ++i)
    {
        peakEnergy = std::max(peakEnergy, static_cast<double>(magnitude[i]) * static_cast<double>(magnitude[i]));
    }

    double spuriousEnergy{0.0};
    for (size_t i = 1; i < magnitude.size(); ++i)
    {
        if (i >= guardLow && i <= guardHigh)
        {
            continue;
        }
        const auto energy = static_cast<double>(magnitude[i]) * static_cast<double>(magnitude[i]);
        spuriousEnergy = std::max(spuriousEnergy, energy);
    }

    if (peakEnergy <= 0.0 || spuriousEnergy <= 0.0)
    {
        return -std::numeric_limits<float>::infinity();
    }
    return static_cast<float>(10.0 * std::log10(spuriousEnergy / peakEnergy));
}
}

TEST(PitchFadeWindowDelayTest, forwardShiftIsFreeOfDiscontinuities)
{
    for (const float semitones : {-12.f, -5.f, 0.f, 7.f, 12.f})
    {
        const Config cfg{semitones, false, MaxSize / 2, MaxSize / 2};
        EXPECT_LT(worstJump(cfg), maxSineSlope(semitones) * 1.5f) << "semitones " << semitones;
    }
}

TEST(PitchFadeWindowDelayTest, reverseShiftIsFreeOfDiscontinuities)
{
    for (const float semitones : {-12.f, 0.f, 12.f})
    {
        const Config cfg{semitones, true, MaxSize / 2, MaxSize / 2};
        EXPECT_LT(worstJump(cfg), maxSineSlope(semitones) * 1.5f) << "semitones " << semitones;
    }
}

// FdnReverb never calls setSize/setFadeTime, so the defaults must be safe on their own.
TEST(PitchFadeWindowDelayTest, defaultGeometryIsFreeOfDiscontinuities)
{
    PitchFadeWindowDelay<MaxSize> sut;
    sut.setPitch(12.f);

    constexpr size_t NumSamples{48000 * 2};
    std::vector<float> out(NumSamples);
    for (size_t i = 0; i < NumSamples; ++i)
    {
        const auto phase = 2.f * std::numbers::pi_v<float> * ProbeFreq * static_cast<float>(i) / SampleRate;
        out[i] = sut.step(std::sin(phase));
    }

    float worst = 0.f;
    for (size_t i = MaxSize + 1; i < NumSamples; ++i)
    {
        worst = std::max(worst, std::fabs(out[i] - out[i - 1]));
    }
    EXPECT_LT(worst, maxSineSlope(12.f) * 1.5f);
}

TEST(PitchFadeWindowDelayTest, fadeTimeLargerThanWindowStillProducesGrains)
{
    PitchFadeWindowDelay<MaxSize> sut;
    sut.setSize(600);
    sut.setFadeTime(MaxSize);
    sut.setPitch(7.f);

    // an over-long fade must not stall the grain machine or drive the output out of range
    for (size_t i = 0; i < MaxSize * 4; ++i)
    {
        const auto value = sut.step(1.f);
        ASSERT_TRUE(std::isfinite(value));
        ASSERT_LE(std::fabs(value), 1.5f) << "at sample " << i;
    }
}

// A 200 Hz tone through the grain machine must not gain broadband content. It did: the
// outgoing head repeated its last sample at every handoff, one step every grain period.
TEST(PitchFadeWindowDelayTest, grainHandoffAddsNoBroadbandSpray)
{
    constexpr float Freq{200.f};
    constexpr float Cutoff{5000.f};

    // a stalled handoff measures about -60 dB here, a clean one about -89 dB
    for (const float semitones : {0.f, 7.f})
    {
        const auto settled = renderSettled(Freq, semitones);
        const auto energy = highBandEnergyDb(settled, Cutoff);
        EXPECT_LT(energy, -80.f) << "broadband energy above " << Cutoff << " Hz is " << energy << " dB, at "
                                 << semitones << " semitones";
    }
}

TEST(PitchFadeWindowDelayTest, unityPitchForwardReproducesInput)
{
    PitchFadeWindowDelay<MaxSize> sut;
    sut.setSize(MaxSize / 2);
    sut.setFadeTime(MaxSize / 8);
    sut.setPitch(0.f);

    constexpr size_t NumSamples{48000};
    float worst = 0.f;
    for (size_t i = 0; i < NumSamples; ++i)
    {
        const auto phase = 2.f * std::numbers::pi_v<float> * ProbeFreq * static_cast<float>(i) / SampleRate;
        const auto value = sut.step(std::sin(phase));
        if (i > MaxSize)
        {
            worst = std::max(worst, std::fabs(value));
        }
    }
    // at unity the grains carry the sine unchanged, so the envelope must stay near full scale
    EXPECT_GT(worst, 0.9f);
    EXPECT_LT(worst, 1.1f);
}

TEST(PitchFadeWindowDelayTest, changingPitchWhileRunningStaysBounded)
{
    PitchFadeWindowDelay<MaxSize> sut;
    sut.setSize(MaxSize / 2);
    sut.setFadeTime(MaxSize / 4);

    for (size_t i = 0; i < 48000; ++i)
    {
        const auto phase = 2.f * std::numbers::pi_v<float> * ProbeFreq * static_cast<float>(i) / SampleRate;
        // sweep the pitch the way a host automating a dial would
        sut.setPitch(12.f * std::sin(static_cast<float>(i) / 4000.f));
        const auto value = sut.step(std::sin(phase));
        ASSERT_TRUE(std::isfinite(value));
        ASSERT_LE(std::fabs(value), 1.5f) << "at sample " << i;
    }
}

TEST(PitchFadeWindowDelayTest, pitchSynchronousModeIsFreeOfDiscontinuities)
{
    for (const float semitones : {-12.f, -5.f, 0.f, 7.f, 12.f})
    {
        const Config cfg{semitones, false, MaxSize / 2, MaxSize / 2, true};
        EXPECT_LT(worstJump(cfg), maxSineSlope(semitones) * 1.5f) << "semitones " << semitones;
    }
}

// The whole point of pitch-synchronous placement: locking grain starts to the detected
// period should measurably reduce the comb sidebands the random jitter placement leaves
// around the carrier. See the class comment in PitchFadeWindowDelay.h ("~10 dB SFDR at
// 200 Hz" for the jittered path).
TEST(PitchFadeWindowDelayTest, pitchSynchronousReducesCombArtifactVsJitter)
{
    constexpr float Freq{200.f};

    for (const float semitones : {0.f, 7.f})
    {
        // the class resamples on readback, so the output tone sits at Freq * ratio, not
        // at the input Freq the analysis marks are detected on
        const auto ratio = std::pow(2.f, semitones / 12.f);
        const auto outputFreq = Freq * ratio;

        const auto jittered = renderSettled(Freq, semitones, false);
        const auto pitchSync = renderSettled(Freq, semitones, true);

        const auto jitteredSfdr = carrierSfdrDb(jittered, outputFreq);
        const auto pitchSyncSfdr = carrierSfdrDb(pitchSync, outputFreq);

        EXPECT_LT(pitchSyncSfdr, jitteredSfdr - 3.f)
            << "at " << semitones << " semitones: jittered SFDR " << jitteredSfdr << " dB, pitch-synchronous SFDR "
            << pitchSyncSfdr << " dB";
    }
}

TEST(PitchFadeWindowDelayTest, pitchSynchronousFallsBackOnSilence)
{
    PitchFadeWindowDelay<MaxSize> sut;
    sut.setSize(MaxSize / 2);
    sut.setFadeTime(MaxSize / 2);
    sut.enablePitchSynchronousMode(SampleRate);
    sut.setPitch(7.f);

    // silence first: Yin never reports a confident pitch, so this exercises the
    // drift/jitter fallback exclusively
    for (size_t i = 0; i < MaxSize * 2; ++i)
    {
        const auto value = sut.step(0.f);
        ASSERT_TRUE(std::isfinite(value));
        ASSERT_LE(std::fabs(value), 1.5f) << "during silence at sample " << i;
    }

    // then a steady tone: the tracker should lock on and stay bounded through the switch
    for (size_t i = 0; i < 48000; ++i)
    {
        const auto phase = 2.f * std::numbers::pi_v<float> * ProbeFreq * static_cast<float>(i) / SampleRate;
        const auto value = sut.step(std::sin(phase));
        ASSERT_TRUE(std::isfinite(value));
        ASSERT_LE(std::fabs(value), 1.5f) << "during tone at sample " << i;
    }
}

TEST(PitchFadeWindowDelayTest, pitchSynchronousModeHandlesPitchChangesGracefully)
{
    PitchFadeWindowDelay<MaxSize> sut;
    sut.setSize(MaxSize / 2);
    sut.setFadeTime(MaxSize / 4);
    sut.enablePitchSynchronousMode(SampleRate);

    for (size_t i = 0; i < 48000; ++i)
    {
        const auto phase = 2.f * std::numbers::pi_v<float> * ProbeFreq * static_cast<float>(i) / SampleRate;
        // sweep the pitch the way a host automating a dial would
        sut.setPitch(12.f * std::sin(static_cast<float>(i) / 4000.f));
        const auto value = sut.step(std::sin(phase));
        ASSERT_TRUE(std::isfinite(value));
        ASSERT_LE(std::fabs(value), 1.5f) << "at sample " << i;
    }
}

// Renders material to listen to; not an assertion. Run with:
//   ./build-tests/test/DelaysTests --gtest_also_run_disabled_tests \
//       --gtest_filter='*SaveTestSignal*'
TEST(PitchFadeWindowDelayTest, DISABLED_SaveTestSignal)
{
    constexpr float Freq{200.f};
    constexpr float Semitones{0.f};
    constexpr bool Reverse{false};
    constexpr auto NumSamples = static_cast<size_t>(4.f * SampleRate);
    const std::string fileName{"/tmp/PitchFadeWindowDelay_200Hz.wav"};

    // the geometry BlockProcPitch (and therefore MaxDiffuser) asks for
    PitchFadeWindowDelay<MaxSize> sut;
    sut.setSize(MaxSize / 2);
    sut.setFadeTime(MaxSize / 2);
    sut.setReverse(Reverse);
    sut.setPitch(Semitones);

    std::vector<float> out(NumSamples);
    for (size_t i = 0; i < NumSamples; ++i)
    {
        const auto phase = 2.f * std::numbers::pi_v<float> * Freq * static_cast<float>(i) / SampleRate;
        out[i] = sut.step(std::sin(phase));
    }

    AudioUtility::SaveWav::saveMonoAs(fileName, out, SampleRate);
    std::cout << "wrote " << fileName << '\n';
}

}
