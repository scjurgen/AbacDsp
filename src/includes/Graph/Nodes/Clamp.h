#pragma once

#include <algorithm>

#include "Graph/Node.h"

namespace AbacDsp::Graph::Nodes
{

/**
 * @ingroup graph
 * @brief Clamps to a range. Ports: in -> out. Parameter 0: min, 1: max.
 */
class Clamp final : public Node
{
  public:
    void process(const std::span<const float*> inputs, const std::span<float*> outputs,
                 const size_t numSamples) noexcept override
    {
        for (size_t i = 0; i < numSamples; ++i)
        {
            outputs[0][i] = std::clamp(inputs[0][i], m_min, m_max);
        }
    }

    void setParameter(const size_t paramIndex, const float value) noexcept override
    {
        if (paramIndex == 0)
        {
            m_min = value;
        }
        else if (paramIndex == 1)
        {
            m_max = value;
        }
    }

  private:
    float m_min{0.0f};
    float m_max{1.0f};
};

}
