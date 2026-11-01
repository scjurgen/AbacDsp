#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "Filters/Biquad.h"

namespace AbacDsp
{

/// @ingroup analysis
/// @brief Cost option of StampedYinPitchDetector.
struct StampedYinOptions
{
    size_t decimation{1};    ///< 1 or a larger integer; the cost falls with its square
    size_t refinePeriods{0}; ///< 0 off; else the pitch is refined over this many periods (3 is a good value)
};

/**
 * @ingroup analysis
 * @brief Streaming monophonic pitch detector after YIN (cumulative mean normalised difference function with an
 * absolute threshold and parabolic interpolation) that reports level and a time stamp with every reading and has a
 * cost control. YinPitchDetector is the same algorithm with a different job; both exist.
 *
 * | | YinPitchDetector | StampedYinPitchDetector |
 * |---|---|---|
 * | Window | four periods of the lowest frequency in a circular buffer, the first half integrated | 2 * maxLag samples
 * (maxLag = fs / minFrequency at the decimated rate) in a double-written ring | | Lags computed | every lag up to the
 * longest period, every hop, then searched | lag by lag, stopping at the first dip that meets the threshold; all lags
 * only when none does | | Cost control | none | integer decimation, the arithmetic falls with its square | | Per
 * reading | pitch and confidence | pitch, confidence, level (RMS of the window) and the count of input samples consumed
 * | | Unvoiced reading | pitch 0 | pitch 0, and the level is still reported | | Reset | none | reset() |
 *
 * The first suits offline analysis and effects that read a pitch now and then (NoteSegmenter, NoteVerifier,
 * PitchFadeWindowDelay use it, with thresholds tuned to its window). This one suits a real-time stream that has to
 * turn readings into note events: the tracker needs the level and the stamp of every reading, and the cost has to
 * be bounded on a small CPU and for low notes, which the early stop and the decimation do.
 *
 * Plain autocorrelation peaks as strongly at twice the period as at the period, which is where its octave errors
 * come from. Dividing the difference function by its running mean makes the later dips shallower than the first, so
 * the first dip below the threshold is the period (steps 1 to 5 of the paper; step 6, the best local estimate, is
 * not done). Confidence is 1 - CMNDF at the chosen lag: near 1 for a clean periodic signal, a measure of
 * periodicity and not a probability. The cumulative sum starts at lag 1 as in the paper; a constant added to it
 * would dominate for quiet signals and make noise look periodic.
 *
 * The window is the newest 2 * maxLag samples: the difference function integrates over the first maxLag of them and
 * looks up to maxLag further. Gate (RMS of the whole window), difference function and level use that one window; its
 * centre lies getLatencySamples() input samples before the newest one, and getLastWindowEnd() is the count of input
 * samples consumed when it was taken, so a reading can be stamped with its centre. A change of pitch is complete in
 * the result one window plus one hop later. A reading is made once per hop and counted in input samples, so the
 * result does not depend on how the input is cut into blocks.
 *
 * With refinePeriods the pitch YIN found is refined by the normalised autocorrelation (peak within 12 percent of the
 * lag, parabolic interpolation) over the newest refinePeriods periods only. YIN integrates over maxLag samples, which
 * for a low range still hold the previous note and the pluck when a new note has sounded for three periods, so its
 * pitch converges slowly (a bass window is 69 ms); a window of the newest periods does not have that history. The
 * refinement is skipped when less than one period of history is left or the correlation is below 0.8.
 *
 * Decimation puts a 4th-order Butterworth lowpass at 0.4 of the decimated rate in front and keeps every n-th sample;
 * the lowpass removes broadband noise that would lift the dips above the threshold, at the price of resolution (n = 4
 * gives 12 samples per period at 1 kHz) and of a group delay of a few input samples that is not counted. All buffers
 * are sized in the constructor; step() allocates nothing.
 * @see de Cheveigne and Kawahara, "YIN, a fundamental frequency estimator for speech and music", JASA 111(4), 2002
 */
class StampedYinPitchDetector
{
  public:
    static constexpr float kThreshold{0.1f};
    static constexpr float kMinimumQuality{0.8f};
    static constexpr float kSilenceRms{1e-6f};
    static constexpr float kMinCumulativeSum{1e-10f};
    static constexpr float kRefineSpan{0.12f};
    static constexpr float kRefineMinCorrelation{0.8f};

