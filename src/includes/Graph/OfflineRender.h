#pragma once

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <numbers>
#include <random>
#include <vector>

#include "Graph/CompiledGraph.h"

namespace AbacDsp::Graph
{

enum class Stimulus
{
    Impulse,
    Sine,
    LogSweep,
    WhiteNoise
};

struct StimulusSpec
{
    Stimulus kind{Stimulus::Impulse};
    float amplitude{1.f};
    /// Sine: the tone. LogSweep: the start frequency.
    float frequencyHz{1000.f};
    float endFrequencyHz{20000.f};
    /// White noise only; the same seed always yields the same samples.
    std::uint32_t seed{1};
};

/**
 * @ingroup graph
 * @brief Deterministic test signals and a block-wise offline driver for a CompiledGraph.
 *
 * Non-realtime. Stimuli are impulse, sine, exponential sweep and white noise; the noise
 * comes straight from std::mt19937 output, so it is identical on every platform.
 * render() feeds whole input vectors through the graph in blocks of any size up to the
 * graph's own maxBlockSize, the last block being short when the length does not divide.
 */
class OfflineRender
{
  public:
    [[nodiscard]] static std::vector<float> stimulus(const StimulusSpec& spec, const size_t numSamples,
                                                     const float sampleRate)
    {
        std::vector<float> signal(numSamples, 0.f);
        switch (spec.kind)
        {
            case Stimulus::Impulse:
                fillImpulse(signal, spec);
                break;
            case Stimulus::Sine:
                fillSine(signal, spec, sampleRate);
                break;
            case Stimulus::LogSweep:
                fillLogSweep(signal, spec, sampleRate);
                break;
            case Stimulus::WhiteNoise:
                fillWhiteNoise(signal, spec);
                break;
        }
        return signal;
    }

    /// Returns one vector of numSamples per graph output. Every input needs at least numSamples.
    [[nodiscard]] static std::vector<std::vector<float>> render(CompiledGraph& graph,
                                                                const std::vector<std::vector<float>>& inputs,
                                                                const size_t numSamples, const size_t blockSize)
    {
        assert(inputs.size() == graph.graphInputCount());
        assert(blockSize > 0);
        assert(std::ranges::all_of(inputs, [numSamples](const auto& input) { return input.size() >= numSamples; }));

        std::vector<std::vector<float>> outputs(graph.graphOutputCount(), std::vector<float>(numSamples, 0.f));
        std::vector<const float*> inputPointers(inputs.size(), nullptr);
        std::vector<float*> outputPointers(outputs.size(), nullptr);
        for (size_t offset = 0; offset < numSamples; offset += blockSize)
        {
            for (size_t i = 0; i < inputs.size(); ++i)
            {
                inputPointers[i] = inputs[i].data() + offset;
            }
            for (size_t i = 0; i < outputs.size(); ++i)
            {
                outputPointers[i] = outputs[i].data() + offset;
            }
            graph.process(inputPointers, outputPointers, std::min(blockSize, numSamples - offset));
        }
        return outputs;
    }

  private:
    static void fillImpulse(std::vector<float>& signal, const StimulusSpec& spec)
    {
        if (!signal.empty())
        {
            signal.front() = spec.amplitude;
        }
    }

    static void fillSine(std::vector<float>& signal, const StimulusSpec& spec, const float sampleRate)
    {
        const double step = 2.0 * std::numbers::pi * static_cast<double>(spec.frequencyHz) / sampleRate;
        for (size_t i = 0; i < signal.size(); ++i)
        {
            signal[i] = spec.amplitude * static_cast<float>(std::sin(step * static_cast<double>(i)));
        }
    }

    // Phase of an exponential sweep: 2*pi*f1*T/ln(k) * (k^(t/T) - 1), k = f2/f1.
    static void fillLogSweep(std::vector<float>& signal, const StimulusSpec& spec, const float sampleRate)
    {
        const double duration = static_cast<double>(signal.size()) / sampleRate;
        const double ratio = static_cast<double>(spec.endFrequencyHz) / spec.frequencyHz;
        const double scale = 2.0 * std::numbers::pi * spec.frequencyHz * duration / std::log(ratio);
        for (size_t i = 0; i < signal.size(); ++i)
        {
            const double t = static_cast<double>(i) / sampleRate;
            const double phase = scale * (std::pow(ratio, t / duration) - 1.0);
            signal[i] = spec.amplitude * static_cast<float>(std::sin(phase));
        }
    }

    static void fillWhiteNoise(std::vector<float>& signal, const StimulusSpec& spec)
    {
        constexpr float kHalfRange{8388608.f};
        std::mt19937 generator{spec.seed};
        for (auto& sample : signal)
        {
            sample = spec.amplitude * (static_cast<float>(generator() >> 8) / kHalfRange - 1.f);
        }
    }
};

}
