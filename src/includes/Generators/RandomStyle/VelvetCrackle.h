#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>
#include <random>

namespace AbacDsp
{

/**
 * @ingroup generators
 * @brief Sparse impulse noise on a velvet grid, each impulse rung through a resonant filter.
 *
 * Velvet noise places one impulse at a random position within each fixed grid
 * cell instead of drawing a sample per frame. The result sounds smoother than
 * white noise at a fraction of the nonzero samples, because the ear hears the
 * absence of correlation, not the density.
 *
 * Ringing each impulse rather than emitting it bare is what turns hiss into
 * crackle: an isolated resonant decay reads as a discrete physical event, so
 * the output suggests vinyl or fire rather than broadband noise.
 * @see Valimaki, Holm-Rasmussen, Alary, Lehtonen, "Late Reverberation Synthesis
 *      Using Filtered Velvet Noise", Applied Sciences 7(5), 2017.
 */
class VelvetCrackleGenerator
{
  public:
    VelvetCrackleGenerator()
    {
        reset();
    }

    void reset() noexcept
    {
        m_phase = 0;
        m_density_envelope = 0.0f;
        std::ranges::fill(m_filter_states, FilterState{});
    }

    [[nodiscard]] float process(const float intensity, const float brightness)
    {
        constexpr int base_grid = 96; // ~500 Hz at 48kHz
        const int density_mod = static_cast<int>(intensity * 384.0f);
        const int grid_size = base_grid + density_mod;

        float impulse = 0.0f;
        if (m_phase % grid_size == 0)
        {
            std::bernoulli_distribution occur_dist(0.7f + intensity * 0.25f);
            if (occur_dist(m_rng))
            {
                std::uniform_int_distribution<int> sign_dist(0, 1);
                impulse = sign_dist(m_rng) ? 1.0f : -1.0f;

                std::exponential_distribution<float> amp_dist(2.5f);
                const float amp_scale = std::min(amp_dist(m_rng), 3.0f);
                impulse *= amp_scale * intensity;

                updateFilterCoefficients(brightness);
            }
        }

        float output = 0.0f;
        for (auto& filt : m_filter_states)
        {
            output += processBandpass(impulse, filt);
        }
        output *= 0.25f;

        m_density_envelope = m_density_envelope * 0.9995f + std::abs(output) * 0.0005f;

        output = m_air_absorption_z1 + 0.35f * (output - m_air_absorption_z1);
        m_air_absorption_z1 = output;
        ++m_phase;
        return output;
    }

  private:
    /// @brief One resonator: its tuning and its two state words. Impulses are assigned to these round-robin
    /// so a new crackle can start before the previous one has finished ringing.
    struct FilterState
    {
        float freq{4000.0f};
        float q{10.0f};
        float z1{0.0f};
        float z2{0.0f};
        float b0{0.0f};
        float b1{0.0f};
        float b2{0.0f};
        float a1{0.0f};
        float a2{0.0f};
    };

    void updateFilterCoefficients(const float brightness)
    {
        constexpr std::array<float, 4> base_freqs = {2200.0f, 4500.0f, 7000.0f, 11000.0f};
        std::uniform_real_distribution<float> detune_dist(-0.15f, 0.15f);
        std::uniform_real_distribution<float> q_dist(7.0f, 13.0f);

        for (size_t i = 0; i < m_filter_states.size(); ++i)
        {
            const float detune = 1.0f + detune_dist(m_rng);
            const float brightness_scale = 0.5f + brightness * 0.8f;
            m_filter_states[i].freq = base_freqs[i] * detune * brightness_scale;
            m_filter_states[i].q = q_dist(m_rng);
            computeBiquadCoeffs(m_filter_states[i]);
        }
    }

    void computeBiquadCoeffs(FilterState& filt)
    {
        constexpr float sample_rate = 48000.0f;
        const float omega = 2.0f * std::numbers::pi_v<float> * filt.freq / sample_rate;
        const float alpha = std::sin(omega) / (2.0f * filt.q);
        const float cos_omega = std::cos(omega);

        const float a0 = 1.0f + alpha;
        filt.b0 = alpha / a0;
        filt.b1 = 0.0f;
        filt.b2 = -alpha / a0;
        filt.a1 = (-2.0f * cos_omega) / a0;
        filt.a2 = (1.0f - alpha) / a0;
    }

    [[nodiscard]] float processBandpass(const float input, FilterState& filt)
    {
        const float output =
            filt.b0 * input + filt.b1 * filt.z1 + filt.b2 * filt.z2 - filt.a1 * filt.z1 - filt.a2 * filt.z2;
        filt.z2 = filt.z1;
        filt.z1 = output;
        return output;
    }

    std::mt19937 m_rng{std::random_device{}()};
    int m_phase{0};
    float m_density_envelope{0.0f};
    float m_air_absorption_z1{0.0f};
    std::array<FilterState, 4> m_filter_states;
};

}