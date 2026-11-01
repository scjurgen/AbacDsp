#pragma once

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstddef>

namespace AbacDsp
{

/**
 * @ingroup generators
 * @brief Polynomial ease curve blended against linear, used to shape one envelope segment.
 *
 * factor 0 is a straight line; positive values bow the curve, negative values
 * bow it the other way with a slightly gentler tangent at the start. Values
 * outside -1..1 overshoot past the segment's target before settling.
 */
class CurvedValue
{
  public:
    CurvedValue() noexcept
    {
        setFactor(0.f);
    }

    void setFactor(const float factor) noexcept
    {
        m_f = factor < 0.f ? curveCoefficients(factor / 1.75f) : curveCoefficients(factor);
    }

    [[nodiscard]] float get(const float x) const noexcept
    {
        const float squareX = x * x;
        const float quartX = squareX * squareX;
        return m_f[0] * quartX * x + m_f[1] * squareX * x + m_f[2] * x + x - m_f[3] * x;
    }

  private:
    [[nodiscard]] static constexpr std::array<float, 4> curveCoefficients(const float a) noexcept
    {
        return {a * 0.0706985022721f, a * -0.640510676908f, a * 1.56981217135f, a};
    }

    std::array<float, 4> m_f{};
};

/**
 * @ingroup generators
 * @brief Exponential approach to the target within one envelope segment, landing on it exactly at x = 1.
 *
 * get(x) = (1 - exp(-k x)) / (1 - exp(-k)) for x in 0..1, with k the number of time constants that fit in the
 * segment. The start is steep: k = 3 covers 55 % of the distance in the first quarter of the segment, k = 5 covers
 * 72 %. Negative k mirrors the curve (slow start, fast end); |k| below 1e-4 is a straight line.
 * k is clamped to -50..50. The polynomial CurvedValue cannot follow k above about 1 without overshooting the target.
 * @see https://en.wikipedia.org/wiki/Exponential_decay
 */
class ExponentialApproach
{
  public:
    static constexpr float kStraightBelow{1e-4f};
    static constexpr float kMaxTimeConstants{50.f};

    void setTimeConstants(const float timeConstants) noexcept
    {
        m_rate = std::clamp(timeConstants, -kMaxTimeConstants, kMaxTimeConstants);
        m_straight = std::abs(m_rate) < kStraightBelow;
        m_scale = m_straight ? 1.f : 1.f / std::expm1(-m_rate);
    }

    [[nodiscard]] float get(const float x) const noexcept
    {
        const float clamped = std::min(x, 1.f);
        return m_straight ? clamped : std::expm1(-m_rate * clamped) * m_scale;
    }

  private:
    float m_rate{0.f};
    float m_scale{1.f};
    bool m_straight{true};
};

/**
 * @ingroup generators
 * @brief Which curve family shapes a segment; the segment's curve value is that family's parameter.
 */
enum class SegmentShape
{
    Polynomial, ///< CurvedValue: the curve value is its factor, 0 is straight
    Exponential ///< ExponentialApproach: the curve value is the number of time constants within the segment
};

/**
 * @ingroup generators
 * @brief One ramp-with-curve segment: from whatever gain it starts at, to a target, over N samples.
 *
 * Once the sample count runs out it holds at the target forever, so a caller
 * never needs to special-case "segment finished" before reading currentGain().
 * The shape of the ramp is a CurvedValue unless the segment asks for SegmentShape::Exponential.
 */
class EnvelopeShaper
{
  public:
    EnvelopeShaper() noexcept = default;

    void reset(const float startGain) noexcept
    {
        m_startGain = startGain;
        m_endGain = startGain;
        m_lastGain = startGain;
        m_numOfSamples = 0;
        m_holdValues = true;
    }

    void setHold(const size_t numOfSamples, const float value) noexcept
    {
        m_numOfSamples = numOfSamples;
        m_holdValues = true;
        m_startGain = value;
        m_endGain = value;
    }

    void setNewFramesAndTarget(const size_t numOfSamples, const float endGain, const float curve,
                               const SegmentShape shape = SegmentShape::Polynomial) noexcept
    {
        m_startGain = m_lastGain;
        m_shape = shape;
        m_curve.setFactor(shape == SegmentShape::Polynomial ? curve : 0.f);
        m_exponential.setTimeConstants(shape == SegmentShape::Exponential ? curve : 0.f);
        m_numOfSamples = numOfSamples;
        m_endGain = endGain;
        if (m_numOfSamples == 0)
        {
            m_startGain = endGain;
            m_gainDelta = 0.f;
            m_x = 0.f;
            m_dx = 0.f;
            m_holdValues = true;
            return;
        }
        m_x = 0.f;
        m_dx = 1.f / static_cast<float>(m_numOfSamples);
        m_gainDelta = m_endGain - m_startGain;
        m_holdValues = false;
    }

