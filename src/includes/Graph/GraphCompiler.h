#pragma once

#include <algorithm>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "CompiledGraph.h"
#include "GraphDescription.h"
#include "GraphValidator.h"
#include "Node.h"
#include "NodeRegistry.h"
#include "NodeSchema.h"

namespace AbacDsp::Graph
{

struct CompileResult
{
    std::optional<CompiledGraph> graph;
    std::vector<Diagnostic> diagnostics;
};

/**
 * @ingroup graph
 * @brief Compiles a validated GraphDescription into a realtime-safe CompiledGraph.
 *
 * Runs GraphValidator first and refuses to compile on any Error diagnostic -
 * one source of truth for correctness. Otherwise: a topological schedule that
 * treats a breaksCycle node's feedback input as a state read rather than an
 * ordering dependency, then a liveness-based buffer-slot assignment that
 * reuses a slot once its last consumer in that schedule has run. A feedback
 * edge's source slot is instead permanently dedicated, since its value must
 * survive from one block into the next rather than within a single block.
 */
class GraphCompiler
{
  public:
    [[nodiscard]] static CompileResult compile(const GraphDescription& description, const NodeRegistry& registry,
                                               const size_t maxBlockSize, const float sampleRate)
    {
        std::vector<Diagnostic> diagnostics = GraphValidator::validate(description, registry);
        const bool hasError = std::any_of(diagnostics.begin(), diagnostics.end(),
                                          [](const Diagnostic& d) { return d.severity == DiagnosticSeverity::Error; });
        if (hasError)
        {
            return {std::nullopt, std::move(diagnostics)};
        }

        BuildState state = buildNodes(description, registry, sampleRate);
        const std::vector<std::string> order = computeSchedule(description, state);
        if (order.size() != state.nodesById.size())
        {
            diagnostics.push_back(
                {DiagnosticSeverity::Error, "internal: residual cycle after cycle-breaking exclusion", "", ""});
            return {std::nullopt, std::move(diagnostics)};
        }

        assignBufferSlots(description, state, order, maxBlockSize);
        buildScheduleEntries(description, state, order);
        std::vector<CompiledGraph::OutputBinding> graphOutputBindings = bindGraphOutputs(description, state);

        std::vector<std::unique_ptr<Node>> ownedNodes;
        ownedNodes.reserve(order.size());
        for (const auto& id : order)
        {
            ownedNodes.push_back(std::move(state.nodesById.at(id).node));
        }

        CompiledGraph graph{std::move(ownedNodes),
                            order,
                            std::move(state.schedule),
                            std::move(state.buffers),
                            std::move(state.graphInputSlots),
                            std::move(graphOutputBindings),
                            std::move(state.feedbackSlots)};
        return {std::move(graph), std::move(diagnostics)};
    }

  private:
    static constexpr size_t kSilenceSlot = 0;
    static constexpr size_t kScratchSlot = 1;

    using PortKey = std::pair<std::string, std::string>;

    struct NodeBuild
    {
        const NodeInstance* instance{nullptr};
        const NodeSchema* schema{nullptr};
        std::unique_ptr<Node> node;
    };

    struct PortUsage
    {
        bool used{false};
        bool isFeedbackSource{false};
        int lastUsePosition{-1};
    };

    struct BuildState
    {
        std::map<std::string, NodeBuild> nodesById;
        std::map<std::string, int> positionById;
        std::vector<bool> isFeedback;
        std::vector<std::vector<float>> buffers;
        std::map<PortKey, size_t> outputSlot;
        std::vector<size_t> graphInputSlots;
        std::vector<size_t> feedbackSlots;
        std::vector<CompiledGraph::ScheduleStep> schedule;
    };

