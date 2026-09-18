#pragma once

#include "Dynamics/Compressor.h"
#include "Graph/Node.h"

namespace AbacDsp::Graph::Nodes
{

/**
 * @ingroup graph
 * @brief Wraps AbacDsp::Compressor - a single compression stage (not a
 * matched compress/expand pair). Ports: in -> out.
 * Parameters 0: thresholdDb, 1: ratio, 2: attackMs, 3: releaseMs.
 */
class Compander final : public Node
{
  public:
    explicit Compander(const float sampleRate) noexcept
        : m_compressor(sampleRate)
    {
    }

    void process(const std::span<const float*> inputs, const std::span<float*> outputs,
                 const size_t numSamples) noexcept override
    {
        for (size_t i = 0; i < numSamples; ++i)
        {
            outputs[0][i] = m_compressor.step(inputs[0][i]);
        }
    }

    void setParameter(const size_t paramIndex, const float value) noexcept override
    {
        switch (paramIndex)
        {
            case 0:
                m_compressor.setThresholdDb(value);
                break;
            case 1:
                m_compressor.setRatio(value);
                break;
            case 2:
                m_compressor.setAttackMs(value);
                break;
            case 3:
                m_compressor.setReleaseMs(value);
                break;
            default:
                break;
        }
    }

  private:
    AbacDsp::Compressor m_compressor;
};

}
