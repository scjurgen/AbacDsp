#pragma once

#include "Graph/Node.h"

namespace AbacDsp::Graph::Nodes
{

/**
 * @ingroup graph
 * @brief Mid/side decode, no parameters. Ports: inM, inS -> outL, outR.
 *
 * outL = M+S, outR = M-S - exact inverse of MsEncode.
 */
class MsDecode final : public Node
{
  public:
    void process(const std::span<const float*> inputs, const std::span<float*> outputs,
                 const size_t numSamples) noexcept override
    {
        for (size_t i = 0; i < numSamples; ++i)
        {
            const float mid = inputs[0][i];
            const float side = inputs[1][i];
            outputs[0][i] = mid + side;
            outputs[1][i] = mid - side;
        }
    }
};

}
