#pragma once

#include "DebugMod.h"

#include "AllpassDelay.h"
#include "AudioProcessing.h"
#include "Fader.h"
#include "Filters/OnePoleFilter.h"
#include "Numbers/BulgeControl.h"
#include "Helpers/ConstructArray.h"
#include "Numbers/PrimeDispatcher.h"
#include "SplitProcessing.h"

#include <array>
#include <cmath>
#include <vector>

namespace AbacDsp
{

template <size_t MaxDelayLength, size_t NumElements>
class DiffuserDelayChain
{
  public:
    explicit DiffuserDelayChain(const float sampleRate, const size_t blkSize)
        : m_delay{constructArray<ModulatingAllPassDelay<MaxDelayLength>, NumElements>(sampleRate)}
        , tmpFadeIn(blkSize, 0.f)
        , tmpFadeOut(blkSize, 0.f)
    {
        float f = 0.53f;
        for (auto& d : m_delay)
        {
            d.setModulationSpeed(f);
            d.setModulationDepth(0.05f);
            f *= 0.96f;
        }
        resetDiffuser(NumElements, 0.5f, 0.6f, 100.f, 1000.f, skipSmoothing);
    }

    void resetDiffuser(const size_t elements, const float feedback, const float bulge, const float bottomSize,
                       const float topSize, const SkipSmoothing_t&)
    {
        resetDiffuserImpl<true>(elements, feedback, bulge, bottomSize, topSize);
    }

    void resetDiffuser(const size_t elements, const float feedback, const float bulge, const float bottomSize,
                       const float topSize)
    {
        resetDiffuserImpl<false>(elements, feedback, bulge, bottomSize, topSize);
    }

    void setBulge(const size_t elements, const float value)
    {
        m_bulge = value;
        Bulge::fillNormalizedTable(m_ratios.data(), elements, m_bulge);
        scaleDiffuser<false>();
    }

    void setTopSize(const float value)
    {
        m_topSize = value;
        scaleDiffuser<false>();
    }

    void setBottomSize(const float value)
    {
        m_bottomSize = value;
        scaleDiffuser<false>();
    }

    void setElements(const size_t elements)
    {
        if (m_fadeInReduceElements || m_fadeOutAugmentElements)
        {
            m_hasNewElementsScheduled = true;
            m_scheduledNewElements = std::clamp<size_t>(elements, 0, NumElements);
            return;
        }
        setNewElements(elements);
    }

    // return the number of elements depending on what has
    // been scheduled and if nothing scheduled the current amount
    [[nodiscard]] size_t elements() const
    {
        if (m_scheduledNewElements)
        {
            return m_scheduledNewElements;
        }
        if (m_newElementsToUse)
        {
            return m_newElementsToUse;
        }
        return m_elementsToUse;
    }

    void setDamper(const float hz)
    {
        for (auto& r : m_delay)
        {
            r.setLowpass(hz);
        }
    }
    auto exponentialInterpolateRatio(const float min, const float max, const float ratio)
    {
        if (min <= 0)
        {
            return 1E32;
        }
        return min * std::pow(max / min, ratio);
    }
    void setAllPass()
    {
        for (size_t i = 0; i < elements(); ++i)
        {
            const auto x = static_cast<float>(i) / static_cast<float>(m_elementsToUse - 1);
            const auto value = exponentialInterpolateRatio(m_allPassFirst, m_allPassLast, x);
            m_delay[i].setAllpass(value);
        }
    }

    void setAllPassFirstCutoff(const float hz)
    {
        m_allPassFirst = hz;
        setAllPass();
    }

    void setAllPassLastCutoff(const float hz)
    {
        m_allPassLast = hz;
        setAllPass();
    }

    void setModulationDepth(const float value)
    {
        // modulation only on half of the elements
        for (size_t i = 0; i < m_delay.size(); i += 2)
        {
            m_delay[i].setModulationDepth(value);
            m_delay[i + 1].setModulationDepth(0.f);
        }
    }

    void setModulationSpeed(const float value)
    {
        float f = value;
        for (auto& ap : m_delay)
        {
            ap.setModulationSpeed(f);
            f *= 0.96f;
        }
    }

    void setFeedback(const float newFeedback)
    {
        if (newFeedback == m_feedback)
        {
            return;
        }
        m_feedback = newFeedback;
        calcFeedbacks();
    }

    void processBlock(const float* source, float* target, const size_t numSamples)
    {
        std::copy_n(source, numSamples, target);
        if (m_fadeInReduceElements)
        {
            decreaseNumElements(target, numSamples);
            return;
        }
        if (m_fadeOutAugmentElements)
        {
            increaseNumElements(target, numSamples);
            return;
        }
        if (std::fpclassify(m_feedback) == FP_ZERO)
        {
            return;
        }
        for (size_t i = 0; i < m_elementsToUse; ++i)
        {
            m_delay[i].processBlockInplace(target, numSamples);
        }
    }

  private:
    template <bool SetFastNoFade>
    void resetDiffuserImpl(const size_t elements, const float feedback, const float bulge, const float bottomSize,
                           const float topSize)
    {
        m_elementsToUse = elements;
        m_bulge = bulge;
        m_bottomSize = bottomSize;
        m_topSize = topSize;
        Bulge::fillNormalizedTable(m_ratios.data(), elements, m_bulge);
        scaleDiffuser<SetFastNoFade>();
        m_feedback = feedback;
        calcFeedbacks();
        if constexpr (SetFastNoFade)
        {
            m_hasNewElementsScheduled = false;
            m_scheduledNewElements = 0;
            m_newElementsToUse = 0;
        }
    }

