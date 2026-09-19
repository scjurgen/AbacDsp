#pragma once

#include <algorithm>
#include <array>
#include <cassert>
#include <cstdlib>
#include <memory>
#include <random>
#include <span>
#include <string>

#include "Delays/WobbleDelay.h"
#include "Graph/NodeRegistry.h"
#include "Graph/NodeSchema.h"
#include "Helpers/ConstructArray.h"
#include "SamplerateConverter/UpDownSampler.h"

namespace AbacDsp::Graph::Nodes
{

/**
 * @ingroup graph
 * @brief Wraps one AbacDsp::WobbleDelay inside an UpDownSampler per channel (no new DSP).
 *
 * Fixed at BufferSize=8192, stereo, one read head. Both channels are identically seeded
 * and run at the same ratio, so their wow and flutter stay coherent. Ports: inL, inR,
 * feedbackL, feedbackR -> outL, outR. Parameters 0-6: transportRatio, wowDepth, wowRate,
 * wowVariance, wowDrift, flutterDepth, flutterRate. numSamples must equal BlockSize.
 */
template <size_t BlockSize>
class TapeDelayNode final : public Node
{
  public:
    static constexpr size_t kBufferSize = 8192;
    static constexpr size_t kNumChannels = 2;
    static constexpr std::mt19937::result_type kSharedSeed{1};
    using Delay = AbacDsp::WobbleDelay<kBufferSize, BlockSize>;
    using Transport = AbacDsp::UpDownSampler<Delay, BlockSize>;

    TapeDelayNode(const float sampleRate, const float baseDelayMs, const float safetyMarginSamples)
        : m_transports(AbacDsp::constructArray<Transport, kNumChannels>(BlockSize, sampleRate))
    {
        forEachDelay(
            [&](Delay& delay)
            {
                delay.seed(kSharedSeed);
                delay.setSafetyMargin(safetyMarginSamples);
                delay.setDelay(baseDelayMs * 0.001f * sampleRate, true);
            });
    }

    void process(const std::span<const float*> inputs, const std::span<float*> outputs,
                 const size_t numSamples) noexcept override
    {
        assert(numSamples == BlockSize);
        for (size_t channel = 0; channel < kNumChannels; ++channel)
        {
            std::array<float, BlockSize> summed{};
            for (size_t i = 0; i < BlockSize; ++i)
            {
                summed[i] = inputs[channel][i] + inputs[channel + kNumChannels][i];
            }
            m_transports[channel].processBlock(summed, std::span<float>{outputs[channel], BlockSize});
        }
    }

    void setParameter(const size_t paramIndex, const float value) noexcept override
    {
        switch (paramIndex)
        {
            case 0:
                for (auto& transport : m_transports)
                {
                    transport.setRatio(std::clamp(value, Transport::kMinRatio, Transport::kMaxRatio));
                }
                break;
            case 1:
                forEachDelay([value](Delay& delay) { delay.setWowDepth(value); });
                break;
            case 2:
                forEachDelay([value](Delay& delay) { delay.setWowRate(value); });
                break;
            case 3:
                forEachDelay([value](Delay& delay) { delay.setWowVariance(value); });
                break;
            case 4:
                forEachDelay([value](Delay& delay) { delay.setWowDrift(value); });
                break;
            case 5:
                forEachDelay([value](Delay& delay) { delay.setFlutterDepth(value); });
                break;
            case 6:
                forEachDelay([value](Delay& delay) { delay.setFlutterRate(value); });
                break;
            default:
                break;
        }
    }

    void reset() noexcept override
    {
        for (auto& transport : m_transports)
        {
            transport.reset();
            transport.processor().reset();
        }
    }

  private:
    template <typename Fn>
    void forEachDelay(Fn&& fn)
    {
        for (auto& transport : m_transports)
        {
            fn(transport.processor());
        }
    }

    std::array<Transport, kNumChannels> m_transports;
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
 * absent): baseDelayMs (8.0), safetyMarginSamples (250.0).
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
                 .id = "transportRatio",
                 .unit = "ratio",
                 .minValue = TapeDelayNode<BlockSize>::Transport::kMinRatio,
                 .maxValue = 8.0f,
                 .defaultValue = 1.0f},
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
            return std::make_unique<TapeDelayNode<BlockSize>>(sampleRate, baseDelayMs, safetyMarginSamples);
        });
}

}
