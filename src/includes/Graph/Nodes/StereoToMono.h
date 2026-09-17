#pragma once

#include "Graph/Node.h"

namespace AbacDsp::Graph::Nodes
{

/**
 * @ingroup graph
 * @brief Weighted L/R sum. Ports: inL, inR -> out. Parameters 0-1: gainL, gainR.
 */
class StereoToMono final : public Node
{
  public:
    void process(const std::span<const float*> inputs, const std::span<float*> outputs,
                 const size_t numSamples) noexcept override
    {
        for (size_t i = 0; i < numSamples; ++i)
        {
            outputs[0][i] = inputs[0][i] * m_gainL + inputs[1][i] * m_gainR;
        }
    }

    void setParameter(const size_t paramIndex, const float value) noexcept override
    {
        if (paramIndex == 0)
        {
            m_gainL = value;
        }
        else if (paramIndex == 1)
        {
            m_gainR = value;
        }
    }

  private:
    float m_gainL{0.5f};
    float m_gainR{0.5f};
};

}
