#pragma once

#include <algorithm>

#include "Graph/Node.h"

namespace AbacDsp::Graph::Nodes
{

/**
 * @ingroup graph
 * @brief User-facing normalized control source. 0 in, 1 out. Parameter 0: value.
 *
 * process() fills every sample with the current value - the same
 * Node::setParameter() mechanism external code (e.g. a UI knob) already uses
 * everywhere else in this toolbox.
 */
class Macro final : public Node
{
  public:
    void process(const std::span<const float*>, const std::span<float*> outputs,
                 const size_t numSamples) noexcept override
    {
        std::fill_n(outputs[0], numSamples, m_value);
    }

    void setParameter(const size_t paramIndex, const float value) noexcept override
    {
        if (paramIndex == 0)
        {
            m_value = value;
        }
    }

  private:
    float m_value{0.0f};
};

}
