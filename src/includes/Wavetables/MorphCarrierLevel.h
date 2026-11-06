#pragma once

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <vector>

#include "Wavetables/WaveTableStorage.h"

namespace AbacDsp
{

/**
 * @ingroup wavetables
 * @brief RMS of what a three-slot WaveTableOscillator reads, from the moments of its tables.
 *
 * The oscillator crossfades the tables of two neighbouring slots, w = (1 - f) a + f b, at the mip level chosen for the
 * pitch. Over one cycle the mean square is (1 - f)^2 <a^2> + f^2 <b^2> + 2 f (1 - f) <ab>, so the three means of every
 * slot pair and level are all that is kept: the level of any morph is then exact, without a table per morph step.
 * It is not flat: higher levels have fewer harmonics, and a crossfade of correlated waves dips in the middle.
 *
 * The morph and level mapping mirrors WaveTableOscillator (setMorph over -1..1, level for twice the phase increment),
 * and the sets must share one level ladder, which the constructor asserts. Construction and setWave() allocate and
 * sweep the tables, so they are not for the audio thread; rms() is.
 */
class MorphCarrierLevel
{
  public:
    static constexpr size_t kSlots{3};

    explicit MorphCarrierLevel(const std::array<BasicWave, kSlots>& waves)
    {
        for (size_t slot = 0; slot < kSlots; ++slot)
        {
            m_sets[slot] = &WaveTableStore::getTableSet(waves[slot]);
        }
        for (size_t pair = 0; pair < m_moments.size(); ++pair)
        {
            computePair(pair);
        }
    }

    /// Replaces a slot and recomputes the moments of both pairs.
    void setWave(const size_t slot, const BasicWave wave)
    {
        assert(slot < kSlots);
        m_sets[slot] = &WaveTableStore::getTableSet(wave);
        computePair(0);
        computePair(1);
    }

    /// The slot pair and the fraction between its two slots that WaveTableOscillator::setMorph(value) chooses.
    struct Position
    {
        size_t pair{0};
        float fraction{0.f};
    };

    [[nodiscard]] static Position positionOf(const float oscillatorMorph) noexcept
    {
        const auto morph = std::clamp((oscillatorMorph + 1) * 0.5f, 0.f, 1.f) * static_cast<float>(kSlots - 1);
        const auto pair = static_cast<size_t>(std::floor(morph));
        if (pair >= kSlots - 1)
        {
            return {kSlots - 2, 0.999999f};
        }
        return {pair, morph - static_cast<float>(pair)};
    }

    /// RMS of the morphed table the oscillator reads at `oscillatorMorph` (-1..1) and a phase increment in cycles.
    [[nodiscard]] float rms(const float oscillatorMorph, const float phaseIncrement) const noexcept
    {
        const auto [pair, f] = positionOf(oscillatorMorph);
        const size_t level = m_sets[pair]->getIndexByFrequency(phaseIncrement * 2);
        const Moments& m = m_moments[pair][level];
        const float meanSquare = (1 - f) * (1 - f) * m.aa + f * f * m.bb + 2 * f * (1 - f) * m.ab;
        return std::sqrt(std::max(meanSquare, 0.f));
    }

    [[nodiscard]] size_t levelCount() const noexcept
    {
        return m_moments[0].size();
    }

  private:
    struct Moments
    {
        float aa{0.f};
        float bb{0.f};
        float ab{0.f};
    };

    void computePair(const size_t pair)
    {
        const WaveTableSet& a = *m_sets[pair];
        const WaveTableSet& b = *m_sets[pair + 1];
        assert(a.tables.size() == b.tables.size());
        m_moments[pair].assign(a.tables.size(), Moments{});
        for (size_t level = 0; level < a.tables.size(); ++level)
        {
            assert(std::abs(a.tables[level].topFreq - b.tables[level].topFreq) < 1e-9f);
            m_moments[pair][level] = momentsOf(a.tables[level], b.tables[level]);
        }
    }

    [[nodiscard]] static Moments momentsOf(const WaveTable& a, const WaveTable& b) noexcept
    {
        double aa{0.0};
        double bb{0.0};
        double ab{0.0};
        for (size_t i = 0; i < WaveTableSize; ++i)
        {
            aa += static_cast<double>(a.data[i]) * static_cast<double>(a.data[i]);
            bb += static_cast<double>(b.data[i]) * static_cast<double>(b.data[i]);
            ab += static_cast<double>(a.data[i]) * static_cast<double>(b.data[i]);
        }
        const auto n = static_cast<double>(WaveTableSize);
        return {static_cast<float>(aa / n), static_cast<float>(bb / n), static_cast<float>(ab / n)};
    }

    std::array<const WaveTableSet*, kSlots> m_sets{};
    std::array<std::vector<Moments>, kSlots - 1> m_moments{};
};

} // namespace AbacDsp