    /// @brief Retargets the segment mid-flight: keeps the remaining sample count, restarts the
    /// curve from the current gain toward a new endGain. For a live control change (e.g. a
    /// sustain-level tweak) that should finish on schedule rather than restart the ramp.
    void modifyTarget(const float endGain) noexcept
    {
        m_startGain = m_lastGain;
        m_endGain = endGain;
        if (m_numOfSamples == 0)
        {
            m_startGain = endGain;
            m_gainDelta = 0.f;
            m_x = 0.f;
            m_dx = 0.f;
            return;
        }
        m_x = 0.f;
        m_dx = 1.f / static_cast<float>(m_numOfSamples);
        m_gainDelta = m_endGain - m_startGain;
        m_holdValues = m_dx == 0.f;
    }

    [[nodiscard]] float step() noexcept
    {
        if (m_holdValues)
        {
            if (m_numOfSamples > 0)
            {
                --m_numOfSamples;
            }
            else
            {
                m_lastGain = m_startGain = m_endGain;
                m_gainDelta = 0.f;
            }
            return m_endGain;
        }
        if (m_numOfSamples > 0)
        {
            --m_numOfSamples;
            m_x += m_dx;
        }
        else
        {
            m_startGain = m_endGain;
            m_gainDelta = 0.f;
            m_holdValues = true;
        }
        m_lastGain = m_startGain + shapeAt(m_x) * m_gainDelta;
        return m_lastGain;
    }

    [[nodiscard]] bool isDone() const noexcept
    {
        return m_numOfSamples == 0;
    }

    [[nodiscard]] float currentGain() const noexcept
    {
        return m_lastGain;
    }

  private:
    [[nodiscard]] float shapeAt(const float x) const noexcept
    {
        return m_shape == SegmentShape::Exponential ? m_exponential.get(x) : m_curve.get(x);
    }

    size_t m_numOfSamples{0};
    float m_startGain{0.f};
    float m_gainDelta{0.f};
    float m_endGain{0.f};
    float m_lastGain{0.f};
    bool m_holdValues{true};
    float m_dx{0.f};
    float m_x{0.f};
    CurvedValue m_curve;
    ExponentialApproach m_exponential;
    SegmentShape m_shape{SegmentShape::Polynomial};
};

/**
 * @ingroup generators
 * @brief Multi-segment envelope: an ordered chain of EnvelopeShaper segments, one of which may
 * be marked as the sustain point.
 *
 * Reaching the sustain segment holds there indefinitely until release() is
 * called; with no sustain point set, or in one-shot mode, the chain simply
 * runs to its last segment and stays at that target. SegmentCount is a
 * template parameter so each concrete envelope shape (see AdsEnvelope below)
 * pays for exactly the segments it uses.
 *
 * A segment is shaped by CurvedValue (curve is its factor) or, with SegmentShape::Exponential, by
 * ExponentialApproach (curve is the number of time constants within the segment). The values are plain
 * numbers: a gain, a MIDI note or a cutoff in any unit.
 */
template <size_t SegmentCount>
class Envelope
{
  public:
    struct SegmentValues
    {
        size_t frames{0};
        float targetGain{0.f};
        float curve{0.f};
        SegmentShape shape{SegmentShape::Polynomial};
    };

    explicit Envelope(const float sampleRate) noexcept
        : m_samplesPerMillisecond(sampleRate / 1000.f)
    {
    }

    void setOneshot(const bool oneshot) noexcept
    {
        m_oneshot = oneshot;
    }

    void setSegment(const size_t index, const float timeInMilliseconds, const float targetGain, const float curve = 0.f,
                    const SegmentShape shape = SegmentShape::Polynomial) noexcept
    {
        assert(index < SegmentCount);
        m_segments[index].frames = static_cast<size_t>(timeInMilliseconds * m_samplesPerMillisecond);
        m_segments[index].targetGain = targetGain;
        m_segments[index].curve = curve;
        m_segments[index].shape = shape;
    }

    void setSegmentHoldPreviousValue(const size_t index, const float timeInMilliseconds) noexcept
    {
        assert(index > 0 && index < SegmentCount);
        m_segments[index].frames = static_cast<size_t>(timeInMilliseconds * m_samplesPerMillisecond);
        m_segments[index].targetGain = m_segments[index - 1].targetGain;
        m_segments[index].curve = 0.f;
        m_segments[index].shape = SegmentShape::Polynomial;
    }

    void markSustainAt(const size_t index) noexcept
    {
        assert(index < SegmentCount);
        m_sustainIndex = index;
        if (index > 0)
        {
            m_segments[index].targetGain = m_segments[index - 1].targetGain;
        }
    }

