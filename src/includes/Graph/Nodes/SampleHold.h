#pragma once

#include "Graph/Node.h"

namespace AbacDsp::Graph::Nodes
{

/**
 * @ingroup graph
 * @brief Holds "in" on each rising edge of "trigger" crossing 0.5. Ports: in,
 * trigger -> out. No parameters.
 *
 * The one node in this file that needs real per-sample logic to be useful -
 * every other control node here writes one repeated value per block.
 */
class SampleHold final : public Node
{
  public:
    void process(const std::span<const float*> inputs, const std::span<float*> outputs,
                 const size_t numSamples) noexcept override
    {
        constexpr float kThreshold = 0.5f;
        for (size_t i = 0; i < numSamples; ++i)
        {
            const float trigger = inputs[1][i];
            if (trigger >= kThreshold && m_prevTrigger < kThreshold)
            {
                m_held = inputs[0][i];
            }
            m_prevTrigger = trigger;
            outputs[0][i] = m_held;
        }
    }

  private:
    float m_held{0.0f};
    float m_prevTrigger{0.0f};
};

}
