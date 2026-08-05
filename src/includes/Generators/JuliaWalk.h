#pragma once

#include <cmath>
#include <numbers>

namespace AbacDsp
{

/**
 * @ingroup generators
 * @brief Escape-time iteration of a Julia set, used as a deterministic modulation source.
 *
 * Iterating z -> z^power + c gives a value that varies wildly with position but
 * is entirely reproducible from its coordinates: no state, no seed, and the
 * same input always yields the same output. That makes it a source of
 * structured irregularity where a random generator would give unrepeatable
 * results and an LFO would give an audible period.
 *
 * The bail radius is far above the usual 2 because the iteration count is
 * wanted as a smooth quantity, not as a set membership test; escaping early
 * would quantise the output into visible steps.
 * @see https://en.wikipedia.org/wiki/Julia_set
 */
class JuliaIter
{
    static constexpr size_t MaxIter{100};
    static constexpr float BailRadius{10000.f};

  public:
    void setCx(const float cx_) noexcept
    {
        cr = cx_;
    }

    void setCy(const float cy_) noexcept
    {
        ci = cy_;
    }

    void setPower(const float power_) noexcept
    {
        power = power_;
    }

    [[nodiscard]] float getIter(const float zr_, const float zi_) noexcept
    {
        auto zr = zr_;
        auto zi = zi_;
        float magnitude{0.f};

        size_t iter = 0;
        while (magnitude < BailRadius)
        {
            magnitude = std::sqrt(zr * zr + zi * zi);
            const auto angle = std::atan2(zi, zr);
            const auto new_magnitude = std::pow(magnitude, power);
            const auto new_angle = angle * power;
            zr = new_magnitude * std::cos(new_angle) + cr;
            zi = new_magnitude * std::sin(new_angle) + ci;
            if (iter++ > MaxIter)
            {
                return 0.f;
            }
        }
        return static_cast<float>(iter) -
               std::log(std::log(magnitude) / std::log(BailRadius) * power) / std::log(power);
    }

  protected:
    float cr{}, ci{};
    float power{2.f};
};

/**
 * @ingroup generators
 * @brief Oscillator that reads its waveform off a circular path through a Julia set.
 *
 * A phasor drives a point around an ellipse in the complex plane and the escape
 * count at that point becomes the output sample. The waveform is therefore
 * whatever the fractal looks like along that path: strictly periodic, so it has
 * a definite pitch, but with harmonic content that changes completely as the
 * path is moved or resized.
 *
 * Not band-limited. The escape count is a step function of position, so the
 * output has discontinuities and will alias.
 */
class JuliaWalk : public JuliaIter
{
  public:
    explicit JuliaWalk(const float sampleRate)
        : m_sampleRate(sampleRate)
    {
        setFrequency(440.f);
    }
    void setCx(const float cx_) noexcept
    {
        cr = cx_;
    }
    void setCy(const float cy_) noexcept
    {
        ci = cy_;
    }
    void setJx(const float jx) noexcept
    {
        m_jr = jx;
    }
    void setJy(const float jy) noexcept
    {
        m_ji = jy;
    }
    void setJxRad(const float jx_rad) noexcept
    {
        m_jr_rad = jx_rad;
    }
    void setJyRad(const float jy_rad) noexcept
    {
        m_ji_rad = jy_rad;
    }
    void setFrequency(const float f) noexcept
    {
        m_advance = f / m_sampleRate;
    }

    [[nodiscard]] float next() noexcept
    {
        m_phase += m_advance;
        if (m_phase >= 2.f * std::numbers::pi_v<float>)
        {
            m_phase -= 2.f * std::numbers::pi_v<float>;
        }
        const auto r = m_jr + std::cos(m_phase) * m_jr_rad;
        const auto i = m_ji + std::sin(m_phase) * m_ji_rad;
        return getIter(r, i);
    }

    const float m_sampleRate;
    float m_jr{};
    float m_ji{};
    float m_jr_rad{};
    float m_ji_rad{};
    float m_phase{0.f};
    float m_advance{};
};

}