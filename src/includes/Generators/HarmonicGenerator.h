#pragma once

#include <array>
#include <cmath>
#include <functional>
#include <numbers>
#include <random>

namespace AbacDsp
{
/// @ingroup generators
/// @brief One partial: frequency, starting gain, decay rate, and how late it enters.
/// The per-partial delay is what separates a struck sound from an additive chord, since real
/// resonators do not excite every mode at the same instant.
struct Harmonic
{
    float f;
    float gain;
    float decay;
    int delay;
};

/**
 * @ingroup generators
 * @brief Builds a partial series from a formula rather than a fixed table.
 *
 * Taking the frequency ratio as a callable of the partial index covers
 * harmonic, stretched and inharmonic series with one class: a piano's
 * progressive sharpening and a bell's non-integer ratios are both just a
 * different function, not a different generator.
 * @see https://en.wikipedia.org/wiki/Additive_synthesis
 */
template <size_t N>
class HarmonicGenerator
{
  public:
    using HarmonicFormula = std::function<float(int, float)>;

    explicit HarmonicGenerator(HarmonicFormula harmonicFormula)
        : m_harmonicFormula(std::move(harmonicFormula))
    {
    }

    void setFormula(HarmonicFormula harmonicFormula)
    {
        m_harmonicFormula = std::move(harmonicFormula);
    }

    void setSkew(const float value) noexcept
    {
        m_skew = std::pow(2.f, value);
    }

    void setStrength(const float value) noexcept
    {
        m_strength = value;
    }

    void setRandPower(const float value) noexcept
    {
        m_randomPower = value;
    }

    void setRandSpread(const float value) noexcept
    {
        m_randomSpread = value;
    }

    [[nodiscard]] size_t addHarmonics(Harmonic* target, size_t idx, const size_t count, const float baseFrequency,
                                      const float power)
    {
        m_power = power;

        for (int overtoneNum = 1; overtoneNum <= count && idx < count; ++overtoneNum)
        {
            const auto overtoneMultiplier = applySpreadRandomness(m_harmonicFormula(overtoneNum, m_strength) * m_skew);
            const auto overtoneFreq = baseFrequency * overtoneMultiplier;

            if (overtoneFreq >= 20000)
            {
                break;
            }

            const auto overtonePosition = static_cast<float>(overtoneNum - 1) / (count - 1);
            const auto overtonePower =
                applyPowerRandomness(calculateOvertonePower(power, m_strength, overtonePosition));
            target[idx] = Harmonic{overtoneFreq, overtonePower, 0.f, 0};
            idx++;
        }
        return idx;
    }

  private:
    float getHumanRandomness() const noexcept
    {
        std::uniform_real_distribution<float> uniform(0.f, 1.f);
        const auto u1 = uniform(m_randomGenerator);
        const auto u2 = uniform(m_randomGenerator);
        const auto gaussian = std::sqrt(-2.f * std::log(u1)) * std::cos(2.f * std::numbers::pi_v<float> * u2);
        return std::clamp(gaussian * 0.3f, -1.f, 1.f);
    }

    mutable std::mt19937 m_randomGenerator{std::random_device{}()};

    [[nodiscard]] float applyPowerRandomness(const float power) const noexcept
    {
        if (m_randomPower > 0.f)
        {
            const auto powerVariation = 1.f + getHumanRandomness() * m_randomPower * 0.3f;
            return power * powerVariation;
        }
        return power;
    }

    [[nodiscard]] float applySpreadRandomness(const float factor) const noexcept
    {
        if (m_randomSpread > 0.f)
        {
            const auto powerVariation = 1.f + getHumanRandomness() * m_randomSpread * 0.3f;
            return factor * powerVariation;
        }
        return factor;
    }

