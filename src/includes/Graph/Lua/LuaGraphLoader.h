#pragma once

#define SOL_USING_CXX_LUA 1

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <optional>
#include <sol/sol.hpp>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "Graph/GraphDescription.h"
#include "Graph/GraphValidator.h"

namespace AbacDsp::Graph::Lua
{

struct LoadResult
{
    std::optional<GraphDescription> description;
    std::vector<Diagnostic> diagnostics;
};

/**
 * @ingroup graph
 * @brief Parses a Lua table (chorus.md's graph DSL) into a GraphDescription.
 *
 * Structural conversion only, matching Lua tables to GraphDescription fields -
 * GraphValidator remains the only place graph semantics are checked, and this
 * loader never sees a NodeRegistry. A script that errors, or does not return
 * a table, yields a single Error diagnostic and no description. Opens a
 * minimal sol2 sandbox (base/math/string/table only, no io/os/package/debug);
 * a fresh sol::state is created per call, so this is safe to call from
 * multiple threads concurrently, just never from the audio thread.
 */
class LuaGraphLoader
{
  public:
    // A script is one short table; these only stop a runaway loop or a memory bomb.
    static constexpr long kInstructionBudget{20'000'000};
    static constexpr int kHookInterval{1000};
    static constexpr size_t kMemoryLimitBytes{64u * 1024u * 1024u};

    [[nodiscard]] static LoadResult loadFromString(const std::string_view source)
    {
        MemoryLimit memory{0, kMemoryLimitBytes};
        sol::state lua(sol::default_at_panic, &limitedAllocator, &memory);
        lua.open_libraries(sol::lib::base, sol::lib::math, sol::lib::string, sol::lib::table);
        tInstructionsLeft = kInstructionBudget;
        lua_sethook(lua.lua_state(), &instructionHook, LUA_MASKCOUNT, kHookInterval);

        const sol::protected_function_result result = lua.safe_script(source, sol::script_pass_on_error);
        if (!result.valid())
        {
            const sol::error err = result;
            return scriptError(err.what());
        }
        const sol::object returned = result;
        if (returned.get_type() != sol::type::table)
        {
            return error("script did not return a table");
        }
        LoadResult loaded = parseGraph(returned.as<sol::table>());
        annotateSourceLines(loaded.diagnostics, source);
        return loaded;
    }

    /// Sets Diagnostic::line for entries naming a node, by finding that node's id, and the
    /// param or config key or "node.port" reference, in the script text. Best effort: Lua keeps no
    /// source positions for table fields, so a repeated id string can point at the wrong line.
    static void annotateSourceLines(std::vector<Diagnostic>& diagnostics, const std::string_view source)
    {
        const std::vector<std::string_view> lines = splitLines(source);
        for (auto& diagnostic : diagnostics)
        {
            if (diagnostic.line == 0 && !diagnostic.nodeId.empty())
            {
                diagnostic.line = lookupLine(lines, diagnostic);
            }
        }
    }

    [[nodiscard]] static LoadResult loadFromFile(const std::string& path)
    {
        std::ifstream file(path);
        if (!file)
        {
            return error("could not open file: " + path);
        }
        std::ostringstream contents;
        contents << file.rdbuf();
        return loadFromString(contents.str());
    }

  private:
    struct MemoryLimit
    {
        size_t used;
        size_t limit;
    };

    // Lua's allocator contract (realloc and free in one function), with a ceiling.
    static void* limitedAllocator(void* userData, void* pointer, const size_t oldSize, const size_t newSize)
    {
        auto* memory = static_cast<MemoryLimit*>(userData);
        const size_t previous = pointer != nullptr ? oldSize : 0;
        if (newSize == 0)
        {
            memory->used -= previous;
            std::free(pointer);
            return nullptr;
        }
        if (memory->used - previous + newSize > memory->limit)
        {
            return nullptr;
        }
        void* grown = std::realloc(pointer, newSize);
        memory->used = grown != nullptr ? memory->used - previous + newSize : memory->used;
        return grown;
    }

    static inline thread_local long tInstructionsLeft{0};

    static void instructionHook(lua_State* state, lua_Debug*)
    {
        tInstructionsLeft -= kHookInterval;
        if (tInstructionsLeft <= 0)
        {
            luaL_error(state, "script exceeded its instruction budget");
        }
    }

    [[nodiscard]] static LoadResult error(std::string message)
    {
        return LoadResult{std::nullopt, {Diagnostic{DiagnosticSeverity::Error, std::move(message), "", ""}}};
    }

