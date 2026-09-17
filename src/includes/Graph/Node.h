#pragma once

#include <cstddef>
#include <span>

namespace AbacDsp::Graph
{

/**
 * @ingroup graph
 * @brief Interface for one compiled graph node's per-block audio processing.
 *
 * Dispatch happens once per block, the same tradeoff BlockProcessorBase makes
 * (@ref blockprocessors), so a runtime-configurable node set costs one vtable
 * call per node per block rather than per sample. The node-type set itself is
 * open-ended and grows every later phase, which is why this is a small
 * interface rather than a closed std::variant.
 *
 * process() is noexcept and must not allocate, lock, or resolve strings.
 * Buffer pointers and numSamples come from CompiledGraph's preallocated
 * storage; a node never owns its own audio buffers.
 */
class Node
{
  public:
    virtual ~Node() = default;

    virtual void process(std::span<const float*> inputs, std::span<float*> outputs, size_t numSamples) noexcept = 0;

    virtual void setParameter(size_t paramIndex, float value) noexcept
    {
        (void) paramIndex;
        (void) value;
    }

    virtual void reset() noexcept {}
};

}
