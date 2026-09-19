#pragma once

#include <array>
#include <cstddef>
#include <format>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "../inc/LuaScriptEngineBase.h"
#include "Graph/CompiledGraph.h"
#include "Graph/GraphCompiler.h"
#include "Graph/GraphInspector.h"
#include "Graph/Lua/LuaGraphLoader.h"
#include "Graph/MacroBank.h"
#include "Graph/MacroLowering.h"
#include "Graph/NodeRegistry.h"
#include "Graph/Nodes/ControlNodes.h"
#include "Graph/Nodes/FilterNodes.h"
#include "Graph/Nodes/MacroInput.h"
#include "Graph/Nodes/StandardNodes.h"
#include "Graph/Nodes/TapeDelayNode.h"
#include "Graph/Presets/ChorusPresets.h"

// One script-claimed knob is the shared LuaUiParamSlot, so the generated processor and its
// authoring server can pass it on unchanged.
using PathfinderKnobSlot = LuaUiParamSlot;

// One macro per generated Lua control knob (luaParam1 to luaParam8).
inline constexpr size_t kPathfinderKnobCount{8};
using PathfinderKnobSlots = std::array<PathfinderKnobSlot, kPathfinderKnobCount>;

struct PathfinderScriptResult
{
    std::optional<AbacDsp::Graph::CompiledGraph> graph;
    std::vector<AbacDsp::Graph::LoweredMacro> macros;
    std::string error;
    std::string warnings;
};

/**
 * @brief Turns a graph script into a compiled graph for Pathfinder; message thread only.
 *
 * The script is the Lua graph DSL: load, check the shape (2 inputs, 2 outputs, size and node
 * limits), lower the macros onto the shared MacroBank, then validate and compile at BlockSize.
 * The macros take the eight knob slots in the order the script declares them; any beyond that are
 * ignored with a warning. An error names the node, field and line where they are known.
 */
template <size_t BlockSize>
class PathfinderScriptEngine
{
  public:
    static constexpr size_t kMaxScriptBytes{64 * 1024};
    static constexpr size_t kMaxNodes{128};
    static constexpr size_t kKnobCount{kPathfinderKnobCount};
    using KnobSlots = PathfinderKnobSlots;

    PathfinderScriptEngine(const AbacDsp::Graph::MacroBank& bank, const float sampleRate)
        : m_sampleRate(sampleRate)
    {
        AbacDsp::Graph::Nodes::registerStandardNodes(m_registry);
        AbacDsp::Graph::Nodes::registerFilterNodes(m_registry);
        AbacDsp::Graph::Nodes::registerControlNodes(m_registry);
        AbacDsp::Graph::Nodes::registerTapeDelayNode<BlockSize>(m_registry);
        AbacDsp::Graph::Nodes::registerMacroInputNode(m_registry, bank);
    }

    [[nodiscard]] PathfinderScriptResult compile(const std::string_view text) const
    {
        PathfinderScriptResult result;
        if (text.size() > kMaxScriptBytes)
        {
            result.error = "script is longer than " + std::to_string(kMaxScriptBytes / 1024) + " KB";
            return result;
        }
        auto loaded = AbacDsp::Graph::Lua::LuaGraphLoader::loadFromString(text);
        if (!loaded.description)
        {
            result.error = summarizeErrors(loaded.diagnostics);
            return result;
        }
        if (const auto problem = shapeProblem(*loaded.description))
        {
            result.error = *problem;
            return result;
        }

        auto lowered = AbacDsp::Graph::MacroLowering::lower(*loaded.description, m_registry,
                                                            std::span<const std::string>{}, 0, kKnobCount);
        auto compiled =
            AbacDsp::Graph::GraphCompiler::compile(lowered.description, m_registry, BlockSize, m_sampleRate);

        std::vector<AbacDsp::Graph::Diagnostic> all = std::move(loaded.diagnostics);
        all.insert(all.end(), lowered.diagnostics.begin(), lowered.diagnostics.end());
        all.insert(all.end(), compiled.diagnostics.begin(), compiled.diagnostics.end());
        AbacDsp::Graph::Lua::LuaGraphLoader::annotateSourceLines(all, text);
        if (!compiled.graph)
        {
            result.error = summarizeErrors(all);
            return result;
        }
        result.graph = std::move(compiled.graph);
        result.macros = std::move(lowered.macros);
        result.warnings = formatWarnings(all);
        return result;
    }

    [[nodiscard]] static KnobSlots knobSlots(const std::vector<AbacDsp::Graph::LoweredMacro>& macros)
    {
        KnobSlots slots{};
        for (const auto& macro : macros)
        {
            auto& slot = slots[macro.slot];
            slot.claimed = true;
            slot.id = macro.id;
            slot.name = macro.label;
            slot.unit = macro.unit;
            slot.rangeMin = macro.displayMin;
            slot.rangeMax = macro.displayMax;
            slot.defaultValue = macro.defaultValue;
            slot.description = "Script macro \"" + macro.id + "\"";
        }
        return slots;
    }

