#pragma once

#include <algorithm>
#include <cstddef>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>
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

    // A ControlEdge's runtime effect: read the source slot's last sample of the
    // block (its most up-to-date value) and push it into the target's parameter.
    struct ParameterApplication
    {
        size_t sourceSlot{0};
        Node* targetNode{nullptr};
        size_t paramIndex{0};
    };

    using ScheduleStep = std::variant<ScheduledNode, ParameterApplication>;

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

        for (auto& step : m_schedule)
        {
            std::visit(
                [this, numSamples](auto& entry) noexcept
                {
                    using T = std::decay_t<decltype(entry)>;
                    if constexpr (std::is_same_v<T, ScheduledNode>)
                    {
                        entry.node->process(std::span<const float*>(entry.inputPointers),
                                            std::span<float*>(entry.outputPointers), numSamples);
                    }
                    else
                    {
                        entry.targetNode->setParameter(entry.paramIndex, m_buffers[entry.sourceSlot][numSamples - 1]);
                    }
                },
                step);
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

    // Linear scan over m_nodeIds - intended for one-time setup (e.g. caching a raw
    // Node* right after compile()), never the per-block audio-thread path.
    [[nodiscard]] Node* findNode(const std::string_view id) noexcept
    {
        for (size_t i = 0; i < m_nodeIds.size(); ++i)
        {
            if (m_nodeIds[i] == id)
            {
                return m_nodes[i].get();
            }
        }
        return nullptr;
    }

    [[nodiscard]] size_t feedbackSlotCount() const noexcept
    {
        return m_feedbackSlots.size();
    }

    // The slot's full maxBlockSize-capacity buffer - may hold samples past
    // whatever numSamples the last process() call actually used.
    [[nodiscard]] std::span<const float> feedbackSlotBuffer(const size_t feedbackIndex) const noexcept
    {
        return m_buffers[m_feedbackSlots[feedbackIndex]];
    }

  private:
    friend class GraphCompiler;

    CompiledGraph(std::vector<std::unique_ptr<Node>> nodes, std::vector<std::string> nodeIds,
                  std::vector<ScheduleStep> schedule, std::vector<std::vector<float>> buffers,
                  std::vector<size_t> graphInputSlots, std::vector<OutputBinding> graphOutputBindings,
                  std::vector<size_t> feedbackSlots)
        : m_nodes(std::move(nodes))
        , m_nodeIds(std::move(nodeIds))
        , m_schedule(std::move(schedule))
        , m_buffers(std::move(buffers))
        , m_graphInputSlots(std::move(graphInputSlots))
        , m_graphOutputBindings(std::move(graphOutputBindings))
        , m_feedbackSlots(std::move(feedbackSlots))
    {
    }

    std::vector<std::unique_ptr<Node>> m_nodes;
    std::vector<std::string> m_nodeIds;
    std::vector<ScheduleStep> m_schedule;
    std::vector<std::vector<float>> m_buffers;
    std::vector<size_t> m_graphInputSlots;
    std::vector<OutputBinding> m_graphOutputBindings;
    std::vector<size_t> m_feedbackSlots;
};

}
