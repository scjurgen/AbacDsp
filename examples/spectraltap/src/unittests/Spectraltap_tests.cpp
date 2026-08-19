#include <cmath>
#include <cstddef>
#include <numbers>
#include <random>

#include "gtest/gtest.h"

#include "impl/SpectraltapImpl.h"

namespace
{
constexpr size_t kBlockSize = 16;
constexpr float kSampleRate = 48000.f;
using Impl = SpectraltapImpl<kBlockSize>;
using Buffer = AbacDsp::AudioBuffer<2, kBlockSize>;

// Runs numBlocks blocks of a constant-value input (default silence) through impl, purely
// to let its block-rate frequency/decay and per-sample gain/pan smoothing settle onto
// whatever targets a prior setScript() call requested.
void settle(Impl& impl, const size_t numBlocks, const float value = 0.f)
{
    Buffer in{};
    Buffer out{};
    for (size_t s = 0; s < kBlockSize; ++s)
    {
        in(s, 0) = value;
        in(s, 1) = value;
    }
    for (size_t b = 0; b < numBlocks; ++b)
    {
        impl.processBlock(in, out);
    }
}

[[nodiscard]] bool allFinite(const Buffer& buffer) noexcept
{
    for (size_t s = 0; s < kBlockSize; ++s)
    {
        if (!std::isfinite(buffer(s, 0)) || !std::isfinite(buffer(s, 1)))
        {
            return false;
        }
    }
    return true;
}

// Lua globals persist across setScript() reloads, so every custom script below prepends
// this to stop kStubScript's still-live OnTiming from overwriting the test's own topology.
constexpr std::string_view kNoAutoTiming = "function OnTiming() end\n";
}

TEST(Spectraltap, NoActiveTapsPassesOnlyDrySignal)
{
    Impl impl{kSampleRate};
    ASSERT_TRUE(impl.setScript(std::string(kNoAutoTiming)));

    Buffer in{};
    Buffer out{};
    for (size_t s = 0; s < kBlockSize; ++s)
    {
        in(s, 0) = 0.5f;
        in(s, 1) = -0.25f;
    }
    impl.processBlock(in, out);

    for (size_t s = 0; s < kBlockSize; ++s)
    {
        EXPECT_FLOAT_EQ(out(s, 0), in(s, 0));
        EXPECT_FLOAT_EQ(out(s, 1), in(s, 1));
    }
}

TEST(Spectraltap, MaxTapCountEveryTypeRunsWithoutNaN)
{
    Impl impl{kSampleRate};
    std::string script{kNoAutoTiming};
    script += "SetMaxTaps(24)\n";
    for (size_t i = 0; i < 24; ++i)
    {
        const size_t type = i % 9;
        script += "SetTap(" + std::to_string(i) + ", " + std::to_string(1.f + static_cast<float>(i)) + ", " +
                  std::to_string(type) + ", 0.5, 0.0)\n";
        script += "SetResonance(" + std::to_string(i) + ", " + std::to_string(100.f + 50.f * static_cast<float>(i)) +
                  ", 0.3)\n";
        script += "SetFormant(" + std::to_string(i) + ", 200, 2.0, 0.5, 3.0, 0.3)\n";
    }
    ASSERT_TRUE(impl.setScript(script));
    settle(impl, 100);

    std::mt19937 rng{42};
    std::uniform_real_distribution<float> dist{-1.f, 1.f};
    Buffer in{};
    Buffer out{};
    for (size_t b = 0; b < 200; ++b)
    {
        for (size_t s = 0; s < kBlockSize; ++s)
        {
            const float noise = dist(rng);
            in(s, 0) = noise;
            in(s, 1) = noise;
        }
        impl.processBlock(in, out);
        ASSERT_TRUE(allFinite(out)) << "block " << b;
        for (size_t s = 0; s < kBlockSize; ++s)
        {
            EXPECT_LT(std::abs(out(s, 0)), 100.f);
            EXPECT_LT(std::abs(out(s, 1)), 100.f);
        }
    }
}

