#pragma once

#include <algorithm>
#include <cmath>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "GraphDescription.h"
#include "NodeRegistry.h"
#include "NodeSchema.h"

namespace AbacDsp::Graph
{

enum class DiagnosticSeverity
{
    Warning,
    Error
};

struct Diagnostic
{
    DiagnosticSeverity severity{DiagnosticSeverity::Error};
    std::string message;
    std::string nodeId;
    std::string portName;
    std::string field;
    int line{0};
};

struct FeedbackComponent
{
    std::vector<std::string> nodeIds;
    std::vector<std::string> breakerIds;
};

/**
 * @ingroup graph
 * @brief Validates a GraphDescription against a NodeRegistry.
 *
 * The single source of truth for whether a graph is well-formed: node/port
 * existence and direction, port category compatibility, fan-in rules, and
 * feedback cycles (every strongly connected component must contain a node
 * whose schema marks breaksCycle). GraphCompiler runs this first and refuses
 * to compile on any Error diagnostic.
 */
class GraphValidator
{
  public:
    [[nodiscard]] static std::vector<Diagnostic> validate(const GraphDescription& description,
                                                          const NodeRegistry& registry)
    {
        std::vector<Diagnostic> diagnostics;
        const NodeMap nodes = resolveNodes(description, registry, diagnostics);
        validateEdges(description, nodes, diagnostics);
        validateControls(description, nodes, diagnostics);
        validateConnectivity(description, nodes, diagnostics);
        validateCycles(description, nodes, diagnostics);
        return diagnostics;
    }

    /// Every feedback cycle (a strongly connected component with an edge inside it), sorted by node id.
    [[nodiscard]] static std::vector<FeedbackComponent> findFeedbackComponents(const GraphDescription& description,
                                                                               const NodeRegistry& registry)
    {
        std::vector<Diagnostic> ignored;
        const NodeMap nodes = resolveNodes(description, registry, ignored);
        std::vector<FeedbackComponent> result;
        for (auto& component : cycleComponents(description, nodes))
        {
            std::ranges::sort(component);
            FeedbackComponent entry;
            for (const auto& id : component)
            {
                if (nodes.at(id).schema->breaksCycle)
                {
                    entry.breakerIds.push_back(id);
                }
            }
            entry.nodeIds = std::move(component);
            result.push_back(std::move(entry));
        }
        return result;
    }

  private:
    struct ResolvedNode
    {
        const NodeInstance* instance{nullptr};
        const NodeSchema* schema{nullptr};
    };

    using NodeMap = std::map<std::string, ResolvedNode>;
    using PortKey = std::pair<std::string, std::string>;

    struct EdgeSide
    {
        bool valid{false};
        PortCategory category{PortCategory::AudioMono};
        bool multiConnectable{false};
    };

    [[nodiscard]] static NodeMap resolveNodes(const GraphDescription& description, const NodeRegistry& registry,
                                              std::vector<Diagnostic>& diagnostics)
    {
        NodeMap nodes;
        for (const auto& node : description.nodes)
        {
            if (nodes.contains(node.id))
            {
                diagnostics.push_back({DiagnosticSeverity::Error, "duplicate node id", node.id, ""});
                continue;
            }
            const NodeSchema* schema = registry.findSchema(node.type);
            if (schema == nullptr)
            {
                diagnostics.push_back({DiagnosticSeverity::Error, "unknown node type: " + node.type, node.id, ""});
                continue;
            }
            nodes.emplace(node.id, ResolvedNode{&node, schema});
        }
        return nodes;
    }

