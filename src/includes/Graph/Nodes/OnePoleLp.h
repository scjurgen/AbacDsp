#pragma once

#include "Filters/OnePoleFilter.h"
#include "Graph/Node.h"

namespace AbacDsp::Graph::Nodes
{

/**
 * @ingroup graph
 * @brief Wraps AbacDsp::OnePoleFilter<LowPass>. Ports: in -> out.
 * Parameter 0: cutoffHz (default 1000).
 */
class OnePoleLp final : public Node
{
  public:
    explicit OnePoleLp(const float sampleRate) noexcept
        : m_filter(sampleRate)
    {
    }

    void process(const std::span<const float*> inputs, const std::span<float*> outputs,
                 const size_t numSamples) noexcept override
    {
        for (size_t i = 0; i < numSamples; ++i)
        {
            outputs[0][i] = m_filter.step(inputs[0][i]);
        }
    }

    void setParameter(const size_t paramIndex, const float value) noexcept override
    {
        if (paramIndex == 0)
        {
            m_filter.setCutoff(value);
        }
    }

  private:
    AbacDsp::OnePoleFilter<AbacDsp::OnePoleFilterCharacteristic::LowPass> m_filter;
};

}