    explicit StampedYinPitchDetector(const float sampleRate, const float minFreq = 80.0f, const float maxFreq = 1000.0f,
                                     const float hopFrequency = 50.0f, const StampedYinOptions options = {})
        : m_sampleRate(sampleRate)
        , m_decimation(std::max<size_t>(options.decimation, 1))
        , m_refinePeriods(options.refinePeriods)
        , m_decimatedRate(sampleRate / static_cast<float>(m_decimation))
        , m_minLag(std::max<size_t>(2, static_cast<size_t>(m_decimatedRate / maxFreq)))
        , m_maxLag(std::max<size_t>(m_minLag + 3, static_cast<size_t>(m_decimatedRate / minFreq) + 2))
        , m_windowSize(2 * m_maxLag)
        , m_hopSize(std::max<size_t>(1, static_cast<size_t>(sampleRate / hopFrequency)))
        , m_ring(2 * m_windowSize, 0.0f)
        , m_cmndf(m_maxLag + 1, 1.0f)
        , m_antiAlias(designAntiAlias(sampleRate, m_decimatedRate))
    {
    }

    /// Clears the window, the filter and the result.
    void reset() noexcept
    {
        std::ranges::fill(m_ring, 0.0f);
        m_antiAlias = designAntiAlias(m_sampleRate, m_decimatedRate);
        m_writeIndex = 0;
        m_decimationPhase = 0;
        m_hopCounter = 0;
        m_inputCount = 0;
        m_newPitch = false;
        m_pitch = 0.0f;
        m_confidence = 0.0f;
        m_level = 0.0f;
        m_lastWindowEnd = 0;
    }

    /// True on the sample after which a new reading (voiced or not) is available.
    [[nodiscard]] bool hasNewPitch() const noexcept
    {
        return m_newPitch;
    }

    /// Takes one input sample; returns the latest pitch in Hz (0 when the last reading was unvoiced).
    float step(const float in) noexcept
    {
        m_newPitch = false;
        ++m_inputCount;
        pushSample(in);
        if (++m_hopCounter >= m_hopSize)
        {
            m_hopCounter = 0;
            analyse();
        }
        return m_pitch;
    }

    /// Runs a block, writing the latest pitch after every sample.
    void process(const float* in, float* out, const size_t numSamples) noexcept
    {
        for (size_t i = 0; i < numSamples; ++i)
        {
            out[i] = step(in[i]);
        }
    }

    [[nodiscard]] float getCurrentPitch() const noexcept
    {
        return m_pitch;
    }

    /// 1 - CMNDF at the chosen lag, 0 when the reading was unvoiced.
    [[nodiscard]] float getLastConfidence() const noexcept
    {
        return m_confidence;
    }

    /// RMS of the window of the last reading, linear.
    [[nodiscard]] float getLastLevel() const noexcept
    {
        return m_level;
    }

    /// Input samples consumed when the window of the last reading was taken (the window ends at the newest of them).
    [[nodiscard]] uint64_t getLastWindowEnd() const noexcept
    {
        return m_lastWindowEnd;
    }

    /// Input samples from the newest sample of a window to its centre.
    [[nodiscard]] size_t getLatencySamples() const noexcept
    {
        return m_maxLag * m_decimation;
    }

    /// Input samples between two readings.
    [[nodiscard]] size_t getHopSize() const noexcept
    {
        return m_hopSize;
    }

    /// Samples in the analysis window, at the decimated rate.
    [[nodiscard]] size_t getWindowSize() const noexcept
    {
        return m_windowSize;
    }

    [[nodiscard]] size_t getMaxLag() const noexcept
    {
        return m_maxLag;
    }

  private:
    using AntiAlias = std::array<Biquad<BiquadFilterType::LowPass>, 2>;

