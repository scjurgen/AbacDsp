#pragma once

#include <algorithm>
#include <cstddef>
#include <memory>
#include <span>
#include <utility>
#include <vector>

#include "Node.h"

namespace AbacDsp::Graph
{

class GraphCompiler;

/**
 * @ingroup graph
 * @brief Realtime-safe executable form of a compiled graph.
 *
 * Owns every node and buffer the graph needs; process() only reads
 * preallocated storage and calls each node's process() once, in the fixed
 * schedule order GraphCompiler computed - no allocation, locks, hash maps, or
 * string operations. Move-only: only GraphCompiler constructs one.
 */
class CompiledGraph
{
  public:
    struct ScheduledNode
    {
        Node* node{nullptr};
        std::vector<const float*> inputPointers;
        std::vector<float*> outputPointers;
    };

    struct OutputBinding
    {
        size_t slot{0};
        float gain{1.0f};
    };

    CompiledGraph(const CompiledGraph&) = delete;
    CompiledGraph& operator=(const CompiledGraph&) = delete;
    CompiledGraph(CompiledGraph&&) noexcept = default;
    CompiledGraph& operator=(CompiledGraph&&) noexcept = default;

    void process(std::span<const float*> graphInputs, std::span<float*> graphOutputs, const size_t numSamples) noexcept
    {
        for (size_t i = 0; i < m_graphInputSlots.size(); ++i)
        {
            std::copy_n(graphInputs[i], numSamples, m_buffers[m_graphInputSlots[i]].data());
        }

        for (auto& scheduled : m_schedule)
        {
            scheduled.node->process(std::span<const float*>(scheduled.inputPointers),
                                    std::span<float*>(scheduled.outputPointers), numSamples);
        }

        for (size_t i = 0; i < m_graphOutputBindings.size(); ++i)
        {
            const auto& binding = m_graphOutputBindings[i];
            const float* source = m_buffers[binding.slot].data();
            float* destination = graphOutputs[i];
            for (size_t n = 0; n < numSamples; ++n)
            {
                destination[n] = source[n] * binding.gain;
            }
        }
    }

    [[nodiscard]] size_t bufferSlotCount() const noexcept
    {
        return m_buffers.size();
    }

    [[nodiscard]] size_t nodeCount() const noexcept
    {
        return m_nodes.size();
    }

  private:
    friend class GraphCompiler;

    CompiledGraph(std::vector<std::unique_ptr<Node>> nodes, std::vector<ScheduledNode> schedule,
                  std::vector<std::vector<float>> buffers, std::vector<size_t> graphInputSlots,
                  std::vector<OutputBinding> graphOutputBindings)
        : m_nodes(std::move(nodes))
        , m_schedule(std::move(schedule))
        , m_buffers(std::move(buffers))
        , m_graphInputSlots(std::move(graphInputSlots))
        , m_graphOutputBindings(std::move(graphOutputBindings))
    {
    }

    std::vector<std::unique_ptr<Node>> m_nodes;
    std::vector<ScheduledNode> m_schedule;
    std::vector<std::vector<float>> m_buffers;
    std::vector<size_t> m_graphInputSlots;
    std::vector<OutputBinding> m_graphOutputBindings;
};

}
