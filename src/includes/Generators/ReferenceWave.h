#pragma once

#include <cmath>
#include <numbers>
#include <vector>

namespace AbacDsp
{
/// @ingroup generators
/// @brief Fills target with a sine, phase accumulated in double.
/// The reference the band-limited generators are measured against, so accuracy matters more than speed here.
inline void renderReferenceSineWave(std::vector<float>& target, const double sampleRate, const double frequency)
{
    double phase{0.0};
    const double advance = frequency / sampleRate;

    for (size_t frameIdx = 0; frameIdx < target.size(); ++frameIdx)
    {
        target[frameIdx] = static_cast<float>(std::sin(phase * 2.0 * std::numbers::pi_v<double>));
        phase += advance;
        if (phase > 1.0)
        {
            phase -= 1.0;
        }
    }
}
}