#pragma once

#include <map>
#include <optional>
#include <string>
#include <vector>

namespace AbacDsp::Graph
{

enum class PortDirection
{
    Input,
    Output
};

enum class PortCategory
{
    AudioMono,
    AudioStereo,
    AudioN,
    ControlScalar,
    ControlAudioRate,
    Event,
    Meter
};

struct PortDescriptor
{
    std::string name;
    PortDirection direction{PortDirection::Input};
    PortCategory category{PortCategory::AudioMono};
    bool multiConnectable{false};
};

struct NodeInstance
{
    std::string id;
    std::string type;
    std::map<std::string, float> params;
    std::map<std::string, std::string> config;
};

/**
 * @ingroup graph
 * @brief One audio-graph connection, source output port to destination input port.
 *
 * An empty fromNode/toNode means the other side of that pair is a graph
 * boundary port name (GraphDescription::Io), not a node.port pair - a graph
 * input behaves as an edge source, a graph output as an edge sink.
 */
struct Edge
{
    std::string fromNode;
    std::string fromPort;
    std::string toNode;
    std::string toPort;
    float gain{1.0f};
    int polarity{1};
    std::string label;
};

struct ControlEdge
{
    std::string fromNode;
    std::string fromPort;
    std::string toNode;
    std::string toParam;
    std::string mapName;
    float smoothingMs{0.0f};
};

struct MacroTarget
{
    std::string toNode;
    std::string toParam;
    std::string mapName;
    // Both or neither: the parameter value at macro 0 and at macro 1; empty means the schema range.
    std::optional<float> minValue{};
    std::optional<float> maxValue{};
    // "linear" (or empty) or "exp": how the macro travels between minValue and maxValue.
    std::string curve{};
};

struct Macro
{
    std::string id;
    std::string label;
    // In display units: between displayMin and displayMax when given, else in 0 to 1.
    float defaultValue{0.0f};
    std::vector<MacroTarget> targets;
    // What a control shows for this macro: a unit label and the range its 0 to 1 travel spans.
    // The macro value itself stays 0 to 1. displayMin and displayMax come as a pair.
    std::string unit{};
    std::optional<float> displayMin{};
    std::optional<float> displayMax{};
};

/**
 * @ingroup graph
 * @brief Named external audio ports. Every entry is an audio.mono port.
 */
struct Io
{
    std::vector<std::string> inputs;
    std::vector<std::string> outputs;
};

/**
 * @ingroup graph
 * @brief Plain data model for one graph: I/O, node instances, and their wiring.
 *
 * Carries no behaviour and knows nothing about Lua - GraphValidator and
 * GraphCompiler are what give it meaning. controls/macros are populated and
 * consumed starting with later phases; this phase only moves the data.
 */
struct GraphDescription
{
    int version{1};
    std::string name;
    Io io;
    std::vector<NodeInstance> nodes;
    std::vector<Edge> edges;
    std::vector<ControlEdge> controls;
    std::vector<Macro> macros;
};

}
