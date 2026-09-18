#pragma once

#include <algorithm>
#include <cmath>

#include "Graph/Node.h"

namespace AbacDsp::Graph::Nodes
{

/**
 * @ingroup graph
 * @brief Power-law response shaper on a normalized [0,1] input. Ports: in -> out.
 * Parameter 0: exponent (default 1, identity).
 *
 * Reshapes response, unlike Map/ExpMap's range remapping - chain a Curve into
 * a Map to reshape then rescale.
 */
class Curve final : public Node
{
  public:
    void process(const std::span<const float*> inputs, const std::span<float*> outputs,
                 const size_t numSamples) noexcept override
    {
        for (size_t i = 0; i < numSamples; ++i)
        {
            outputs[0][i] = std::pow(std::clamp(inputs[0][i], 0.0f, 1.0f), m_exponent);
        }
    }

    void setParameter(const size_t paramIndex, const float value) noexcept override
    {
        if (paramIndex == 0)
        {
            m_exponent = value;
        }
    }

  private:
    float m_exponent{1.0f};
};

}