    [[nodiscard]] static AntiAlias designAntiAlias(const float sampleRate, const float decimatedRate)
    {
        AntiAlias sections{};
        const float cutoff = 0.4f * decimatedRate;
        sections[0].computeCoefficients(sampleRate, cutoff, 0.5412f, 0.0f);
        sections[1].computeCoefficients(sampleRate, cutoff, 1.3066f, 0.0f);
        return sections;
    }

    void pushSample(const float in) noexcept
    {
        float x = in;
        if (m_decimation > 1)
        {
            x = m_antiAlias[1].singleStepLowPass(m_antiAlias[0].singleStepLowPass(in));
            if (++m_decimationPhase < m_decimation)
            {
                return;
            }
            m_decimationPhase = 0;
        }
        // double-written ring: the newest m_windowSize samples are contiguous from m_writeIndex (the oldest)
        m_ring[m_writeIndex] = x;
        m_ring[m_writeIndex + m_windowSize] = x;
        m_writeIndex = m_writeIndex + 1 == m_windowSize ? 0 : m_writeIndex + 1;
    }

    /// Takes the window at the hop and runs the whole analysis on it.
    void analyse() noexcept
    {
        m_lastWindowEnd = m_inputCount;
        m_source = &m_ring[m_writeIndex];
        float sumSquares = 0.0f;
        for (size_t i = 0; i < m_windowSize; ++i)
        {
            sumSquares += m_source[i] * m_source[i];
        }
        m_level = std::sqrt(sumSquares / static_cast<float>(m_windowSize));
        if (m_level <= kSilenceRms)
        {
            publish(0.0f, 0.0f);
            return;
        }
        m_cmndf[0] = 1.0f;
        float cumulativeSum = 0.0f;
        for (size_t lag = 1; lag <= m_maxLag; ++lag)
        {
            const float difference = differenceAt(lag);
            cumulativeSum += difference;
            m_cmndf[lag] =
                cumulativeSum > kMinCumulativeSum ? difference * static_cast<float>(lag) / cumulativeSum : 1.0f;
            // the dip at lag - 1 is known to be a local minimum once its upper neighbour exists
            const size_t candidate = lag - 1;
            if (candidate >= m_minLag && candidate < m_maxLag && m_cmndf[candidate] < kThreshold &&
                m_cmndf[candidate] <= m_cmndf[candidate - 1] && m_cmndf[candidate] <= m_cmndf[lag])
            {
                finish(candidate);
                return;
            }
        }
        const auto first = m_cmndf.begin() + static_cast<std::ptrdiff_t>(m_minLag);
        const auto last = m_cmndf.begin() + static_cast<std::ptrdiff_t>(m_maxLag);
        const auto minimum = std::min_element(first, last);
        if (minimum != last && *minimum < kMinimumQuality)
        {
            finish(static_cast<size_t>(minimum - m_cmndf.begin()));
        }
        else
        {
            publish(0.0f, 0.0f);
        }
    }

    /// The difference function at one lag over the first m_maxLag samples; four partial sums so the loop can be
    /// vectorised without reordering floating point sums.
    [[nodiscard]] float differenceAt(const size_t lag) const noexcept
    {
        const float* a = m_source;
        const float* b = m_source + lag;
        float s0 = 0.0f;
        float s1 = 0.0f;
        float s2 = 0.0f;
        float s3 = 0.0f;
        size_t j = 0;
        for (; j + 4 <= m_maxLag; j += 4)
        {
            const float e0 = a[j] - b[j];
            const float e1 = a[j + 1] - b[j + 1];
            const float e2 = a[j + 2] - b[j + 2];
            const float e3 = a[j + 3] - b[j + 3];
            s0 += e0 * e0;
            s1 += e1 * e1;
            s2 += e2 * e2;
            s3 += e3 * e3;
        }
        for (; j < m_maxLag; ++j)
        {
            const float e = a[j] - b[j];
            s0 += e * e;
        }
        return (s0 + s1) + (s2 + s3);
    }

    void finish(const size_t lag) noexcept
    {
        const float confidence = std::clamp(1.0f - m_cmndf[lag], 0.0f, 1.0f);
        const float yinLag = interpolate(lag);
        publish(m_decimatedRate / (m_refinePeriods > 0 ? refineLag(yinLag) : yinLag), confidence);
    }

