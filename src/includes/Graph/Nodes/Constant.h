#pragma once

#include <algorithm>

#include "Graph/Node.h"

namespace AbacDsp::Graph::Nodes
{

/**
 * @ingroup graph
 * @brief A fixed value built into the graph. 0 in, 1 out, no parameters.
 *
 * Its value is NodeInstance::config ("value", static - requires recompilation
 * to change), not a parameter - unlike Macro, whose whole point is being
 * externally settable while the graph runs.
 */
class Constant final : public Node
{
  public:
    explicit Constant(const float value) noexcept
        : m_value(value)
    {
    }

    void process(const std::span<const float*>, const std::span<float*> outputs,
                 const size_t numSamples) noexcept override
    {
        std::fill_n(outputs[0], numSamples, m_value);
    }

  private:
    const float m_value;
};

}
