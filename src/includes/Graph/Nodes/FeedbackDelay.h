#pragma once

#include <algorithm>

#include "Graph/Node.h"

namespace AbacDsp::Graph::Nodes
{

/**
 * @ingroup graph
 * @brief Explicit cycle-breaker. Ports: in -> out. No parameters.
 *
 * Structurally a pass-through - the one-block delay a feedback edge needs is
 * already a property of GraphCompiler's own schedule/buffer-liveness
 * mechanism (a feedback-classified edge's source gets a permanently-dedicated
 * buffer slot and is excluded from the topological ordering), not something
 * this node implements itself. Registered with breaksCycle: true.
 */
class FeedbackDelay final : public Node
{
  public:
    void process(const std::span<const float*> inputs, const std::span<float*> outputs,
                 const size_t numSamples) noexcept override
    {
        std::copy_n(inputs[0], numSamples, outputs[0]);
    }
};

}