    [[nodiscard]] static BuildState buildNodes(const GraphDescription& description, const NodeRegistry& registry,
                                               const float sampleRate)
    {
        BuildState state;
        for (const auto& instance : description.nodes)
        {
            const NodeSchema* schema = registry.findSchema(instance.type);
            if (schema == nullptr || state.nodesById.contains(instance.id))
            {
                continue;
            }
            std::unique_ptr<Node> node = registry.create(instance.type, instance, sampleRate);
            for (const auto& [paramId, value] : instance.params)
            {
                const int index = schema->findParameterIndex(paramId);
                if (index >= 0)
                {
                    node->setParameter(static_cast<size_t>(index), value);
                }
            }
            state.nodesById.emplace(instance.id, NodeBuild{&instance, schema, std::move(node)});
        }
        return state;
    }

    [[nodiscard]] static bool canReach(const std::string& from, const std::string& target,
                                       const std::map<std::string, std::vector<std::string>>& adjacency)
    {
        std::set<std::string> visited;
        std::vector<std::string> stack{from};
        while (!stack.empty())
        {
            const std::string current = stack.back();
            stack.pop_back();
            if (current == target)
            {
                return true;
            }
            if (!visited.insert(current).second)
            {
                continue;
            }
            const auto it = adjacency.find(current);
            if (it != adjacency.end())
            {
                for (const auto& next : it->second)
                {
                    stack.push_back(next);
                }
            }
        }
        return false;
    }

    [[nodiscard]] static std::vector<bool> classifyFeedbackEdges(const GraphDescription& description,
                                                                 const BuildState& state)
    {
        std::map<std::string, std::vector<std::string>> adjacency;
        for (const auto& edge : description.edges)
        {
            if (!edge.fromNode.empty() && !edge.toNode.empty())
            {
                adjacency[edge.fromNode].push_back(edge.toNode);
            }
        }

        std::vector<bool> isFeedback(description.edges.size(), false);
        for (size_t i = 0; i < description.edges.size(); ++i)
        {
            const auto& edge = description.edges[i];
            if (edge.fromNode.empty() || edge.toNode.empty())
            {
                continue;
            }
            const auto it = state.nodesById.find(edge.toNode);
            if (it == state.nodesById.end() || !it->second.schema->breaksCycle)
            {
                continue;
            }
            isFeedback[i] = canReach(edge.toNode, edge.fromNode, adjacency);
        }
        return isFeedback;
    }

    [[nodiscard]] static std::vector<std::string> computeSchedule(const GraphDescription& description,
                                                                  BuildState& state)
    {
        state.isFeedback = classifyFeedbackEdges(description, state);

        std::map<std::string, std::vector<std::string>> forwardAdjacency;
        std::map<std::string, int> inDegree;
        for (const auto& [id, build] : state.nodesById)
        {
            inDegree[id] = 0;
        }
        for (size_t i = 0; i < description.edges.size(); ++i)
        {
            const auto& edge = description.edges[i];
            if (edge.fromNode.empty() || edge.toNode.empty() || state.isFeedback[i])
            {
                continue;
            }
            if (!state.nodesById.contains(edge.fromNode) || !state.nodesById.contains(edge.toNode))
            {
                continue;
            }
            forwardAdjacency[edge.fromNode].push_back(edge.toNode);
            inDegree[edge.toNode] += 1;
        }
        // Control edges never break a cycle (no breaksCycle exemption here) - a
        // control-edge-closed cycle surfaces as the residual-cycle check below.
        for (const auto& control : description.controls)
        {
            if (!state.nodesById.contains(control.fromNode) || !state.nodesById.contains(control.toNode))
            {
                continue;
            }
            forwardAdjacency[control.fromNode].push_back(control.toNode);
            inDegree[control.toNode] += 1;
        }

        std::set<std::string> ready;
        for (const auto& [id, degree] : inDegree)
        {
            if (degree == 0)
            {
                ready.insert(id);
            }
        }

        std::vector<std::string> order;
        while (!ready.empty())
        {
            const std::string id = *ready.begin();
            ready.erase(ready.begin());
            order.push_back(id);
            state.positionById[id] = static_cast<int>(order.size()) - 1;

            const auto it = forwardAdjacency.find(id);
            if (it != forwardAdjacency.end())
            {
                for (const auto& next : it->second)
                {
                    if (--inDegree[next] == 0)
                    {
                        ready.insert(next);
                    }
                }
            }
        }
        return order;
    }