    void setNewElements(const size_t elements)
    {
        m_newElementsToUse = std::clamp<size_t>(elements, 0, NumElements);
        if (m_newElementsToUse > m_elementsToUse)
        {
            size_t totalTime = 0;
            for (size_t i = m_elementsToUse; i < m_newElementsToUse; ++i)
            {
                totalTime += m_decayTimeInSamples[i];
            }
            const auto fadeSteps = std::clamp<size_t>(totalTime, 12000, 48000ul * 4ul);
            m_fadeOut.reset(fadeSteps);
            m_fadeIn.reset(fadeSteps);
            m_fadeOutAugmentElements = true;
            for (size_t i = m_elementsToUse; i < m_newElementsToUse; ++i)
            {
                m_delay[i].clear();
            }
        }
        else if (m_newElementsToUse < m_elementsToUse)
        {
            size_t totalTime = 0;
            for (size_t i = m_newElementsToUse; i < m_elementsToUse; ++i)
            {
                totalTime += m_decayTimeInSamples[i];
            }
            m_fadeInReduceElements = true;
            const auto fadeSteps = std::clamp<size_t>(totalTime, 12000, 48000ul * 4ul);
            m_fadeOut.reset(4096);
            m_fadeIn.reset(fadeSteps);
        }
    }

    template <bool fastSet>
    void scaleDiffuser()
    {
        if (m_elementsToUse == 0)
        {
            return;
        }
        std::array<size_t, NumElements> sourceSizes{};
        std::array<size_t, NumElements> primeValues{};

        std::transform(m_ratios.begin(), m_ratios.end(), sourceSizes.begin(),
                       [this](const float ratio)
                       {
                           const auto val = static_cast<long>(m_bottomSize + (m_topSize - m_bottomSize) * ratio);
                           return static_cast<size_t>(val);
                       });
        generateUniquePrimeSet<11u>(sourceSizes.data(), primeValues.data(), m_elementsToUse);
        for (size_t i = 0; i < m_elementsToUse; ++i)
        {
            if constexpr (fastSet)
            {
                m_delay[i].setSize(primeValues[i], skipSmoothing);
            }
            else
            {
                m_delay[i].setSize(primeValues[i]);
            }
        }
    }

    void calcFeedbacks()
    {
        size_t index = 0;
        for (auto& ap : m_delay)
        {
            ap.setFeedback(m_feedback);
            auto t = ap.getDecayTimeInSamples(-40);
            m_decayTimeInSamples[index++] = static_cast<size_t>(t);
        }
    }

    float step(float value)
    {
        auto tmp = value;
        for (size_t i = 0; i < m_elementsToUse; ++i)
        {
            tmp = m_delay[i].step(tmp);
        }
        return tmp;
    }

    void checkChangeElementsDone()
    {
        if (m_fadeIn.isDone() && m_fadeOut.isDone())
        {
            m_fadeInReduceElements = false;
            m_fadeOutAugmentElements = false;
            m_elementsToUse = m_newElementsToUse;
            calcFeedbacks();
            if (m_hasNewElementsScheduled)
            {
                setNewElements(m_scheduledNewElements);
                m_hasNewElementsScheduled = false;
            }
            else
            {
                scaleDiffuser<false>();
            }
        }
    }

    void processFade(float* target, size_t numSamples, size_t elements)
    {
        for (size_t i = 0; i < elements; ++i)
        {
            m_delay[i].processBlockInplace(target, numSamples);
        }
        m_fadeIn.processBlock(target, tmpFadeIn.data(), numSamples);
        m_fadeOut.processBlock(target, tmpFadeOut.data(), numSamples);
    }

    void increaseNumElements(float* target, size_t numSamples)
    {
        processFade(target, numSamples, m_elementsToUse);
        m_delay[m_elementsToUse].processBlockInplace(tmpFadeIn.data(), numSamples);
        for (size_t i = m_elementsToUse + 1; i < m_newElementsToUse; ++i)
        {
            m_delay[i].processBlockInplace(tmpFadeIn.data(), numSamples);
        }
        checkChangeElementsDone();
        std::copy_n(tmpFadeIn.data(), numSamples, target);
        blockSum(tmpFadeOut.data(), target, numSamples);
    }

    void decreaseNumElements(float* target, size_t numSamples)
    {
        processFade(target, numSamples, m_newElementsToUse);
        m_delay[m_newElementsToUse].processBlock(tmpFadeOut.data(), target, numSamples);
        for (size_t i = m_newElementsToUse + 1; i < m_elementsToUse; ++i)
        {
            m_delay[i].processBlockInplace(target, numSamples);
        }
        checkChangeElementsDone();
        blockSum(tmpFadeIn.data(), target, numSamples);
    }

    float m_bottomSize{100.f};
    float m_topSize{1000.f};
    float m_feedback{0.0f};
    std::array<float, NumElements> m_ratios{};
    float m_bulge{0.6f};
    float m_allPassFirst{200.f};
    float m_allPassLast{2000.f};

    std::array<ModulatingAllPassDelay<MaxDelayLength>, NumElements> m_delay{};
    std::array<size_t, NumElements> m_decayTimeInSamples{};

    bool m_hasNewElementsScheduled{false};
    size_t m_scheduledNewElements{NumElements};
    size_t m_newElementsToUse{NumElements};
    size_t m_elementsToUse{NumElements};

    Fader<FadeMode::In, FadeCurve::Sine> m_fadeIn;
    Fader<FadeMode::Out, FadeCurve::Sine> m_fadeOut;

    std::vector<float> tmpFadeIn{};
    std::vector<float> tmpFadeOut{};
    bool m_fadeOutAugmentElements{false};
    bool m_fadeInReduceElements{false};
};


}