    // sol2 prefixes a script error with `[string "..."]:LINE:`.
    [[nodiscard]] static LoadResult scriptError(std::string message)
    {
        const int line = scriptErrorLine(message);
        LoadResult result = error(std::move(message));
        result.diagnostics.front().line = line;
        return result;
    }

    [[nodiscard]] static int scriptErrorLine(const std::string& message)
    {
        const size_t marker = message.find("]:");
        if (marker == std::string::npos)
        {
            return 0;
        }
        size_t end = marker + 2;
        while (end < message.size() && std::isdigit(static_cast<unsigned char>(message[end])))
        {
            ++end;
        }
        if (end == marker + 2 || end >= message.size() || message[end] != ':')
        {
            return 0;
        }
        return std::stoi(message.substr(marker + 2, end - marker - 2));
    }

    [[nodiscard]] static std::vector<std::string_view> splitLines(const std::string_view source)
    {
        std::vector<std::string_view> lines;
        size_t start = 0;
        while (start <= source.size())
        {
            const size_t end = std::min(source.find('\n', start), source.size());
            lines.push_back(source.substr(start, end - start));
            start = end + 1;
        }
        return lines;
    }

    [[nodiscard]] static bool isWordChar(const char c)
    {
        return std::isalnum(static_cast<unsigned char>(c)) != 0 || c == '_';
    }

    [[nodiscard]] static std::string_view trimRight(std::string_view text)
    {
        while (!text.empty() && std::isspace(static_cast<unsigned char>(text.back())) != 0)
        {
            text.remove_suffix(1);
        }
        return text;
    }

    // True when the text before a quoted id reads `id =`, with `id` a whole word.
    [[nodiscard]] static bool endsWithIdAssignment(std::string_view prefix)
    {
        prefix = trimRight(prefix);
        if (prefix.empty() || prefix.back() != '=')
        {
            return false;
        }
        prefix = trimRight(prefix.substr(0, prefix.size() - 1));
        return prefix.ends_with("id") && (prefix.size() == 2 || !isWordChar(prefix[prefix.size() - 3]));
    }

    [[nodiscard]] static bool declaresAnyId(const std::string_view line)
    {
        const size_t quote = line.find('"');
        return quote != std::string_view::npos && endsWithIdAssignment(line.substr(0, quote));
    }

    [[nodiscard]] static bool declaresId(const std::string_view line, const std::string& id)
    {
        const size_t position = line.find('"' + id + '"');
        return position != std::string_view::npos && endsWithIdAssignment(line.substr(0, position));
    }

    [[nodiscard]] static bool assignsKey(const std::string_view line, const std::string& key)
    {
        for (size_t position = line.find(key); position != std::string_view::npos;
             position = line.find(key, position + 1))
        {
            const size_t after = position + key.size();
            const bool wholeWord = (position == 0 || !isWordChar(line[position - 1])) &&
                                   (after >= line.size() || !isWordChar(line[after]));
            const size_t next = line.find_first_not_of(" \t", after);
            if (wholeWord && next != std::string_view::npos && line[next] == '=')
            {
                return true;
            }
        }
        return false;
    }

    [[nodiscard]] static int findLine(const std::vector<std::string_view>& lines, const size_t from,
                                      const std::string& needle)
    {
        for (size_t i = from; i < lines.size(); ++i)
        {
            if (lines[i].find(needle) != std::string_view::npos)
            {
                return static_cast<int>(i) + 1;
            }
        }
        return 0;
    }

    [[nodiscard]] static int findIdLine(const std::vector<std::string_view>& lines, const std::string& id)
    {
        for (size_t i = 0; i < lines.size(); ++i)
        {
            if (declaresId(lines[i], id))
            {
                return static_cast<int>(i) + 1;
            }
        }
        return 0;
    }

    // Scans the node's own table: from its id line until the next id declaration.
    [[nodiscard]] static int findKeyLine(const std::vector<std::string_view>& lines, const int idLine,
                                         const std::string& key)
    {
        for (size_t i = static_cast<size_t>(idLine) - 1; i < lines.size(); ++i)
        {
            if (i + 1 != static_cast<size_t>(idLine) && declaresAnyId(lines[i]))
            {
                return 0;
            }
            if (assignsKey(lines[i], key))
            {
                return static_cast<int>(i) + 1;
            }
        }
        return 0;
    }

