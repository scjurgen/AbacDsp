#include <algorithm>
#include <cmath>
#include <complex>
#include <cstdio>
#include <functional>
#include <numbers>
#include <vector>

#include "gtest/gtest.h"

#include "Filters/Chebyshev4Lowpass.h"

namespace AbacDsp::Test
{

namespace
{
constexpr float kSampleRate{48000.0f};
constexpr double kPi{std::numbers::pi};

double toDb(const double gain)
{
    return 20.0 * std::log10(std::max(std::abs(gain), 1e-12));
}

/// |H| of one normalised biquad section at the frequency cf in cycles per sample.
double sectionMagnitude(const double cf, const BiquadSection& c)
{
    const std::complex<double> z1 = std::polar(1.0, -2.0 * kPi * cf);
    const std::complex<double> z2 = z1 * z1;
    const auto num = static_cast<double>(c.b0) + static_cast<double>(c.b1) * z1 + static_cast<double>(c.b2) * z2;
    const auto den = 1.0 + static_cast<double>(c.a1) * z1 + static_cast<double>(c.a2) * z2;
    return std::abs(num / den);
}

/// Gain of a processor for a sine at frequencyHz: RMS ratio over the second half of one second.
double measureSineGain(const std::function<float(float)>& process, const double frequencyHz, const double sampleRate)
{
    const auto length = static_cast<size_t>(sampleRate);
    double inSum{0.0};
    double outSum{0.0};
    for (size_t n = 0; n < length; ++n)
    {
        const auto x =
            static_cast<float>(0.5 * std::sin(2.0 * kPi * frequencyHz * static_cast<double>(n) / sampleRate));
        const double y = static_cast<double>(process(x));
        if (n >= length / 2)
        {
            inSum += static_cast<double>(x) * static_cast<double>(x);
            outSum += y * y;
        }
    }
    return std::sqrt(outSum / inSum);
}

/// Magnitude in dB of the two sections in series at the frequency in Hz.
double responseDb(const Chebyshev4Lowpass& filter, const double frequencyHz)
{
    const double cf = frequencyHz / static_cast<double>(kSampleRate);
    double db = 0.0;
    for (size_t i = 0; i < 2; ++i)
    {
        db += toDb(sectionMagnitude(cf, filter.section(i).getCurrent()));
    }
    return db;
}

Chebyshev4Lowpass designed(const float cutoffHz, const float rippleDb)
{
    Chebyshev4Lowpass filter{kSampleRate};
    filter.set(cutoffHz, rippleDb, true);
    return filter;
}
} // namespace

TEST(Chebyshev4Lowpass, passbandRippleEqualsTheSetRipple)
{
    for (const float ripple : {0.1f, 1.0f, 3.0f})
    {
        const auto filter = designed(2000.0f, ripple);
        double highest = -1e9;
        double lowest = 1e9;
        for (int i = 1; i <= 400; ++i)
        {
            const double db = responseDb(filter, 2000.0 * i / 400.0);
            highest = std::max(highest, db);
            lowest = std::min(lowest, db);
        }
        std::printf("[ ripple   ] set %.1f dB: peak %.3f dB, lowest %.3f dB, measured ripple %.3f dB\n",
                    static_cast<double>(ripple), highest, lowest, highest - lowest);
        EXPECT_NEAR(highest, 0.0, 0.1);
        EXPECT_NEAR(highest - lowest, static_cast<double>(ripple), 0.1);
    }
}

TEST(Chebyshev4Lowpass, dcSitsAtMinusRippleAndThePassbandPeaksAtZero)
{
    for (const float ripple : {0.1f, 1.0f, 3.0f})
    {
        const auto filter = designed(1000.0f, ripple);
        EXPECT_NEAR(responseDb(filter, 0.01), -static_cast<double>(ripple), 0.01);
    }
}

TEST(Chebyshev4Lowpass, attenuationAtOneAndAHalfTimesTheCutoffMatchesTheory)
{
    const auto filter = designed(2000.0f, 1.0f);
    const double db = responseDb(filter, 3000.0);
    // Analog prototype: -10 log10(1 + eps^2 T4(x)^2), eps^2 = 10^0.1 - 1, T4(x) = 8x^4 - 8x^2 + 1.
    // At x = 1.5 that is -21.58 dB. The bilinear transform warps the axis: 1.5 times the cutoff
    // in Hz is x = tan(pi 3000 / fs) / tan(pi 2000 / fs) = 1.5109 in the prototype.
    const double eps2 = std::pow(10.0, 0.1) - 1.0;
    const auto analog = [&](const double x)
    { return -10.0 * std::log10(1.0 + eps2 * std::pow(8.0 * x * x * x * x - 8.0 * x * x + 1.0, 2.0)); };
    const double warped = std::tan(kPi * 3000.0 / 48000.0) / std::tan(kPi * 2000.0 / 48000.0);
    std::printf("[ stopband ] 1.5 x cutoff at 1 dB ripple: %.3f dB (analog %.3f dB, warped x %.4f: %.3f dB)\n", db,
                analog(1.5), warped, analog(warped));
    EXPECT_NEAR(analog(1.5), -21.6, 0.05);
    EXPECT_NEAR(db, -21.6, 1.5);
    EXPECT_NEAR(db, analog(warped), 0.01);
}

TEST(Chebyshev4Lowpass, processedSineHasTheDesignedGain)
{
    auto filter = designed(4000.0f, 1.0f);
    const double measured = toDb(measureSineGain([&](const float x) { return filter.process(x); }, 6000.0, 48000.0));
    EXPECT_NEAR(measured, responseDb(filter, 6000.0), 0.05);
}

TEST(Chebyshev4Lowpass, responseMatchesTheClosedFormOverTheWholeRange)
{
    // 1 / (1 + eps^2 T4(x)^2), x = tan(pi f / fs) / tan(pi fc / fs): exact for the bilinear design.
    double worst = 0.0;
    for (const float ripple : {0.1f, 0.5f, 1.0f, 2.0f, 3.0f})
    {
        const double eps2 = std::pow(10.0, static_cast<double>(ripple) / 10.0) - 1.0;
        for (double cutoff = Chebyshev4Lowpass::kMinCutoffRatio * kSampleRate; cutoff < 0.45 * kSampleRate;
             cutoff *= 1.3)
        {
            const auto filter = designed(static_cast<float>(cutoff), ripple);
            for (double f = 5.0; f < 0.499 * kSampleRate; f *= 1.07)
            {
                const double x = std::tan(kPi * f / kSampleRate) / std::tan(kPi * cutoff / kSampleRate);
                const double t4 = 8.0 * x * x * x * x - 8.0 * x * x + 1.0;
                const double reference = -10.0 * std::log10(1.0 + eps2 * t4 * t4);
                if (reference > -70.0)
                {
                    worst = std::max(worst, std::abs(responseDb(filter, f) - reference));
                }
            }
        }
    }
    std::printf("[ design   ] largest error against the closed form, floor to 0.45 fs, 0.1 to 3 dB: %.4f dB\n", worst);
    EXPECT_LT(worst, 0.05);
}

TEST(Chebyshev4Lowpass, stableAtTheEndsOfTheCutoffRange)
{
    for (const float cutoff : {Chebyshev4Lowpass::kMinCutoffRatio * kSampleRate, 0.449f * kSampleRate})
    {
        for (const float ripple : {0.1f, 3.0f})
        {
            auto filter = designed(cutoff, ripple);
            for (size_t i = 0; i < 2; ++i)
            {
                const BiquadSection& c = filter.section(i).getCurrent();
                EXPECT_LT(c.a2, 1.0f);
                EXPECT_LT(std::abs(c.a1), 1.0f + c.a2);
            }
            float peak = 0.0f;
            float tail = 0.0f;
            const size_t length = 200000;
            for (size_t n = 0; n < length; ++n)
            {
                const float y = filter.process(n == 0 ? 1.0f : 0.0f);
                ASSERT_TRUE(std::isfinite(y));
                peak = std::max(peak, std::abs(y));
                if (n >= length - 1000)
                {
                    tail = std::max(tail, std::abs(y));
                }
            }
            EXPECT_LT(tail, 1e-6f * std::max(peak, 1e-3f))
                << "cutoff " << static_cast<double>(cutoff) << " ripple " << static_cast<double>(ripple);
        }
    }
}

namespace
{
/// An A minor chord, peak about 0.6, for the sweep tests.
float chord(const size_t n)
{
    const double t = static_cast<double>(n) / static_cast<double>(kSampleRate);
    return static_cast<float>(
        0.2 * (std::sin(2.0 * kPi * 110.0 * t) + std::sin(2.0 * kPi * 165.0 * t) + std::sin(2.0 * kPi * 220.0 * t)) +
        0.1 * std::sin(2.0 * kPi * 1320.0 * t));
}

float largestStep(const std::vector<float>& signal, const size_t from)
{
    float step = 0.0f;
    for (size_t i = from + 1; i < signal.size(); ++i)
    {
        step = std::max(step, std::abs(signal[i] - signal[i - 1]));
    }
    return step;
}

constexpr size_t kWarmUp{4800};

/// Chord through a cutoff swept logarithmically from 20 kHz to 200 Hz over sweepSamples after a
/// warm-up at 20 kHz, the target updated every block samples and the coefficients ramped over
/// ramp samples. The warm-up is part of the result and is left out when the steps are measured.
std::vector<float> sweptChord(const size_t block, const size_t ramp, const size_t sweepSamples)
{
    Chebyshev4Lowpass filter{kSampleRate};
    filter.setRampSamples(ramp);
    filter.set(20000.0f, 1.0f, true);
    const size_t length = kWarmUp + sweepSamples + 4800;
    std::vector<float> out(length);
    for (size_t n = 0; n < length; ++n)
    {
        if (n >= kWarmUp && (n - kWarmUp) % block == 0)
        {
            const double position = std::min(1.0, static_cast<double>(n - kWarmUp) / static_cast<double>(sweepSamples));
            filter.set(static_cast<float>(20000.0 * std::pow(0.01, position)), 1.0f);
        }
        out[n] = filter.process(chord(n));
    }
    return out;
}
}

TEST(Chebyshev4Lowpass, cutoffSweepOnAChordStaysWithinOneAndAHalfSteadySteps)
{
    std::vector<float> steady(48000);
    Chebyshev4Lowpass fixed{kSampleRate};
    fixed.set(20000.0f, 1.0f, true);
    for (size_t n = 0; n < steady.size(); ++n)
    {
        steady[n] = fixed.process(chord(n));
    }
    const float steadyStep = largestStep(steady, kWarmUp);
    const float slowStep = largestStep(sweptChord(16, 16, 48000), kWarmUp);
    const float fastStep = largestStep(sweptChord(128, 128, 960), kWarmUp);
    const float fastAtOnceStep = largestStep(sweptChord(128, 0, 960), kWarmUp);
    std::printf("[ sweep    ] steady step %.5f; 1 s sweep with ramps %.5f (%.3f x); 20 ms sweep in blocks of 128: "
                "ramped %.5f (%.3f x), at once %.5f (%.3f x)\n",
                static_cast<double>(steadyStep), static_cast<double>(slowStep),
                static_cast<double>(slowStep / steadyStep), static_cast<double>(fastStep),
                static_cast<double>(fastStep / steadyStep), static_cast<double>(fastAtOnceStep),
                static_cast<double>(fastAtOnceStep / steadyStep));
    EXPECT_LT(slowStep, 1.5f * steadyStep);
    EXPECT_LT(fastStep, 1.5f * steadyStep);
    EXPECT_GT(fastAtOnceStep, 1.5f * steadyStep) << "the control must click, or the test proves nothing";
}

TEST(Chebyshev4Lowpass, aRippleSweepStaysWithinOneAndAHalfSteadySteps)
{
    Chebyshev4Lowpass filter{kSampleRate};
    filter.setRampSamples(16);
    filter.set(1500.0f, 0.1f, true);
    float steadyStep = 0.0f;
    float previous = 0.0f;
    const size_t length = 48000;
    for (size_t n = 0; n < length; ++n)
    {
        if (n % 16 == 0)
        {
            filter.set(1500.0f, 0.1f + 2.9f * static_cast<float>(n) / static_cast<float>(length));
        }
        const float y = filter.process(chord(n));
        if (n > 0)
        {
            steadyStep = std::max(steadyStep, std::abs(y - previous));
        }
        previous = y;
    }
    Chebyshev4Lowpass fixed{kSampleRate};
    fixed.set(1500.0f, 3.0f, true);
    float referenceStep = 0.0f;
    previous = 0.0f;
    for (size_t n = 0; n < length; ++n)
    {
        const float y = fixed.process(chord(n));
        if (n > 4800)
        {
            referenceStep = std::max(referenceStep, std::abs(y - previous));
        }
        previous = y;
    }
    std::printf("[ sweep    ] ripple 0.1 to 3 dB: %.5f against the steady %.5f (%.3f x)\n",
                static_cast<double>(steadyStep), static_cast<double>(referenceStep),
                static_cast<double>(steadyStep / referenceStep));
    EXPECT_LT(steadyStep, 1.5f * referenceStep);
}

TEST(Chebyshev4Lowpass, clampsCutoffAndRipple)
{
    const auto low = designed(1.0f, 0.0f);
    const auto atLimit = designed(Chebyshev4Lowpass::kMinCutoffRatio * kSampleRate, Chebyshev4Lowpass::kMinRippleDb);
    EXPECT_EQ(low.section(0).getCurrent().a1, atLimit.section(0).getCurrent().a1);
    EXPECT_EQ(low.section(1).getCurrent().a2, atLimit.section(1).getCurrent().a2);
    const auto high = designed(1e6f, 100.0f);
    EXPECT_TRUE(high.isOpen());
}

TEST(Chebyshev4Lowpass, anOpenFilterIsBitIdentical)
{
    Chebyshev4Lowpass filter{kSampleRate};
    filter.set(1e6f, 1.0f, true);
    ASSERT_TRUE(filter.isOpen());
    for (int i = 0; i < 1000; ++i)
    {
        const auto x = static_cast<float>(std::sin(0.37 * i) * 0.8);
        EXPECT_EQ(filter.process(x), x);
    }
}

TEST(Chebyshev4Lowpass, leavingTheOpenStateClearsTheFilter)
{
    Chebyshev4Lowpass filter{kSampleRate};
    filter.set(1e6f, 1.0f, true);
    for (int i = 0; i < 100; ++i)
    {
        static_cast<void>(filter.process(1.0f));
    }
    filter.set(1000.0f, 1.0f, true);
    ASSERT_FALSE(filter.isOpen());
    // a rested filter answers an impulse like a fresh one
    Chebyshev4Lowpass fresh = designed(1000.0f, 1.0f);
    for (int i = 0; i < 50; ++i)
    {
        const float x = i == 0 ? 1.0f : 0.0f;
        EXPECT_EQ(filter.process(x), fresh.process(x));
    }
}

TEST(Chebyshev4Lowpass, settingTheSameValuesAgainDoesNotRestartARamp)
{
    Chebyshev4Lowpass filter{kSampleRate};
    filter.setRampSamples(16);
    filter.set(1e6f, 1.0f, true);
    for (int block = 0; block < 10; ++block)
    {
        filter.set(1e6f, 1.0f);
        for (int i = 0; i < 16; ++i)
        {
            const auto x = static_cast<float>(std::sin(0.31 * (block * 16 + i)));
            EXPECT_EQ(x, filter.process(x));
        }
    }
    filter.set(1000.0f, 1.0f);
    EXPECT_TRUE(filter.section(0).isRamping());
    for (int i = 0; i < 16; ++i)
    {
        static_cast<void>(filter.process(0.1f));
    }
    filter.set(1000.0f, 1.0f);
    EXPECT_FALSE(filter.section(0).isRamping());
}

} // namespace AbacDsp::Test
