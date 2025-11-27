#pragma once

#include <algorithm>
#include <array>

namespace AbacDsp
{
/**
 * Pink noise filter implementation based on Paul Kellett's algorithm.
 *
 * This templated filter converts white noise to pink noise using a weighted sum
 * of first-order filters with carefully chosen pole positions.
 *
 * @tparam FastPink If true (default), uses the 3-pole "economy" version providing *                  ±0.5dB accuracy
 * above 9.2Hz at 44.1kHz sampling rate. If false, uses the 7-pole "accurate" version providing ±0.05dB accuracy with
 * higher computational cost.
 *
 * Both variants maintain the characteristic -3dB/octave rolloff of pink noise.
 * The algorithm uses fixed coefficients optimized for real-time performance.
 *
 * @see Original implementation by Paul Kellett on music-dsp mailing list
 * @see [http://www.firstpr.com.au/dsp/pink-noise/](http://www.firstpr.com.au/dsp/pink-noise/) for detailed analysis
 */


template <bool FastPink>
struct PinkCoeffs;

// 3-pole (fast) coefficients
template <>
struct PinkCoeffs<true>
{
    static constexpr auto scale = 0.05f;
    static constexpr std::array<float, 3> a{0.99765f, 0.96300f, 0.57000f};
    static constexpr std::array<float, 3> b{0.0990460f, 0.2965164f, 1.0526913f};
    static constexpr float direct = 0.1848f;
};

// 7-pole (more accurate) coefficients
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
    PinkFilter() noexcept
    {
        m_v.fill(0.0f);
    }

    /** Process a single white-noise sample into pink noise. */
    float step(float in) noexcept
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
    std::array<float, (FastPink ? 3 : 7)> m_v;
};

}