TEST(Spectraltap, PanIsMonoSafeAtCenterAndHardLeftRight)
{
    const auto measure = [](const float pan)
    {
        Impl impl{kSampleRate};
        EXPECT_TRUE(impl.setScript(std::string(kNoAutoTiming) + "SetMaxTaps(1)\nSetTap(0, 0, 0, 1.0, " +
                                   std::to_string(pan) + ")"));
        settle(impl, 100);

        Buffer in{};
        Buffer out{};
        for (size_t s = 0; s < kBlockSize; ++s)
        {
            in(s, 0) = 1.f;
            in(s, 1) = 1.f;
        }
        impl.processBlock(in, out);
        return std::make_pair(out(0, 0), out(0, 1));
    };

    const auto [centerL, centerR] = measure(0.f);
    EXPECT_NEAR(centerL, centerR, 1e-4f);

    const auto [leftL, leftR] = measure(-1.f);
    EXPECT_GT(leftL, leftR);
    EXPECT_NEAR(leftR, 1.f, 0.01f); // dry stays centered even at a hard-panned wet tap

    const auto [rightL, rightR] = measure(1.f);
    EXPECT_GT(rightR, rightL);
}

TEST(Spectraltap, CombResonatorStaysStableAtMaxDecay)
{
    Impl impl{kSampleRate};
    ASSERT_TRUE(impl.setScript(std::string(kNoAutoTiming) +
                               "SetMaxTaps(1)\nSetTap(0, 0, 7, 1.0, 0.0)\nSetResonance(0, 220, 20.0)"));
    settle(impl, 20);

    Buffer in{};
    Buffer out{};
    in(0, 0) = 1.f;
    in(0, 1) = 1.f;
    impl.processBlock(in, out);
    ASSERT_TRUE(allFinite(out));

    settle(impl, 2000); // ~666 ms of free ring-down at the maximum decay time

    Buffer silence{};
    impl.processBlock(silence, out);
    ASSERT_TRUE(allFinite(out));
    for (size_t s = 0; s < kBlockSize; ++s)
    {
        EXPECT_LT(std::abs(out(s, 0)), 10.f);
        EXPECT_LT(std::abs(out(s, 1)), 10.f);
    }
}

TEST(Spectraltap, FormantNearNyquistIsClamped)
{
    Impl impl{kSampleRate};
    ASSERT_TRUE(impl.setScript(std::string(kNoAutoTiming) +
                               "SetMaxTaps(1)\nSetTap(0, 0, 6, 1.0, 0.0)\nSetFormant(0, 15000, 16, 1.0, 16, 1.0)"));
    settle(impl, 100);

    std::mt19937 rng{7};
    std::uniform_real_distribution<float> dist{-1.f, 1.f};
    Buffer in{};
    Buffer out{};
    for (size_t b = 0; b < 50; ++b)
    {
        for (size_t s = 0; s < kBlockSize; ++s)
        {
            const float noise = dist(rng);
            in(s, 0) = noise;
            in(s, 1) = noise;
        }
        impl.processBlock(in, out);
        ASSERT_TRUE(allFinite(out)) << "block " << b;
    }
}

TEST(Spectraltap, InvalidTapDoesNotCorruptExistingTopology)
{
    // Two identically-configured instances; only impl's script also attempts the two
    // invalid SetTap calls - if those are true no-ops, both stay in lockstep exactly.
    Impl impl{kSampleRate};
    Impl reference{kSampleRate};
    const std::string setup = std::string(kNoAutoTiming) + "SetMaxTaps(1)\nSetTap(0, 0, 3, 1.0, 0.0)\n"
                                                           "SetResonance(0, 300, 0.5)";
    ASSERT_TRUE(impl.setScript(setup));
    ASSERT_TRUE(reference.setScript(setup));
    settle(impl, 100);
    settle(reference, 100);

    ASSERT_TRUE(impl.setScript("SetTap(99, 100, 0, 1.0, 0.0)\nSetTap(0, 100, 12, 1.0, 0.0)"));
    ASSERT_TRUE(reference.setScript(""));
    settle(impl, 5);
    settle(reference, 5);

    Buffer in{};
    Buffer out{};
    Buffer referenceOut{};
    for (size_t s = 0; s < kBlockSize; ++s)
    {
        in(s, 0) = 1.f;
        in(s, 1) = 1.f;
    }
    impl.processBlock(in, out);
    reference.processBlock(in, referenceOut);

    for (size_t s = 0; s < kBlockSize; ++s)
    {
        EXPECT_FLOAT_EQ(out(s, 0), referenceOut(s, 0));
        EXPECT_FLOAT_EQ(out(s, 1), referenceOut(s, 1));
    }
}