    [[nodiscard]] static int lookupLine(const std::vector<std::string_view>& lines, const Diagnostic& diagnostic)
    {
        const int idLine = findIdLine(lines, diagnostic.nodeId);
        const size_t dot = diagnostic.field.find('.');
        if (idLine != 0 && dot != std::string::npos)
        {
            const int keyLine = findKeyLine(lines, idLine, diagnostic.field.substr(dot + 1));
            if (keyLine != 0)
            {
                return keyLine;
            }
        }
        if (!diagnostic.portName.empty())
        {
            const int portLine = findLine(lines, 0, '"' + diagnostic.nodeId + '.' + diagnostic.portName + '"');
            if (portLine != 0)
            {
                return portLine;
            }
        }
        return idLine;
    }

    // Splits "node.port" on the first '.'; no dot means the whole string names
    // a graph boundary port, matching Edge/ControlEdge's fromNode="" convention.
    [[nodiscard]] static std::pair<std::string, std::string> splitNodePort(const std::string& value)
    {
        const size_t dot = value.find('.');
        if (dot == std::string::npos)
        {
            return {"", value};
        }
        return {value.substr(0, dot), value.substr(dot + 1)};
    }

    [[nodiscard]] static std::optional<std::string> configValueToString(const sol::object& value,
                                                                        std::vector<Diagnostic>& diagnostics,
                                                                        const std::string& nodeId,
                                                                        const std::string& key)
    {
        const sol::type type = value.get_type();
        if (type == sol::type::number)
        {
            return std::to_string(value.as<double>());
        }
        if (type == sol::type::string)
        {
            return value.as<std::string>();
        }
        if (type == sol::type::boolean)
        {
            return value.as<bool>() ? "true" : "false";
        }
        diagnostics.push_back({DiagnosticSeverity::Error,
                               "node config value must be a number, string or boolean, not a table", nodeId, key,
                               "config." + key});
        return std::nullopt;
    }

    [[nodiscard]] static bool parseNodes(const sol::table& root, std::vector<NodeInstance>& nodes,
                                         std::vector<Diagnostic>& diagnostics)
    {
        const sol::optional<sol::table> nodesTable = root["nodes"];
        if (!nodesTable)
        {
            return true;
        }
        bool ok = true;
        for (size_t i = 1; i <= nodesTable->size(); ++i)
        {
            const sol::optional<sol::table> entry = (*nodesTable)[i];
            if (!entry)
            {
                diagnostics.push_back({DiagnosticSeverity::Error, "nodes[" + std::to_string(i) + "] is not a table", "",
                                       "", "nodes[" + std::to_string(i) + "]"});
                ok = false;
                continue;
            }
            const sol::optional<std::string> id = (*entry)["id"];
            const sol::optional<std::string> type = (*entry)["type"];
            if (!id || !type)
            {
                diagnostics.push_back({DiagnosticSeverity::Error,
                                       "nodes[" + std::to_string(i) + "] missing \"id\" or \"type\"", "", "",
                                       "nodes[" + std::to_string(i) + "]"});
                ok = false;
                continue;
            }

            NodeInstance instance{.id = *id, .type = *type};

            const sol::optional<sol::table> params = (*entry)["params"];
            if (params)
            {
                for (const auto& [key, value] : *params)
                {
                    if (key.get_type() != sol::type::string)
                    {
                        continue;
                    }
                    if (value.get_type() != sol::type::number)
                    {
                        diagnostics.push_back({DiagnosticSeverity::Error, "node param must be a number", instance.id,
                                               key.as<std::string>(), "params." + key.as<std::string>()});
                        ok = false;
                        continue;
                    }
                    instance.params[key.as<std::string>()] = value.as<float>();
                }
            }

            const sol::optional<sol::table> config = (*entry)["config"];
            if (config)
            {
                for (const auto& [key, value] : *config)
                {
                    if (key.get_type() != sol::type::string)
                    {
                        continue;
                    }
                    const std::string keyStr = key.as<std::string>();
                    const std::optional<std::string> converted =
                        configValueToString(value, diagnostics, instance.id, keyStr);
                    if (!converted)
                    {
                        ok = false;
                        continue;
                    }
                    instance.config[keyStr] = *converted;
                }
            }

            nodes.push_back(std::move(instance));
        }
        return ok;
    }

