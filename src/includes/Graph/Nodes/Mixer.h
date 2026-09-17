#pragma once

#include "Graph/Node.h"

namespace AbacDsp::Graph::Nodes
{

/**
 * @ingroup graph
 * @brief Fixed 2-input stereo sum, no parameters.
 *
 * Ports: in1L, in1R, in2L, in2R -> outL, outR. Per-input trim is upstream
 * Gain nodes' job, matching every chorus.md Mixer usage.
 */
class Mixer final : public Node
{
  public:
    void process(const std::span<const float*> inputs, const std::span<float*> outputs,
                 const size_t numSamples) noexcept override
    {
        for (size_t i = 0; i < numSamples; ++i)
        {
            outputs[0][i] = inputs[0][i] + inputs[2][i];
            outputs[1][i] = inputs[1][i] + inputs[3][i];
        }
    }
};

}
