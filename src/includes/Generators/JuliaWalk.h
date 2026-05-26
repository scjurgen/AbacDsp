#pragma once

#include <cmath>
#include <numbers>

namespace AbacDsp
{

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

}  // namespace AbacDsp