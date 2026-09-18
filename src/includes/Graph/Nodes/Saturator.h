#pragma once

#include "Filters/Distortion.h"
#include "Graph/Node.h"

namespace AbacDsp::Graph::Nodes
{

/**
 * @ingroup graph
 * @brief Wraps AbacDsp::AtanhDrive - a memoryless tanh-family waveshaper
 * (aliases; no band-limiting). Ports: in -> out. Parameter 0: drive (default 0).
 */
class Saturator final : public Node
{
  public:
    void process(const std::span<const float*> inputs, const std::span<float*> outputs,
                 const size_t numSamples) noexcept override
    {
        m_drive.processBlock(inputs[0], outputs[0], numSamples);
    }

    void setParameter(const size_t paramIndex, const float value) noexcept override
    {
        if (paramIndex == 0)
        {
            m_drive.setDrive(value);
        }
    }

  private:
    AbacDsp::AtanhDrive m_drive;
};

}
