#pragma once

#include <cstdlib>
#include <memory>
#include <string>
#include <utility>

#include "Graph/NodeRegistry.h"
#include "Graph/NodeSchema.h"
#include "Graph/Nodes/Add.h"
#include "Graph/Nodes/Clamp.h"
#include "Graph/Nodes/Constant.h"
#include "Graph/Nodes/Curve.h"
#include "Graph/Nodes/EnvelopeFollower.h"
#include "Graph/Nodes/ExpMap.h"
#include "Graph/Nodes/Lfo.h"
#include "Graph/Nodes/Macro.h"
#include "Graph/Nodes/Map.h"
#include "Graph/Nodes/Multiply.h"
#include "Graph/Nodes/PhaseOffset.h"
#include "Graph/Nodes/SampleHold.h"
#include "Graph/Nodes/ScaleOffset.h"
#include "Graph/Nodes/Slew.h"

namespace AbacDsp::Graph::Nodes
{

namespace Detail
{

// Named distinctly from StandardNodes.h's Detail::audioPort/param, which this
// file does not include, to avoid a same-signature redefinition if a caller
// ends up including both headers in one translation unit.
[[nodiscard]] inline PortDescriptor ctrlPort(std::string name, const PortDirection direction)
{
    return PortDescriptor{.name = std::move(name), .direction = direction, .category = PortCategory::ControlAudioRate};
}

[[nodiscard]] inline PortDescriptor ctrlAudioInPort(std::string name)
{
    return PortDescriptor{
        .name = std::move(name), .direction = PortDirection::Input, .category = PortCategory::AudioMono};
}

[[nodiscard]] inline ParameterDescriptor ctrlParam(std::string id, const float minValue, const float maxValue,
                                                   const float defaultValue)
{
    return ParameterDescriptor{.id = std::move(id),
                               .unit = "linear",
                               .minValue = minValue,
                               .maxValue = maxValue,
                               .defaultValue = defaultValue};
}

[[nodiscard]] inline float configValueOrDefault(const NodeInstance& instance, const std::string& key,
                                                const float defaultValue)
{
    const auto it = instance.config.find(key);
    return it == instance.config.end() ? defaultValue : std::strtof(it->second.c_str(), nullptr);
}

} // namespace Detail

/**
 * @ingroup graph
 * @brief Registers Macro, Constant, Add, Multiply, ScaleOffset, Clamp, Map,
 * Curve, ExpMap, PhaseOffset, SampleHold, Slew, LFO, and EnvelopeFollower
 * under chorus.md's exact type-name spelling.
 */
inline void registerControlNodes(NodeRegistry& registry)
{
    using Detail::ctrlAudioInPort;
    using Detail::ctrlParam;
    using Detail::ctrlPort;

    registry.registerType(
        "Macro", NodeSchema{{ctrlPort("out", PortDirection::Output)}, {ctrlParam("value", 0.0f, 1.0f, 0.0f)}, false},
        [](const NodeInstance&, float) { return std::make_unique<Macro>(); });

    registry.registerType(
        "Constant", NodeSchema{{ctrlPort("out", PortDirection::Output)}, {}, false},
        [](const NodeInstance& instance, float)
        { return std::make_unique<Constant>(Detail::configValueOrDefault(instance, "value", 0.0f)); });

    registry.registerType("Add",
                          NodeSchema{{ctrlPort("inA", PortDirection::Input), ctrlPort("inB", PortDirection::Input),
                                      ctrlPort("out", PortDirection::Output)},
                                     {},
                                     false},
                          [](const NodeInstance&, float) { return std::make_unique<Add>(); });

    registry.registerType("Multiply",
                          NodeSchema{{ctrlPort("inA", PortDirection::Input), ctrlPort("inB", PortDirection::Input),
                                      ctrlPort("out", PortDirection::Output)},
                                     {},
                                     false},
                          [](const NodeInstance&, float) { return std::make_unique<Multiply>(); });

    registry.registerType(
        "ScaleOffset",
        NodeSchema{{ctrlPort("in", PortDirection::Input), ctrlPort("out", PortDirection::Output)},
                   {ctrlParam("scale", -100.0f, 100.0f, 1.0f), ctrlParam("offset", -100.0f, 100.0f, 0.0f)},
                   false},
        [](const NodeInstance&, float) { return std::make_unique<ScaleOffset>(); });

    registry.registerType("Clamp",
                          NodeSchema{{ctrlPort("in", PortDirection::Input), ctrlPort("out", PortDirection::Output)},
                                     {ctrlParam("min", -100.0f, 100.0f, 0.0f), ctrlParam("max", -100.0f, 100.0f, 1.0f)},
                                     false},
                          [](const NodeInstance&, float) { return std::make_unique<Clamp>(); });

    registry.registerType(
        "Map",
        NodeSchema{{ctrlPort("in", PortDirection::Input), ctrlPort("out", PortDirection::Output)},
                   {ctrlParam("inMin", -1000.0f, 1000.0f, 0.0f), ctrlParam("inMax", -1000.0f, 1000.0f, 1.0f),
                    ctrlParam("outMin", -1000.0f, 1000.0f, 0.0f), ctrlParam("outMax", -1000.0f, 1000.0f, 1.0f)},
                   false},
        [](const NodeInstance&, float) { return std::make_unique<Map>(); });

    registry.registerType("Curve",
                          NodeSchema{{ctrlPort("in", PortDirection::Input), ctrlPort("out", PortDirection::Output)},
                                     {ctrlParam("exponent", 0.01f, 100.0f, 1.0f)},
                                     false},
                          [](const NodeInstance&, float) { return std::make_unique<Curve>(); });

    registry.registerType(
        "ExpMap",
        NodeSchema{{ctrlPort("in", PortDirection::Input), ctrlPort("out", PortDirection::Output)},
                   {ctrlParam("inMin", -1000.0f, 1000.0f, 0.0f), ctrlParam("inMax", -1000.0f, 1000.0f, 1.0f),
                    ctrlParam("outMin", 0.0001f, 100000.0f, 1.0f), ctrlParam("outMax", 0.0001f, 100000.0f, 2.0f)},
                   false},
        [](const NodeInstance&, float) { return std::make_unique<ExpMap>(); });

    registry.registerType("PhaseOffset",
                          NodeSchema{{ctrlPort("in", PortDirection::Input), ctrlPort("out", PortDirection::Output)},
                                     {ctrlParam("offset", -1.0f, 1.0f, 0.0f)},
                                     false},
                          [](const NodeInstance&, float) { return std::make_unique<PhaseOffset>(); });

    registry.registerType("SampleHold",
                          NodeSchema{{ctrlPort("in", PortDirection::Input), ctrlPort("trigger", PortDirection::Input),
                                      ctrlPort("out", PortDirection::Output)},
                                     {},
                                     false},
                          [](const NodeInstance&, float) { return std::make_unique<SampleHold>(); });

    registry.registerType("Slew",
                          NodeSchema{{ctrlPort("in", PortDirection::Input), ctrlPort("out", PortDirection::Output)},
                                     {ctrlParam("timeMs", 0.01f, 10000.0f, 20.0f)},
                                     false},
                          [](const NodeInstance&, const float sampleRate)
                          { return std::make_unique<Slew>(sampleRate); });

    registry.registerType(
        "LFO",
        NodeSchema{{ctrlPort("out", PortDirection::Output)},
                   {ctrlParam("rateHz", 0.0f, 20.0f, 1.0f), ctrlParam("phaseOffsetDegrees", -360.0f, 360.0f, 0.0f)},
                   false},
        [](const NodeInstance& instance, const float sampleRate)
        {
            const auto it = instance.config.find("waveform");
            const std::string waveform = it == instance.config.end() ? "sine" : it->second;
            return std::make_unique<Lfo>(sampleRate, lfoWaveformFromConfig(waveform));
        });

    registry.registerType(
        "EnvelopeFollower",
        NodeSchema{{ctrlAudioInPort("in"), ctrlPort("out", PortDirection::Output)},
                   {ctrlParam("attackMs", 0.01f, 5000.0f, 5.0f), ctrlParam("releaseMs", 0.01f, 5000.0f, 50.0f)},
                   false},
        [](const NodeInstance&, const float sampleRate) { return std::make_unique<EnvelopeFollower>(sampleRate); });
}

}
