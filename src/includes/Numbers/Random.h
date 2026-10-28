#pragma once

#include <array>
#include <cmath>
#include <cstdint>
#include <numbers>

namespace AbacDsp
{

/**
 * @ingroup numbers
 * @brief Seeded pseudo random generator: xoshiro256** with splitmix64 seeding, the same numbers on every platform.
 *
 * Every instance owns its 256-bit state, so instances never share or disturb a sequence, and a seed reproduces the
 * same integer stream everywhere because the algorithm is pure 64-bit integer arithmetic. The std:: distributions
 * are avoided on purpose: libc++ and libstdc++ implement them differently, so the same seed would give different
 * sequences on different platforms.
 *
 * Uniform floats take the top 24 bits (the low bits of xoshiro256** are the weakest), which is exactly the float
 * mantissa: the result is k * 2^-24 with k in [0, 2^24). gaussian() uses the Box-Muller transform; it involves
 * log, cos and sin, so its values can differ in the last bit between C libraries although the integer stream is
 * identical. No allocation, no locks; real-time safe.
 * @see https://prng.di.unimi.it/ (D. Blackman, S. Vigna, "Scrambled Linear Pseudorandom Number Generators", ACM TOMS
 * 47(4), 2021)
 * @see https://en.wikipedia.org/wiki/Box%E2%80%93Muller_transform
 */
class Random
{
  public:
    static constexpr uint64_t kDefaultSeed{0x4a61797061636b73ULL};

    explicit Random(const uint64_t seedValue = kDefaultSeed) noexcept
    {
        seed(seedValue);
    }

    /// Expands one 64-bit seed into the full state with splitmix64; any seed, including 0, gives a valid state.
    void seed(uint64_t seedValue) noexcept
    {
        for (auto& word : m_state)
        {
            word = splitMix64(seedValue);
        }
        m_hasSpareGaussian = false;
    }

    /// Sets the raw state, e.g. to reproduce reference vectors. The state must not be all zero.
    void setState(const std::array<uint64_t, 4>& state) noexcept
    {
        m_state = state;
        m_hasSpareGaussian = false;
    }

    [[nodiscard]] uint64_t next() noexcept
    {
        const uint64_t result = rotateLeft(m_state[1] * 5, 7) * 9;
        const uint64_t shifted = m_state[1] << 17;
        m_state[2] ^= m_state[0];
        m_state[3] ^= m_state[1];
        m_state[1] ^= m_state[2];
        m_state[0] ^= m_state[3];
        m_state[2] ^= shifted;
        m_state[3] = rotateLeft(m_state[3], 45);
        return result;
    }

    /// Uniform in [0, 1), 24-bit resolution.
    [[nodiscard]] float uniform() noexcept
    {
        return static_cast<float>(next() >> 40) * 0x1.0p-24f;
    }

    /// Uniform in [0, 1), 53-bit resolution.
    [[nodiscard]] double uniformDouble() noexcept
    {
        return static_cast<double>(next() >> 11) * 0x1.0p-53;
    }

    /// Standard normal (mean 0, standard deviation 1). Box-Muller makes pairs; the second value is kept.
    [[nodiscard]] float gaussian() noexcept
    {
        if (m_hasSpareGaussian)
        {
            m_hasSpareGaussian = false;
            return m_spareGaussian;
        }
        const double u1 = static_cast<double>((next() >> 11) + 1) * 0x1.0p-53; // (0, 1], keeps log finite
        const double u2 = uniformDouble();
        const double radius = std::sqrt(-2.0 * std::log(u1));
        const double angle = 2.0 * std::numbers::pi * u2;
        m_spareGaussian = static_cast<float>(radius * std::sin(angle));
        m_hasSpareGaussian = true;
        return static_cast<float>(radius * std::cos(angle));
    }

  private:
    [[nodiscard]] static constexpr uint64_t rotateLeft(const uint64_t x, const int k) noexcept
    {
        return (x << k) | (x >> (64 - k));
    }

    [[nodiscard]] static uint64_t splitMix64(uint64_t& x) noexcept
    {
        uint64_t z = (x += 0x9e3779b97f4a7c15ULL);
        z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
        z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
        return z ^ (z >> 31);
    }

    std::array<uint64_t, 4> m_state{};
    float m_spareGaussian{0.0f};
    bool m_hasSpareGaussian{false};
};

}
