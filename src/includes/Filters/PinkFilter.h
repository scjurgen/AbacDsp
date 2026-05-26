#pragma once

#include <array>

namespace AbacDsp
{
// Paul Kellett's pink noise algorithm; FastPink=true: 3-pole ±0.5dB, false: 7-pole ±0.05dB
template <bool FastPink>
struct PinkCoeffs;

template <>
struct PinkCoeffs<true>
{
    static constexpr auto scale = 0.05f;
    static constexpr std::array<float, 3> a{0.99765f, 0.96300f, 0.57000f};
    static constexpr std::array<float, 3> b{0.0990460f, 0.2965164f, 1.0526913f};
    static constexpr float direct = 0.1848f;
};

template <>
struct PinkCoeffs<false>
{
    static constexpr auto scale = 0.055f;
    static constexpr std::array<float, 7> a{0.99886f, 0.99332f, 0.96900f, 0.86650f, 0.55000f, 0.22500f, 0.12500f};
    static constexpr std::array<float, 7> b{0.0555179f, 0.0750759f, 0.1538520f, 0.3104856f,
                                            0.5329522f, 0.0168980f, 0.115926f};
    static constexpr float direct = 0.5362f;
};

template <bool FastPink = true>
class PinkFilter
{
  public:
    PinkFilter() noexcept = default;

    [[nodiscard]] float step(const float in) noexcept
    {
        using C = PinkCoeffs<FastPink>;
        for (size_t i = 0; i < m_v.size(); ++i)
        {
            m_v[i] = C::a[i] * m_v[i] + C::b[i] * in;
        }

        float sum = in * C::direct;
        for (auto v : m_v)
        {
            sum += v;
        }
        return sum * C::scale;
    }

    void processBlock(float* data, const size_t numSamples) noexcept
    {
        for (size_t i = 0; i < numSamples; ++i)
        {
            data[i] = step(data[i]);
        }
    }

    void processBlock(const float* in, float* out, const size_t numSamples) noexcept
    {
        for (size_t i = 0; i < numSamples; ++i)
        {
            out[i] = step(in[i]);
        }
    }

  private:
    std::array<float, (FastPink ? 3 : 7)> m_v{};
};

}