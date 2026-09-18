#pragma once

#define SOL_USING_CXX_LUA 1

#include <algorithm>
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
    [[nodiscard]] static LoadResult loadFromString(const std::string_view source)
    {
        sol::state lua;
        lua.open_libraries(sol::lib::base, sol::lib::math, sol::lib::string, sol::lib::table);

        const sol::protected_function_result result = lua.safe_script(source, sol::script_pass_on_error);
        if (!result.valid())
        {
            const sol::error err = result;
            return error(err.what());
        }
        const sol::object returned = result;
        if (returned.get_type() != sol::type::table)
        {
            return error("script did not return a table");
        }
        return parseGraph(returned.as<sol::table>());
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
    [[nodiscard]] static LoadResult error(std::string message)
    {
        return LoadResult{std::nullopt, {Diagnostic{DiagnosticSeverity::Error, std::move(message), "", ""}}};
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
                               "node config value must be a number, string or boolean, not a table", nodeId, key});
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
                diagnostics.push_back(
                    {DiagnosticSeverity::Error, "nodes[" + std::to_string(i) + "] is not a table", "", ""});
                ok = false;
                continue;
            }
            const sol::optional<std::string> id = (*entry)["id"];
            const sol::optional<std::string> type = (*entry)["type"];
            if (!id || !type)
            {
                diagnostics.push_back(
                    {DiagnosticSeverity::Error, "nodes[" + std::to_string(i) + "] missing \"id\" or \"type\"", "", ""});
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
                                               key.as<std::string>()});
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
                diagnostics.push_back(
                    {DiagnosticSeverity::Error, "edges[" + std::to_string(i) + "] missing \"from\" or \"to\"", "", ""});
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
                                       "controls[" + std::to_string(i) + "] missing \"from\" or \"to\"", "", ""});
                ok = false;
                continue;
            }
            const auto [toNode, toParam] = splitNodePort(*to);
            if (toNode.empty())
            {
                diagnostics.push_back({DiagnosticSeverity::Error,
                                       "controls[" + std::to_string(i) + "] \"to\" must be \"node.param\"", "", ""});
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
                                           toParam});
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
                diagnostics.push_back(
                    {DiagnosticSeverity::Error, "macros[" + std::to_string(i) + "] missing \"id\"", "", ""});
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
                             "macro \"" + macro.id + "\" target " + std::to_string(t) + " missing \"to\"", "", ""});
                        ok = false;
                        continue;
                    }
                    const auto [toNode, toParam] = splitNodePort(*to);
                    if (toNode.empty())
                    {
                        diagnostics.push_back(
                            {DiagnosticSeverity::Error,
                             "macro \"" + macro.id + "\" target \"" + *to + "\" must be \"node.param\"", "", ""});
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
                                                   toNode, toParam});
                            ok = false;
                            continue;
                        }
                        target.mapName = mapObj.as<std::string>();
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
            diagnostics.push_back(
                {DiagnosticSeverity::Error, "unsupported graph version: " + std::to_string(*version), "", ""});
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
