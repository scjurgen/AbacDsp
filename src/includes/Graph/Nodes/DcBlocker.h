#pragma once

#include "Filters/OnePoleFilter.h"
#include "Graph/Node.h"

namespace AbacDsp::Graph::Nodes
{

/**
 * @ingroup graph
 * @brief Wraps AbacDsp::OnePoleFilter<HighPass> pinned at a fixed 20 Hz
 * cutoff. Ports: in -> out. No parameters.
 */
class DcBlocker final : public Node
{
  public:
    explicit DcBlocker(const float sampleRate) noexcept
        : m_filter(sampleRate, kCutoffHz)
    {
    }

    void process(const std::span<const float*> inputs, const std::span<float*> outputs,
                 const size_t numSamples) noexcept override
    {
        for (size_t i = 0; i < numSamples; ++i)
        {
            outputs[0][i] = m_filter.step(inputs[0][i]);
        }
    }

  private:
    static constexpr float kCutoffHz{20.0f};
    AbacDsp::OnePoleFilter<AbacDsp::OnePoleFilterCharacteristic::HighPass> m_filter;
};

}
