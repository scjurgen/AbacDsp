#pragma once

#include "Graph/Node.h"
#include "Numbers/Convert.h"

namespace AbacDsp::Graph::Nodes
{

/**
 * @ingroup graph
 * @brief Stereo, linked gain. Ports: inL, inR -> outL, outR. Parameter 0: gainDb.
 *
 * Converts dB to linear once in setParameter(), not per sample.
 */
class Gain final : public Node
{
  public:
    void process(const std::span<const float*> inputs, const std::span<float*> outputs,
                 const size_t numSamples) noexcept override
    {
        for (size_t i = 0; i < numSamples; ++i)
        {
            outputs[0][i] = inputs[0][i] * m_linearGain;
            outputs[1][i] = inputs[1][i] * m_linearGain;
        }
    }

    void setParameter(const size_t paramIndex, const float value) noexcept override
    {
        if (paramIndex == 0)
        {
            m_linearGain = Convert::dbToGain(value);
        }
    }

  private:
    float m_linearGain{1.0f};
};

}