TEST(Spectraltap, RapidParameterUpdatesRemainStable)
{
    Impl impl{kSampleRate};
    ASSERT_TRUE(impl.setScript(std::string(kNoAutoTiming) + "SetMaxTaps(4)\n"
                                                            "SetTap(0, 10, 3, 0.7, -0.5)\nSetTap(1, 20, 5, 0.7, 0.5)\n"
                                                            "SetTap(2, 30, 6, 0.7, -0.2)\nSetTap(3, 40, 7, 0.7, 0.2)"));
    settle(impl, 50);

    std::mt19937 rng{99};
    std::uniform_real_distribution<float> freqDist{50.f, 8000.f};
    std::uniform_real_distribution<float> decayDist{0.01f, 5.f};
    std::uniform_real_distribution<float> gainDist{0.f, 2.f};
    std::uniform_real_distribution<float> panDist{-1.f, 1.f};

    Buffer in{};
    Buffer out{};
    for (size_t s = 0; s < kBlockSize; ++s)
    {
        in(s, 0) = 0.3f;
        in(s, 1) = -0.3f;
    }

    for (size_t b = 0; b < 100; ++b)
    {
        const std::string script = "SetFrequency(0, " + std::to_string(freqDist(rng)) + ")\n" + "SetResonance(1, " +
                                   std::to_string(freqDist(rng)) + ", " + std::to_string(decayDist(rng)) + ")\n" +
                                   "SetGain(2, " + std::to_string(gainDist(rng)) + ")\n" + "SetPan(3, " +
                                   std::to_string(panDist(rng)) + ")\n";
        ASSERT_TRUE(impl.setScript(script));
        impl.processBlock(in, out);
        ASSERT_TRUE(allFinite(out)) << "block " << b;
    }
}

TEST(Spectraltap, CurrentBpmFollowsManualDialUntilHostSynced)
{
    Impl impl{kSampleRate};
    impl.setBpm(140.f);
    EXPECT_FLOAT_EQ(impl.currentBpm(), 140.f);
    EXPECT_FALSE(impl.isHostSynced());

    EffectBase::HostTransport transport{};
    transport.bpm = 90.0;
    impl.setHostTransport(transport);
    EXPECT_FLOAT_EQ(impl.currentBpm(), 140.f); // still manual - Host Sync isn't on yet

    impl.setHostSync(true);
    EXPECT_TRUE(impl.isHostSynced());
    EXPECT_FLOAT_EQ(impl.currentBpm(), 90.f);
}

TEST(Spectraltap, RetuneCrossfadesWithoutDiscontinuity)
{
    Impl impl{kSampleRate};
    ASSERT_TRUE(impl.setScript(std::string(kNoAutoTiming) + "SetMaxTaps(1)\nSetTap(0, 5, 0, 1.0, 0.0)"));
    settle(impl, 50);

    constexpr float kToneFreqHz = 300.f;
    size_t sampleCounter = 0;
    Buffer in{};
    Buffer out{};
    float lastL = 0.f;
    float lastR = 0.f;
    bool haveLast = false;
    float maxJump = 0.f;

    const auto processAndTrackJump = [&]()
    {
        for (size_t s = 0; s < kBlockSize; ++s)
        {
            const float t = static_cast<float>(sampleCounter++) / kSampleRate;
            const float v = std::sin(2.f * std::numbers::pi_v<float> * kToneFreqHz * t);
            in(s, 0) = v;
            in(s, 1) = v;
        }
        impl.processBlock(in, out);
        for (size_t s = 0; s < kBlockSize; ++s)
        {
            if (haveLast)
            {
                maxJump = std::max(maxJump, std::abs(out(s, 0) - lastL));
                maxJump = std::max(maxJump, std::abs(out(s, 1) - lastR));
            }
            lastL = out(s, 0);
            lastR = out(s, 1);
            haveLast = true;
        }
    };

    for (int b = 0; b < 20; ++b)
    {
        processAndTrackJump();
    }

    // Retunes tap 0 from a 5 ms to a 50 ms delay mid-tone - a hard in-place reset would
    // jump the read position and voice state discontinuously right here.
    ASSERT_TRUE(impl.setScript("SetTap(0, 50, 0, 1.0, 0.0)"));

    for (int b = 0; b < 200; ++b)
    {
        processAndTrackJump();
    }

    EXPECT_LT(maxJump, 0.5f);
}

