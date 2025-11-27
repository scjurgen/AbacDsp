#pragma once

#include "AudioProcessing.h"
#include "InterpolationCollection.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace AbacDsp
{
enum class ChangeSizeMode
{
    HARDSWITCH,
    FADE,
    PITCH,
};

/*
 * no filters
 * no feedback
 * modulation
 * soft size adaption
 *
 * sizes: changed realtime when pitching or jump
 * in FADE it waits until fade done, then starts new fade for last position request
 *
 */

template <size_t MAXSIZE>
class ModulationDelayNoFeedback
{
  public:
    ModulationDelayNoFeedback()
    {
        m_buffer.resize(MAXSIZE + 6, 0.f);
    }

    ModulationDelayNoFeedback(const float sampleRate)
        : m_sampleRate(sampleRate)
    {
        m_buffer.resize(MAXSIZE + 6, 0.f);
    }

    void setSampleRate(const float rate)
    {
        m_sampleRate = rate;
    }

    void relaxedInit()
    {
        if (!m_buffer.size())
        {
            m_buffer.resize(MAXSIZE + 6, 0.f);
        }
    }

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
            m_advance = 0.80f;
        }
        else
        {
            if (m_currentDelayWidth == 0)
            {
                return;
            }
            m_advanceSteps = true;
            m_advance = 1.25;
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
            //            print_log("new delay width: %d hd[0]: %f hd[1]: %f steps: %d (max: %d)", m_currentDelayWidth,
            //            m_headRead[0],
            //                      m_headRead[1], m_fadeSteps, MAXSIZE);
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
                    mhd = static_cast<float>(m_headWrite) - newSize;
                    if (mhd < 0)
                    {
                        mhd += MAXSIZE;
                    }
                }
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
        m_newModWidth = depth * 100.f;
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

    float step(const float in)
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
            if (m_headWrite > m_headRead[index])
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

    float next(const float in)
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

    float nextHeadRead(const size_t index)
    {
        auto dHead = m_headRead[index];
        float returnValue;
        if (std::abs(m_modWidth) > 1E-7f)
        {
            const auto depth = m_modWidth * (fabsf(m_currentPhase)) + 1; // triangular wave
            dHead += depth;
            if (dHead >= MAXSIZE)
            {
                dHead -= MAXSIZE;
            }
            float intTailPosition;
            const auto fraction = modff(dHead, &intTailPosition);
            returnValue = Interpolation<float>::linearPt2(&m_buffer[static_cast<size_t>(intTailPosition)], fraction);
        }
        else
        {
            // if (m_setNewModWidth)
            //{
            //     m_modWidth = m_newModWidth;
            //     m_currentPhase = 0;
            // }
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
        std::transform(source, source + numSamples, target, [this](float in) { return next(in); });
    }

    size_t size() const
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