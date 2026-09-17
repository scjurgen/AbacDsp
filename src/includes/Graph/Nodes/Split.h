#pragma once

#include "Graph/Node.h"

namespace AbacDsp::Graph::Nodes
{

/**
 * @ingroup graph
 * @brief One stereo input duplicated to two stereo outputs, no parameters.
 *
 * Ports: inL, inR -> out1L, out1R, out2L, out2R.
 */
class Split final : public Node
{
  public:
    void process(const std::span<const float*> inputs, const std::span<float*> outputs,
                 const size_t numSamples) noexcept override
    {
        for (size_t i = 0; i < numSamples; ++i)
        {
            outputs[0][i] = inputs[0][i];
            outputs[1][i] = inputs[1][i];
            outputs[2][i] = inputs[0][i];
            outputs[3][i] = inputs[1][i];
        }
    }
};

}
