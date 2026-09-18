#pragma once

#include "Filters/Biquad.h"
#include "Graph/Node.h"

namespace AbacDsp::Graph::Nodes
{

/**
 * @ingroup graph
 * @brief Wraps AbacDsp::Biquad<HiShelf> - high shelf only. Ports: in -> out.
 * Parameters 0: frequencyHz (default 4000), 1: gainDb (default 0), 2: Q (default 0.7071).
 */
class ShelfEQ final : public Node
{
  public:
    explicit ShelfEQ(const float sampleRate) noexcept
        : m_sampleRate(sampleRate)
    {
        updateCoefficients();
    }

    void process(const std::span<const float*> inputs, const std::span<float*> outputs,
                 const size_t numSamples) noexcept override
    {
        m_filter.processBlock(inputs[0], outputs[0], numSamples);
    }

    void setParameter(const size_t paramIndex, const float value) noexcept override
    {
        switch (paramIndex)
        {
            case 0:
                m_frequencyHz = value;
                break;
            case 1:
                m_gainDb = value;
                break;
            case 2:
                m_q = value;
                break;
            default:
                return;
        }
        updateCoefficients();
    }

  private:
    void updateCoefficients() noexcept
    {
        m_filter.computeCoefficients(m_sampleRate, m_frequencyHz, m_q, m_gainDb);
    }

    float m_sampleRate;
    float m_frequencyHz{4000.0f};
    float m_gainDb{0.0f};
    float m_q{0.70710678f};
    AbacDsp::Biquad<AbacDsp::BiquadFilterType::HiShelf> m_filter;
};

}