TEST(Spectraltap, SetScriptReprimesTimingSoNewScriptConfiguresTaps)
{
    Impl impl{kSampleRate};
    settle(impl, 5); // lets the stub's own initial OnTiming (fires on the first block) settle

    // bpm/division haven't changed since that first notify, so without resendTiming() this
    // new script's OnTiming would never fire, leaving tap 0 as the old script left it.
    ASSERT_TRUE(impl.setScript("function OnTiming(bpm, divisionIndex)\n"
                               "    SetMaxTaps(1)\n"
                               "    SetTap(0, 0, 0, 1.0, 0.0)\n"
                               "end\n"));
    settle(impl, 100);

    Buffer in{};
    Buffer out{};
    for (size_t s = 0; s < kBlockSize; ++s)
    {
        in(s, 0) = 0.5f;
        in(s, 1) = 0.5f;
    }
    impl.processBlock(in, out);

    // A Bypass tap passes this DC-ish input straight through, well above pure-dry (0.5); a
    // stale BandPass tap from the old script would reject it and stay near 0.5 instead.
    EXPECT_GT(out(0, 0), 0.5f);
    EXPECT_GT(out(0, 1), 0.5f);
}

TEST(Spectraltap, RingModulatorMultipliesRatherThanPassingThrough)
{
    Impl impl{kSampleRate};
    ASSERT_TRUE(
        impl.setScript(std::string(kNoAutoTiming) + "SetMaxTaps(1)\nSetTap(0, 0, 8, 1.0, 0.0)\nSetFrequency(0, 1000)"));
    settle(impl, 100);

    // A constant input ring-modulated by a 1 kHz carrier should itself trace out that
    // carrier - bounded by the input amplitude and clearly non-constant, unlike Bypass.
    Buffer in{};
    Buffer out{};
    for (size_t s = 0; s < kBlockSize; ++s)
    {
        in(s, 0) = 1.f;
        in(s, 1) = 1.f;
    }
    float minVal = 1e9f;
    float maxVal = -1e9f;
    for (int b = 0; b < 30; ++b)
    {
        impl.processBlock(in, out);
        for (size_t s = 0; s < kBlockSize; ++s)
        {
            minVal = std::min(minVal, out(s, 0));
            maxVal = std::max(maxVal, out(s, 0));
        }
    }
    EXPECT_GT(maxVal - minVal, 0.5f); // clearly oscillating, not a flat passthrough
    EXPECT_LE(maxVal, 2.1f);          // dry(1) + wet*carrier(<=1), never amplifies further
}

