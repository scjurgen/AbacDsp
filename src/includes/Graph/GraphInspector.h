#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <format>
#include <map>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "Graph/CompiledGraph.h"
#include "Graph/GraphDescription.h"
#include "Graph/GraphValidator.h"
#include "Graph/NodeRegistry.h"

namespace AbacDsp::Graph
{

/**
 * @ingroup graph
 * @brief Text views of a graph for debugging: DOT and JSON export, cycles, macros, buffer layout.
 *
 * Non-realtime and read-only. Output order follows the description's own order and sorted
 * ids, so the same graph always yields the same text. A feedback edge is one that closes a
 * cycle into a breaksCycle node, the same rule GraphCompiler schedules by. JSON is written
 * by hand to keep Graph/ free of dependencies.
 */
class GraphInspector
{
  public:
    [[nodiscard]] static std::string toDot(const GraphDescription& description, const NodeRegistry& registry)
    {
        const auto feedback = feedbackEdgeFlags(description, registry);
        std::string dot = "digraph " + dotQuote(description.name) + " {\n  rankdir=LR;\n";
        for (const auto& input : description.io.inputs)
        {
            dot += "  " + dotQuote("in:" + input) + " [shape=plaintext, label=" + dotQuote(input) + "];\n";
        }
        for (const auto& output : description.io.outputs)
        {
            dot += "  " + dotQuote("out:" + output) + " [shape=plaintext, label=" + dotQuote(output) + "];\n";
        }
        for (const auto& node : description.nodes)
        {
            const auto* schema = registry.findSchema(node.type);
            const std::string shape = schema != nullptr && schema->breaksCycle ? "box, peripheries=2" : "box";
            dot += "  " + dotQuote(node.id) + " [shape=" + shape + ", label=" + dotQuote(node.id + "\n" + node.type) +
                   "];\n";
        }
        dot += dotEdges(description, feedback);
        dot += dotControlsAndMacros(description);
        return dot + "}\n";
    }

    [[nodiscard]] static std::string toJson(const GraphDescription& description, const NodeRegistry& registry,
                                            const CompiledGraph* compiled = nullptr)
    {
        std::string json = "{\n";
        json += "  \"name\": " + jsonString(description.name) + ",\n";
        json += "  \"version\": " + std::to_string(description.version) + ",\n";
        json += "  \"io\": {\"inputs\": " + jsonStrings(description.io.inputs) +
                ", \"outputs\": " + jsonStrings(description.io.outputs) + "},\n";
        json += "  \"nodes\": " + jsonNodes(description, registry) + ",\n";
        json += "  \"edges\": " + jsonEdges(description, feedbackEdgeFlags(description, registry)) + ",\n";
        json += "  \"controls\": " + jsonControls(description) + ",\n";
        json += "  \"macros\": " + jsonMacros(description) + ",\n";
        json += "  \"cycles\": " + jsonCycles(GraphValidator::findFeedbackComponents(description, registry));
        if (compiled != nullptr)
        {
            json += ",\n  \"schedule\": " + jsonStrings(compiled->nodeIds());
            json += ",\n  \"layout\": " + jsonLayout(compiled->layout());
        }
        return json + "\n}\n";
    }

    [[nodiscard]] static std::string describeCycles(const GraphDescription& description, const NodeRegistry& registry)
    {
        const auto components = GraphValidator::findFeedbackComponents(description, registry);
        if (components.empty())
        {
            return "no feedback cycles\n";
        }
        std::string text;
        for (size_t i = 0; i < components.size(); ++i)
        {
            const auto& component = components[i];
            text += std::format("cycle {}: {} (", i + 1, join(component.nodeIds));
            text += component.breakerIds.empty() ? "NO breaker" : "breaker: " + join(component.breakerIds);
            text += ")\n";
        }
        return text;
    }