    [[nodiscard]] static std::map<PortKey, PortUsage> analyzeOutputPorts(const GraphDescription& description,
                                                                         const BuildState& state,
                                                                         const size_t scheduleLength)
    {
        std::map<PortKey, PortUsage> usage;
        for (size_t i = 0; i < description.edges.size(); ++i)
        {
            const auto& edge = description.edges[i];
            if (edge.fromNode.empty())
            {
                continue;
            }
            PortUsage& info = usage[{edge.fromNode, edge.fromPort}];
            info.used = true;
            if (state.isFeedback[i])
            {
                info.isFeedbackSource = true;
                continue;
            }
            const int consumerPosition =
                edge.toNode.empty() ? static_cast<int>(scheduleLength) : state.positionById.at(edge.toNode);
            info.lastUsePosition = std::max(info.lastUsePosition, consumerPosition);
        }
        for (const auto& control : description.controls)
        {
            if (control.fromNode.empty() || !state.positionById.contains(control.toNode))
            {
                continue;
            }
            PortUsage& info = usage[{control.fromNode, control.fromPort}];
            info.used = true;
            info.lastUsePosition = std::max(info.lastUsePosition, state.positionById.at(control.toNode));
        }
        return usage;
    }

    static void reserveFixedSlots(const GraphDescription& description, BuildState& state, const size_t maxBlockSize)
    {
        state.buffers.push_back(std::vector<float>(maxBlockSize, 0.0f)); // kSilenceSlot
        state.buffers.push_back(std::vector<float>(maxBlockSize, 0.0f)); // kScratchSlot

        state.graphInputSlots.reserve(description.io.inputs.size());
        for (size_t i = 0; i < description.io.inputs.size(); ++i)
        {
            state.graphInputSlots.push_back(state.buffers.size());
            state.buffers.push_back(std::vector<float>(maxBlockSize, 0.0f));
        }
    }

    static void releaseExpiredSlots(const int currentPosition, std::map<size_t, int>& slotLastUse,
                                    std::vector<size_t>& freeSlots)
    {
        for (auto it = slotLastUse.begin(); it != slotLastUse.end();)
        {
            if (it->second < currentPosition)
            {
                freeSlots.push_back(it->first);
                it = slotLastUse.erase(it);
            }
            else
            {
                ++it;
            }
        }
    }

    static void assignBufferSlots(const GraphDescription& description, BuildState& state,
                                  const std::vector<std::string>& order, const size_t maxBlockSize)
    {
        reserveFixedSlots(description, state, maxBlockSize);

        const auto usage = analyzeOutputPorts(description, state, order.size());
        for (const auto& [key, info] : usage)
        {
            if (info.isFeedbackSource)
            {
                state.outputSlot[key] = state.buffers.size();
                state.feedbackSlots.push_back(state.buffers.size());
                state.buffers.push_back(std::vector<float>(maxBlockSize, 0.0f));
            }
        }

        std::vector<size_t> freeSlots;
        std::map<size_t, int> slotLastUse;
        for (const auto& id : order)
        {
            releaseExpiredSlots(state.positionById.at(id), slotLastUse, freeSlots);

            for (const auto& port : state.nodesById.at(id).schema->ports)
            {
                if (port.direction != PortDirection::Output)
                {
                    continue;
                }
                const PortKey key{id, port.name};
                const auto usageIt = usage.find(key);
                if (state.outputSlot.contains(key) || usageIt == usage.end() || !usageIt->second.used)
                {
                    continue;
                }

                size_t slot;
                if (!freeSlots.empty())
                {
                    slot = freeSlots.back();
                    freeSlots.pop_back();
                }
                else
                {
                    slot = state.buffers.size();
                    state.buffers.push_back(std::vector<float>(maxBlockSize, 0.0f));
                }
                state.outputSlot[key] = slot;
                slotLastUse[slot] = usageIt->second.lastUsePosition;
            }
        }
    }

