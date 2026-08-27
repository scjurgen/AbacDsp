#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>
#include <vector>

#include "Audio/FixedSizeProcessor.h"
#include "Numbers/Interpolation.h"

namespace AbacDsp
{
/// @ingroup reverbs
/// @brief How a delay line gets from its current length to a requested one.
enum class ChangeSizeMode
{
    HARDSWITCH, ///< Move the read head at once. Cheapest, and audible as a click.
    FADE,       ///< Crossfade two read heads. Briefly comb-filters while both are live.
    PITCH,      ///< Glide the read rate. Bends pitch for the duration of the move.
};

/**
 * @ingroup reverbs
 * @brief Modulated delay line with no feedback and no filtering, sized at runtime.
 *
 * Deliberately stripped: a reverb tank supplies its own feedback path, damping
 * and mixing, so a line that duplicated any of it would either double the
 * filtering or fight the tank's own loop gain. What remains is storage,
 * interpolated reads, modulation, and the three ways of changing length.
 *
 * In FADE mode a request arriving mid-fade is held until the current one
 * finishes, then applied, so length changes queue rather than interrupt.
 * Six samples of wrap padding serve the interpolator.
 */
template <size_t MAXSIZE>
class ModulationDelayNoFeedback
{
  public:
    /// Samples kept between the modulated read head and the write head, so modulation cannot overtake it.
    static constexpr float modulationSafetyMargin{8.f};

    /// Target duration of a PITCH glide, independent of how large the resize is.
    static constexpr float kGlideDurationSeconds{0.25f};
    /// Upper bound on the net per-sample catch-up rate a glide can reach for a very large
    /// resize (keeps m_advance within (0.1, 1.9), away from 0 or negative - see
    /// adjustBufferByPitching()) - such a resize just takes longer than kGlideDurationSeconds
    /// instead of ever destabilizing the read head.
    static constexpr float kMaxNetGlideRate{0.9f};

    explicit ModulationDelayNoFeedback()
    {
        m_buffer.resize(MAXSIZE + 6, 0.f);
    }

    explicit ModulationDelayNoFeedback(const float sampleRate)
        : m_sampleRate(sampleRate)
    {
        m_buffer.resize(MAXSIZE + 6, 0.f);
    }

    void setSampleRate(const float rate)
    {
        m_sampleRate = rate;
    }

    void setChangeSizeMode(const ChangeSizeMode mode) noexcept
    {
        m_changeSizeMode = mode;
    }

    [[nodiscard]] ChangeSizeMode changeSizeMode() const noexcept
    {
        return m_changeSizeMode;
    }

    void relaxedInit()
    {
        if (!m_buffer.size())
        {
            m_buffer.resize(MAXSIZE + 6, 0.f);
        }
    }

    // Glide rate scales with the size of the change, targeting kGlideDurationSeconds
    // regardless of delta - a fixed rate (as this used to be) means settle time grows with
    // the delta, which for a resize spanning tens of thousands of samples (e.g. FdnTankGlide's
    // "Size" knob at ~144 samples/meter, see ModulationDelayNoFeedback_test.cpp) took multiple
    // seconds instead of a bounded, musically brief glide.
    void adjustBufferByPitching(const size_t newSize)
    {
        m_newDelayWidth = newSize;
        if (newSize > m_currentDelayWidth)
        {
            if (newSize == 0)
            {
                return;
            }
            m_advanceSteps = true;
            m_advance = 1.f - computeNetGlideRate(newSize - m_currentDelayWidth);
        }
        else
        {
            if (m_currentDelayWidth == 0)
            {
                return;
            }
            m_advanceSteps = true;
            m_advance = 1.f + computeNetGlideRate(m_currentDelayWidth - newSize);
        }
    }

    void setWidthInMsecs(const float milliseconds)
    {
        const auto newSize = getSamplesPerMillisecond(milliseconds, m_sampleRate, MAXSIZE);
        setSize(newSize);
    }

    void startNewFade()
    {
        if (m_newFadeSize && !m_fadeSteps)
        {
            m_fadeFactorIn = 0.0f;
            m_fadeFactorOut = 1.0f;

            m_fadeSteps = m_currentDelayWidth / 4;
            if (m_fadeSteps < 1024)
            {
                m_fadeSteps = 1024;
            }
            m_fadeAdvance = 1.0f / static_cast<float>(m_fadeSteps);
            m_headRead[1] = static_cast<float>(m_headWrite) - static_cast<float>(m_newFadeSize);
            if (m_headRead[1] < 0)
            {
                m_headRead[1] += MAXSIZE;
            }
            m_oldDelayWidth = m_currentDelayWidth;
            m_currentDelayWidth = m_newFadeSize;
        }
    }