    [[nodiscard]] static EdgeSide resolveEdgeSide(const std::string& nodeId, const std::string& portName,
                                                  const std::vector<std::string>& boundaryPorts, const NodeMap& nodes,
                                                  const PortDirection requiredDirection, const char* sideLabel,
                                                  std::vector<Diagnostic>& diagnostics)
    {
        if (nodeId.empty())
        {
            const bool found = std::find(boundaryPorts.begin(), boundaryPorts.end(), portName) != boundaryPorts.end();
            if (!found)
            {
                diagnostics.push_back(
                    {DiagnosticSeverity::Error, std::string("unknown graph ") + sideLabel + " port", "", portName});
                return {};
            }
            return {true, PortCategory::AudioMono, false};
        }

        const auto it = nodes.find(nodeId);
        if (it == nodes.end())
        {
            diagnostics.push_back(
                {DiagnosticSeverity::Error, std::string("unknown ") + sideLabel + " node", nodeId, portName});
            return {};
        }

        const PortDescriptor* port = it->second.schema->findPort(portName);
        if (port == nullptr)
        {
            diagnostics.push_back({DiagnosticSeverity::Error, "unknown port", nodeId, portName});
            return {};
        }

        if (port->direction != requiredDirection)
        {
            const char* expected = requiredDirection == PortDirection::Output ? "an output port" : "an input port";
            diagnostics.push_back({DiagnosticSeverity::Error, std::string("edge ") + sideLabel + " must be " + expected,
                                   nodeId, portName});
            return {};
        }

        return {true, port->category, port->multiConnectable};
    }

    static void validateEdges(const GraphDescription& description, const NodeMap& nodes,
                              std::vector<Diagnostic>& diagnostics)
    {
        std::map<PortKey, int> fanIn;
        std::map<PortKey, bool> fanInAllowed;

        for (const auto& edge : description.edges)
        {
            const auto source = resolveEdgeSide(edge.fromNode, edge.fromPort, description.io.inputs, nodes,
                                                PortDirection::Output, "source", diagnostics);
            const auto destination = resolveEdgeSide(edge.toNode, edge.toPort, description.io.outputs, nodes,
                                                     PortDirection::Input, "destination", diagnostics);

            if (source.valid && destination.valid && source.category != destination.category)
            {
                diagnostics.push_back(
                    {DiagnosticSeverity::Error, "incompatible port categories", edge.toNode, edge.toPort});
            }

            const PortKey key{edge.toNode, edge.toPort};
            fanIn[key] += 1;
            fanInAllowed[key] = destination.multiConnectable;
        }

        for (const auto& [key, count] : fanIn)
        {
            if (count > 1 && !fanInAllowed[key])
            {
                diagnostics.push_back({DiagnosticSeverity::Error, "implicit fan-in on a non-multi-connectable input",
                                       key.first, key.second});
            }
        }
    }

    // Source category is not enforced here - every control node's ports use
    // ControlAudioRate uniformly for now (see Phase 6 plan), so nothing yet
    // distinguishes a scalar from an audio-rate control source.
    static void validateControls(const GraphDescription& description, const NodeMap& nodes,
                                 std::vector<Diagnostic>& diagnostics)
    {
        for (const auto& control : description.controls)
        {
            if (control.fromNode.empty())
            {
                diagnostics.push_back({DiagnosticSeverity::Error,
                                       "control edge source must be a node, not a graph boundary port", control.toNode,
                                       control.toParam});
                continue;
            }
            const auto sourceIt = nodes.find(control.fromNode);
            if (sourceIt == nodes.end())
            {
                diagnostics.push_back(
                    {DiagnosticSeverity::Error, "unknown control source node", control.fromNode, control.fromPort});
                continue;
            }
            const PortDescriptor* sourcePort = sourceIt->second.schema->findPort(control.fromPort);
            if (sourcePort == nullptr || sourcePort->direction != PortDirection::Output)
            {
                diagnostics.push_back({DiagnosticSeverity::Error, "control edge source must be an output port",
                                       control.fromNode, control.fromPort});
                continue;
            }

            const auto targetIt = nodes.find(control.toNode);
            if (targetIt == nodes.end())
            {
                diagnostics.push_back(
                    {DiagnosticSeverity::Error, "unknown control target node", control.toNode, control.toParam});
                continue;
            }
            const int paramIndex = targetIt->second.schema->findParameterIndex(control.toParam);
            if (paramIndex < 0)
            {
                diagnostics.push_back(
                    {DiagnosticSeverity::Error, "unknown control target parameter", control.toNode, control.toParam});
                continue;
            }
            if (!targetIt->second.schema->parameters[static_cast<size_t>(paramIndex)].automatable)
            {
                diagnostics.push_back({DiagnosticSeverity::Error, "control target parameter is not automatable",
                                       control.toNode, control.toParam});
            }
        }
    }