    [[nodiscard]] static std::string describeMacros(const GraphDescription& description)
    {
        if (description.macros.empty())
        {
            return "no macros\n";
        }
        std::string text;
        for (const auto& macro : description.macros)
        {
            text += std::format("macro {} (\"{}\", default {})\n", macro.id, macro.label, macro.defaultValue);
            for (const auto& target : macro.targets)
            {
                text += "  -> " + target.toNode + "." + target.toParam;
                text += target.mapName.empty() ? "" : " via " + target.mapName;
                text += "\n";
            }
        }
        return text;
    }

    [[nodiscard]] static std::string describeLayout(const CompiledGraph& graph)
    {
        const auto& layout = graph.layout();
        std::string text = std::format("{} buffer slots ({} reserved for silence, scratch and graph inputs)\n",
                                       layout.slotCount, layout.reservedSlotCount);
        text += "schedule: " + join(graph.nodeIds()) + "\n";

        std::map<size_t, std::vector<const CompiledGraph::PortSlot*>> bySlot;
        for (const auto& port : layout.ports)
        {
            bySlot[port.slot].push_back(&port);
        }
        for (auto& [slot, ports] : bySlot)
        {
            std::ranges::sort(ports, {}, &CompiledGraph::PortSlot::firstPosition);
            text += std::format("slot {}:", slot);
            for (const auto* port : ports)
            {
                text += " " + port->nodeId + "." + port->port;
                text += port->isFeedback ? " (feedback)"
                                         : std::format(" (steps {}-{})", port->firstPosition, port->lastPosition);
            }
            text += "\n";
        }
        return text;
    }

    [[nodiscard]] static std::string formatDiagnostic(const Diagnostic& diagnostic)
    {
        std::vector<std::string> where;
        if (!diagnostic.nodeId.empty())
        {
            where.push_back("node '" + diagnostic.nodeId + "'");
        }
        if (!diagnostic.portName.empty())
        {
            where.push_back("port '" + diagnostic.portName + "'");
        }
        if (!diagnostic.field.empty())
        {
            where.push_back("field '" + diagnostic.field + "'");
        }
        if (diagnostic.line > 0)
        {
            where.push_back("line " + std::to_string(diagnostic.line));
        }
        std::string text = diagnostic.severity == DiagnosticSeverity::Error ? "error: " : "warning: ";
        text += diagnostic.message;
        text += where.empty() ? "" : " (" + join(where, ", ") + ")";
        return text;
    }

    [[nodiscard]] static std::string formatDiagnostics(const std::span<const Diagnostic> diagnostics)
    {
        std::string text;
        for (const auto& diagnostic : diagnostics)
        {
            text += formatDiagnostic(diagnostic) + "\n";
        }
        return text;
    }

  private:
    [[nodiscard]] static std::string join(const std::vector<std::string>& items,
                                          const std::string_view separator = ", ")
    {
        std::string text;
        for (size_t i = 0; i < items.size(); ++i)
        {
            text += (i == 0 ? "" : std::string{separator}) + items[i];
        }
        return text;
    }

    [[nodiscard]] static std::string number(const float value)
    {
        return std::isfinite(value) ? std::format("{}", value) : "null";
    }

    // Feedback edges: both ends inside one cycle and the destination breaks it.
    [[nodiscard]] static std::vector<bool> feedbackEdgeFlags(const GraphDescription& description,
                                                             const NodeRegistry& registry)
    {
        const auto components = GraphValidator::findFeedbackComponents(description, registry);
        const auto contains = [](const std::vector<std::string>& ids, const std::string& id)
        { return std::ranges::find(ids, id) != ids.end(); };
        std::vector<bool> flags(description.edges.size(), false);
        for (size_t i = 0; i < description.edges.size(); ++i)
        {
            const auto& edge = description.edges[i];
            for (const auto& component : components)
            {
                flags[i] = flags[i] ||
                           (contains(component.nodeIds, edge.fromNode) && contains(component.nodeIds, edge.toNode) &&
                            contains(component.breakerIds, edge.toNode));
            }
        }
        return flags;
    }

    [[nodiscard]] static std::string dotQuote(const std::string& text)
    {
        std::string quoted = "\"";
        for (const char c : text)
        {
            if (c == '"' || c == '\\')
            {
                quoted += '\\';
                quoted += c;
            }
            else
            {
                quoted += c == '\n' ? std::string{"\\n"} : std::string{c};
            }
        }
        return quoted + "\"";
    }

