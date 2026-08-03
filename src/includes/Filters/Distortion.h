#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>

namespace AbacDsp
{

// Symmetric atanh waveshaper. `drive` is a linear pre-gain applied before the input is clamped
// to +-tanh(1); atanh(tanh(1)) == 1 by construction, so the output is bounded to [-1, 1] without
// an explicit rescale, and drive 0 gives unity gain for small signals (atanh(x) ~= x near 0).
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
    static constexpr float kClampMargin{0.7615941560f};

    [[nodiscard]] float shape(const float v) const noexcept
    {
        const auto driven = std::clamp(v * (1.f + m_drive), -kClampMargin, kClampMargin);
        return std::atanh(driven);
    }

    float m_drive{0.f};
};

}
