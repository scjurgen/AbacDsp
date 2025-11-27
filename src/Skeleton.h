#pragma once

#include <algorithm>
#include <array>
#include <vector>

namespace AbacDsp
{

template <size_t BlockSize>
class Skeleton
{
  public:
    Skeleton(const float sampleRate)
        : m_sampleRate(sampleRate)
    {
    }

    float step(const float in) noexcept
    {
        return in;
    }

    void processBlock(const float* source, float* target, const size_t numSamples) noexcept
    {
        std::transform(source, source + numSamples, target, [this](const float in) { return step(in); });
    }

    void processBlock(const float* source, float* target) noexcept
    {
        std::transform(source, source + BlockSize, target, [this](const float in) { return step(in); });
    }
    void processBlock(const std::array<float, BlockSize>& source, std::array<float, BlockSize>& target) noexcept
    {
        std::transform(source.begin(), source.end(), target.begin(), [this](const float in) { return step(in); });
    }

  private:
    float m_sampleRate;
};

}
