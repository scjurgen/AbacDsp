#pragma once

#include "Graph/Node.h"

namespace AbacDsp::Graph::Nodes
{

/**
 * @ingroup graph
 * @brief Duplicate mono to both channels, no parameters. Ports: in -> outL, outR.
 */
class MonoToStereo final : public Node
{
  public:
    void process(const std::span<const float*> inputs, const std::span<float*> outputs,
                 const size_t numSamples) noexcept override
    {
        for (size_t i = 0; i < numSamples; ++i)
        {
            outputs[0][i] = inputs[0][i];
            outputs[1][i] = inputs[0][i];
        }
    }
};

}
