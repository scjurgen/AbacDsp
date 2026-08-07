#pragma once

#include <algorithm>
#include <array>
#include <cstddef>

#include "Generators/KarplusStrongVoice.h"
#include "Helpers/ConstructArray.h"

namespace AbacDsp
{

/**
 * @ingroup generators
 * @brief A fixed bank of NumVoices independent KarplusStrongVoice instances, summed to one output.
 *
 * Carries no sequencing logic of its own - it is "pluck string i" and "step
 * all voices, sum" only. What plucks which string, when, is the caller's
 * concern (a pattern sequencer for an instrument like tanpura).
 */
template <size_t NumVoices, size_t MaxLength>
class KarplusStrongEnsemble
{
  public:
    explicit KarplusStrongEnsemble(const float sampleRate)
        : m_voices{constructArray<KarplusStrongVoice<MaxLength>, NumVoices>(sampleRate)}
    {
    }

    [[nodiscard]] KarplusStrongVoice<MaxLength>& voice(const size_t index) noexcept
    {
        return m_voices[index];
    }

    [[nodiscard]] const KarplusStrongVoice<MaxLength>& voice(const size_t index) const noexcept
    {
        return m_voices[index];
    }

    [[nodiscard]] static constexpr size_t numVoices() noexcept
    {
        return NumVoices;
    }

    [[nodiscard]] bool isAnyActive() const noexcept
    {
        return std::ranges::any_of(m_voices, [](const auto& v) { return v.isActive(); });
    }

    [[nodiscard]] float step() noexcept
    {
        float sum = 0.f;
        for (auto& v : m_voices)
        {
            sum += v.step();
        }
        return sum;
    }

  private:
    std::array<KarplusStrongVoice<MaxLength>, NumVoices> m_voices;
};

}