    [[nodiscard]] static bool parseEdges(const sol::table& root, std::vector<Edge>& edges,
                                         std::vector<Diagnostic>& diagnostics)
    {
        const sol::optional<sol::table> edgesTable = root["edges"];
        if (!edgesTable)
        {
            return true;
        }
        bool ok = true;
        for (size_t i = 1; i <= edgesTable->size(); ++i)
        {
            const sol::optional<sol::table> entry = (*edgesTable)[i];
            const sol::optional<std::string> from = entry ? sol::optional<std::string>((*entry)["from"]) : std::nullopt;
            const sol::optional<std::string> to = entry ? sol::optional<std::string>((*entry)["to"]) : std::nullopt;
            if (!from || !to)
            {
                diagnostics.push_back({DiagnosticSeverity::Error,
                                       "edges[" + std::to_string(i) + "] missing \"from\" or \"to\"", "", "",
                                       "edges[" + std::to_string(i) + "]"});
                ok = false;
                continue;
            }
            const auto [fromNode, fromPort] = splitNodePort(*from);
            const auto [toNode, toPort] = splitNodePort(*to);
            edges.push_back(Edge{.fromNode = fromNode, .fromPort = fromPort, .toNode = toNode, .toPort = toPort});
        }
        return ok;
    }

    // No concrete "controls" example exists in chorus.md; shaped to mirror macro targets -
    // "from" is a boundary port or "node.port" like an edge, "to" is "node.param"
    // (required), "map" is string-only like a macro target's map.
    [[nodiscard]] static bool parseControls(const sol::table& root, std::vector<ControlEdge>& controls,
                                            std::vector<Diagnostic>& diagnostics)
    {
        const sol::optional<sol::table> controlsTable = root["controls"];
        if (!controlsTable)
        {
            return true;
        }
        bool ok = true;
        for (size_t i = 1; i <= controlsTable->size(); ++i)
        {
            const sol::optional<sol::table> entry = (*controlsTable)[i];
            const sol::optional<std::string> from = entry ? sol::optional<std::string>((*entry)["from"]) : std::nullopt;
            const sol::optional<std::string> to = entry ? sol::optional<std::string>((*entry)["to"]) : std::nullopt;
            if (!from || !to)
            {
                diagnostics.push_back({DiagnosticSeverity::Error,
                                       "controls[" + std::to_string(i) + "] missing \"from\" or \"to\"", "", "",
                                       "controls[" + std::to_string(i) + "]"});
                ok = false;
                continue;
            }
            const auto [toNode, toParam] = splitNodePort(*to);
            if (toNode.empty())
            {
                diagnostics.push_back({DiagnosticSeverity::Error,
                                       "controls[" + std::to_string(i) + "] \"to\" must be \"node.param\"", "", "",
                                       "controls[" + std::to_string(i) + "].to"});
                ok = false;
                continue;
            }
            const auto [fromNode, fromPort] = splitNodePort(*from);
            ControlEdge edge{.fromNode = fromNode, .fromPort = fromPort, .toNode = toNode, .toParam = toParam};

            const sol::object mapObj = (*entry)["map"];
            if (mapObj.valid())
            {
                if (mapObj.get_type() != sol::type::string)
                {
                    diagnostics.push_back({DiagnosticSeverity::Error,
                                           "controls[" + std::to_string(i) + "].map must be a string map name", toNode,
                                           toParam, "controls[" + std::to_string(i) + "].map"});
                    ok = false;
                    continue;
                }
                edge.mapName = mapObj.as<std::string>();
            }
            edge.smoothingMs = (*entry).get_or("smoothingMs", 0.0f);
            controls.push_back(std::move(edge));
        }
        return ok;
    }

    // min and max come as a pair; curve is "linear" or "exp".
    [[nodiscard]] static bool parseTargetRange(const sol::table& entry, const std::string& macroId,
                                               const std::string& to, MacroTarget& target,
                                               std::vector<Diagnostic>& diagnostics)
    {
        const std::string where = "macro \"" + macroId + "\" target \"" + to + "\"";
        const sol::optional<float> minValue = entry["min"];
        const sol::optional<float> maxValue = entry["max"];
        if (minValue.has_value() != maxValue.has_value())
        {
            diagnostics.push_back({DiagnosticSeverity::Error, where + " needs both min and max, or neither",
                                   target.toNode, target.toParam, "macros." + macroId + ".targets"});
            return false;
        }
        if (minValue)
        {
            target.minValue = *minValue;
            target.maxValue = *maxValue;
        }

        const sol::optional<std::string> curve = entry["curve"];
        if (curve && *curve != "linear" && *curve != "exp")
        {
            diagnostics.push_back({DiagnosticSeverity::Error, where + " curve must be \"linear\" or \"exp\"",
                                   target.toNode, target.toParam, "macros." + macroId + ".targets"});
            return false;
        }
        target.curve = curve.value_or(std::string{});
        return true;
    }

