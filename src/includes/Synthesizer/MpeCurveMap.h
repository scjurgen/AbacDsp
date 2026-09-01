#pragma once

#include <algorithm>
#include <array>
#include <cmath>

namespace AbacDsp
{

/**
 * @ingroup generators
 * @brief Five fixed lookup tables mapping a normalised position through curves
 * from cube-root through cubic.
 *
 * Precomputed once at construction rather than evaluating pow() per lookup;
 * curveIndex selects response shape (0: cube-root, 1: square-root, 2: linear,
 * 3: square, 4: cube), position selects where on that curve to read.
 */
class MpeCurveMap
{
  public:
    static constexpr size_t kMapSize{16384};
    static constexpr size_t kNumCurves{5};

    MpeCurveMap() noexcept
    {
        initWithPower(m_curves[0], 1.0f / 3.0f);
        initWithPower(m_curves[1], 0.5f);
        initWithPower(m_curves[2], 1.0f);
        initWithPower(m_curves[3], 2.0f);
        initWithPower(m_curves[4], 3.0f);
    }

    [[nodiscard]] float get(const size_t curveIndex, const int position) const noexcept
    {
        const auto clamped = std::clamp(position, 0, static_cast<int>(kMapSize) - 1);
        return m_curves[curveIndex][static_cast<size_t>(clamped)];
    }

  private:
    static void initWithPower(std::array<float, kMapSize>& curve, const float exponent) noexcept
    {
        for (size_t i = 0; i < curve.size(); ++i)
        {
            const auto x = static_cast<float>(i) / static_cast<float>(curve.size() - 1);
            curve[i] = std::pow(x, exponent);
        }
    }

    std::array<std::array<float, kMapSize>, kNumCurves> m_curves{};
};

}
