#pragma once

#include "Filters/Biquad.h"
#include "Graph/Node.h"

namespace AbacDsp::Graph::Nodes
{

/**
 * @ingroup graph
 * @brief One Biquad<LoShelf> + one Biquad<HiShelf> in series, sharing one
 * tiltDb split evenly between them. Ports: in -> out.
 * Parameters 0: tiltDb (default 0, flat), 1: pivotHz (default 1000).
 *
 * Positive tilt cuts low, boosts high; negative does the reverse.
 */
class TiltEQ final : public Node
{
  public:
    explicit TiltEQ(const float sampleRate) noexcept
        : m_sampleRate(sampleRate)
    {
        updateCoefficients();
    }

    void process(const std::span<const float*> inputs, const std::span<float*> outputs,
                 const size_t numSamples) noexcept override
    {
        m_loShelf.processBlock(inputs[0], outputs[0], numSamples);
        m_hiShelf.processBlock(outputs[0], outputs[0], numSamples);
    }

    void setParameter(const size_t paramIndex, const float value) noexcept override
    {
        if (paramIndex == 0)
        {
            m_tiltDb = value;
        }
        else if (paramIndex == 1)
        {
            m_pivotHz = value;
        }
        else
        {
            return;
        }
        updateCoefficients();
    }

  private:
    static constexpr float kQ{0.70710678f};

    void updateCoefficients() noexcept
    {
        m_loShelf.computeCoefficients(m_sampleRate, m_pivotHz, kQ, -m_tiltDb * 0.5f);
        m_hiShelf.computeCoefficients(m_sampleRate, m_pivotHz, kQ, m_tiltDb * 0.5f);
    }

    float m_sampleRate;
    float m_tiltDb{0.0f};
    float m_pivotHz{1000.0f};
    AbacDsp::Biquad<AbacDsp::BiquadFilterType::LoShelf> m_loShelf;
    AbacDsp::Biquad<AbacDsp::BiquadFilterType::HiShelf> m_hiShelf;
};

}
