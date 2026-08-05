#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>

namespace AbacDsp
{

/**
 * @ingroup filters
 * @brief Symmetric atanh waveshaper with a linear pre-gain.
 *
 * atanh expands rather than compresses: the curve steepens towards its limit
 * instead of flattening, so this is a harder-edged shaper than the usual tanh
 * and produces its harmonics right up against the clip point rather than
 * easing into them.
 *
 * The input is clamped to +/-tanh(1) before shaping. Since atanh(tanh(1)) is
 * exactly 1, that bounds the output to [-1, 1] with no rescaling pass, and
 * drive 0 leaves small signals at unity because atanh(x) approaches x near 0.
 *
 * Memoryless, so it aliases: the harmonics it creates are not band-limited.
 * @see https://ccrma.stanford.edu/~jos/pasp/Memoryless_Nonlinearities.html
 */
class AtanhDrive
{
  public:
    void setDrive(const float drive) noexcept
    {
        m_drive = std::max(drive, 0.f);
    }

    void processBlock(const float* in, float* out, const size_t numSamples) const noexcept
    {
        std::transform(in, in + numSamples, out, [this](const float v) noexcept { return shape(v); });
    }

  private:
    /// tanh(1), the input at which atanh reaches exactly 1.
    static constexpr float kClampMargin{0.7615941560f};

    [[nodiscard]] float shape(const float v) const noexcept
    {
        const auto driven = std::clamp(v * (1.f + m_drive), -kClampMargin, kClampMargin);
        return std::atanh(driven);
    }

    float m_drive{0.f};
};

}