    // The node types a script can use, as a markdown table for the README: ports, parameters, config.
    [[nodiscard]] std::string nodeReferenceMarkdown() const
    {
        std::string text = "| Type | Ports | Parameters | Config |\n|---|---|---|---|\n";
        for (const auto& name : m_registry.typeNames())
        {
            if (name == "MacroInput")
            {
                continue;
            }
            const auto* schema = m_registry.findSchema(name);
            text += "| `" + name + "` | " + portsCell(*schema) + " | " + parametersCell(*schema) + " | " +
                    configCell(name) + " |\n";
        }
        return text;
    }

    [[nodiscard]] static std::string skeleton()
    {
        return std::string{AbacDsp::Graph::Presets::kTapeVibrato};
    }

  private:
    [[nodiscard]] static std::string portList(const AbacDsp::Graph::NodeSchema& schema,
                                              const AbacDsp::Graph::PortDirection direction)
    {
        std::string list;
        for (const auto& port : schema.ports)
        {
            if (port.direction != direction)
            {
                continue;
            }
            const bool control = port.category == AbacDsp::Graph::PortCategory::ControlAudioRate ||
                                 port.category == AbacDsp::Graph::PortCategory::ControlScalar;
            list += (list.empty() ? "" : ", ") + port.name + (control ? " (ctl)" : "");
        }
        return list;
    }

    [[nodiscard]] static std::string portsCell(const AbacDsp::Graph::NodeSchema& schema)
    {
        const std::string inputs = portList(schema, AbacDsp::Graph::PortDirection::Input);
        const std::string outputs = portList(schema, AbacDsp::Graph::PortDirection::Output);
        return (inputs.empty() ? "" : "in: " + inputs) + (inputs.empty() || outputs.empty() ? "" : "; ") +
               (outputs.empty() ? "" : "out: " + outputs);
    }

    [[nodiscard]] static std::string parametersCell(const AbacDsp::Graph::NodeSchema& schema)
    {
        std::string cell;
        for (const auto& parameter : schema.parameters)
        {
            const std::string unit = parameter.unit == "linear" || parameter.unit.empty() ? "" : " " + parameter.unit;
            cell += (cell.empty() ? "" : "; ") + std::format("`{}` ({:g} to {:g}, default {:g}{})", parameter.id,
                                                             parameter.minValue, parameter.maxValue,
                                                             parameter.defaultValue, unit);
        }
        return cell;
    }

    // Config keys are read by the node factories and are not part of a schema, so they are listed here.
    [[nodiscard]] static std::string configCell(const std::string& typeName)
    {
        if (typeName == "TapeDelay")
        {
            return "`baseDelayMs` (8), `safetyMarginSamples` (250), `seed` (1)";
        }
        if (typeName == "Biquad")
        {
            return "`mode`: lowpass (default), highpass, notch, peak";
        }
        if (typeName == "LFO")
        {
            return "`waveform`: sine (default), triangle, saw, square, noise";
        }
        if (typeName == "Constant")
        {
            return "`value` (0)";
        }
        return {};
    }

    [[nodiscard]] static std::optional<std::string> shapeProblem(const AbacDsp::Graph::GraphDescription& description)
    {
        if (description.io.inputs.size() != 2 || description.io.outputs.size() != 2)
        {
            return "the graph needs exactly 2 inputs and 2 outputs: io = { inputs = { \"inL\", \"inR\" }, outputs = { "
                   "\"outL\", \"outR\" } }";
        }
        if (description.nodes.size() > kMaxNodes)
        {
            return "the graph has more than " + std::to_string(kMaxNodes) + " nodes";
        }
        return std::nullopt;
    }

    [[nodiscard]] static std::string summarizeErrors(const std::vector<AbacDsp::Graph::Diagnostic>& diagnostics)
    {
        std::vector<const AbacDsp::Graph::Diagnostic*> errors;
        for (const auto& diagnostic : diagnostics)
        {
            if (diagnostic.severity == AbacDsp::Graph::DiagnosticSeverity::Error)
            {
                errors.push_back(&diagnostic);
            }
        }
        if (errors.empty())
        {
            return "the script could not be compiled";
        }
        std::string text = AbacDsp::Graph::GraphInspector::formatDiagnostic(*errors.front());
        text += errors.size() > 1 ? " (+" + std::to_string(errors.size() - 1) + " more)" : std::string{};
        return text;
    }

    [[nodiscard]] static std::string formatWarnings(const std::vector<AbacDsp::Graph::Diagnostic>& diagnostics)
    {
        std::string text;
        for (const auto& diagnostic : diagnostics)
        {
            if (diagnostic.severity == AbacDsp::Graph::DiagnosticSeverity::Warning)
            {
                text += AbacDsp::Graph::GraphInspector::formatDiagnostic(diagnostic) + "\n";
            }
        }
        return text;
    }

    AbacDsp::Graph::NodeRegistry m_registry;
    float m_sampleRate;
};