TEST(Spectraltap, ResonatorMatchesBandPassLoudnessAtSameFrequencyAndDecay)
{
    Impl impl{kSampleRate};
    ASSERT_TRUE(impl.setScript(std::string(kNoAutoTiming) + "SetMaxTaps(2)\n"
                                                            "SetTap(0, 0, 3, 1.0, 0.0)\nSetResonance(0, 440, 1.2)\n"
                                                            "SetTap(1, 0, 5, 1.0, 0.0)\nSetResonance(1, 440, 1.2)"));
    settle(impl, 200);

    // Both taps are driven by the same signal (the shared mono downmix), so measuring the
    // combined wet output at resonance isn't useful per-tap - instead disable one tap's
    // gain at a time and compare the resulting peak levels.
    const auto measurePeak = [&](const size_t activeTapIndex)
    {
        Impl solo{kSampleRate};
        const std::string type = activeTapIndex == 0 ? "3" : "5";
        EXPECT_TRUE(solo.setScript(std::string(kNoAutoTiming) + "SetMaxTaps(1)\nSetTap(0, 0, " + type +
                                   ", 1.0, 0.0)\nSetResonance(0, 440, 1.2)"));
        settle(solo, 200);

        Buffer in{};
        Buffer out{};
        float peak = 0.f;
        size_t sampleCounter = 0;
        for (int b = 0; b < 100; ++b)
        {
            for (size_t s = 0; s < kBlockSize; ++s)
            {
                const float t = static_cast<float>(sampleCounter++) / kSampleRate;
                const float v = std::sin(2.f * std::numbers::pi_v<float> * 440.f * t);
                in(s, 0) = v;
                in(s, 1) = v;
            }
            solo.processBlock(in, out);
            if (b > 80) // steady state
            {
                for (size_t s = 0; s < kBlockSize; ++s)
                {
                    peak = std::max(peak, std::abs(out(s, 0)));
                }
            }
        }
        return peak;
    };

    const float bandPassPeak = measurePeak(0);
    const float resonatorPeak = measurePeak(1);
    // Same Q-from-decay formula and gain-boost cancellation for both, so they should land
    // within a small margin of each other rather than differing by an order of magnitude.
    EXPECT_NEAR(bandPassPeak, resonatorPeak, bandPassPeak * 0.25f);
}

TEST(Spectraltap, FeedbackHeadroomLimiterIsNearTransparentAtDefaultZeroFeedback)
{
    // Every write passes through the headroom limiter unconditionally, even with no
    // feedback configured, so this checks near-transparency at normal levels, not exact
    // equality to a pre-limiter reference.
    Impl impl{kSampleRate};
    ASSERT_TRUE(impl.setScript(std::string(kNoAutoTiming) + "SetMaxTaps(1)\nSetTap(0, 0, 0, 1.0, 0.0)"));
    settle(impl, 50);

    Buffer in{};
    Buffer out{};
    for (size_t s = 0; s < kBlockSize; ++s)
    {
        in(s, 0) = 0.6f;
        in(s, 1) = -0.4f;
    }
    impl.processBlock(in, out);

    const float monoIn = 0.5f * (0.6f - 0.4f);
    const float panGain = std::cos(std::numbers::pi_v<float> / 4.f); // pan = 0.0
    EXPECT_NEAR(out(0, 0), in(0, 0) + monoIn * panGain, 0.001f);
}

TEST(Spectraltap, GlobalFeedbackProducesATempoSyncedRepeat)
{
    Impl impl{kSampleRate};
    impl.setBpm(120.f);
    impl.setFeedbackBeats(1.f); // 1 beat -> 500 ms at 120 bpm -> 24000 samples at 48 kHz
    impl.setFeedback(50.f);
    ASSERT_TRUE(impl.setScript(std::string(kNoAutoTiming) + "SetMaxTaps(1)\nSetTap(0, 0, 0, 1.0, 0.0)"));
    settle(impl, 50); // lets m_feedback's smoothing ramp onto its 50% target

    Buffer in{};
    Buffer out{};
    in(0, 0) = 1.f;
    in(0, 1) = 1.f;
    impl.processBlock(in, out); // one-sample impulse at the very start of this block
    in(0, 0) = 0.f;
    in(0, 1) = 0.f;

    constexpr size_t kExpectedRepeatSample = 24000;
    const size_t targetBlock = kExpectedRepeatSample / kBlockSize;
    float peakNearRepeat = 0.f;
    for (size_t b = 1; b < targetBlock + 5; ++b)
    {
        impl.processBlock(in, out);
        if (b + 2 >= targetBlock && b <= targetBlock + 2)
        {
            for (size_t s = 0; s < kBlockSize; ++s)
            {
                peakNearRepeat = std::max(peakNearRepeat, std::abs(out(s, 0)));
            }
        }
    }
    EXPECT_GT(peakNearRepeat, 0.05f); // a real echo, not just smoothing/limiter residue
}