    static void validateConnectivity(const GraphDescription& description, const NodeMap& nodes,
                                     std::vector<Diagnostic>& diagnostics)
    {
        std::set<PortKey> connectedInputs;
        std::set<std::string> nodesWithOutgoingEdge;
        std::set<std::string> nodesWithIncomingEdge;

        for (const auto& edge : description.edges)
        {
            if (!edge.toNode.empty())
            {
                connectedInputs.emplace(edge.toNode, edge.toPort);
                nodesWithIncomingEdge.insert(edge.toNode);
            }
            if (!edge.fromNode.empty())
            {
                nodesWithOutgoingEdge.insert(edge.fromNode);
            }
        }
        for (const auto& control : description.controls)
        {
            nodesWithOutgoingEdge.insert(control.fromNode);
            nodesWithIncomingEdge.insert(control.toNode);
        }

        for (const auto& [id, resolved] : nodes)
        {
            for (const auto& port : resolved.schema->ports)
            {
                if (port.direction == PortDirection::Input && !connectedInputs.contains({id, port.name}))
                {
                    diagnostics.push_back({DiagnosticSeverity::Warning, "unconnected input", id, port.name});
                }
            }

            if (!nodesWithIncomingEdge.contains(id) && !nodesWithOutgoingEdge.contains(id))
            {
                diagnostics.push_back({DiagnosticSeverity::Warning, "unused node", id, ""});
            }
        }
    }

    struct TarjanState
    {
        std::map<std::string, int> index;
        std::map<std::string, int> lowLink;
        std::map<std::string, bool> onStack;
        std::vector<std::string> stack;
        int counter{0};
        std::vector<std::vector<std::string>> components;
    };

    static void tarjanVisit(const std::string& id, const std::map<std::string, std::vector<std::string>>& adjacency,
                            TarjanState& state)
    {
        state.index[id] = state.counter;
        state.lowLink[id] = state.counter;
        ++state.counter;
        state.stack.push_back(id);
        state.onStack[id] = true;

        const auto it = adjacency.find(id);
        if (it != adjacency.end())
        {
            for (const auto& neighbour : it->second)
            {
                if (!state.index.contains(neighbour))
                {
                    tarjanVisit(neighbour, adjacency, state);
                    state.lowLink[id] = std::min(state.lowLink[id], state.lowLink[neighbour]);
                }
                else if (state.onStack[neighbour])
                {
                    state.lowLink[id] = std::min(state.lowLink[id], state.index[neighbour]);
                }
            }
        }

        if (state.lowLink[id] == state.index[id])
        {
            std::vector<std::string> component;
            while (true)
            {
                const std::string top = state.stack.back();
                state.stack.pop_back();
                state.onStack[top] = false;
                component.push_back(top);
                if (top == id)
                {
                    break;
                }
            }
            state.components.push_back(std::move(component));
        }
    }

    [[nodiscard]] static bool hasSelfLoop(const std::string& id,
                                          const std::map<std::string, std::vector<std::string>>& adjacency)
    {
        const auto it = adjacency.find(id);
        if (it == adjacency.end())
        {
            return false;
        }
        return std::find(it->second.begin(), it->second.end(), id) != it->second.end();
    }

    [[nodiscard]] static float resolveParamValue(const ResolvedNode& node, const std::string& paramId,
                                                 const float fallback)
    {
        const auto it = node.instance->params.find(paramId);
        if (it != node.instance->params.end())
        {
            return it->second;
        }
        const int index = node.schema->findParameterIndex(paramId);
        return index >= 0 ? node.schema->parameters[static_cast<size_t>(index)].defaultValue : fallback;
    }

