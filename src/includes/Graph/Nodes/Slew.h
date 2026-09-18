#pragma once

#include <algorithm>

#include "Filters/OnePoleFilter.h"
#include "Graph/Node.h"

namespace AbacDsp::Graph::Nodes
{

/**
 * @ingroup graph
 * @brief One-pole symmetric lag. Ports: in -> out. Parameter 0: timeMs (default 20).
 *
 * Wraps AbacDsp::OnePoleFilter<LowPass> via setDecayTime() - a control-rate
 * "reach 90% of a step within timeMs" smoother, not a hand-rolled coefficient.
 */
class Slew final : public Node
{
  public:
    explicit Slew(const float sampleRate) noexcept
        : m_filter(sampleRate)
    {
        m_filter.setDecayTime(kDefaultTimeMs * 0.001f);
    }

    void process(const std::span<const float*> inputs, const std::span<float*> outputs,
                 const size_t numSamples) noexcept override
    {
        for (size_t i = 0; i < numSamples; ++i)
        {
            outputs[0][i] = m_filter.step(inputs[0][i]);
        }
    }

    void setParameter(const size_t paramIndex, const float value) noexcept override
    {
        if (paramIndex == 0)
        {
            m_filter.setDecayTime(std::max(value, kMinTimeMs) * 0.001f);
        }
    }

  private:
    static constexpr float kDefaultTimeMs{20.0f};
    static constexpr float kMinTimeMs{0.01f};
    AbacDsp::OnePoleFilter<AbacDsp::OnePoleFilterCharacteristic::LowPass> m_filter;
};

}
