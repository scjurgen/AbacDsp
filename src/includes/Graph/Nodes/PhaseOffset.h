#pragma once

#include <cmath>

#include "Graph/Node.h"

namespace AbacDsp::Graph::Nodes
{

/**
 * @ingroup graph
 * @brief Wraps a normalized [0,1) cyclic value by a fixed offset. Ports: in ->
 * out. Parameter 0: offset (default 0).
 */
class PhaseOffset final : public Node
{
  public:
    void process(const std::span<const float*> inputs, const std::span<float*> outputs,
                 const size_t numSamples) noexcept override
    {
        for (size_t i = 0; i < numSamples; ++i)
        {
            float wrapped = std::fmod(inputs[0][i] + m_offset, 1.0f);
            if (wrapped < 0.0f)
            {
                wrapped += 1.0f;
            }
            outputs[0][i] = wrapped;
        }
    }

    void setParameter(const size_t paramIndex, const float value) noexcept override
    {
        if (paramIndex == 0)
        {
            m_offset = value;
        }
    }

  private:
    float m_offset{0.0f};
};

}