TEST(Spectraltap, HeavyFeedbackStaysFiniteAndBounded)
{
    Impl impl{kSampleRate};
    impl.setBpm(120.f);
    impl.setFeedbackBeats(1.f); // shortest available repeat time
    impl.setFeedback(95.f);
    ASSERT_TRUE(impl.setScript(std::string(kNoAutoTiming) +
                               "SetMaxTaps(4)\n"
                               "SetTap(0, 5, 3, 1.0, -0.5)\nSetResonance(0, 300, 1.0)\nSetTapFeedback(0, 0.95)\n"
                               "SetTap(1, 15, 3, 1.0, 0.5)\nSetResonance(1, 450, 1.0)\nSetTapFeedback(1, 0.95)\n"
                               "SetTap(2, 25, 3, 1.0, -0.3)\nSetResonance(2, 600, 1.0)\nSetTapFeedback(2, 0.95)\n"
                               "SetTap(3, 35, 3, 1.0, 0.3)\nSetResonance(3, 900, 1.0)\nSetTapFeedback(3, 0.95)"));
    settle(impl, 100);

    std::mt19937 rng{123};
    std::uniform_real_distribution<float> dist{-1.f, 1.f};
    Buffer in{};
    Buffer out{};
    for (size_t b = 0; b < 2000; ++b)
    {
        for (size_t s = 0; s < kBlockSize; ++s)
        {
            const float noise = dist(rng);
            in(s, 0) = noise;
            in(s, 1) = noise;
        }
        impl.processBlock(in, out);
        ASSERT_TRUE(allFinite(out)) << "block " << b;
        for (size_t s = 0; s < kBlockSize; ++s)
        {
            // Loose bound: the point is catching unbounded runaway growth, not pinning
            // down the exact peak level of a deliberately near-unstable configuration.
            EXPECT_LT(std::abs(out(s, 0)), 2000.f);
            EXPECT_LT(std::abs(out(s, 1)), 2000.f);
        }
    }
}

TEST(Spectraltap, ReverbStaysSilentAtDefaultWetLevel)
{
    Impl impl{kSampleRate};
    ASSERT_TRUE(impl.setScript(std::string(kNoAutoTiming) + "SetMaxTaps(1)\nSetTap(0, 0, 0, 1.0, 0.0)"));
    settle(impl, 50);

    Buffer in{};
    Buffer out{};
    for (size_t s = 0; s < kBlockSize; ++s)
    {
        in(s, 0) = 0.8f;
        in(s, 1) = -0.6f;
    }
    for (int b = 0; b < 50; ++b)
    {
        impl.processBlock(in, out);
    }

    const float monoIn = 0.5f * (0.8f - 0.6f);
    const float panGain = std::cos(std::numbers::pi_v<float> / 4.f); // pan = 0.0
    const float expected = in(0, 0) + monoIn * panGain;              // dry(1) + wet(1)*tap
    EXPECT_NEAR(out(0, 0), expected, 0.001f);
}

TEST(Spectraltap, ReverbProducesAudibleTailWhenTurnedUp)
{
    Impl impl{kSampleRate};
    impl.setReverbWet(0.f); // unity
    ASSERT_TRUE(impl.setScript(std::string(kNoAutoTiming) + "SetMaxTaps(1)\nSetTap(0, 0, 0, 1.0, 0.0)"));
    settle(impl, 50);

    std::mt19937 rng{7};
    std::uniform_real_distribution<float> dist{-1.f, 1.f};
    Buffer in{};
    Buffer out{};
    for (int b = 0; b < 50; ++b)
    {
        for (size_t s = 0; s < kBlockSize; ++s)
        {
            const float noise = dist(rng);
            in(s, 0) = noise;
            in(s, 1) = noise;
        }
        impl.processBlock(in, out);
        ASSERT_TRUE(allFinite(out)) << "drive block " << b;
    }

    // A Bypass tap alone goes silent the instant input does; any energy that outlives it
    // once input stops can only be the FDN's tail.
    Buffer silence{};
    float tailEnergy = 0.f;
    for (int b = 0; b < 100; ++b)
    {
        impl.processBlock(silence, out);
        ASSERT_TRUE(allFinite(out)) << "tail block " << b;
        for (size_t s = 0; s < kBlockSize; ++s)
        {
            tailEnergy += out(s, 0) * out(s, 0) + out(s, 1) * out(s, 1);
        }
    }
    EXPECT_GT(tailEnergy, 1e-6f);
}
