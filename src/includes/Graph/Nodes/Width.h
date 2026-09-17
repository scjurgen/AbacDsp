#pragma once

#include "Graph/Node.h"

namespace AbacDsp::Graph::Nodes
{

/**
 * @ingroup graph
 * @brief M/S-side gain. Ports: inL, inR -> outL, outR. Parameter 0: width.
 *
 * width=0 collapses to mono, 1 is unchanged, above 1 widens the side signal.
 */
class Width final : public Node
{
  public:
    void process(const std::span<const float*> inputs, const std::span<float*> outputs,
                 const size_t numSamples) noexcept override
    {
        for (size_t i = 0; i < numSamples; ++i)
        {
            const float mid = 0.5f * (inputs[0][i] + inputs[1][i]);
            const float side = 0.5f * (inputs[0][i] - inputs[1][i]) * m_width;
            outputs[0][i] = mid + side;
            outputs[1][i] = mid - side;
        }
    }

    void setParameter(const size_t paramIndex, const float value) noexcept override
    {
        if (paramIndex == 0)
        {
            m_width = value;
        }
    }

  private:
    float m_width{1.0f};
};

}
