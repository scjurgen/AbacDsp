#pragma once

#include <string>
#include <variant>

#include "Filters/Biquad.h"
#include "Graph/Node.h"

namespace AbacDsp::Graph::Nodes
{

enum class BiquadMode
{
    LowPass,
    HighPass,
    Notch,
    Peak
};

/**
 * @ingroup graph
 * @brief Runtime-selectable-at-construction Biquad mode. Ports: in -> out.
 * Parameters 0: frequencyHz, 1: Q, 2: peakGainDb (peakGainDb is inert outside
 * Peak mode). Config "mode": "lowpass" (default), "highpass", "notch", "peak".
 *
 * Mode is fixed once, at construction, into one alternative of a
 * std::variant<AbacDsp::Biquad<LowPass|HighPass|Notch|Peak>> - the underlying
 * C++ type is compile-time fixed either way, so a runtime-mutable mode was
 * never possible; LoShelf/HiShelf/BandPass are separate node types instead.
 */
class Biquad final : public Node
{
  public:
    Biquad(const float sampleRate, const BiquadMode mode) noexcept
        : m_sampleRate(sampleRate)
    {
        switch (mode)
        {
            case BiquadMode::LowPass:
                m_filter.emplace<AbacDsp::Biquad<AbacDsp::BiquadFilterType::LowPass>>();
                break;
            case BiquadMode::HighPass:
                m_filter.emplace<AbacDsp::Biquad<AbacDsp::BiquadFilterType::HighPass>>();
                break;
            case BiquadMode::Notch:
                m_filter.emplace<AbacDsp::Biquad<AbacDsp::BiquadFilterType::Notch>>();
                break;
            case BiquadMode::Peak:
                m_filter.emplace<AbacDsp::Biquad<AbacDsp::BiquadFilterType::Peak>>();
                break;
        }
        updateCoefficients();
    }

    void process(const std::span<const float*> inputs, const std::span<float*> outputs,
                 const size_t numSamples) noexcept override
    {
        std::visit([&](auto& filter) { filter.processBlock(inputs[0], outputs[0], numSamples); }, m_filter);
    }

    void setParameter(const size_t paramIndex, const float value) noexcept override
    {
        switch (paramIndex)
        {
            case 0:
                m_frequencyHz = value;
                break;
            case 1:
                m_q = value;
                break;
            case 2:
                m_peakGainDb = value;
                break;
            default:
                return;
        }
        updateCoefficients();
    }

  private:
    void updateCoefficients() noexcept
    {
        std::visit([this](auto& filter) { filter.computeCoefficients(m_sampleRate, m_frequencyHz, m_q, m_peakGainDb); },
                   m_filter);
    }

    float m_sampleRate;
    float m_frequencyHz{1000.0f};
    float m_q{0.70710678f};
    float m_peakGainDb{0.0f};
    std::variant<AbacDsp::Biquad<AbacDsp::BiquadFilterType::LowPass>,
                 AbacDsp::Biquad<AbacDsp::BiquadFilterType::HighPass>,
                 AbacDsp::Biquad<AbacDsp::BiquadFilterType::Notch>, AbacDsp::Biquad<AbacDsp::BiquadFilterType::Peak>>
        m_filter;
};

/// @brief Parses Biquad's "mode" config string; unrecognized names fall back to LowPass.
[[nodiscard]] inline BiquadMode biquadModeFromConfig(const std::string& name)
{
    if (name == "highpass")
    {
        return BiquadMode::HighPass;
    }
    if (name == "notch")
    {
        return BiquadMode::Notch;
    }
    if (name == "peak")
    {
        return BiquadMode::Peak;
    }
    return BiquadMode::LowPass;
}

}
