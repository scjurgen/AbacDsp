#pragma once

#include <string>

#include "Generators/SynthLfo.h"
#include "Graph/Node.h"

namespace AbacDsp::Graph::Nodes
{

/**
 * @ingroup graph
 * @brief Wraps AbacDsp::SynthLfo (speed-dependent onset smoothing already
 * built in). 0 in, 1 out (per-sample, audio-rate).
 *
 * Parameters 0: rateHz, 1: phaseOffsetDegrees. Config "waveform" (static,
 * since a waveform choice is not representable as a float parameter):
 * "sine" (default), "triangle", "saw", "square", "noise".
 */
class Lfo final : public Node
{
  public:
    Lfo(const float sampleRate, const AbacDsp::LfoType waveform) noexcept
        : m_lfo(sampleRate)
    {
        m_lfo.setWaveForm(waveform);
    }

    void process(const std::span<const float*>, const std::span<float*> outputs,
                 const size_t numSamples) noexcept override
    {
        for (size_t i = 0; i < numSamples; ++i)
        {
            outputs[0][i] = m_lfo.getValue();
        }
    }

    void setParameter(const size_t paramIndex, const float value) noexcept override
    {
        if (paramIndex == 0)
        {
            m_lfo.setSpeed(value);
        }
        else if (paramIndex == 1)
        {
            m_lfo.setPhase(value);
        }
    }

  private:
    AbacDsp::SynthLfo m_lfo;
};

/// @brief Parses Lfo's "waveform" config string; unrecognized names fall back to Sine.
[[nodiscard]] inline AbacDsp::LfoType lfoWaveformFromConfig(const std::string& name)
{
    if (name == "triangle")
    {
        return AbacDsp::LfoType::Triangle;
    }
    if (name == "saw")
    {
        return AbacDsp::LfoType::Saw;
    }
    if (name == "square")
    {
        return AbacDsp::LfoType::Square;
    }
    if (name == "noise")
    {
        return AbacDsp::LfoType::Noise;
    }
    return AbacDsp::LfoType::Sine;
}

}
