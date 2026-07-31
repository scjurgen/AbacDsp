#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <functional>
#include <vector>

#include "AllpassDelay.h"
#include "Audio/Fader.h"
#include "Filters/Biquad.h"
#include "Helpers/ConstructArray.h"
#include "Helpers/SkipSmoothing.h"
#include "Numbers/BulgeControl.h"
#include "Numbers/Convert.h"
#include "Numbers/PrimeDispatcher.h"

namespace AbacDsp
{

template <size_t MaxDelayLength, size_t NumElements, AllpassFeedbackStyle Style = AllpassFeedbackStyle::Direct>
class DiffuserDelayChain
{
  public:
    explicit DiffuserDelayChain(const float sampleRate, const size_t blkSize)
        : m_sampleRate(sampleRate)
        , m_delay{constructArray<ModulatingAllPassDelay<MaxDelayLength, Style>, NumElements>(sampleRate)}
        , m_bandScratch(blkSize, 0.f)
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
        resetDiffuser(NumElements, 0.5f, 0.6f, 0.5f, 5.f, skipSmoothing);
    }

    void resetDiffuser(const size_t elements, const float feedback, const float bulge, const float bottomSizeInMeters,
                       const float topSizeInMeters, const SkipSmoothing_t&)
    {
        resetDiffuserImpl<true>(elements, feedback, bulge, bottomSizeInMeters, topSizeInMeters);
    }

    void resetDiffuser(const size_t elements, const float feedback, const float bulge, const float bottomSizeInMeters,
                       const float topSizeInMeters)
    {
        resetDiffuserImpl<false>(elements, feedback, bulge, bottomSizeInMeters, topSizeInMeters);
    }

    void setBulge(const size_t elements, const float value)
    {
        m_bulge = value;
        Bulge::fillNormalizedTable(m_ratios.data(), elements, m_bulge);
        scaleDiffuser<false>();
    }

    void setTopSize(const float valueInMeters)
    {
        m_topSize = valueInMeters;
        scaleDiffuser<false>();
    }