    [[nodiscard]] static std::string dotEdges(const GraphDescription& description, const std::vector<bool>& feedback)
    {
        std::string dot;
        for (size_t i = 0; i < description.edges.size(); ++i)
        {
            const auto& edge = description.edges[i];
            const std::string from = edge.fromNode.empty() ? "in:" + edge.fromPort : edge.fromNode;
            const std::string to = edge.toNode.empty() ? "out:" + edge.toPort : edge.toNode;
            std::string label = edge.fromNode.empty() ? edge.toPort
                                : edge.toNode.empty() ? edge.fromPort
                                                      : edge.fromPort + " -> " + edge.toPort;
            label += edge.gain != 1.0f ? " x" + number(edge.gain) : "";
            label += edge.polarity < 0 ? " (inverted)" : "";
            label += edge.label.empty() ? "" : " [" + edge.label + "]";
            const std::string style = feedback[i] ? ", style=dashed, color=red" : "";
            dot += "  " + dotQuote(from) + " -> " + dotQuote(to) + " [label=" + dotQuote(label) + style + "];\n";
        }
        return dot;
    }

    [[nodiscard]] static std::string dotControlsAndMacros(const GraphDescription& description)
    {
        std::string dot;
        for (const auto& control : description.controls)
        {
            const std::string from = control.fromNode.empty() ? "in:" + control.fromPort : control.fromNode;
            const std::string label = control.fromPort + " -> " + control.toParam +
                                      (control.mapName.empty() ? "" : " (" + control.mapName + ")");
            dot += "  " + dotQuote(from) + " -> " + dotQuote(control.toNode) +
                   " [style=dotted, label=" + dotQuote(label) + "];\n";
        }
        for (const auto& macro : description.macros)
        {
            const std::string id = "macro:" + macro.id;
            dot += "  " + dotQuote(id) + " [shape=note, label=" + dotQuote("macro " + macro.id + "\n" + macro.label) +
                   "];\n";
            for (const auto& target : macro.targets)
            {
                const std::string label = target.toParam + (target.mapName.empty() ? "" : " (" + target.mapName + ")");
                dot += "  " + dotQuote(id) + " -> " + dotQuote(target.toNode) +
                       " [style=dotted, label=" + dotQuote(label) + "];\n";
            }
        }
        return dot;
    }

    [[nodiscard]] static std::string jsonString(const std::string& text)
    {
        std::string quoted = "\"";
        for (const char c : text)
        {
            switch (c)
            {
                case '"':
                    quoted += "\\\"";
                    break;
                case '\\':
                    quoted += "\\\\";
                    break;
                case '\n':
                    quoted += "\\n";
                    break;
                case '\t':
                    quoted += "\\t";
                    break;
                default:
                    quoted += static_cast<unsigned char>(c) < 0x20 ? std::format("\\u{:04x}", static_cast<int>(c))
                                                                   : std::string{c};
            }
        }
        return quoted + "\"";
    }

    [[nodiscard]] static std::string jsonStrings(const std::vector<std::string>& items)
    {
        std::string json = "[";
        for (size_t i = 0; i < items.size(); ++i)
        {
            json += (i == 0 ? "" : ", ") + jsonString(items[i]);
        }
        return json + "]";
    }

    // Wraps already-encoded elements one per line.
    [[nodiscard]] static std::string jsonArray(const std::vector<std::string>& elements)
    {
        if (elements.empty())
        {
            return "[]";
        }
        std::string json = "[\n";
        for (size_t i = 0; i < elements.size(); ++i)
        {
            json += "    " + elements[i] + (i + 1 < elements.size() ? ",\n" : "\n");
        }
        return json + "  ]";
    }