    void noSustain() noexcept
    {
        m_sustainIndex = SegmentCount;
    }

    /// @brief If the given segment is the one currently playing, retargets it in place (keeping
    /// its remaining time) to the target gain it's now configured with. A no-op otherwise: it
    /// only catches a live change to a segment already in flight, not one still ahead.
    void modifyTargetIfActive(const size_t index) noexcept
    {
        if (m_currentIndex == index)
        {
            m_currentSegment.modifyTarget(m_segments[m_currentIndex].targetGain);
        }
    }

    /// @brief If the given segment is the one currently playing, restarts it over numSamples
    /// toward its configured target. Unlike modifyTargetIfActive(), this also changes the
    /// remaining duration - for a change that should land quickly rather than finish on schedule.
    void quickModifyIfSegmentActive(const size_t index, const size_t numSamples = 100) noexcept
    {
        if (m_currentIndex == index)
        {
            startSegment(m_currentIndex, numSamples);
        }
    }

    /// @brief Jumps straight to the last segment with a caller-chosen (typically short) release
    /// time, overriding whatever segment was playing. For voice stealing: a declick that doesn't
    /// wait for the normal release to reach its turn.
    void emergencyRelease(const float timeInMilliseconds) noexcept
    {
        m_currentIndex = SegmentCount - 1;
        startSegment(m_currentIndex, static_cast<size_t>(timeInMilliseconds * m_samplesPerMillisecond));
    }

    /// Starts the chain at startValue, jumping there at once; trigger() continues from the present value instead.
    void triggerFrom(const float startValue) noexcept
    {
        m_currentIndex = 0;
        m_currentSegment.reset(startValue);
        startSegment(0, m_segments[0].frames);
        m_isDone = false;
    }

    void trigger() noexcept
    {
        triggerFrom(m_currentSegment.currentGain());
    }

    void release() noexcept
    {
        if (m_oneshot || m_sustainIndex >= SegmentCount - 1)
        {
            return;
        }
        m_currentIndex = m_sustainIndex + 1;
        startSegment(m_currentIndex, m_segments[m_currentIndex].frames);
    }

    [[nodiscard]] float step() noexcept
    {
        if (m_isDone)
        {
            return m_currentSegment.currentGain();
        }
        const auto result = m_currentSegment.step();
        if (m_currentIndex == m_sustainIndex && !m_oneshot)
        {
            return result;
        }
        if (m_currentSegment.isDone())
        {
            ++m_currentIndex;
            if (m_currentIndex == SegmentCount)
            {
                m_isDone = true;
                return result;
            }
            if (m_currentIndex == m_sustainIndex && !m_oneshot)
            {
                m_currentSegment.setHold(0, m_segments[m_currentIndex].targetGain);
            }
            else
            {
                startSegment(m_currentIndex, m_segments[m_currentIndex].frames);
            }
        }
        return result;
    }

    [[nodiscard]] bool isDone() const noexcept
    {
        return m_isDone;
    }

    [[nodiscard]] size_t currentSegmentIndex() const noexcept
    {
        return m_currentIndex;
    }

  private:
    void startSegment(const size_t index, const size_t frames) noexcept
    {
        const SegmentValues& segment = m_segments[index];
        m_currentSegment.setNewFramesAndTarget(frames, segment.targetGain, segment.curve, segment.shape);
    }

    float m_samplesPerMillisecond;
    std::array<SegmentValues, SegmentCount> m_segments{};
    size_t m_currentIndex{0};
    size_t m_sustainIndex{SegmentCount};
    EnvelopeShaper m_currentSegment{};
    bool m_isDone{true};
    bool m_oneshot{false};
};

/**
 * @ingroup generators
 * @brief Attack-Decay-Sustain envelope: ramps to full scale, decays to a sustain level, then
 * holds there indefinitely.
 *
 * No release segment - built for voices with no note-off gate (a plucked or
 * struck string keeps ringing or gets re-triggered, it is never gated off).
 * The sustain point is a third, zero-length segment rather than the decay
 * segment itself: markSustainAt() overwrites its target with the previous
 * segment's, so marking the decay segment directly would clobber the sustain
 * level it was just given.
 */
class AdsEnvelope : public Envelope<3>
{
  public:
    explicit AdsEnvelope(const float sampleRate) noexcept
        : Envelope<3>(sampleRate)
    {
    }

    void setAttackDecaySustain(const float attackTimeMsecs, const float decayTimeMsecs, const float sustainLevel,
                               const float curve = 0.f) noexcept
    {
        setSegment(0, attackTimeMsecs, 1.f, curve);
        setSegment(1, decayTimeMsecs, sustainLevel, curve);
        setSegmentHoldPreviousValue(2, 0.f);
        markSustainAt(2);
    }
};

}