    void setSize(size_t newSize)
    {
        if (newSize < 48)
        {
            newSize = 48;
        }
        if (newSize >= MAXSIZE)
        {
            newSize = MAXSIZE - 1;
        }
        if (newSize == m_currentDelayWidth)
        {
            return;
        }
        switch (m_changeSizeMode)
        {
            case ChangeSizeMode::PITCH:
                m_lastDelayWidthRequested = newSize;
                break;
            case ChangeSizeMode::HARDSWITCH:
                for (auto& mhd : m_headRead)
                {
                    mhd = static_cast<float>(m_headWrite) - static_cast<float>(newSize);
                    if (mhd < 0)
                    {
                        mhd += MAXSIZE;
                    }
                }
                // Was previously left stale here (unlike the PITCH/FADE branches), which meant
                // size()/the modulation-depth safety clamp (see nextHeadRead()) both still saw
                // whatever width was set before, not the one HARDSWITCH just jumped to.
                m_currentDelayWidth = newSize;
                break;
            case ChangeSizeMode::FADE:
                if (m_newFadeSize)
                {
                    m_newFadeSizeScheduled = newSize;
                }
                else
                {
                    m_newFadeSize = newSize;
                }
                break;
        }
    }

    void setModDepth(const float depth)
    {
        m_newModWidth = depth * 500.f;
        m_setNewModWidth = true;
    }

    void setModSpeed(const float speedHz)
    {
        // update only every 16 samples
        m_modAdvanceTick = 2.f * speedHz / 3000.f;
    }

    void sweepTick()
    {
        const auto nextPhase = m_currentPhase + m_modAdvanceTick;
        if (m_setNewModWidth)
        {
            if (m_currentPhase < 0 && nextPhase >= 0.0)
            {
                m_modWidth = m_newModWidth;
                m_setNewModWidth = false;
            }
        }
        m_currentPhase = nextPhase;
        if (m_currentPhase >= 1.0f)
        {
            m_currentPhase -= 2.0f;
        }
    }

    [[nodiscard]] float step(const float in)
    {
        return next(in);
    }

    void feedWrite(const float in)
    {
        m_buffer[m_headWrite] = in;

        // replicate values of begin at end, so we don't need to do handle InterPolate on splitted buffer
        if (m_headWrite < 6)
        {
            size_t padIndex = m_headWrite + MAXSIZE;
            m_buffer[padIndex] = m_buffer[m_headWrite];
        }

        if (++m_headWrite >= MAXSIZE)
        {
            m_headWrite = 0;
        }
    }
    void pitchAdvance(const size_t index)
    {
        if (m_advanceSteps)
        {
            int64_t dt;
            if (static_cast<int64_t>(m_headWrite) > static_cast<int64_t>(m_headRead[index]))
            {
                dt = static_cast<int64_t>(m_headWrite) - static_cast<int64_t>(m_headRead[index]);
            }
            else
            {
                dt = static_cast<int64_t>(m_headWrite + MAXSIZE) - static_cast<int64_t>(m_headRead[index]);
            }

            m_currentDelayWidth = static_cast<size_t>(dt);
            if (m_currentDelayWidth == m_newDelayWidth)
            {
                m_advanceSteps = false;
                m_advance = 1.0f;
            }
        }
        else
        {
            if (m_lastDelayWidthRequested)
            {
                adjustBufferByPitching(m_lastDelayWidthRequested);
                m_lastDelayWidthRequested = 0;
            }
        }
    }

    int_fast8_t m_tick{0};

    [[nodiscard]] float next(const float in)
    {
        m_tick++;
        m_tick &= 0xf;
        if (m_tick == 0)
        {
            sweepTick();
        }
        startNewFade();
        feedWrite(in);
        if (m_fadeSteps)
        {
            auto outvalue = nextHeadRead(0) * m_fadeFactorOut;
            auto invalue = nextHeadRead(1) * m_fadeFactorIn;
            m_fadeFactorOut -= m_fadeAdvance;
            m_fadeFactorIn += m_fadeAdvance;
            if (--m_fadeSteps == 0)
            {
                m_headRead[0] = m_headRead[1];
                m_newFadeSize = m_newFadeSizeScheduled;
                m_newFadeSizeScheduled = 0;
            }
            return outvalue + invalue;
        }
        else
        {
            pitchAdvance(0);
            return nextHeadRead(0);
        }
    }