    [[nodiscard]] static std::string jsonNodes(const GraphDescription& description, const NodeRegistry& registry)
    {
        std::vector<std::string> elements;
        for (const auto& node : description.nodes)
        {
            const auto* schema = registry.findSchema(node.type);
            std::string params;
            for (const auto& [key, value] : node.params)
            {
                params += (params.empty() ? "" : ", ") + jsonString(key) + ": " + number(value);
            }
            std::string config;
            for (const auto& [key, value] : node.config)
            {
                config += (config.empty() ? "" : ", ") + jsonString(key) + ": " + jsonString(value);
            }
            elements.push_back("{\"id\": " + jsonString(node.id) + ", \"type\": " + jsonString(node.type) +
                               ", \"breaksCycle\": " + (schema != nullptr && schema->breaksCycle ? "true" : "false") +
                               ", \"params\": {" + params + "}, \"config\": {" + config + "}}");
        }
        return jsonArray(elements);
    }

    [[nodiscard]] static std::string jsonEdges(const GraphDescription& description, const std::vector<bool>& feedback)
    {
        std::vector<std::string> elements;
        for (size_t i = 0; i < description.edges.size(); ++i)
        {
            const auto& edge = description.edges[i];
            elements.push_back(
                "{\"fromNode\": " + jsonString(edge.fromNode) + ", \"fromPort\": " + jsonString(edge.fromPort) +
                ", \"toNode\": " + jsonString(edge.toNode) + ", \"toPort\": " + jsonString(edge.toPort) +
                ", \"gain\": " + number(edge.gain) + ", \"polarity\": " + std::to_string(edge.polarity) +
                ", \"label\": " + jsonString(edge.label) + ", \"feedback\": " + (feedback[i] ? "true" : "false") + "}");
        }
        return jsonArray(elements);
    }

    [[nodiscard]] static std::string jsonControls(const GraphDescription& description)
    {
        std::vector<std::string> elements;
        for (const auto& control : description.controls)
        {
            elements.push_back(
                "{\"fromNode\": " + jsonString(control.fromNode) + ", \"fromPort\": " + jsonString(control.fromPort) +
                ", \"toNode\": " + jsonString(control.toNode) + ", \"toParam\": " + jsonString(control.toParam) +
                ", \"map\": " + jsonString(control.mapName) + ", \"smoothingMs\": " + number(control.smoothingMs) +
                "}");
        }
        return jsonArray(elements);
    }

    [[nodiscard]] static std::string jsonMacros(const GraphDescription& description)
    {
        std::vector<std::string> elements;
        for (const auto& macro : description.macros)
        {
            std::string targets;
            for (const auto& target : macro.targets)
            {
                targets += (targets.empty() ? "" : ", ") + std::string{"{\"toNode\": "} + jsonString(target.toNode) +
                           ", \"toParam\": " + jsonString(target.toParam) + ", \"map\": " + jsonString(target.mapName) +
                           "}";
            }
            elements.push_back("{\"id\": " + jsonString(macro.id) + ", \"label\": " + jsonString(macro.label) +
                               ", \"default\": " + number(macro.defaultValue) + ", \"targets\": [" + targets + "]}");
        }
        return jsonArray(elements);
    }

    [[nodiscard]] static std::string jsonCycles(const std::vector<FeedbackComponent>& components)
    {
        std::vector<std::string> elements;
        for (const auto& component : components)
        {
            elements.push_back("{\"nodes\": " + jsonStrings(component.nodeIds) +
                               ", \"breakers\": " + jsonStrings(component.breakerIds) + "}");
        }
        return jsonArray(elements);
    }

    [[nodiscard]] static std::string jsonLayout(const CompiledGraph::Layout& layout)
    {
        std::vector<std::string> elements;
        for (const auto& port : layout.ports)
        {
            elements.push_back("{\"node\": " + jsonString(port.nodeId) + ", \"port\": " + jsonString(port.port) +
                               std::format(", \"slot\": {}, \"first\": {}, \"last\": {}, \"feedback\": {}}}", port.slot,
                                           port.firstPosition, port.lastPosition, port.isFeedback));
        }
        return "{\"slotCount\": " + std::to_string(layout.slotCount) +
               ", \"reservedSlotCount\": " + std::to_string(layout.reservedSlotCount) +
               ", \"ports\": " + jsonArray(elements) + "}";
    }
};

}