    [[nodiscard]] static float calculateOvertonePower(const float basePower, const float value,
                                                      const float overtonePosition) noexcept
    {
        float p;
        if (value <= 0.5f)
        {
            const auto decayFactor = 1.f - overtonePosition;
            const auto blend = value * 2.f;
            p = basePower * value * (decayFactor * (1.f - blend) + blend);
        }
        else
        {
            const auto increaseFactor = overtonePosition;
            const auto blend = value * 2.f - 1.f;
            p = basePower * 0.5f * (1.f - blend + increaseFactor * blend);
        }
        return p * p * p;
    }

    float m_randomPower{0.f};
    float m_randomSpread{0.f};
    float m_strength{1.f};
    float m_skew{1.f};
    float m_power{1.f};
    HarmonicFormula m_harmonicFormula;
};

/**
 * @ingroup generators
 * @brief Ready-made partial-ratio formulas for HarmonicGenerator.
 *
 * Each returns a callable of (partial index, strength) giving a frequency ratio
 * against the fundamental. The chord and tuning entries index a small ratio
 * table by index modulo its size and multiply by the octave, `index / size + 1`;
 * that integer division is deliberate floor division, not an accidental
 * truncation of a float expression.
 *
 * The percussion sets are measured inharmonic ratios, which is why they sound
 * like struck metal: no ratio is a whole multiple of another, so the partials
 * never fuse into a single perceived pitch.
 */
namespace HarmonicFormulas
{
inline auto odd()
{
    return [](const int overtoneNum, float) { return static_cast<float>(2 * overtoneNum + 1); };
}

inline auto even()
{
    return [](const int overtoneNum, float) { return static_cast<float>(2 * overtoneNum); };
}

// piano stretch forumla n*sqrt(1+N*N)
inline auto stretched()
{
    return [](const int overtoneNum, const float strength)
    {
        const auto B = strength * 0.01f;
        const auto harmonicNum = overtoneNum + 1;
        return static_cast<float>(harmonicNum) * std::sqrt(1.f + B * harmonicNum * harmonicNum);
    };
}

// Subharmonic series: f/n for n = 1, 2, 3, ...
inline auto subharmonic()
{
    return [](const int overtoneNum, float) { return 1.f / static_cast<float>(overtoneNum + 1); };
}

// Subharmonic series with configurable denominator range
inline auto subharmonic_range(const int maxDenominator)
{
    return [maxDenominator](const int overtoneNum, float)
    {
        const auto denom = std::min(overtoneNum + 1, maxDenominator);
        return 1.f / static_cast<float>(denom);
    };
}

// Just intonation major triad ratios: 4:5:6
inline auto just_major_triad()
{
    return [](const int overtoneNum, float)
    {
        constexpr std::array<float, 3> ratios = {1.f, 5.f / 4.f, 3.f / 2.f};
        return ratios[static_cast<size_t>(overtoneNum % 3)] * static_cast<float>((overtoneNum / 3) + 1);
    };
}

// Just intonation minor triad ratios: 10:12:15
inline auto just_minor_triad()
{
    return [](const int overtoneNum, float)
    {
        constexpr std::array<float, 3> ratios = {1.f, 6.f / 5.f, 3.f / 2.f};
        return ratios[static_cast<size_t>(overtoneNum % 3)] * static_cast<float>((overtoneNum / 3) + 1);
    };
}

// Pythagorean tuning based on 3:2 perfect fifths
inline auto pythagorean()
{
    return [](const int overtoneNum, float)
    {
        // Powers of 3/2, normalized to octave
        const auto fifths = static_cast<float>(overtoneNum);
        const auto ratio = std::pow(3.f / 2.f, fifths);
        // Normalize to single octave
        return ratio / std::pow(2.f, std::floor(std::log2(ratio)));
    };
}

// Bell inharmonic series based on physical modeling
inline auto bell_partials()
{
    return [](const int overtoneNum, float)
    {
        // Approximation of church bell partials
        constexpr std::array<float, 8> bell_ratios = {
            0.5f,  // Hum (sub-octave)
            1.f,   // Prime
            1.2f,  // Minor third
            1.5f,  // Perfect fifth
            2.f,   // Octave
            2.66f, // Major tenth
            3.f,   // Twelfth
            4.f    // Double octave
        };
        return bell_ratios[static_cast<size_t>(overtoneNum % 8)] * static_cast<float>((overtoneNum / 8) + 1);
    };
}

// Cymbal/brass inharmonic series with metallic character
inline auto cymbal_partials()
{
    return [](const int overtoneNum, float)
    {
        // Non-integer ratios for metallic noise
        const auto base = static_cast<float>(overtoneNum + 1);
        return base * (1.f + 0.1f * std::fmod(base * 1.618034f, 1.f));
    };
}

// Plate reverb inharmonic series
inline auto plate_partials()
{
    return [](const int overtoneNum, float)
    {
        // Dense, non-harmonic series for plate reverb simulation
        const auto n = static_cast<float>(overtoneNum + 1);
        return n * std::sqrt(2.f) + 0.5f * std::fmod(n * 1.414f, 1.f);
    };
}

// Vowel formant series (vowel morphing via strength parameter)
inline auto vowel_formants()
{
    return [](const int overtoneNum, const float strength)
    {
        // strength parameter morphs between vowels: 0.0 = "ah", 1.0 = "ee"
        constexpr std::array<float, 3> f1_range = {730.f, 270.f};   // F1: ah to ee
        constexpr std::array<float, 3> f2_range = {1090.f, 2290.f}; // F2: ah to ee
        constexpr std::array<float, 3> f3_range = {2440.f, 3010.f}; // F3: ah to ee

        const auto f1 = f1_range[0] + strength * (f1_range[1] - f1_range[0]);
        const auto f2 = f2_range[0] + strength * (f2_range[1] - f2_range[0]);
        const auto f3 = f3_range[0] + strength * (f3_range[1] - f3_range[0]);

        const std::array<float, 3> formants = {f1, f2, f3};
        return formants[static_cast<size_t>(overtoneNum % 3)];
    };
}

// Vocal tract length scaling (strength parameter scales VTL)
inline auto formant_scaled(const float vtl)
{
    return [vtl](const int overtoneNum, float)
    {
        // Base formants for average male voice
        constexpr std::array<float, 4> base_formants = {.5f, 1.5f, 2.5f, 3.5f};
        const auto vtl_scale = 1.f / (1.f + vtl * 0.5f); // VTL scaling factor
        return base_formants[static_cast<size_t>(overtoneNum % 4)] * vtl_scale;
    };
}

// Golden ratio series for dissonant textures
inline auto golden_ratio()
{
    return [](const int overtoneNum, float)
    {
        constexpr float phi = 1.61803398875f;
        return std::pow(phi, static_cast<float>(overtoneNum));
    };
}

// Fibonacci series ratios (converges to golden ratio)
inline auto fibonacci()
{
    return [](const int overtoneNum, float)
    {
        // Generate Fibonacci numbers and return ratios
        auto fib = [](int n)
        {
            float a = 0.f, b = 1.f;
            for (int i = 0; i < n; ++i)
            {
                const auto temp = a + b;
                a = b;
                b = temp;
            }
            return b;
        };
        return fib(overtoneNum + 2) / fib(overtoneNum + 1);
    };
}

// Prime number series for atonal textures
inline auto prime_harmonics()
{
    return [](const int overtoneNum, float)
    {
        // First few primes as ratios
        constexpr std::array primes = {2,   3,   5,   7,   11,  13,  17,  19,  23,  29,  31,  37,  41,  43,  47,
                                       53,  59,  61,  67,  71,  73,  79,  83,  89,  97,  101, 103, 107, 109, 113,
                                       127, 131, 137, 139, 149, 151, 157, 163, 167, 173, 179, 181, 191, 193, 197,
                                       199, 211, 223, 227, 229, 233, 239, 241, 251, 257, 263, 269, 271, 277, 281};
        return primes[static_cast<size_t>(overtoneNum) % primes.size()];
    };
}
}
}