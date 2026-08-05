#pragma once

#include <array>

#include "Helpers/ConstructArray.h"
#include "Numbers/BulgeControl.h"
#include "Numbers/PrimeDispatcher.h"
#include "SchroederAllpass.h"

namespace AbacDsp
{

/**
 * @ingroup diffuser
 * @brief Distributes delay-line sizes between a bottom and a top bound along a bulge curve.
 *
 * Bulge bends the distribution away from linear, clustering sizes towards one
 * end so that echo density is not spread evenly across the chain. The resulting
 * sizes are then snapped to distinct primes, which keeps the sections from
 * sharing a common period and reinforcing each other into an audible pitch.
 */
template <size_t NumElements>
class Bulge2
{
  public:
    Bulge2() = default;
    void setBulge(const float value)
    {
        m_bulge = value;
        Bulge::fillNormalizedTable(m_ratios.data(), NumElements, m_bulge);
        scaleDiffuser<false>();
    }

    void setTopSize(const int value)
    {
        m_topSize = value;
        scaleDiffuser<false>();
    }

    void setBottomSize(const int value)
    {
        m_bottomSize = value;
        scaleDiffuser<false>();
    }
    template <bool fastSet>
    void scaleDiffuser()
    {
        std::array<size_t, NumElements> sourceSizes{};
        std::array<size_t, NumElements> primeValues{};

        std::transform(m_ratios.begin(), m_ratios.end(), sourceSizes.begin(), [this](const float ratio)
                       { return static_cast<size_t>(m_bottomSize + (m_topSize - m_bottomSize) * ratio); });
        generateUniquePrimeSet<11u>(sourceSizes.data(), primeValues.data(), NumElements);
    }
    int m_bottomSize{100};
    int m_topSize{1000};
    float m_feedback{0.0f};
    std::array<float, NumElements> m_ratios{};
    float m_bulge{0.6f};
};
/* use gain 0 and size 1 if you want to bypass elements
 */

/**
 * @ingroup diffuser
 * @brief Series chain of Schroeder allpass sections with prime-spaced lengths.
 *
 * Each section multiplies the echo density of the one before it, so a chain of
 * n sections reaches density that a single section could only approach with an
 * impractically high gain and audible ringing. Sizes come from Bulge2, spread
 * between a bottom and top bound and snapped to distinct primes.
 * @see https://ccrma.stanford.edu/~jos/pasp/Schroeder_Allpass_Sections.html
 */
template <size_t MaxDelayLength, size_t NumElements, size_t BlockSize>
class SchroederDiffuser
{
  public:
    explicit SchroederDiffuser(const float sampleRate)
        : m_delay{constructArray<SchroederAllPassSoftTransition<MaxDelayLength, BlockSize>, NumElements>(sampleRate)}
    {
    }

    void setBulge(const size_t elements, const float value)
    {
        m_bulge = value;
        Bulge::fillNormalizedTable(m_ratios.data(), elements, m_bulge);
        scaleDiffuser<false>();
    }

    void setTopSize(const int value)
    {
        m_topSize = value;
        scaleDiffuser<false>();
    }

    void setBottomSize(const int value)
    {
        m_bottomSize = value;
        scaleDiffuser<false>();
    }

    void setFeedback(const float newFeedback) noexcept
    {
        if (newFeedback == m_feedback)
        {
            return;
        }
        m_feedback = newFeedback;
        for (size_t i = 0; i < m_delay.size(); ++i)
        {
            m_delay[i].setGain(m_feedback);
        }
    }

    void processBlock(const float* source, float* target) noexcept
    {
        std::copy_n(source, BlockSize, target);
        for (size_t i = 0; i < m_delay.size(); ++i)
        {
            m_delay[i].processBlockInplace(target);
        }
    }

  private:
    template <bool fastSet>
    void scaleDiffuser()
    {
        std::array<size_t, NumElements> sourceSizes{};
        std::array<size_t, NumElements> primeValues{};

        std::transform(m_ratios.begin(), m_ratios.end(), sourceSizes.begin(), [this](const float ratio)
                       { return static_cast<size_t>(m_bottomSize + (m_topSize - m_bottomSize) * ratio); });
        generateUniquePrimeSet<11u>(sourceSizes.data(), primeValues.data(), m_delay.size());
        for (size_t i = 0; i < m_delay.size(); ++i)
        {
            if constexpr (fastSet)
            {
                m_delay[i].setSize(primeValues[i], true);
            }
            else
            {
                m_delay[i].setSize(primeValues[i]);
            }
        }
    }

    [[nodiscard]] float step(const float value)
    {
        auto tmp = value;
        for (size_t i = 0; i < m_delay.size(); ++i)
        {
            tmp = m_delay[i].step(tmp);
        }
        return tmp;
    }

    int m_bottomSize{100};
    int m_topSize{1000};
    float m_feedback{0.0f};
    std::array<float, NumElements> m_ratios{};
    float m_bulge{0.6f};

    std::array<SchroederAllPassSoftTransition<MaxDelayLength, BlockSize>, NumElements> m_delay{};
};

}