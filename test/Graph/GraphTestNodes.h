#pragma once

#include <algorithm>

#include "Graph/Node.h"
#include "Graph/NodeRegistry.h"
#include "Graph/NodeSchema.h"

namespace AbacDsp::Graph::Test
{

/**
 * @brief 1 in -> 1 out, copies its input straight through. No parameters.
 */
class PassThroughStubNode final : public Node
{
  public:
    void process(const std::span<const float*> inputs, const std::span<float*> outputs,
                 const size_t numSamples) noexcept override
    {
        std::copy_n(inputs[0], numSamples, outputs[0]);
    }
};

/**
 * @brief breaksCycle stand-in. Behaves like PassThroughStubNode: inside a
 * feedback loop, GraphCompiler schedules it before its own feedback
 * producer, so it reads that producer's previous-block buffer content - the
 * one-block delay is a property of the schedule, not of this node's code.
 */
class CycleBreakerStubNode final : public Node
{
  public:
    void process(const std::span<const float*> inputs, const std::span<float*> outputs,
                 const size_t numSamples) noexcept override
    {
        std::copy_n(inputs[0], numSamples, outputs[0]);
    }
};

/**
 * @brief 1 in -> 1 out, scaled by an automatable "gain" parameter (index 0).
 */
class GainStubNode final : public Node
{
  public:
    void process(const std::span<const float*> inputs, const std::span<float*> outputs,
                 const size_t numSamples) noexcept override
    {
        for (size_t i = 0; i < numSamples; ++i)
        {
            outputs[0][i] = inputs[0][i] * m_gain;
        }
    }

    void setParameter(const size_t paramIndex, const float value) noexcept override
    {
        if (paramIndex == 0)
        {
            m_gain = value;
        }
    }

  private:
    float m_gain{1.0f};
};

/**
 * @brief 2 fixed inputs -> 1 out, sample-wise sum.
 */
class SumStubNode final : public Node
{
  public:
    void process(const std::span<const float*> inputs, const std::span<float*> outputs,
                 const size_t numSamples) noexcept override
    {
        for (size_t i = 0; i < numSamples; ++i)
        {
            outputs[0][i] = inputs[0][i] + inputs[1][i];
        }
    }
};

[[nodiscard]] inline PortDescriptor audioInPort(std::string name)
{
    return PortDescriptor{std::move(name), PortDirection::Input, PortCategory::AudioMono, false};
}

[[nodiscard]] inline PortDescriptor audioOutPort(std::string name)
{
    return PortDescriptor{std::move(name), PortDirection::Output, PortCategory::AudioMono, false};
}

inline void registerTestNodes(NodeRegistry& registry)
{
    registry.registerType("PassThroughStub", NodeSchema{{audioInPort("in"), audioOutPort("out")}, {}, false},
                          [](const NodeInstance&) { return std::make_unique<PassThroughStubNode>(); });

    registry.registerType("CycleBreakerStub", NodeSchema{{audioInPort("in"), audioOutPort("out")}, {}, true},
                          [](const NodeInstance&) { return std::make_unique<CycleBreakerStubNode>(); });

    registry.registerType(
        "GainStub",
        NodeSchema{{audioInPort("in"), audioOutPort("out")},
                   {ParameterDescriptor{"gain", "linear", 0.0f, 4.0f, 1.0f, ParameterMapping::Linear, 0.0f, true}},
                   false},
        [](const NodeInstance&) { return std::make_unique<GainStubNode>(); });

    registry.registerType("SumStub",
                          NodeSchema{{audioInPort("in1"), audioInPort("in2"), audioOutPort("out")}, {}, false},
                          [](const NodeInstance&) { return std::make_unique<SumStubNode>(); });
}

}
