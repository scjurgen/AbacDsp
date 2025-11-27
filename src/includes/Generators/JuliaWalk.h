#pragma once


#include <cmath>
#include <numbers>

class JuliaIter
{
    static constexpr size_t MaxIter{100};
    static constexpr float BailRadius{10000.f};

  public:
    void setCx(const float cx_)
    {
        cr = cx_;
    }

    void setCy(const float cy_)
    {
        ci = cy_;
    }
    void setPower(const float power_)
    {
        power = power_;
    }

    float getIter(const float zr_, const float zi_)
    {
        auto zr = zr_;
        auto zi = zi_;
        float magnitude{0};

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
                return 0;
            }
        }
        return static_cast<float>(iter) - log(log(magnitude) / log(BailRadius) * power) / log(power);
        //        return static_cast<float>(iter) + (2.0f - std::log(magnitude) / std::log(BailRadius));
    }

  protected:
    float cr{}, ci{};
    float power{2};
};

class JuliaWalk : public JuliaIter
{
  public:
    explicit JuliaWalk(const float sampleRate)
        : m_sampleRate(sampleRate)
    {
        setFrequency(440.f);
    }
    void setCx(const float cx_)
    {
        cr = cx_;
    }
    void setCy(const float cy_)
    {
        ci = cy_;
    }
    void setJx(const float jx)
    {
        m_jr = jx;
    }
    void setJy(const float jy)
    {
        m_ji = jy;
    }
    void setJxRad(const float jx_rad)
    {
        m_jr_rad = jx_rad;
    }
    void setJyRad(const float jy_rad)
    {
        m_ji_rad = jy_rad;
    }
    void setFrequency(const float f)
    {
        m_advance = f / m_sampleRate;
    }

    float next()
    {
        m_phase += m_advance;
        if (m_phase >= 2 * std::numbers::pi_v<float>)
        {
            m_phase -= 2 * std::numbers::pi_v<float>;
        }
        const auto r = m_jr + std::cos(m_phase) * m_jr_rad;
        const auto i = m_ji + std::sin(m_phase) * m_ji_rad;
        return getIter(r, i);
    }

    float m_sampleRate;
    float m_jr{};
    float m_ji{};
    float m_jr_rad{};
    float m_ji_rad{};
    float m_phase{0};
    float m_advance{};
};