    [[nodiscard]] static bool parseMacros(const sol::table& root, std::vector<Macro>& macros,
                                          std::vector<Diagnostic>& diagnostics)
    {
        const sol::optional<sol::table> macrosTable = root["macros"];
        if (!macrosTable)
        {
            return true;
        }
        bool ok = true;
        for (size_t i = 1; i <= macrosTable->size(); ++i)
        {
            const sol::optional<sol::table> entry = (*macrosTable)[i];
            const sol::optional<std::string> id = entry ? sol::optional<std::string>((*entry)["id"]) : std::nullopt;
            if (!id)
            {
                diagnostics.push_back({DiagnosticSeverity::Error, "macros[" + std::to_string(i) + "] missing \"id\"",
                                       "", "", "macros[" + std::to_string(i) + "].id"});
                ok = false;
                continue;
            }

            Macro macro{.id = *id,
                        .label = entry->get_or("label", std::string{}),
                        .defaultValue = entry->get_or("default", 0.0f)};

            const sol::optional<sol::table> targets = (*entry)["targets"];
            if (targets)
            {
                for (size_t t = 1; t <= targets->size(); ++t)
                {
                    const sol::optional<sol::table> targetEntry = (*targets)[t];
                    const sol::optional<std::string> to =
                        targetEntry ? sol::optional<std::string>((*targetEntry)["to"]) : std::nullopt;
                    if (!to)
                    {
                        diagnostics.push_back(
                            {DiagnosticSeverity::Error,
                             "macro \"" + macro.id + "\" target " + std::to_string(t) + " missing \"to\"", "", "",
                             "macros." + macro.id + ".targets[" + std::to_string(t) + "].to"});
                        ok = false;
                        continue;
                    }
                    const auto [toNode, toParam] = splitNodePort(*to);
                    if (toNode.empty())
                    {
                        diagnostics.push_back(
                            {DiagnosticSeverity::Error,
                             "macro \"" + macro.id + "\" target \"" + *to + "\" must be \"node.param\"", "", "",
                             "macros." + macro.id + ".targets[" + std::to_string(t) + "].to"});
                        ok = false;
                        continue;
                    }
                    MacroTarget target{.toNode = toNode, .toParam = toParam};

                    const sol::object mapObj = (*targetEntry)["map"];
                    if (mapObj.valid())
                    {
                        if (mapObj.get_type() != sol::type::string)
                        {
                            diagnostics.push_back({DiagnosticSeverity::Error,
                                                   "macro \"" + macro.id + "\" target \"" + *to +
                                                       "\".map must be a string map name, not an inline table",
                                                   toNode, toParam,
                                                   "macros." + macro.id + ".targets[" + std::to_string(t) + "].map"});
                            ok = false;
                            continue;
                        }
                        target.mapName = mapObj.as<std::string>();
                    }
                    if (!parseTargetRange(*targetEntry, macro.id, *to, target, diagnostics))
                    {
                        ok = false;
                        continue;
                    }
                    macro.targets.push_back(std::move(target));
                }
            }
            macros.push_back(std::move(macro));
        }
        return ok;
    }

    [[nodiscard]] static LoadResult parseGraph(const sol::table& root)
    {
        std::vector<Diagnostic> diagnostics;

        const sol::optional<int> version = root["version"];
        if (version && *version != 1)
        {
            diagnostics.push_back({DiagnosticSeverity::Error, "unsupported graph version: " + std::to_string(*version),
                                   "", "", "version"});
        }

        GraphDescription description;
        description.name = root.get_or("name", std::string{});

        const sol::optional<sol::table> io = root["io"];
        if (io)
        {
            const sol::optional<sol::table> inputs = (*io)["inputs"];
            if (inputs)
            {
                for (size_t i = 1; i <= inputs->size(); ++i)
                {
                    description.io.inputs.push_back((*inputs)[i]);
                }
            }
            const sol::optional<sol::table> outputs = (*io)["outputs"];
            if (outputs)
            {
                for (size_t i = 1; i <= outputs->size(); ++i)
                {
                    description.io.outputs.push_back((*outputs)[i]);
                }
            }
        }

        bool ok = parseNodes(root, description.nodes, diagnostics);
        ok = parseEdges(root, description.edges, diagnostics) && ok;
        ok = parseControls(root, description.controls, diagnostics) && ok;
        ok = parseMacros(root, description.macros, diagnostics) && ok;

        const bool hasError = std::any_of(diagnostics.begin(), diagnostics.end(),
                                          [](const Diagnostic& d) { return d.severity == DiagnosticSeverity::Error; });
        if (!ok || hasError)
        {
            return LoadResult{std::nullopt, std::move(diagnostics)};
        }
        return LoadResult{std::move(description), std::move(diagnostics)};
    }
};

}
