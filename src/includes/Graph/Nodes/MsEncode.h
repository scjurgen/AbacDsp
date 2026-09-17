#pragma once

#include "Graph/Node.h"

namespace AbacDsp::Graph::Nodes
{

/**
 * @ingroup graph
 * @brief Mid/side encode, no parameters. Ports: inL, inR -> outM, outS.
 *
 * outM = 0.5(L+R), outS = 0.5(L-R) - exact inverse of MsDecode.
 */
class MsEncode final : public Node
{
  public:
    void process(const std::span<const float*> inputs, const std::span<float*> outputs,
                 const size_t numSamples) noexcept override
    {
        for (size_t i = 0; i < numSamples; ++i)
        {
            const float inL = inputs[0][i];
            const float inR = inputs[1][i];
            outputs[0][i] = 0.5f * (inL + inR);
            outputs[1][i] = 0.5f * (inL - inR);
        }
    }
};

}
