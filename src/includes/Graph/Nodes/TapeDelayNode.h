#pragma once

#include <array>
#include <cassert>
#include <cstdlib>
#include <memory>
#include <span>
#include <string>

#include "Delays/OrganicChorusTransport.h"
#include "Filters/Sinc/sinc_4.h"
#include "Graph/NodeRegistry.h"
#include "Graph/NodeSchema.h"

namespace AbacDsp::Graph::Nodes
{

/**
 * @ingroup graph
 * @brief Wraps AbacDsp::OrganicChorusTransport (no new DSP) as a graph node.
 *
 * Fixed at BufferSize=8192, stereo, one read head - pathfinder v1's exact
 * configuration. Ports: inL, inR, feedbackL, feedbackR -> outL, outR.
 * Parameters 0-6: transportRatio, wowDepth, wowRate, wowVariance, wowDrift,
 * flutterDepth, flutterRate. numSamples must equal BlockSize on every call -
 * OrganicChorusTransport::feed()/readBlock() take a fixed-size tile.
 */
template <size_t BlockSize>
class TapeDelayNode final : public Node
{
  public:
    static constexpr size_t kBufferSize = 8192;
    static constexpr size_t kNumChannels = 2;
    static constexpr size_t kNumReadHeads = 1;
    using Transport = AbacDsp::OrganicChorusTransport<kBufferSize, kNumChannels, kNumReadHeads, BlockSize>;

    TapeDelayNode(const float sampleRate, const float baseDelayMs, const float safetyMarginSamples,
                  const float correctionThresholdSamples)
        : m_transport(sampleRate, std::make_shared<AbacDsp::SincFilter>(sinc4))
    {
        m_transport.setReadHeadSafetyMargin(safetyMarginSamples);
        m_transport.setReadHeadCorrectionThreshold(0, correctionThresholdSamples);
        m_transport.setReadHead(0, baseDelayMs * 0.001f * sampleRate, true);
        m_transport.setRatio(1.0f, true);
    }

    void process(const std::span<const float*> inputs, const std::span<float*> outputs,
                 const size_t numSamples) noexcept override
    {
        assert(numSamples == BlockSize);
        std::array<float, kNumChannels * BlockSize> interleavedIn{};
        for (size_t i = 0; i < BlockSize; ++i)
        {
            interleavedIn[2 * i] = inputs[0][i] + inputs[2][i];
            interleavedIn[2 * i + 1] = inputs[1][i] + inputs[3][i];
        }
        m_transport.feed(interleavedIn);

        std::array<float, kNumChannels * BlockSize> interleavedOut{};
        m_transport.readBlock(0, interleavedOut);
        for (size_t i = 0; i < BlockSize; ++i)
        {
            outputs[0][i] = interleavedOut[2 * i];
            outputs[1][i] = interleavedOut[2 * i + 1];
        }
    }

    void setParameter(const size_t paramIndex, const float value) noexcept override
    {
        switch (paramIndex)
        {
            case 0:
                m_transport.setRatio(value, false);
                break;
            case 1:
                m_transport.setWowDepth(value);
                break;
            case 2:
                m_transport.setWowRate(value);
                break;
            case 3:
                m_transport.setWowVariance(value);
                break;
            case 4:
                m_transport.setWowDrift(value);
                break;
            case 5:
                m_transport.setFlutterDepth(value);
                break;
            case 6:
                m_transport.setFlutterRate(value);
                break;
            default:
                break;
        }
    }

    void reset() noexcept override
    {
        m_transport.reset();
    }

  private:
    Transport m_transport;
};

namespace Detail
{

[[nodiscard]] inline float configOrDefault(const NodeInstance& instance, const std::string& key,
                                           const float defaultValue)
{
    const auto it = instance.config.find(key);
    return it == instance.config.end() ? defaultValue : std::strtof(it->second.c_str(), nullptr);
}

} // namespace Detail

/**
 * @ingroup graph
 * @brief Registers "TapeDelay" for one fixed BlockSize.
 *
 * Static config (NodeInstance::config, pathfinder v1's own defaults when
 * absent): baseDelayMs (8.0), safetyMarginSamples (250.0),
 * correctionThresholdSamples (190.0).
 */
template <size_t BlockSize>
void registerTapeDelayNode(NodeRegistry& registry)
{
    registry.registerType(
        "TapeDelay",
        NodeSchema{
            {PortDescriptor{.name = "inL", .direction = PortDirection::Input, .category = PortCategory::AudioMono},
             PortDescriptor{.name = "inR", .direction = PortDirection::Input, .category = PortCategory::AudioMono},
             PortDescriptor{
                 .name = "feedbackL", .direction = PortDirection::Input, .category = PortCategory::AudioMono},
             PortDescriptor{
                 .name = "feedbackR", .direction = PortDirection::Input, .category = PortCategory::AudioMono},
             PortDescriptor{.name = "outL", .direction = PortDirection::Output, .category = PortCategory::AudioMono},
             PortDescriptor{.name = "outR", .direction = PortDirection::Output, .category = PortCategory::AudioMono}},
            {ParameterDescriptor{
                 .id = "transportRatio", .unit = "ratio", .minValue = 0.001f, .maxValue = 8.0f, .defaultValue = 1.0f},
             ParameterDescriptor{
                 .id = "wowDepth", .unit = "linear", .minValue = 0.0f, .maxValue = 1.0f, .defaultValue = 0.1f},
             ParameterDescriptor{
                 .id = "wowRate", .unit = "Hz", .minValue = 0.0f, .maxValue = 10.0f, .defaultValue = 0.4f},
             ParameterDescriptor{
                 .id = "wowVariance", .unit = "linear", .minValue = 0.0f, .maxValue = 1.0f, .defaultValue = 0.1f},
             ParameterDescriptor{
                 .id = "wowDrift", .unit = "linear", .minValue = 0.0f, .maxValue = 1.0f, .defaultValue = 0.05f},
             ParameterDescriptor{
                 .id = "flutterDepth", .unit = "linear", .minValue = 0.0f, .maxValue = 1.0f, .defaultValue = 0.1f},
             ParameterDescriptor{
                 .id = "flutterRate", .unit = "Hz", .minValue = 0.0f, .maxValue = 20.0f, .defaultValue = 0.4f}},
            false},
        [](const NodeInstance& instance, const float sampleRate)
        {
            const float baseDelayMs = Detail::configOrDefault(instance, "baseDelayMs", 8.0f);
            const float safetyMarginSamples = Detail::configOrDefault(instance, "safetyMarginSamples", 250.0f);
            const float correctionThresholdSamples =
                Detail::configOrDefault(instance, "correctionThresholdSamples", 190.0f);
            return std::make_unique<TapeDelayNode<BlockSize>>(sampleRate, baseDelayMs, safetyMarginSamples,
                                                              correctionThresholdSamples);
        });
}

}