    // Not an exhaustive resonance taxonomy - Biquad in "peak" mode and BandPass
    // are the two node types this phase actually ships with resonance.
    [[nodiscard]] static bool isResonantFilterNode(const ResolvedNode& node)
    {
        constexpr float kResonantQThreshold = 2.0f;
        const bool isPeakBiquad = node.instance->type == "Biquad" && node.instance->config.contains("mode") &&
                                  node.instance->config.at("mode") == "peak";
        const bool isBandPass = node.instance->type == "BandPass";
        if (!isPeakBiquad && !isBandPass)
        {
            return false;
        }
        return resolveParamValue(node, "Q", 0.0f) > kResonantQThreshold;
    }

    static void validateFeedbackGainLimits(const GraphDescription& description,
                                           const std::vector<std::string>& component,
                                           std::vector<Diagnostic>& diagnostics)
    {
        constexpr float kMaxFeedbackGain = 1.0f;
        for (const auto& edge : description.edges)
        {
            const bool bothInComponent =
                std::find(component.begin(), component.end(), edge.fromNode) != component.end() &&
                std::find(component.begin(), component.end(), edge.toNode) != component.end();
            if (bothInComponent && std::abs(edge.gain) > kMaxFeedbackGain)
            {
                diagnostics.push_back({DiagnosticSeverity::Error, "feedback edge gain exceeds the safe limit of 1.0",
                                       edge.toNode, edge.toPort});
            }
        }
    }

    [[nodiscard]] static std::vector<std::vector<std::string>> cycleComponents(const GraphDescription& description,
                                                                               const NodeMap& nodes)
    {
        std::map<std::string, std::vector<std::string>> adjacency;
        for (const auto& edge : description.edges)
        {
            if (!edge.fromNode.empty() && !edge.toNode.empty() && nodes.contains(edge.fromNode) &&
                nodes.contains(edge.toNode))
            {
                adjacency[edge.fromNode].push_back(edge.toNode);
            }
        }

        TarjanState state;
        for (const auto& [id, resolved] : nodes)
        {
            if (!state.index.contains(id))
            {
                tarjanVisit(id, adjacency, state);
            }
        }

        std::vector<std::vector<std::string>> cycles;
        for (auto& component : state.components)
        {
            if (component.size() > 1 || hasSelfLoop(component.front(), adjacency))
            {
                cycles.push_back(std::move(component));
            }
        }
        return cycles;
    }

    static void validateCycles(const GraphDescription& description, const NodeMap& nodes,
                               std::vector<Diagnostic>& diagnostics)
    {
        for (const auto& component : cycleComponents(description, nodes))
        {
            const bool hasBreaker = std::any_of(component.begin(), component.end(), [&nodes](const std::string& id)
                                                { return nodes.at(id).schema->breaksCycle; });
            if (!hasBreaker)
            {
                std::string message = "feedback cycle has no breaksCycle node:";
                for (const auto& id : component)
                {
                    message += " " + id;
                }
                diagnostics.push_back({DiagnosticSeverity::Error, message, component.front(), ""});
            }

            validateFeedbackGainLimits(description, component, diagnostics);

            const bool hasDamping = std::any_of(component.begin(), component.end(), [&nodes](const std::string& id)
                                                { return nodes.at(id).schema->providesDamping; });
            if (!hasDamping)
            {
                std::string message = "feedback cycle has no damping filter:";
                for (const auto& id : component)
                {
                    message += " " + id;
                }
                diagnostics.push_back({DiagnosticSeverity::Warning, message, component.front(), ""});

                const bool hasResonant = std::any_of(component.begin(), component.end(), [&nodes](const std::string& id)
                                                     { return isResonantFilterNode(nodes.at(id)); });
                if (hasResonant)
                {
                    diagnostics.push_back({DiagnosticSeverity::Warning,
                                           "resonant filter inside an undamped feedback cycle", component.front(), ""});
                }
            }
        }
    }
};

}