    /// The peak of the normalised autocorrelation of the newest samples near `lag`; `lag` when the window has too
    /// little history for it or the correlation is weak.
    [[nodiscard]] float refineLag(const float lag) const noexcept
    {
        const auto low = static_cast<size_t>(std::max(static_cast<float>(m_minLag), lag * (1.0f - kRefineSpan)));
        const auto high = std::min(m_maxLag - 1, static_cast<size_t>(std::ceil(lag * (1.0f + kRefineSpan))));
        const size_t length =
            std::min(static_cast<size_t>(static_cast<float>(m_refinePeriods) * lag), m_windowSize - high - 1);
        if (low < 2 || high < low + 2 || static_cast<float>(length) < lag)
        {
            return lag;
        }
        float energy = 0.0f;
        const float* newest = m_source + m_windowSize - length;
        for (size_t j = 0; j < length; ++j)
        {
            energy += newest[j] * newest[j];
        }
        size_t bestLag = low;
        float best = -2.0f;
        for (size_t candidate = low; candidate <= high; ++candidate)
        {
            const float r = correlationAt(candidate, length, energy);
            if (r > best)
            {
                best = r;
                bestLag = candidate;
            }
        }
        if (best < kRefineMinCorrelation || bestLag == low || bestLag == high)
        {
            return best >= kRefineMinCorrelation ? static_cast<float>(bestLag) : lag;
        }
        const float y1 = correlationAt(bestLag - 1, length, energy);
        const float y3 = correlationAt(bestLag + 1, length, energy);
        const float a = (y1 - 2.0f * best + y3) / 2.0f;
        const float b = (y3 - y1) / 2.0f;
        return static_cast<float>(bestLag) + (std::abs(a) < 1e-9f ? 0.0f : std::clamp(-b / (2.0f * a), -0.5f, 0.5f));
    }

    [[nodiscard]] float correlationAt(const size_t lag, const size_t length, const float energy) const noexcept
    {
        const float* a = m_source + m_windowSize - length;
        const float* b = a - lag;
        float product = 0.0f;
        float delayedEnergy = 0.0f;
        for (size_t j = 0; j < length; ++j)
        {
            product += a[j] * b[j];
            delayedEnergy += b[j] * b[j];
        }
        return energy > 0.0f && delayedEnergy > 0.0f ? product / std::sqrt(energy * delayedEnergy) : 0.0f;
    }

    /// Parabolic interpolation of the minimum, clamped to half a sample: a flat curve around the lag would otherwise
    /// send the estimate far off.
    [[nodiscard]] float interpolate(const size_t lag) const noexcept
    {
        if (lag == 0 || lag >= m_maxLag)
        {
            return static_cast<float>(lag);
        }
        const float y1 = m_cmndf[lag - 1];
        const float y2 = m_cmndf[lag];
        const float y3 = m_cmndf[lag + 1];
        const float a = (y1 - 2.0f * y2 + y3) / 2.0f;
        const float b = (y3 - y1) / 2.0f;
        if (std::abs(a) < 1e-6f)
        {
            return static_cast<float>(lag);
        }
        return static_cast<float>(lag) + std::clamp(-b / (2.0f * a), -0.5f, 0.5f);
    }

    void publish(const float pitch, const float confidence) noexcept
    {
        m_pitch = pitch;
        m_confidence = confidence;
        m_newPitch = true;
    }

    const float m_sampleRate;
    const size_t m_decimation;
    const size_t m_refinePeriods;
    const float m_decimatedRate;
    const size_t m_minLag;
    const size_t m_maxLag;
    const size_t m_windowSize;
    const size_t m_hopSize;

    std::vector<float> m_ring;
    std::vector<float> m_cmndf;
    AntiAlias m_antiAlias;

    const float* m_source{nullptr};
    size_t m_writeIndex{0};
    size_t m_decimationPhase{0};
    size_t m_hopCounter{0};
    uint64_t m_inputCount{0};
    uint64_t m_lastWindowEnd{0};
    bool m_newPitch{false};
    float m_pitch{0.0f};
    float m_confidence{0.0f};
    float m_level{0.0f};
};

}
