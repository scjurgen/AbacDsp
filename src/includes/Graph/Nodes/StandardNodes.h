#pragma once

#include <memory>
#include <utility>

#include "Graph/NodeRegistry.h"
#include "Graph/NodeSchema.h"
#include "Graph/Nodes/Gain.h"
#include "Graph/Nodes/Matrix.h"
#include "Graph/Nodes/Mixer.h"
#include "Graph/Nodes/MonoToStereo.h"
#include "Graph/Nodes/MsDecode.h"
#include "Graph/Nodes/MsEncode.h"
#include "Graph/Nodes/Split.h"
#include "Graph/Nodes/StereoToMono.h"
#include "Graph/Nodes/Width.h"

namespace AbacDsp::Graph::Nodes
{

namespace Detail
{

[[nodiscard]] inline PortDescriptor audioPort(std::string name, const PortDirection direction)
{
    return PortDescriptor{.name = std::move(name), .direction = direction, .category = PortCategory::AudioMono};
}

[[nodiscard]] inline ParameterDescriptor param(std::string id, std::string unit, const float minValue,
                                               const float maxValue, const float defaultValue)
{
    return ParameterDescriptor{.id = std::move(id),
                               .unit = std::move(unit),
                               .minValue = minValue,
                               .maxValue = maxValue,
                               .defaultValue = defaultValue};
}

} // namespace Detail

/**
 * @ingroup graph
 * @brief Registers Gain, Mixer, Matrix, Split, StereoToMono, MonoToStereo,
 * MS_Encode, MS_Decode, and Width under chorus.md's exact type-name spelling.
 */
inline void registerStandardNodes(NodeRegistry& registry)
{
    using Detail::audioPort;
    using Detail::param;

    registry.registerType(
        "Gain",
        NodeSchema{{audioPort("inL", PortDirection::Input), audioPort("inR", PortDirection::Input),
                    audioPort("outL", PortDirection::Output), audioPort("outR", PortDirection::Output)},
                   {param("gainDb", "dB", -60.0f, 24.0f, 0.0f)},
                   false},
        [](const NodeInstance&, float) { return std::make_unique<Gain>(); });

    registry.registerType(
        "Mixer",
        NodeSchema{{audioPort("in1L", PortDirection::Input), audioPort("in1R", PortDirection::Input),
                    audioPort("in2L", PortDirection::Input), audioPort("in2R", PortDirection::Input),
                    audioPort("outL", PortDirection::Output), audioPort("outR", PortDirection::Output)},
                   {},
                   false},
        [](const NodeInstance&, float) { return std::make_unique<Mixer>(); });

    registry.registerType(
        "Matrix",
        NodeSchema{{audioPort("inL", PortDirection::Input), audioPort("inR", PortDirection::Input),
                    audioPort("outL", PortDirection::Output), audioPort("outR", PortDirection::Output)},
                   {param("gainLL", "linear", -2.0f, 2.0f, 1.0f), param("gainLR", "linear", -2.0f, 2.0f, 0.0f),
                    param("gainRL", "linear", -2.0f, 2.0f, 0.0f), param("gainRR", "linear", -2.0f, 2.0f, 1.0f)},
                   false},
        [](const NodeInstance&, float) { return std::make_unique<Matrix>(); });

    registry.registerType(
        "Split",
        NodeSchema{{audioPort("inL", PortDirection::Input), audioPort("inR", PortDirection::Input),
                    audioPort("out1L", PortDirection::Output), audioPort("out1R", PortDirection::Output),
                    audioPort("out2L", PortDirection::Output), audioPort("out2R", PortDirection::Output)},
                   {},
                   false},
        [](const NodeInstance&, float) { return std::make_unique<Split>(); });

    registry.registerType(
        "StereoToMono",
        NodeSchema{{audioPort("inL", PortDirection::Input), audioPort("inR", PortDirection::Input),
                    audioPort("out", PortDirection::Output)},
                   {param("gainL", "linear", 0.0f, 1.0f, 0.5f), param("gainR", "linear", 0.0f, 1.0f, 0.5f)},
                   false},
        [](const NodeInstance&, float) { return std::make_unique<StereoToMono>(); });

    registry.registerType("MonoToStereo",
                          NodeSchema{{audioPort("in", PortDirection::Input), audioPort("outL", PortDirection::Output),
                                      audioPort("outR", PortDirection::Output)},
                                     {},
                                     false},
                          [](const NodeInstance&, float) { return std::make_unique<MonoToStereo>(); });

    registry.registerType(
        "MS_Encode",
        NodeSchema{{audioPort("inL", PortDirection::Input), audioPort("inR", PortDirection::Input),
                    audioPort("outM", PortDirection::Output), audioPort("outS", PortDirection::Output)},
                   {},
                   false},
        [](const NodeInstance&, float) { return std::make_unique<MsEncode>(); });

    registry.registerType(
        "MS_Decode",
        NodeSchema{{audioPort("inM", PortDirection::Input), audioPort("inS", PortDirection::Input),
                    audioPort("outL", PortDirection::Output), audioPort("outR", PortDirection::Output)},
                   {},
                   false},
        [](const NodeInstance&, float) { return std::make_unique<MsDecode>(); });

    registry.registerType(
        "Width",
        NodeSchema{{audioPort("inL", PortDirection::Input), audioPort("inR", PortDirection::Input),
                    audioPort("outL", PortDirection::Output), audioPort("outR", PortDirection::Output)},
                   {param("width", "linear", 0.0f, 2.0f, 1.0f)},
                   false},
        [](const NodeInstance&, float) { return std::make_unique<Width>(); });
}

}
