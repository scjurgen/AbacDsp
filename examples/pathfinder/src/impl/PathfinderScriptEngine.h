#pragma once

#include <array>
#include <cstddef>
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

inline constexpr size_t kPathfinderDialMacroCount{4};
inline constexpr size_t kPathfinderKnobCount{AbacDsp::Graph::MacroBank::kSlotCount - kPathfinderDialMacroCount};
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
 * The macros depth, speed, aggressivity and character take the four dial slots, any others take
 * one of the knob slots in order. An error names the node, field and line where they are known.
 */
template <size_t BlockSize>
class PathfinderScriptEngine
{
  public:
    static constexpr size_t kMaxScriptBytes{64 * 1024};
    static constexpr size_t kMaxNodes{128};
    static constexpr size_t kDialMacroCount{kPathfinderDialMacroCount};
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

        const std::array<std::string, kDialMacroCount> dialIds{"depth", "speed", "aggressivity", "character"};
        auto lowered = AbacDsp::Graph::MacroLowering::lower(*loaded.description, m_registry, dialIds, kDialMacroCount);
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
            if (macro.slot < kDialMacroCount)
            {
                continue;
            }
            auto& slot = slots[macro.slot - kDialMacroCount];
            slot.claimed = true;
            slot.id = macro.id;
            slot.name = macro.label;
            slot.defaultValue = macro.defaultValue;
            slot.description = "Script macro \"" + macro.id + "\"";
        }
        return slots;
    }

    [[nodiscard]] static std::string skeleton()
    {
        return std::string{AbacDsp::Graph::Presets::kTapeVibrato};
    }

  private:
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
