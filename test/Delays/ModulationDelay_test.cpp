
#include <algorithm>
#include <cmath>
#include <limits>
#include <tuple>
#include <vector>

#include "gtest/gtest.h"

#include "Delays/ModulationDelay.h"

namespace AbacDsp::Test
{

namespace
{

template <size_t MaxSizeInSamples>
void settleAfterSizeChange(ModulatingDelayPitchedAdjust<MaxSizeInSamples>& sut, const size_t maxSteps = 8000)
{
    for (size_t i = 0; i < maxSteps; ++i)
    {
        std::ignore = sut.step(0.f);
        if (!sut.isAdvancing())
        {
            return;
        }
    }
    FAIL() << "delay did not settle within " << maxSteps << " steps";
}

float peakAbsNear(const std::vector<float>& buffer, const size_t center, const size_t halfWindow = 3)
{
    const size_t lo = center > halfWindow ? center - halfWindow : 0;
    const size_t hi = std::min(buffer.size(), center + halfWindow + 1);
    float best = 0.f;
    for (size_t i = lo; i < hi; ++i)
    {
        best = std::max(best, std::abs(buffer[i]));
    }
    return best;
}

float signedPeakNear(const std::vector<float>& buffer, const size_t center, const size_t halfWindow = 3)
{
    const size_t lo = center > halfWindow ? center - halfWindow : 0;
    const size_t hi = std::min(buffer.size(), center + halfWindow + 1);
    float best = 0.f;
    for (size_t i = lo; i < hi; ++i)
    {
        if (std::abs(buffer[i]) > std::abs(best))
        {
            best = buffer[i];
        }
    }
    return best;
}

size_t indexOfPeakAbs(const std::vector<float>& buffer, const size_t from, const size_t to)
{
    size_t best = from;
    float bestValue = 0.f;
    for (size_t i = from; i < std::min(to, buffer.size()); ++i)
    {
        if (std::abs(buffer[i]) > bestValue)
        {
            bestValue = std::abs(buffer[i]);
            best = i;
        }
    }
    return best;
}

double energyInWindow(const std::vector<float>& buffer, const double center, const double halfWindow)
{
    const auto lo = static_cast<size_t>(std::max(0.0, center - halfWindow));
    const auto hi = static_cast<size_t>(std::min(static_cast<double>(buffer.size()), center + halfWindow));
    double energy = 0.0;
    for (size_t i = lo; i < hi; ++i)
    {
        energy += static_cast<double>(buffer[i]) * buffer[i];
    }
    return energy;
}

}

TEST(ModulatingDelayPitchedAdjustTest, simpleFeedAndEat)
{
    constexpr auto epsilon = std::numeric_limits<float>::epsilon();

    ModulatingDelayPitchedAdjust<1000> sut(48000.f);
    sut.setSize(100);
    // settle pitching to correct position
    for (size_t i = 0; i < 100; ++i)
    {
        std::ignore = sut.step(0);
    }

    std::ignore = sut.step(1);
    for (size_t i = 0; i < 95; ++i)
    {
        EXPECT_TRUE(std::abs(sut.step(0)) <= epsilon) << "failed at step " << i;
    }
    EXPECT_TRUE(std::abs(sut.step(0) - 0.99f) > epsilon);
}

TEST(ModulatingDelayPitchedAdjustTest, blockFillMatchesStepByStep)
{
    constexpr size_t bufferSize = 500;
    std::vector<float> in(bufferSize);
    for (size_t i = 0; i < bufferSize; ++i)
    {
        in[i] = std::sin(static_cast<float>(i) * 0.07f) * 0.5f;
    }

    ModulatingDelayPitchedAdjust<1000> stepped(48000.f);
    ModulatingDelayPitchedAdjust<1000> blocked(48000.f);
    stepped.setSize(150);
    blocked.setSize(150);

    std::vector<float> outStepped(bufferSize);
    for (size_t i = 0; i < bufferSize; ++i)
    {
        outStepped[i] = stepped.step(in[i]);
    }

    std::vector<float> outBlocked(bufferSize);
    blocked.blockFill(in, outBlocked);

    EXPECT_EQ(outStepped, outBlocked);
}

TEST(ModulatingDelayPitchedAdjustTest, isAdvancingDuringGrowAndShrinkGlide)
{
    ModulatingDelayPitchedAdjust<1000> sut(48000.f);
    EXPECT_FALSE(sut.isAdvancing());

    sut.setSize(400); // grow from the InitialDistance default
    std::ignore = sut.step(0.f);
    EXPECT_TRUE(sut.isAdvancing());
    settleAfterSizeChange(sut);
    EXPECT_FALSE(sut.isAdvancing());

    sut.setSize(60); // shrink back down
    std::ignore = sut.step(0.f);
    EXPECT_TRUE(sut.isAdvancing());
    settleAfterSizeChange(sut);
    EXPECT_FALSE(sut.isAdvancing());
}

TEST(ModulatingDelayPitchedAdjustTest, feedbackGainSetsExponentialEchoDecay)
{
    constexpr float sampleRate = 48000.f;
    ModulatingDelayPitchedAdjust<1000> sut(sampleRate);
    sut.setModDepth(0.f); // isolate the feedback path from LFO-driven fractional jitter
    sut.setSize(100);
    settleAfterSizeChange(sut);
    sut.setFeedback(0.6f);

    constexpr size_t numEchoes = 4;
    std::vector<float> trace(700, 0.f);
    trace[0] = 1.f;
    std::vector<float> out(trace.size());
    for (size_t i = 0; i < trace.size(); ++i)
    {
        out[i] = sut.step(trace[i]);
    }

    const size_t d = indexOfPeakAbs(out, 1, 300);
    ASSERT_GT(d, 0u);

    std::vector<float> peaks;
    for (size_t k = 1; k <= numEchoes; ++k)
    {
        peaks.push_back(peakAbsNear(out, d * k));
    }
    for (size_t k = 1; k < numEchoes; ++k)
    {
        EXPECT_NEAR(peaks[k] / peaks[k - 1], 0.6f, 0.02f) << "echo " << k;
    }
}

TEST(ModulatingDelayPitchedAdjustTest, feedBackByTimeNegativeFlipsEchoSignEachRepeat)
{
    // feedBackByTime's "db" parameter is the level reached only after the full "msecs"
    // has elapsed, not the per-echo gain directly; the per-echo gain is
    // db^(period / (msecs/1000)), so the expected ratio is derived the same way here.
    constexpr float sampleRate = 48000.f;
    ModulatingDelayPitchedAdjust<1000> sut(sampleRate);
    sut.setModDepth(0.f);
    sut.setSize(80);
    settleAfterSizeChange(sut);
    constexpr float msecs = 50.f;
    constexpr float dbTarget = 0.5f;
    sut.feedBackByTime(msecs, dbTarget, true);

    std::vector<float> trace(500, 0.f);
    trace[0] = 1.f;
    std::vector<float> out(trace.size());
    for (size_t i = 0; i < trace.size(); ++i)
    {
        out[i] = sut.step(trace[i]);
    }

    const size_t d = indexOfPeakAbs(out, 1, 300);
    ASSERT_GT(d, 0u);

    const auto e1 = signedPeakNear(out, d);
    const auto e2 = signedPeakNear(out, d * 2);
    EXPECT_GT(e1, 0.f);
    EXPECT_LT(e2, 0.f);

    const auto expectedGain =
        std::pow(static_cast<double>(dbTarget), static_cast<double>(d) / sampleRate / (msecs / 1000.0));
    EXPECT_NEAR(std::abs(e2 / e1), expectedGain, expectedGain * 0.05 + 1e-6);
}

TEST(ModulatingDelayPitchedAdjustTest, feedbackDecayRemainsCorrectAcrossBufferWraparound)
{
    // A buffer this small forces m_headWrite/m_headRead/dHead past MaxSizeInSamples
    // repeatedly within the trace, exercising the ring-buffer wraparound branches.
    constexpr float sampleRate = 48000.f;
    constexpr size_t maxSize = 128;
    ModulatingDelayPitchedAdjust<maxSize> sut(sampleRate);
    sut.setModDepth(0.f);
    sut.setSize(20);
    settleAfterSizeChange(sut);
    sut.setFeedback(0.5f);

    constexpr size_t numGenerations = 6;
    std::vector<float> trace(maxSize * 3, 0.f);
    trace[0] = 1.f;
    std::vector<float> out(trace.size());
    for (size_t i = 0; i < trace.size(); ++i)
    {
        out[i] = sut.step(trace[i]);
    }

    // The settled delay length can land on a sub-sample offset, which spreads each echo's
    // energy across several taps instead of one clean peak; sum energy per window rather
    // than compare single-sample peaks so the assertion is robust to that spreading.
    const size_t d = indexOfPeakAbs(out, 1, maxSize);
    ASSERT_GT(d, 0u);
    ASSERT_LT(d * numGenerations, out.size());

    std::vector<double> energies;
    const double halfWindow = static_cast<double>(d) / 2.0 - 1.0;
    for (size_t k = 1; k <= numGenerations; ++k)
    {
        energies.push_back(energyInWindow(out, static_cast<double>(d * k), halfWindow));
        ASSERT_TRUE(std::isfinite(energies.back()));
    }
    for (size_t k = 1; k < energies.size(); ++k)
    {
        EXPECT_LT(energies[k], energies[k - 1]) << "echo energy should keep decaying, generation " << k;
    }
    // per-period energy ratio approaches feedback^2 = 0.25 once the smeared impulse response stabilizes
    EXPECT_NEAR(energies[numGenerations - 1] / energies[numGenerations - 2], 0.25, 0.08);
}

TEST(ModulatingDelayPitchedAdjustTest, setSizeClampsRequestAtOrAboveMaxSize)
{
    constexpr size_t maxSize = 200;
    ModulatingDelayPitchedAdjust<maxSize> sut(48000.f);
    sut.setModDepth(0.f);
    sut.setFeedback(0.f);
    sut.setSize(500); // >= MaxSizeInSamples, must clamp to MaxSizeInSamples - 1
    settleAfterSizeChange(sut);

    std::vector<float> trace(400, 0.f);
    trace[0] = 1.f;
    std::vector<float> out(trace.size());
    for (size_t i = 0; i < trace.size(); ++i)
    {
        out[i] = sut.step(trace[i]);
    }

    const size_t measuredDelay = indexOfPeakAbs(out, 1, out.size());
    EXPECT_NEAR(static_cast<double>(measuredDelay), static_cast<double>(maxSize - 1), 5.0);
}

TEST(ModulatingDelayPitchedAdjustTest, setWidthInMsecsIsNoOpWhenAlreadyAtRequestedSize)
{
    ModulatingDelayPitchedAdjust<1000> sut(48000.f);
    // InitialDistance defaults to MaxSizeInSamples/10 = 100 samples; pick a width that maps
    // to exactly 100 samples so setWidthInMsecs() takes its early-return, no-change branch.
    constexpr float widthMsMatchingInitialDistance = 100.f / 48000.f * 1000.f;
    sut.setWidthInMsecs(widthMsMatchingInitialDistance);
    std::ignore = sut.step(0.f);
    EXPECT_FALSE(sut.isAdvancing());
}

TEST(ModulatingDelayPitchedAdjustTest, setFeedbackClampsExtremeGainToStableRange)
{
    ModulatingDelayPitchedAdjust<200> sut(48000.f);
    sut.setModDepth(0.f);
    sut.setSize(20);
    settleAfterSizeChange(sut);
    sut.setFeedback(5.f); // must clamp, else this would diverge

    std::vector<float> trace(2000, 0.f);
    trace[0] = 1.f;
    for (const auto v : trace)
    {
        const auto out = sut.step(v);
        ASSERT_TRUE(std::isfinite(out));
        ASSERT_LE(std::abs(out), 1.01f) << "feedback gain was not clamped below unity";
    }
}

TEST(ModulatingDelayPitchedAdjustTest, setWidthInMsecsProducesExpectedSampleDelay)
{
    constexpr float sampleRate = 48000.f;
    ModulatingDelayPitchedAdjust<1000> sut(sampleRate);
    sut.setModDepth(0.f);
    sut.setFeedback(0.f);

    constexpr float widthMs = 2.0f;
    sut.setWidthInMsecs(widthMs);
    settleAfterSizeChange(sut);

    std::vector<float> trace(400, 0.f);
    trace[0] = 1.f;
    std::vector<float> out(trace.size());
    for (size_t i = 0; i < trace.size(); ++i)
    {
        out[i] = sut.step(trace[i]);
    }

    const size_t measuredDelay = indexOfPeakAbs(out, 1, out.size());
    const size_t expectedDelay = getSamplesPerMillisecond(widthMs, sampleRate, 1000);
    EXPECT_NEAR(static_cast<double>(measuredDelay), static_cast<double>(expectedDelay), 5.0);
}

TEST(ModulatingDelayPitchedAdjustTest, setWidthInMsecsAppliesTimeBasedFeedbackOnCompletion)
{
    // Completing a setWidthInMsecs() glide auto-applies feedBackByTime(decayMsecs) with the
    // default target level (0.001) and 100ms decay time; verify the resulting per-echo gain.
    constexpr float sampleRate = 48000.f;
    ModulatingDelayPitchedAdjust<1000> sut(sampleRate);
    sut.setModDepth(0.f);

    sut.setWidthInMsecs(2.0f);
    settleAfterSizeChange(sut);

    std::vector<float> trace(1200, 0.f);
    trace[0] = 1.f;
    std::vector<float> out(trace.size());
    for (size_t i = 0; i < trace.size(); ++i)
    {
        out[i] = sut.step(trace[i]);
    }

    const size_t d = indexOfPeakAbs(out, 1, 400);
    ASSERT_GT(d, 0u);
    const auto peak1 = peakAbsNear(out, d);
    const auto peak2 = peakAbsNear(out, d * 2);
    ASSERT_GT(peak1, 0.f);

    const auto measuredGain = static_cast<double>(peak2 / peak1);
    const auto expectedGain = std::pow(0.001, static_cast<double>(d) / sampleRate / (100.0 / 1000.0));
    EXPECT_NEAR(measuredGain, expectedGain, expectedGain * 0.1 + 1e-6);
}

TEST(ModulatingDelayPitchedAdjustTest, modulationDepthAndSpeedAlterOutputOverTime)
{
    constexpr float sampleRate = 48000.f;
    std::vector<float> in(6000);
    for (size_t i = 0; i < in.size(); ++i)
    {
        in[i] = std::sin(static_cast<float>(i) * 0.05f);
    }

    ModulatingDelayPitchedAdjust<1000> unmodulated(sampleRate);
    unmodulated.setModDepth(0.f);
    unmodulated.setSize(200);
    settleAfterSizeChange(unmodulated);

    ModulatingDelayPitchedAdjust<1000> modulated(sampleRate);
    modulated.setModDepth(3.f);
    modulated.setModSpeed(10.f);
    modulated.setSize(200);
    settleAfterSizeChange(modulated);

    std::vector<float> outA(in.size());
    std::vector<float> outB(in.size());
    unmodulated.blockFill(in, outA);
    modulated.blockFill(in, outB);

    double sumSquaredDiff = 0.0;
    for (size_t i = 0; i < in.size(); ++i)
    {
        const auto diff = static_cast<double>(outA[i] - outB[i]);
        sumSquaredDiff += diff * diff;
    }
    const auto rmsDiff = std::sqrt(sumSquaredDiff / static_cast<double>(in.size()));
    EXPECT_GT(rmsDiff, 0.05);
}

}
