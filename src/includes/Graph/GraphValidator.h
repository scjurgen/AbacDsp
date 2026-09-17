#pragma once

#include <algorithm>
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
        validateConnectivity(description, nodes, diagnostics);
        validateCycles(description, nodes, diagnostics);
        return diagnostics;
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

    static void validateCycles(const GraphDescription& description, const NodeMap& nodes,
                               std::vector<Diagnostic>& diagnostics)
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

        for (const auto& component : state.components)
        {
            const bool isCycle = component.size() > 1 || hasSelfLoop(component.front(), adjacency);
            if (!isCycle)
            {
                continue;
            }
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
        }
    }
};

}
