#pragma once

#include "Analysis/EnvelopeFollower.h"
#include "Graph/Node.h"

namespace AbacDsp::Graph::Nodes
{

/**
 * @ingroup graph
 * @brief Wraps AbacDsp::PeakEnvelopeFollower<60> (60 dB range, matching
 * Dynamics/Compressor.h's own control-purpose envelope, not the metering
 * examples' 100 dB). Ports: in (audio) -> out (control, audio-rate).
 * Parameters 0: attackMs, 1: releaseMs.
 */
class EnvelopeFollower final : public Node
{
  public:
    explicit EnvelopeFollower(const float sampleRate) noexcept
        : m_follower(sampleRate)
    {
        m_follower.setAttackInMsecs(kDefaultAttackMs);
        m_follower.setReleaseInMsecs(kDefaultReleaseMs);
    }

    void process(const std::span<const float*> inputs, const std::span<float*> outputs,
                 const size_t numSamples) noexcept override
    {
        m_follower.blockAnalyze(std::span<const float>(inputs[0], numSamples),
                                std::span<float>(outputs[0], numSamples));
    }

    void setParameter(const size_t paramIndex, const float value) noexcept override
    {
        if (paramIndex == 0)
        {
            m_follower.setAttackInMsecs(value);
        }
        else if (paramIndex == 1)
        {
            m_follower.setReleaseInMsecs(value);
        }
    }

  private:
    static constexpr float kDefaultAttackMs{5.0f};
    static constexpr float kDefaultReleaseMs{50.0f};
    AbacDsp::PeakEnvelopeFollower<60> m_follower;
};

}
