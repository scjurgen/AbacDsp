#pragma once

#include <memory>
#include <string>
#include <utility>

#include "Graph/NodeRegistry.h"
#include "Graph/NodeSchema.h"
#include "Graph/Nodes/BandPass.h"
#include "Graph/Nodes/Biquad.h"
#include "Graph/Nodes/Compander.h"
#include "Graph/Nodes/CrossoverLR4.h"
#include "Graph/Nodes/DcBlocker.h"
#include "Graph/Nodes/OnePoleHp.h"
#include "Graph/Nodes/OnePoleLp.h"
#include "Graph/Nodes/Saturator.h"
#include "Graph/Nodes/ShelfEQ.h"
#include "Graph/Nodes/TiltEQ.h"

namespace AbacDsp::Graph::Nodes
{

namespace Detail
{

// Named distinctly from StandardNodes.h's and ControlNodes.h's own Detail
// helpers, which this file does not include, to avoid a same-signature
// redefinition if a caller ends up including more than one in one TU.
[[nodiscard]] inline PortDescriptor filterPort(std::string name, const PortDirection direction)
{
    return PortDescriptor{.name = std::move(name), .direction = direction, .category = PortCategory::AudioMono};
}

[[nodiscard]] inline ParameterDescriptor filterParam(std::string id, const float minValue, const float maxValue,
                                                     const float defaultValue)
{
    return ParameterDescriptor{.id = std::move(id),
                               .unit = "linear",
                               .minValue = minValue,
                               .maxValue = maxValue,
                               .defaultValue = defaultValue};
}

} // namespace Detail

/**
 * @ingroup graph
 * @brief Registers OnePoleLP, OnePoleHP, Biquad, BandPass, ShelfEQ, TiltEQ,
 * CrossoverLR4, Saturator, Compander, and DCBlocker under chorus.md's exact
 * type-name spelling.
 */
inline void registerFilterNodes(NodeRegistry& registry)
{
    using Detail::filterParam;
    using Detail::filterPort;

    registry.registerType("OnePoleLP",
                          NodeSchema{{filterPort("in", PortDirection::Input), filterPort("out", PortDirection::Output)},
                                     {filterParam("cutoffHz", 20.0f, 20000.0f, 1000.0f)},
                                     false},
                          [](const NodeInstance&, const float sampleRate)
                          { return std::make_unique<OnePoleLp>(sampleRate); });

    registry.registerType("OnePoleHP",
                          NodeSchema{{filterPort("in", PortDirection::Input), filterPort("out", PortDirection::Output)},
                                     {filterParam("cutoffHz", 20.0f, 20000.0f, 1000.0f)},
                                     false},
                          [](const NodeInstance&, const float sampleRate)
                          { return std::make_unique<OnePoleHp>(sampleRate); });

    registry.registerType(
        "Biquad",
        NodeSchema{{filterPort("in", PortDirection::Input), filterPort("out", PortDirection::Output)},
                   {filterParam("frequencyHz", 20.0f, 20000.0f, 1000.0f), filterParam("Q", 0.1f, 20.0f, 0.70710678f),
                    filterParam("peakGainDb", -24.0f, 24.0f, 0.0f)},
                   false},
        [](const NodeInstance& instance, const float sampleRate)
        {
            const auto it = instance.config.find("mode");
            const std::string mode = it == instance.config.end() ? "lowpass" : it->second;
            return std::make_unique<Biquad>(sampleRate, biquadModeFromConfig(mode));
        });

    registry.registerType(
        "BandPass",
        NodeSchema{{filterPort("in", PortDirection::Input), filterPort("out", PortDirection::Output)},
                   {filterParam("frequencyHz", 20.0f, 20000.0f, 1000.0f), filterParam("Q", 0.1f, 20.0f, 0.70710678f)},
                   false},
        [](const NodeInstance&, const float sampleRate) { return std::make_unique<BandPass>(sampleRate); });

    registry.registerType(
        "ShelfEQ",
        NodeSchema{{filterPort("in", PortDirection::Input), filterPort("out", PortDirection::Output)},
                   {filterParam("frequencyHz", 20.0f, 20000.0f, 4000.0f), filterParam("gainDb", -24.0f, 24.0f, 0.0f),
                    filterParam("Q", 0.1f, 20.0f, 0.70710678f)},
                   false},
        [](const NodeInstance&, const float sampleRate) { return std::make_unique<ShelfEQ>(sampleRate); });

    registry.registerType(
        "TiltEQ",
        NodeSchema{{filterPort("in", PortDirection::Input), filterPort("out", PortDirection::Output)},
                   {filterParam("tiltDb", -24.0f, 24.0f, 0.0f), filterParam("pivotHz", 20.0f, 20000.0f, 1000.0f)},
                   false},
        [](const NodeInstance&, const float sampleRate) { return std::make_unique<TiltEQ>(sampleRate); });

    registry.registerType(
        "CrossoverLR4",
        NodeSchema{{filterPort("in", PortDirection::Input), filterPort("lowOut", PortDirection::Output),
                    filterPort("highOut", PortDirection::Output)},
                   {filterParam("frequencyHz", 20.0f, 20000.0f, 1000.0f)},
                   false},
        [](const NodeInstance&, const float sampleRate) { return std::make_unique<CrossoverLR4>(sampleRate); });

    registry.registerType("Saturator",
                          NodeSchema{{filterPort("in", PortDirection::Input), filterPort("out", PortDirection::Output)},
                                     {filterParam("drive", 0.0f, 20.0f, 0.0f)},
                                     false},
                          [](const NodeInstance&, float) { return std::make_unique<Saturator>(); });

    registry.registerType(
        "Compander",
        NodeSchema{{filterPort("in", PortDirection::Input), filterPort("out", PortDirection::Output)},
                   {filterParam("thresholdDb", -60.0f, 0.0f, 0.0f), filterParam("ratio", 1.0f, 20.0f, 1.0f),
                    filterParam("attackMs", 0.01f, 1000.0f, 10.0f), filterParam("releaseMs", 0.01f, 2000.0f, 100.0f)},
                   false},
        [](const NodeInstance&, const float sampleRate) { return std::make_unique<Compander>(sampleRate); });

    registry.registerType(
        "DCBlocker",
        NodeSchema{{filterPort("in", PortDirection::Input), filterPort("out", PortDirection::Output)}, {}, false},
        [](const NodeInstance&, const float sampleRate) { return std::make_unique<DcBlocker>(sampleRate); });
}

}
