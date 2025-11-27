#pragma once

#include <array>
#include <cmath>
#include <random>
#include <algorithm>

// Generates sparse crackling noise for lightning discharge simulation
// Uses interleaved velvet noise with time-varying density and resonant filtering
class VelvetCrackleGenerator
{
  public:
    VelvetCrackleGenerator()
    {
        reset();
    }

    void reset()
    {
        m_phase = 0;
        m_density_envelope = 0.0f;
        std::fill(m_filter_states.begin(), m_filter_states.end(), FilterState{});
    }

    // intensity: 0-1, controls crackling density and amplitude
    // brightness: 0-1, shifts spectral balance (higher = more high-frequency)
    float process(float intensity, float brightness)
    {
        // Time-varying pulse density based on intensity
        const int base_grid = 96; // ~500 Hz at 48kHz
        const int density_mod = static_cast<int>(intensity * 384.0f);
        const int grid_size = base_grid + density_mod;

        // Generate velvet impulse
        float impulse = 0.0f;
        if (m_phase % grid_size == 0)
        {
            // Bernoulli process for impulse polarity and occurrence
            std::bernoulli_distribution occur_dist(0.7f + intensity * 0.25f);
            if (occur_dist(m_rng))
            {
                std::uniform_int_distribution<int> sign_dist(0, 1);
                impulse = sign_dist(m_rng) ? 1.0f : -1.0f;

                // Exponential amplitude distribution for occasional loud transients
                std::exponential_distribution<float> amp_dist(2.5f);
                float amp_scale = std::min(amp_dist(m_rng), 3.0f);
                impulse *= amp_scale * intensity;

                // Randomize filter frequencies for this impulse
                updateFilterCoefficients(brightness);
            }
        }

        // Parallel bandpass filtering with resonant modes
        float output = 0.0f;
        for (auto& filt : m_filter_states)
        {
            output += processBandpass(impulse, filt);
        }
        output *= 0.25f; // Normalize for 4 filters

        // Envelope follower for natural decay
        m_density_envelope = m_density_envelope * 0.9995f + std::abs(output) * 0.0005f;

        // High-frequency air absorption (progressive low-pass)
        output = m_air_absorption_z1 + 0.35f * (output - m_air_absorption_z1);
        m_air_absorption_z1 = output;
        ++m_phase;
        return output;
    }

  private:
    struct FilterState
    {
        float freq = 4000.0f;
        float q = 10.0f;
        float z1 = 0.0f;
        float z2 = 0.0f;
        // Biquad coefficients
        float b0 = 0.0f, b1 = 0.0f, b2 = 0.0f;
        float a1 = 0.0f, a2 = 0.0f;
    };

    void updateFilterCoefficients(float brightness)
    {
        // Four resonant modes with randomized detuning
        const std::array<float, 4> base_freqs = {2200.0f, 4500.0f, 7000.0f, 11000.0f};
        std::uniform_real_distribution<float> detune_dist(-0.15f, 0.15f);
        std::uniform_real_distribution<float> q_dist(7.0f, 13.0f);

        for (size_t i = 0; i < m_filter_states.size(); ++i)
        {
            float detune = 1.0f + detune_dist(m_rng);
            float brightness_scale = 0.5f + brightness * 0.8f;
            m_filter_states[i].freq = base_freqs[i] * detune * brightness_scale;
            m_filter_states[i].q = q_dist(m_rng);
            computeBiquadCoeffs(m_filter_states[i]);
        }
    }

    void computeBiquadCoeffs(FilterState& filt)
    {
        constexpr float sample_rate = 48000.0f;
        const float omega = 2.0f * M_PI * filt.freq / sample_rate;
        const float alpha = std::sin(omega) / (2.0f * filt.q);
        const float cos_omega = std::cos(omega);

        // Bandpass coefficients
        const float a0 = 1.0f + alpha;
        filt.b0 = alpha / a0;
        filt.b1 = 0.0f;
        filt.b2 = -alpha / a0;
        filt.a1 = (-2.0f * cos_omega) / a0;
        filt.a2 = (1.0f - alpha) / a0;
    }

    float processBandpass(float input, FilterState& filt)
    {
        const float output =
            filt.b0 * input + filt.b1 * filt.z1 + filt.b2 * filt.z2 - filt.a1 * filt.z1 - filt.a2 * filt.z2;
        filt.z2 = filt.z1;
        filt.z1 = output;
        return output;
    }

    std::mt19937 m_rng{std::random_device{}()};
    int m_phase = 0;
    float m_density_envelope = 0.0f;
    float m_air_absorption_z1 = 0.0f;
    std::array<FilterState, 4> m_filter_states;
};