    // Clamping m_modWidth here (rather than only where it's set) matters because
    // m_currentDelayWidth can keep shrinking sample by sample while PITCH-gliding towards a
    // shorter target - a depth that was safe for the old width could otherwise let the read
    // head reach the write head mid-glide, before any setModDepth/setSize call re-checks it.
    [[nodiscard]] float nextHeadRead(const size_t index)
    {
        auto dHead = m_headRead[index];
        float returnValue{};
        if (std::abs(m_modWidth) > 1E-7f)
        {
            const auto maxSafeWidth = std::max(0.f, static_cast<float>(m_currentDelayWidth) - modulationSafetyMargin);
            const auto safeModWidth = std::clamp(m_modWidth, -maxSafeWidth, maxSafeWidth);
            // m_currentPhase is a sawtooth in (-1, 1]; std::abs() of it would trace a *linear*
            // triangle (0 at the trough, 1 at both edges) whose rate of change is a constant that
            // flips sign at the trough - an abruptly alternating pitch shift rather than a smooth
            // one (see SineModulation.h / Modulation_test.cpp for the same issue and fix). Shaping
            // it as a raised cosine instead keeps the same 0-at-trough/1-at-edges range (so the
            // existing depth/size clamping and trough-only deferred update below stay correct)
            // but with a continuously-varying derivative.
            const auto shapedPhase = 0.5f * (1.0f - std::cos(std::numbers::pi_v<float> * m_currentPhase));
            const auto depth = safeModWidth * shapedPhase + 1.f;
            dHead += depth;
            if (dHead >= MAXSIZE)
            {
                dHead -= MAXSIZE;
            }
            float intTailPosition{};
            const auto fraction = std::modf(dHead, &intTailPosition);
            returnValue = Interpolation::linearPt2(&m_buffer[static_cast<size_t>(intTailPosition)], fraction);
        }
        else
        {
            returnValue = m_buffer[static_cast<size_t>(dHead)];
        }

        m_headRead[index] += m_advance;
        if (m_headRead[index] >= MAXSIZE)
        {
            m_headRead[index] -= MAXSIZE;
        }
        return returnValue;
    }

    void processBlock(const float* source, float* target, const size_t numSamples)
    {
        relaxedInit();
        std::transform(source, source + numSamples, target, [this](const float in) { return next(in); });
    }

    [[nodiscard]] size_t size() const noexcept
    {
        return m_currentDelayWidth;
    }

    // if fading, fade this buffer too.
    // this design is a bit ugly for fading, coupled too tight
    float readTap(const float percentage)
    {
        if (m_fadeSteps)
        {
            int headOut = MAXSIZE * 100 + static_cast<int>(m_headRead[0] + m_oldDelayWidth * (1.0 - percentage));
            int headIn = MAXSIZE * 100 + static_cast<int>(m_headRead[1] + m_currentDelayWidth * (1.0 - percentage));
            return m_buffer[headOut % MAXSIZE] * m_fadeFactorOut + m_buffer[headIn % MAXSIZE] * m_fadeFactorIn;
        }
        int head =
            MAXSIZE * 100 + static_cast<int>(m_headRead[m_currentHeadRead] + m_currentDelayWidth * (1.0 - percentage));
        return m_buffer[head % MAXSIZE];
    }

  private:
    // Net gap-change-per-sample needed to close `delta` samples in kGlideDurationSeconds,
    // clamped away from kMaxNetGlideRate so adjustBufferByPitching() never lets m_advance
    // reach 0 (frozen read head) or go negative (read head moving backwards - out of bounds
    // once cast to size_t in nextHeadRead()).
    [[nodiscard]] float computeNetGlideRate(const size_t delta) const noexcept
    {
        const auto glideDurationSamples = kGlideDurationSeconds * m_sampleRate;
        return std::clamp(static_cast<float>(delta) / glideDurationSamples, 0.f, kMaxNetGlideRate);
    }

    ChangeSizeMode m_changeSizeMode{ChangeSizeMode::FADE};

    float m_sampleRate{48000.0f}; // samplerate is needed for setting MAXSIZE in seconds
    std::array<float, 2> m_headRead{0, 0};

    size_t m_currentHeadRead{0};
    size_t m_headWrite{MAXSIZE / 8};
    // fade strategy
    size_t m_fadeSteps{0};
    float m_fadeFactorIn{0.0f};
    float m_fadeFactorOut{0.0f};
    float m_fadeAdvance{0.0f};
    size_t m_newFadeSize{0};
    size_t m_newFadeSizeScheduled{0};

    // adapt soft buffersize *speed up/down*
    size_t m_currentDelayWidth{MAXSIZE / 8};
    size_t m_oldDelayWidth{0};
    size_t m_newDelayWidth{0};
    size_t m_lastDelayWidthRequested{0};
    bool m_advanceSteps{false};
    float m_advance{1.0f};

    // modulation
    float m_modWidth{.1f};
    bool m_setNewModWidth{false};
    float m_newModWidth{0.0f};
    float m_modAdvanceTick{0.01f};
    float m_currentPhase{0.0f};
    //
    std::vector<float> m_buffer{};
};
}