    [[nodiscard]] static const float* resolveInputSlotPointer(const GraphDescription& description,
                                                              const BuildState& state, const std::string& nodeId,
                                                              const std::string& portName)
    {
        for (const auto& edge : description.edges)
        {
            if (edge.toNode != nodeId || edge.toPort != portName)
            {
                continue;
            }
            if (edge.fromNode.empty())
            {
                const auto begin = description.io.inputs.begin();
                const auto found = std::find(begin, description.io.inputs.end(), edge.fromPort);
                return state.buffers[state.graphInputSlots[static_cast<size_t>(found - begin)]].data();
            }
            const auto it = state.outputSlot.find({edge.fromNode, edge.fromPort});
            if (it != state.outputSlot.end())
            {
                return state.buffers[it->second].data();
            }
        }
        return state.buffers[kSilenceSlot].data();
    }

    [[nodiscard]] static std::map<std::string, std::vector<CompiledGraph::ParameterApplication>>
    resolveControlApplications(const GraphDescription& description, const BuildState& state)
    {
        std::map<std::string, std::vector<CompiledGraph::ParameterApplication>> applicationsByTarget;
        for (const auto& control : description.controls)
        {
            if (!state.nodesById.contains(control.fromNode) || !state.nodesById.contains(control.toNode))
            {
                continue;
            }
            const auto sourceIt = state.outputSlot.find({control.fromNode, control.fromPort});
            if (sourceIt == state.outputSlot.end())
            {
                continue;
            }
            const auto& target = state.nodesById.at(control.toNode);
            const int paramIndex = target.schema->findParameterIndex(control.toParam);
            if (paramIndex < 0)
            {
                continue;
            }
            applicationsByTarget[control.toNode].push_back(
                {sourceIt->second, target.node.get(), static_cast<size_t>(paramIndex)});
        }
        return applicationsByTarget;
    }

    static void buildScheduleEntries(const GraphDescription& description, BuildState& state,
                                     const std::vector<std::string>& order)
    {
        const auto applicationsByTarget = resolveControlApplications(description, state);

        state.schedule.reserve(order.size());
        for (const auto& id : order)
        {
            const auto appIt = applicationsByTarget.find(id);
            if (appIt != applicationsByTarget.end())
            {
                for (const auto& application : appIt->second)
                {
                    state.schedule.push_back(application);
                }
            }

            const auto& build = state.nodesById.at(id);
            CompiledGraph::ScheduledNode scheduled;
            scheduled.node = build.node.get();

            for (const auto& port : build.schema->ports)
            {
                if (port.direction == PortDirection::Input)
                {
                    scheduled.inputPointers.push_back(resolveInputSlotPointer(description, state, id, port.name));
                }
            }
            for (const auto& port : build.schema->ports)
            {
                if (port.direction == PortDirection::Output)
                {
                    const auto it = state.outputSlot.find({id, port.name});
                    scheduled.outputPointers.push_back(it == state.outputSlot.end() ? state.buffers[kScratchSlot].data()
                                                                                    : state.buffers[it->second].data());
                }
            }
            state.schedule.push_back(std::move(scheduled));
        }
    }

    [[nodiscard]] static std::vector<CompiledGraph::OutputBinding> bindGraphOutputs(const GraphDescription& description,
                                                                                    const BuildState& state)
    {
        std::vector<CompiledGraph::OutputBinding> bindings;
        bindings.reserve(description.io.outputs.size());
        for (const auto& outputName : description.io.outputs)
        {
            CompiledGraph::OutputBinding binding{};
            binding.slot = kSilenceSlot;
            for (const auto& edge : description.edges)
            {
                if (!edge.toNode.empty() || edge.toPort != outputName)
                {
                    continue;
                }
                binding.gain = edge.gain;
                if (edge.fromNode.empty())
                {
                    const auto begin = description.io.inputs.begin();
                    const auto found = std::find(begin, description.io.inputs.end(), edge.fromPort);
                    binding.slot = state.graphInputSlots[static_cast<size_t>(found - begin)];
                }
                else
                {
                    binding.slot = state.outputSlot.at({edge.fromNode, edge.fromPort});
                }
                break;
            }
            bindings.push_back(binding);
        }
        return bindings;
    }
};

}