    void setBottomSize(const float valueInMeters)
    {
        m_bottomSize = valueInMeters;
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

    // returns scheduled element count if pending, else active count
    [[nodiscard]] size_t elements() const noexcept
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

    // Active elements' sizes in meters, in the same bulge-shaped distribution scaleDiffuser()
    // uses to size the actual delay lines. Elements beyond the active count are left at 0.
    [[nodiscard]] std::array<float, NumElements> getElementSizesInMeters() const noexcept
    {
        std::array<float, NumElements> sizes{};
        for (size_t i = 0; i < m_elementsToUse; ++i)
        {
            sizes[i] = m_bottomSize + (m_topSize - m_bottomSize) * m_ratios[i];
        }
        return sizes;
    }

    // Opt-in per-element level tap for visualisation. Null by default (single branch,
    // no cost) until a caller registers a sink.
    void setLevelMeterSink(std::array<std::atomic<float>, NumElements + 1>* sink) noexcept
    {
        m_levelSink = sink;
    }

    using BandLevelSink = std::array<std::array<std::atomic<float>, 3>, NumElements + 1>;

    // Configures the fixed low/mid/high per-bin filter bank used by the band-level sink below.
    // lowHz/highHz are the low-pass and high-pass corners; the mid band-pass sits at their
    // geometric mean with a wide Q so the three bands overlap.
    void configureBandFilters(const float sampleRate, const float lowHz, const float highHz) noexcept
    {
        constexpr float kOuterQ = 0.7071f;
        constexpr float kMidQ = 0.5f;
        const auto midHz = std::sqrt(lowHz * highHz);
        for (auto& f : m_bandLowFilters)
        {
            f.computeCoefficients(sampleRate, lowHz, kOuterQ, 0.f);
        }
        for (auto& f : m_bandMidFilters)
        {
            f.computeCoefficients(sampleRate, midHz, kMidQ, 0.f);
        }
        for (auto& f : m_bandHighFilters)
        {
            f.computeCoefficients(sampleRate, highHz, kOuterQ, 0.f);
        }
    }

    // Opt-in per-element, per-band level tap for visualisation. Null by default (single branch,
    // no cost) until a caller registers a sink and calls configureBandFilters().
    void setBandLevelMeterSink(BandLevelSink* sink) noexcept
    {
        m_bandLevelSink = sink;
    }
    [[nodiscard]] float exponentialInterpolateRatio(const float min, const float max, const float ratio) const noexcept
    {
        if (min <= 0)
        {
            return 1E32f;
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
        if (std::equal_to<float>{}(newFeedback, m_feedback))
        {
            return;
        }
        m_feedback = newFeedback;
        calcFeedbacks();
    }

    void processBlock(const float* source, float* target, const size_t numSamples)
    {
        std::copy_n(source, numSamples, target);
        meterAll(0, target, numSamples);
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
        clearInactiveBins();
        if (std::fpclassify(m_feedback) == FP_ZERO)
        {
            for (size_t i = 1; i <= m_elementsToUse; ++i)
            {
                meterAll(i, target, numSamples);
            }
            return;
        }
        for (size_t i = 0; i < m_elementsToUse; ++i)
        {
            m_delay[i].processBlockInplace(target, numSamples);
            meterAll(i + 1, target, numSamples);
        }
    }

  private:
    // Raw peak tap for the visualisation sink; no-ops when no sink is registered. Reports the
    // unfiltered per-block peak (linear gain) and leaves any ballistic smoothing to the consumer.
    void meterBin(const size_t bin, const float* buf, const size_t numSamples) noexcept
    {
        if (!m_levelSink)
        {
            return;
        }
        float peak = 0.f;
        for (size_t i = 0; i < numSamples; ++i)
        {
            peak = std::max(peak, std::abs(buf[i]));
        }
        (*m_levelSink)[bin].store(peak, std::memory_order_relaxed);
    }

    // Raw per-band peak tap for the stacked-bands visualisation; no-ops when no sink is
    // registered. Filters run on a copy of buf so the metered stage's own output is untouched.
    void meterBinBands(const size_t bin, const float* buf, const size_t numSamples) noexcept
    {
        if (!m_bandLevelSink)
        {
            return;
        }
        const auto peakOfAbs = [](const float* data, const size_t count) noexcept
        {
            float peak = 0.f;
            for (size_t i = 0; i < count; ++i)
            {
                peak = std::max(peak, std::abs(data[i]));
            }
            return peak;
        };

        m_bandLowFilters[bin].processBlock(buf, m_bandScratch.data(), numSamples);
        const auto lowPeak = peakOfAbs(m_bandScratch.data(), numSamples);
        m_bandMidFilters[bin].processBlock(buf, m_bandScratch.data(), numSamples);
        const auto midPeak = peakOfAbs(m_bandScratch.data(), numSamples);
        m_bandHighFilters[bin].processBlock(buf, m_bandScratch.data(), numSamples);
        const auto highPeak = peakOfAbs(m_bandScratch.data(), numSamples);

        (*m_bandLevelSink)[bin][0].store(lowPeak, std::memory_order_relaxed);
        (*m_bandLevelSink)[bin][1].store(midPeak, std::memory_order_relaxed);
        (*m_bandLevelSink)[bin][2].store(highPeak, std::memory_order_relaxed);
    }

    void meterAll(const size_t bin, const float* buf, const size_t numSamples) noexcept
    {
        meterBin(bin, buf, numSamples);
        meterBinBands(bin, buf, numSamples);
    }

    void clearInactiveBins() noexcept
    {
        if (m_levelSink)
        {
            for (size_t i = m_elementsToUse + 1; i <= NumElements; ++i)
            {
                (*m_levelSink)[i].store(0.f, std::memory_order_relaxed);
            }
        }
        if (m_bandLevelSink)
        {
            for (size_t i = m_elementsToUse + 1; i <= NumElements; ++i)
            {
                (*m_bandLevelSink)[i][0].store(0.f, std::memory_order_relaxed);
                (*m_bandLevelSink)[i][1].store(0.f, std::memory_order_relaxed);
                (*m_bandLevelSink)[i][2].store(0.f, std::memory_order_relaxed);
            }
        }
    }

    template <bool SetFastNoFade>
    void resetDiffuserImpl(const size_t elements, const float feedback, const float bulge,
                           const float bottomSizeInMeters, const float topSizeInMeters)
    {
        m_elementsToUse = elements;
        m_bulge = bulge;
        m_bottomSize = bottomSizeInMeters;
        m_topSize = topSizeInMeters;
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

    // Converts the meters-based bottomSize/topSize to samples, clamped to what MaxDelayLength can
    // hold, before the existing ratio interpolation and prime search run unchanged on sample counts.
    [[nodiscard]] float metersToClampedSamples(const float meters) const noexcept
    {
        return std::clamp(Convert::metersToSamples(meters, m_sampleRate), 11.f, static_cast<float>(MaxDelayLength));
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

        const auto bottomSamples = metersToClampedSamples(m_bottomSize);
        const auto topSamples = metersToClampedSamples(m_topSize);
        std::transform(m_ratios.begin(), m_ratios.end(), sourceSizes.begin(),
                       [bottomSamples, topSamples](const float ratio)
                       { return static_cast<size_t>(bottomSamples + (topSamples - bottomSamples) * ratio); });
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

    [[nodiscard]] float step(const float value)
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

    // Meters the settled part of the chain (shared by both fade directions) so the bins gauge
    // keeps updating during a crossfade instead of freezing at its pre-change reading.
    void processFade(float* target, size_t numSamples, size_t elements)
    {
        for (size_t i = 0; i < elements; ++i)
        {
            m_delay[i].processBlockInplace(target, numSamples);
            meterAll(i + 1, target, numSamples);
        }
        m_fadeIn.processBlock(target, tmpFadeIn.data(), numSamples);
        m_fadeOut.processBlock(target, tmpFadeOut.data(), numSamples);
    }

    void increaseNumElements(float* target, size_t numSamples)
    {
        processFade(target, numSamples, m_elementsToUse);
        m_delay[m_elementsToUse].processBlockInplace(tmpFadeIn.data(), numSamples);
        meterAll(m_elementsToUse + 1, tmpFadeIn.data(), numSamples);
        for (size_t i = m_elementsToUse + 1; i < m_newElementsToUse; ++i)
        {
            m_delay[i].processBlockInplace(tmpFadeIn.data(), numSamples);
            meterAll(i + 1, tmpFadeIn.data(), numSamples);
        }
        checkChangeElementsDone();
        std::copy_n(tmpFadeIn.data(), numSamples, target);
        std::transform(target, target + numSamples, tmpFadeOut.data(), target, std::plus<>{});
    }

    void decreaseNumElements(float* target, size_t numSamples)
    {
        processFade(target, numSamples, m_newElementsToUse);
        m_delay[m_newElementsToUse].processBlock(tmpFadeOut.data(), target, numSamples);
        meterAll(m_newElementsToUse + 1, target, numSamples);
        for (size_t i = m_newElementsToUse + 1; i < m_elementsToUse; ++i)
        {
            m_delay[i].processBlockInplace(target, numSamples);
            meterAll(i + 1, target, numSamples);
        }
        checkChangeElementsDone();
        std::transform(target, target + numSamples, tmpFadeIn.data(), target, std::plus<>{});
    }

    float m_sampleRate;
    float m_bottomSize{0.5f};
    float m_topSize{5.f};
    float m_feedback{0.0f};
    std::array<float, NumElements> m_ratios{};
    float m_bulge{0.6f};
    float m_allPassFirst{200.f};
    float m_allPassLast{2000.f};

    std::array<ModulatingAllPassDelay<MaxDelayLength, Style>, NumElements> m_delay{};
    std::array<size_t, NumElements> m_decayTimeInSamples{};

    bool m_hasNewElementsScheduled{false};
    size_t m_scheduledNewElements{NumElements};
    size_t m_newElementsToUse{NumElements};
    size_t m_elementsToUse{NumElements};

    std::array<std::atomic<float>, NumElements + 1>* m_levelSink{nullptr};

    std::array<Biquad<BiquadFilterType::LowPass>, NumElements + 1> m_bandLowFilters{};
    std::array<Biquad<BiquadFilterType::BandPass>, NumElements + 1> m_bandMidFilters{};
    std::array<Biquad<BiquadFilterType::HighPass>, NumElements + 1> m_bandHighFilters{};
    BandLevelSink* m_bandLevelSink{nullptr};
    std::vector<float> m_bandScratch{};

    Fader<FadeMode::In, FadeCurve::Sine> m_fadeIn;
    Fader<FadeMode::Out, FadeCurve::Sine> m_fadeOut;

    std::vector<float> tmpFadeIn{};
    std::vector<float> tmpFadeOut{};
    bool m_fadeOutAugmentElements{false};
    bool m_fadeInReduceElements{false};
};

}