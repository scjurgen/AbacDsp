#pragma once

#include <algorithm>
#include <cstddef>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "Graph/GraphDescription.h"
#include "Graph/GraphValidator.h"
#include "Graph/MacroBank.h"
#include "Graph/NodeRegistry.h"
#include "Graph/NodeSchema.h"

namespace AbacDsp::Graph
{

struct LoweredMacro
{
    std::string id;
    std::string label;
    float defaultValue{0.f};
    size_t slot{0};
};

struct LoweredGraph
{
    GraphDescription description;
    std::vector<LoweredMacro> macros;
    std::vector<Diagnostic> diagnostics;
};

/**
 * @ingroup graph
 * @brief Rewrites a description's macros into ordinary graph nodes, so a control never touches a node.
 *
 * Each usable macro becomes a MacroInput node reading one MacroBank slot, and each of its targets a
 * ScaleOffset (linear) or ExpMap (curve "exp") node feeding a control edge into the parameter. The
 * audio thread then applies every macro itself; a callback on any thread only writes the bank.
 * Ids listed in reservedIds take the slot of their position, the rest fill slots from
 * firstFreeSlot. A macro or target that cannot be lowered is skipped with a Warning.
 */
class MacroLowering
{
  public:
    [[nodiscard]] static LoweredGraph lower(const GraphDescription& description, const NodeRegistry& registry,
                                            const std::span<const std::string> reservedIds, const size_t firstFreeSlot)
    {
        LoweredGraph result{description, {}, {}};
        if (description.macros.empty())
        {
            return result;
        }
        if (!hasRequiredTypes(registry))
        {
            result.diagnostics.push_back(
                Diagnostic{DiagnosticSeverity::Warning,
                           "macros are ignored: MacroInput, ScaleOffset and ExpMap must be registered"});
            return result;
        }
        size_t nextFree = firstFreeSlot;
        for (const auto& macro : description.macros)
        {
            const auto slot = assignSlot(macro.id, reservedIds, nextFree);
            if (!slot)
            {
                result.diagnostics.push_back(Diagnostic{
                    DiagnosticSeverity::Warning, "macro \"" + macro.id + "\" has no free knob slot and is ignored", "",
                    "", "macros." + macro.id});
                continue;
            }
            lowerMacro(macro, *slot, description, registry, result);
        }
        return result;
    }

    [[nodiscard]] static std::string macroNodeId(const std::string& macroId)
    {
        return "$macro:" + macroId;
    }

  private:
    struct ResolvedTarget
    {
        const MacroTarget* target{nullptr};
        float minValue{0.f};
        float maxValue{1.f};
        bool exponential{false};
    };

    [[nodiscard]] static bool hasRequiredTypes(const NodeRegistry& registry)
    {
        return registry.findSchema("MacroInput") != nullptr && registry.findSchema("ScaleOffset") != nullptr &&
               registry.findSchema("ExpMap") != nullptr;
    }

    [[nodiscard]] static std::optional<size_t> assignSlot(const std::string& id,
                                                          const std::span<const std::string> reservedIds,
                                                          size_t& nextFree)
    {
        for (size_t i = 0; i < reservedIds.size(); ++i)
        {
            if (reservedIds[i] == id)
            {
                return i;
            }
        }
        if (nextFree < MacroBank::kSlotCount)
        {
            return nextFree++;
        }
        return std::nullopt;
    }

    static void warn(LoweredGraph& result, const LoweredMacro& macro, const MacroTarget& target, std::string message)
    {
        result.diagnostics.push_back(Diagnostic{DiagnosticSeverity::Warning,
                                                "macro \"" + macro.id + "\" target: " + std::move(message),
                                                target.toNode, target.toParam, "macros." + macro.id});
    }

    [[nodiscard]] static std::optional<ResolvedTarget> resolveTarget(const LoweredMacro& macro,
                                                                     const MacroTarget& target,
                                                                     const GraphDescription& description,
                                                                     const NodeRegistry& registry, LoweredGraph& result)
    {
        const auto node = std::ranges::find(description.nodes, target.toNode, &NodeInstance::id);
        const NodeSchema* schema = node == description.nodes.end() ? nullptr : registry.findSchema(node->type);
        if (schema == nullptr)
        {
            warn(result, macro, target, "unknown node, skipped");
            return std::nullopt;
        }
        const int index = schema->findParameterIndex(target.toParam);
        if (index < 0)
        {
            warn(result, macro, target, "unknown parameter, skipped");
            return std::nullopt;
        }
        const auto& parameter = schema->parameters[static_cast<size_t>(index)];
        ResolvedTarget resolved{&target, parameter.minValue, parameter.maxValue, target.curve == "exp"};
        if (target.minValue && target.maxValue)
        {
            resolved.minValue = *target.minValue;
            resolved.maxValue = *target.maxValue;
        }
        if (resolved.exponential && (resolved.minValue <= 0.f || resolved.maxValue <= 0.f))
        {
            warn(result, macro, target, "an exp curve needs a positive min and max, using linear");
            resolved.exponential = false;
        }
        return resolved;
    }

    static void addTarget(const std::string& macroId, const size_t index, const ResolvedTarget& resolved,
                          LoweredGraph& result)
    {
        const std::string sourceId = macroNodeId(macroId);
        const std::string mapperId = sourceId + ":" + std::to_string(index);
        NodeInstance mapper{.id = mapperId, .type = resolved.exponential ? "ExpMap" : "ScaleOffset"};
        if (resolved.exponential)
        {
            mapper.params = {
                {"inMin", 0.f}, {"inMax", 1.f}, {"outMin", resolved.minValue}, {"outMax", resolved.maxValue}};
        }
        else
        {
            mapper.params = {{"scale", resolved.maxValue - resolved.minValue}, {"offset", resolved.minValue}};
        }
        result.description.nodes.push_back(std::move(mapper));
        result.description.edges.push_back(
            Edge{.fromNode = sourceId, .fromPort = "out", .toNode = mapperId, .toPort = "in"});
        result.description.controls.push_back(ControlEdge{.fromNode = mapperId,
                                                          .fromPort = "out",
                                                          .toNode = resolved.target->toNode,
                                                          .toParam = resolved.target->toParam});
    }

    static void lowerMacro(const Macro& macro, const size_t slot, const GraphDescription& description,
                           const NodeRegistry& registry, LoweredGraph& result)
    {
        const LoweredMacro lowered{macro.id, macro.label.empty() ? macro.id : macro.label, macro.defaultValue, slot};
        std::vector<ResolvedTarget> targets;
        for (const auto& target : macro.targets)
        {
            if (auto resolved = resolveTarget(lowered, target, description, registry, result))
            {
                targets.push_back(*resolved);
            }
        }
        if (targets.empty())
        {
            result.diagnostics.push_back(Diagnostic{DiagnosticSeverity::Warning,
                                                    "macro \"" + macro.id + "\" has no usable target and is ignored",
                                                    "", "", "macros." + macro.id});
            return;
        }
        result.description.nodes.push_back(NodeInstance{
            .id = macroNodeId(macro.id), .type = "MacroInput", .config = {{"slot", std::to_string(slot)}}});
        for (size_t i = 0; i < targets.size(); ++i)
        {
            addTarget(macro.id, i, targets[i], result);
        }
        result.macros.push_back(lowered);
    }
};

}
