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

// Lua globals persist across setScript() reloads unless the new script redefines them
// (see LuaScriptEngineBase::loadScript() - it recompiles in the same environment, it
// doesn't reset it). Every custom test script below prepends this so the construction-time
// kStubScript's still-live OnTiming/RetuneTaps globals can't silently fire (via
// notifyTimingIfChanged() on the very first processBlock()) and overwrite the topology the
// test is trying to set up.
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
        const size_t type = i % 8;
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
    // Two identically-configured instances so each keeps its own filter state history;
    // only impl's script additionally attempts the two invalid SetTap calls (index 99 is
    // out of range, type 12 is unknown) - if those are true no-ops, both instances stay in
    // lockstep sample-for-sample, not just approximately.
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

    // Retunes tap 0 from a 5 ms to a 50 ms delay while the tone keeps playing - a hard
    // in-place reset would jump the delay-read position (and reset the Bypass voice)
    // discontinuously right here; the crossfade should keep every sample-to-sample step
    // small throughout, not just before/after.
    ASSERT_TRUE(impl.setScript("SetTap(0, 50, 0, 1.0, 0.0)"));

    for (int b = 0; b < 200; ++b)
    {
        processAndTrackJump();
    }

    EXPECT_LT(maxJump, 0.5f);
}
