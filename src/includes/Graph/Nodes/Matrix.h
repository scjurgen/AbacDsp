#pragma once

#include "Graph/Node.h"

namespace AbacDsp::Graph::Nodes
{

/**
 * @ingroup graph
 * @brief 2x2 stereo routing/crossfeed matrix.
 *
 * Ports: inL, inR -> outL, outR. Parameters 0-3: gainLL, gainLR, gainRL,
 * gainRR, defaulting to the identity (no crossfeed).
 */
class Matrix final : public Node
{
  public:
    void process(const std::span<const float*> inputs, const std::span<float*> outputs,
                 const size_t numSamples) noexcept override
    {
        for (size_t i = 0; i < numSamples; ++i)
        {
            const float inL = inputs[0][i];
            const float inR = inputs[1][i];
            outputs[0][i] = inL * m_gainLL + inR * m_gainLR;
            outputs[1][i] = inL * m_gainRL + inR * m_gainRR;
        }
    }

    void setParameter(const size_t paramIndex, const float value) noexcept override
    {
        switch (paramIndex)
        {
            case 0:
                m_gainLL = value;
                break;
            case 1:
                m_gainLR = value;
                break;
            case 2:
                m_gainRL = value;
                break;
            case 3:
                m_gainRR = value;
                break;
            default:
                break;
        }
    }

  private:
    float m_gainLL{1.0f};
    float m_gainLR{0.0f};
    float m_gainRL{0.0f};
    float m_gainRR{1.0f};
};

}
