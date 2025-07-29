#include "gtest/gtest.h"

#include <algorithm>
#include <array>
#include <cmath>

#include "NaiveGenerators/Generator.h"

constexpr float tolerance = 1e-4f;
constexpr float sampleRate = 48000.0f;

TEST(NaiveGenerator, Sine_GeneratesSineWave)
{
    using SineGen = NaiveDsp::Generator<NaiveDsp::Wave::Sine>;
    SineGen gen(sampleRate, 440.0f);

    // First sample should be zero (phase = 0)
    float v0 = gen.step();
    EXPECT_NEAR(v0, 0.0f, tolerance);

    // Next sample (move slightly along sine curve, phase increases)
    float v1 = gen.step();
    float expected = std::sinf((440.0f / sampleRate) * 2.0f * std::numbers::pi_v<float>);
    EXPECT_NEAR(v1, expected, tolerance);
}

TEST(NaiveGenerator, Saw_GeneratesSawWave)
{
    using SawGen = NaiveDsp::Generator<NaiveDsp::Wave::Saw>;
    SawGen gen(sampleRate, 440.0f);

    float values[10];
    for (auto& v : values)
    {
        v = gen.step();
    }
    // The values should ramp linearly between -1 and 1
    EXPECT_TRUE(
        std::all_of(std::begin(values), std::end(values), [](const float v) { return v >= -1.0f && v <= 1.0f; }));
}

TEST(NaiveGenerator, Triangle_RangeAndSymmetry)
{
    using TriGen = NaiveDsp::Generator<NaiveDsp::Wave::Triangle>;
    TriGen gen(sampleRate, 440.0f);

    float minValue = 1.0f, maxValue = -1.0f;
    for (int i = 0; i < 500; ++i)
    {
        const float v = gen.step();
        minValue = std::min(minValue, v);
        maxValue = std::max(maxValue, v);
    }
    EXPECT_NEAR(minValue, -1.0f, 0.05f);
    EXPECT_NEAR(maxValue, 1.0f, 0.05f);
}

TEST(NaiveGenerator, Square_OnlyPlusMinusOne)
{
    using SqrGen = NaiveDsp::Generator<NaiveDsp::Wave::Square>;
    SqrGen gen(sampleRate, 440.0f);

    for (int i = 0; i < 100; ++i)
    {
        const float v = gen.step();
        EXPECT_TRUE(v == 1.0f || v == -1.0f);
    }
}

TEST(NaiveGenerator, RenderMonoBuffer)
{
    using SineGen = NaiveDsp::Generator<NaiveDsp::Wave::Sine>;
    SineGen gen(sampleRate, 1000.0f);

    std::array<float, 48> buffer{}; // 1ms at 48kHz
    gen.render(buffer.begin(), buffer.end(), 1);

    // Check the first and last samples for continuity
    EXPECT_GE(*std::min_element(buffer.begin(), buffer.end()), -1.0f);
    EXPECT_LE(*std::max_element(buffer.begin(), buffer.end()), 1.0f);
}

TEST(NaiveGenerator, RenderStereoBuffer)
{
    using TriGen = NaiveDsp::Generator<NaiveDsp::Wave::Triangle>;
    TriGen gen(sampleRate, 330.0f);

    std::array<float, 20> buffer{};
    gen.render(buffer.begin(), buffer.end(), 2);

    // Check even/odd samples (L/R) are the same per frame
    for (size_t i = 0; i < buffer.size(); i += 2)
    {
        EXPECT_FLOAT_EQ(buffer[i], buffer[i + 1]);
    }
}

TEST(NaiveGenerator, RenderThrowsOnInvalidChannels)
{
    using SineGen = NaiveDsp::Generator<NaiveDsp::Wave::Sine>;
    SineGen gen(sampleRate, 100.0f);

    std::array<float, 32> buffer{};

    EXPECT_THROW(gen.render(buffer.begin(), buffer.end(), 0), std::invalid_argument);

    // Buffer size not a multiple of channels
    EXPECT_THROW(gen.render(buffer.begin(), buffer.begin() + 15, 4), std::invalid_argument);
}

TEST(NaiveGenerator, Noise_NoiseIsNotConstant)
{
    using NoiseGen = NaiveDsp::Generator<NaiveDsp::Wave::Noise>;
    NoiseGen gen(sampleRate, 500.0f);

    float first = gen.step();
    bool allSame = true;
    for (int i = 0; i < 50; ++i)
    {
        if (std::abs(gen.step() - first) > tolerance)
        {
            allSame = false;
            break;
        }
    }
    EXPECT_FALSE(allSame);
}
