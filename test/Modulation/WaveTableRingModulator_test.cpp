#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <new>
#include <numbers>
#include <numeric>
#include <vector>

#include "gtest/gtest.h"

#include "Filters/Chebyshev4Lowpass.h"
#include "Modulation/RingModulator.h"
#include "Modulation/WaveTableRingModulator.h"

namespace
{
thread_local bool tCountAllocations{false};
thread_local size_t tAllocations{0};
} // namespace

void* operator new(const std::size_t size)
{
    if (tCountAllocations)
    {
        ++tAllocations;
    }
    if (void* pointer = std::malloc(size))
    {
        return pointer;
    }
    throw std::bad_alloc{};
}

void operator delete(void* pointer) noexcept
{
    std::free(pointer);
}

void operator delete(void* pointer, std::size_t) noexcept
{
    std::free(pointer);
}

namespace AbacDsp::Test
{

namespace
{

constexpr float sampleRate{48000.f};
constexpr double twoPi{2.0 * std::numbers::pi};

/// The carrier alone: a constant input of 1 turns the product into the carrier times its gain.
std::vector<float> carrierOf(WaveTableRingModulator& sut, const size_t count)
{
    const std::vector<float> ones(count, 1.f);
    std::vector<float> out(count);
    sut.processBlock(ones, out);
    return out;
}

double rmsOf(const std::vector<float>& v)
{
    const double sum = std::accumulate(v.begin(), v.end(), 0.0, [](const double acc, const float x)
                                       { return acc + static_cast<double>(x) * x; });
    return std::sqrt(sum / static_cast<double>(v.size()));
}

double meanOf(const std::vector<float>& v)
{
    return std::accumulate(v.begin(), v.end(), 0.0) / static_cast<double>(v.size());
}

/// Amplitude of harmonic k of a signal that holds a whole number of cycles, from a projection on sine and cosine.
double harmonicAmplitude(const std::vector<float>& v, const size_t cycles, const size_t k)
{
    double re{0.0};
    double im{0.0};
    for (size_t i = 0; i < v.size(); ++i)
    {
        const double phase = twoPi * static_cast<double>(cycles * k * i) / static_cast<double>(v.size());
        re += static_cast<double>(v[i]) * std::cos(phase);
        im += static_cast<double>(v[i]) * std::sin(phase);
    }
    return 2.0 * std::hypot(re, im) / static_cast<double>(v.size());
}

} // namespace

TEST(WaveTableRingModulator, aSineCarrierEqualsTheSineRingModulatorScaledByItsRms)
{
    constexpr float carrierHz{1234.5f};
    constexpr float sineRms{0.70710678f};
    WaveTableRingModulator sut{sampleRate};
    sut.setFrequency(carrierHz);
    RingModulator reference{sampleRate};
    reference.setFrequency(carrierHz);
    for (size_t i = 0; i < 4800; ++i)
    {
        const float in = 0.5f * std::sin(static_cast<float>(twoPi) * 317.f * static_cast<float>(i) / sampleRate);
        const float expected = reference.step(in) * WaveTableRingModulator::kDefaultCarrierRms / sineRms;
        ASSERT_NEAR(expected, sut.step(in), 2e-4f) << "sample " << i;
    }
}

TEST(WaveTableRingModulator, aSineTimesASineHasItsEnergyOnTheSumAndTheDifferenceLine)
{
    constexpr size_t length{65536};
    constexpr double inputHz = 1365.0 * 48000.0 / static_cast<double>(length);
    constexpr double carrierHz = 410.0 * 48000.0 / static_cast<double>(length);
    constexpr double carrierRms{0.57};
    WaveTableRingModulator sut{sampleRate, static_cast<float>(carrierRms)};
    sut.setFrequency(static_cast<float>(carrierHz));
    std::vector<float> in(length);
    std::vector<float> out(length);
    for (size_t i = 0; i < length; ++i)
    {
        in[i] = static_cast<float>(std::sin(twoPi * inputHz * static_cast<double>(i) / 48000.0));
    }
    sut.processBlock(in, out);
    constexpr double carrierAmplitude = carrierRms * std::numbers::sqrt2;
    double residual{0.0};
    for (size_t i = 0; i < length; ++i)
    {
        const double t = static_cast<double>(i) / 48000.0;
        const double expected =
            0.5 * carrierAmplitude *
            (std::cos(twoPi * (inputHz - carrierHz) * t) - std::cos(twoPi * (inputHz + carrierHz) * t));
        residual += (static_cast<double>(out[i]) - expected) * (static_cast<double>(out[i]) - expected);
    }
    const double residualRms = std::sqrt(residual / static_cast<double>(length));
    EXPECT_LT(20.0 * std::log10(residualRms / rmsOf(out)), -80.0);
}

TEST(WaveTableRingModulator, theCarrierHasTheRequestedRmsAtEveryMorphAndPitch)
{
    constexpr size_t cycles{20};
    for (const size_t period : {480u, 240u, 100u, 48u, 24u, 12u})
    {
        const float hz = sampleRate / static_cast<float>(period);
        for (int step = 0; step <= 20; ++step)
        {
            WaveTableRingModulator sut{sampleRate};
            sut.setFrequency(hz);
            sut.setMorph(static_cast<float>(step) / 20.f);
            const double dB = 20.0 * std::log10(rmsOf(carrierOf(sut, period * cycles)) / 0.57);
            EXPECT_NEAR(0.0, dB, 0.1) << hz << " Hz morph " << step << "/20";
        }
    }
}

TEST(WaveTableRingModulator, theRequestedRmsIsTheConstructorArgument)
{
    WaveTableRingModulator sut{sampleRate, 0.3f};
    sut.setFrequency(480.f);
    sut.setMorph(0.75f);
    EXPECT_NEAR(0.3, rmsOf(carrierOf(sut, 100 * 20)), 0.3 * 0.012);
}

TEST(WaveTableRingModulator, theDefaultWavesHaveNoDcAtAnyMorph)
{
    for (int step = 0; step <= 20; ++step)
    {
        WaveTableRingModulator sut{sampleRate};
        sut.setFrequency(480.f);
        sut.setMorph(static_cast<float>(step) / 20.f);
        EXPECT_NEAR(0.0, meanOf(carrierOf(sut, 100 * 20)), 1e-3) << step;
    }
}

TEST(WaveTableRingModulator, theMorphWalksFromTheSineToTheSquareToTheSharkFin)
{
    constexpr size_t cycles{20};
    constexpr size_t period{480};
    const auto harmonics = [&](const float morph)
    {
        WaveTableRingModulator sut{sampleRate};
        sut.setFrequency(sampleRate / static_cast<float>(period));
        sut.setMorph(morph);
        const auto carrier = carrierOf(sut, period * cycles);
        std::array<double, 10> amplitudes{};
        for (size_t k = 1; k < amplitudes.size(); ++k)
        {
            amplitudes[k] = harmonicAmplitude(carrier, cycles, k) / harmonicAmplitude(carrier, cycles, 1);
        }
        return amplitudes;
    };
    const auto& fin = WaveTableStore::getTableSet(BasicWave::SharkFin).tables[0].data;
    const std::vector<float> finCycle(fin.begin(), fin.begin() + WaveTableSize);
    const auto sine = harmonics(0.f);
    const auto square = harmonics(0.5f);
    const auto sharkFin = harmonics(1.f);
    for (size_t k = 2; k < sine.size(); ++k)
    {
        EXPECT_LT(sine[k], 1e-3) << "sine, harmonic " << k;
        EXPECT_NEAR(k % 2 == 1 ? 1.0 / static_cast<double>(k) : 0.0, square[k], 0.01) << "square, harmonic " << k;
        EXPECT_NEAR(harmonicAmplitude(finCycle, 1, k) / harmonicAmplitude(finCycle, 1, 1), sharkFin[k], 0.01)
            << "shark fin, harmonic " << k;
    }
}

TEST(WaveTableRingModulator, theFrequencyIsLimitedAndAnUnusableValueIsIgnored)
{
    WaveTableRingModulator sut{sampleRate};
    sut.setFrequency(1.0e9f);
    EXPECT_FLOAT_EQ(0.25f * sampleRate, sut.frequency());
    sut.setFrequency(0.f);
    EXPECT_FLOAT_EQ(WaveTableRingModulator::kMinFrequency, sut.frequency());
    sut.setFrequency(-5.f);
    EXPECT_FLOAT_EQ(WaveTableRingModulator::kMinFrequency, sut.frequency());
    sut.setFrequency(1000.f);
    sut.setFrequency(std::nanf(""));
    EXPECT_FLOAT_EQ(1000.f, sut.frequency());
    sut.setFrequency(INFINITY);
    EXPECT_FLOAT_EQ(1000.f, sut.frequency());
}

TEST(WaveTableRingModulator, theMorphIsClamped)
{
    WaveTableRingModulator sut{sampleRate};
    sut.setMorph(3.f);
    EXPECT_FLOAT_EQ(1.f, sut.morph());
    sut.setMorph(-3.f);
    EXPECT_FLOAT_EQ(0.f, sut.morph());
}

TEST(WaveTableRingModulator, aChangeOfFrequencyInsideTheBlockLeavesNoJump)
{
    const auto largestStep = [](const std::vector<float>& v)
    {
        double step{0.0};
        for (size_t i = 1; i < v.size(); ++i)
        {
            step = std::max(step, static_cast<double>(std::abs(v[i] - v[i - 1])));
        }
        return step;
    };
    WaveTableRingModulator moved{sampleRate};
    moved.setFrequency(1000.f);
    std::vector<float> trace = carrierOf(moved, 64);
    moved.setFrequency(1100.f);
    const auto after = carrierOf(moved, 480);
    trace.insert(trace.end(), after.begin(), after.end());
    WaveTableRingModulator steady{sampleRate};
    steady.setFrequency(1100.f);
    EXPECT_LE(largestStep(trace), 1.05 * largestStep(carrierOf(steady, 2000)));
}

TEST(WaveTableRingModulator, aBlockEqualsTheSamplesOneByOneAndMayWorkInPlace)
{
    WaveTableRingModulator blockwise{sampleRate};
    WaveTableRingModulator single{sampleRate};
    for (WaveTableRingModulator* sut : {&blockwise, &single})
    {
        sut->setFrequency(777.f);
        sut->setMorph(0.37f);
    }
    std::vector<float> in(1000);
    std::ranges::generate(in, [n = 0]() mutable { return std::sin(0.01f * static_cast<float>(n++)); });
    std::vector<float> inPlace{in};
    blockwise.processBlock(inPlace, inPlace);
    for (size_t i = 0; i < in.size(); ++i)
    {
        ASSERT_FLOAT_EQ(single.step(in[i]), inPlace[i]) << "sample " << i;
    }
}

TEST(WaveTableRingModulator, resettingThePhaseRestartsTheCarrierAtZero)
{
    WaveTableRingModulator sut{sampleRate};
    sut.setFrequency(1000.f);
    static_cast<void>(carrierOf(sut, 37));
    sut.resetPhase();
    EXPECT_NEAR(0.0f, sut.step(1.f), 1e-6f);
}

TEST(WaveTableRingModulator, everyWaveOfTheStoreIsFreeOfDcSoTheInputNeverPassesThrough)
{
    for (const BasicWave wave : {BasicWave::RectifiedSine, BasicWave::RectifiedTriangle, BasicWave::Pulse1})
    {
        WaveTableRingModulator sut{sampleRate};
        sut.setWaveset(0, wave);
        sut.setMorph(0.f);
        sut.setFrequency(480.f);
        EXPECT_NEAR(0.0, meanOf(carrierOf(sut, 100 * 20)), 1e-3) << static_cast<int>(wave);
    }
}

TEST(WaveTableRingModulator, theWavesHaveTheirTextbookPolarity)
{
    constexpr size_t period{480};
    const auto cycle = [&](const float morph)
    {
        WaveTableRingModulator sut{sampleRate};
        sut.setFrequency(sampleRate / static_cast<float>(period));
        sut.setMorph(morph);
        sut.resetPhase();
        return carrierOf(sut, period);
    };
    const auto sine = cycle(0.f);
    const auto square = cycle(0.5f);
    const auto sharkFin = cycle(1.f);
    EXPECT_GT(sine[period / 4], 0.5f);
    EXPECT_LT(sine[3 * period / 4], -0.5f);
    EXPECT_GT(square[period / 4], 0.3f);
    EXPECT_LT(square[3 * period / 4], -0.3f);
    EXPECT_GT(sharkFin[period / 10], sharkFin[period / 4]);
    EXPECT_GT(sharkFin[period / 4], sharkFin[2 * period / 5]);
}

TEST(WaveTableRingModulator, replacingAWaveKeepsTheRequestedRms)
{
    WaveTableRingModulator sut{sampleRate};
    sut.setWaveset(2, BasicWave::Triangle);
    sut.setFrequency(480.f);
    sut.setMorph(1.f);
    EXPECT_NEAR(0.57, rmsOf(carrierOf(sut, 100 * 20)), 0.57 * 0.012);
}

TEST(WaveTableRingModulator, anOpenInputFilterLeavesTheProductAlone)
{
    WaveTableRingModulator sut{sampleRate};
    WaveTableRingModulator carrierOnly{sampleRate};
    sut.setFrequency(900.f);
    carrierOnly.setFrequency(900.f);
    sut.setInputCutoff(Chebyshev4Lowpass::kOpenCutoff);
    for (size_t i = 0; i < 2000; ++i)
    {
        const float in = 0.8f * std::sin(0.37f * static_cast<float>(i));
        ASSERT_NEAR(carrierOnly.step(1.f) * in, sut.step(in), 1e-6f) << "sample " << i;
    }
}

TEST(WaveTableRingModulator, aClosedInputFilterKeepsHighInputOutOfTheProduct)
{
    const auto outputRms = [](const float cutoff)
    {
        WaveTableRingModulator sut{sampleRate};
        sut.setFrequency(300.f);
        sut.setInputCutoff(cutoff);
        std::vector<float> out(24000);
        for (size_t i = 0; i < out.size(); ++i)
        {
            out[i] =
                sut.step(0.5f * std::sin(static_cast<float>(twoPi) * 10000.f * static_cast<float>(i) / sampleRate));
        }
        return rmsOf(std::vector<float>(out.begin() + 4800, out.end()));
    };
    EXPECT_LT(20.0 * std::log10(outputRms(1000.f) / outputRms(Chebyshev4Lowpass::kOpenCutoff)), -40.0);
}

TEST(WaveTableRingModulator, theInputFilterRemovesWhatWouldFoldBackAgainstASquareCarrier)
{
    const auto outputRms = [](const float cutoff)
    {
        WaveTableRingModulator sut{sampleRate};
        sut.setWaveset(1, BasicWave::Square);
        sut.setMorph(0.5f);
        sut.setFrequency(5000.f);
        sut.setInputCutoff(cutoff);
        std::vector<float> out(24000);
        for (size_t i = 0; i < out.size(); ++i)
        {
            out[i] = sut.step(0.5f * std::sin(static_cast<float>(twoPi) * 0.4f * static_cast<float>(i)));
        }
        return rmsOf(std::vector<float>(out.begin() + 4800, out.end()));
    };
    EXPECT_LT(20.0 * std::log10(outputRms(0.2f * sampleRate) / outputRms(Chebyshev4Lowpass::kOpenCutoff)), -40.0);
}

TEST(WaveTableRingModulator, anUnusableInputCutoffIsIgnored)
{
    WaveTableRingModulator a{sampleRate};
    WaveTableRingModulator b{sampleRate};
    a.setInputCutoff(1000.f);
    b.setInputCutoff(1000.f);
    b.setInputCutoff(std::nanf(""));
    b.setInputCutoff(INFINITY);
    for (size_t i = 0; i < 500; ++i)
    {
        const float in = std::sin(0.2f * static_cast<float>(i));
        ASSERT_EQ(a.step(in), b.step(in)) << "sample " << i;
    }
}

TEST(WaveTableRingModulator, resetClearsTheInputFilterAndTheCarrierPhase)
{
    WaveTableRingModulator used{sampleRate};
    WaveTableRingModulator rested{sampleRate};
    for (WaveTableRingModulator* sut : {&used, &rested})
    {
        sut->setFrequency(1000.f);
        sut->setInputCutoff(2000.f);
    }
    for (int i = 0; i < 300; ++i)
    {
        static_cast<void>(used.step(0.7f * std::sin(0.1f * static_cast<float>(i))));
        static_cast<void>(rested.step(0.f));
    }
    used.reset();
    rested.reset();
    for (int i = 0; i < 100; ++i)
    {
        const float in = 0.5f * std::sin(0.3f * static_cast<float>(i));
        ASSERT_EQ(rested.step(in), used.step(in)) << "sample " << i;
    }
}

TEST(WaveTableRingModulator, settingTheInputCutoffEveryControlBlockKeepsAnOpenFilterBitExact)
{
    WaveTableRingModulator sut{sampleRate};
    WaveTableRingModulator carrierOnly{sampleRate};
    sut.setFrequency(700.f);
    carrierOnly.setFrequency(700.f);
    for (size_t block = 0; block < 100; ++block)
    {
        sut.setInputCutoff(Chebyshev4Lowpass::kOpenCutoff);
        for (size_t i = 0; i < 16; ++i)
        {
            const float in = 0.6f * std::sin(0.11f * static_cast<float>(block * 16 + i));
            ASSERT_NEAR(carrierOnly.step(1.f) * in, sut.step(in), 1e-6f);
        }
    }
}

TEST(WaveTableRingModulator, processingAndTheSettersAllocateNothing)
{
    WaveTableRingModulator sut{sampleRate};
    std::vector<float> in(256, 0.25f);
    std::vector<float> out(256);
    tAllocations = 0;
    tCountAllocations = true;
    for (int i = 0; i < 50; ++i)
    {
        sut.setFrequency(200.f + 17.f * static_cast<float>(i));
        sut.setMorph(0.02f * static_cast<float>(i));
        sut.processBlock(in, out);
        static_cast<void>(sut.step(0.1f));
    }
    tCountAllocations = false;
    EXPECT_EQ(0u, tAllocations);
}

} // namespace AbacDsp::Test
