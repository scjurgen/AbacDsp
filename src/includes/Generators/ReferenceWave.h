#pragma once

#include <cmath>
#include <numbers>
#include <vector>
namespace AbacDsp
{
inline void renderReferenceSineWave(std::vector<float>& target, const double sampleRate, const double frequency)
{
    double phase = 0.0;
    const double advance = frequency / sampleRate;

    for (size_t frameIdx = 0; frameIdx < target.size(); ++frameIdx)
    {
        target[frameIdx] = static_cast<float>(sin(phase * std::numbers::pi_v<float> * 2));
        phase += advance;
        if (phase > 1)
        {
            phase -= 1;
        }
    }
}
}