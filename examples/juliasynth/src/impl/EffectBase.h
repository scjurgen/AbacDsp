#pragma once

#include <cstdint>
#include <vector>

class EffectBase
{
  public:
    virtual ~EffectBase() = default;
    explicit EffectBase(const float sampleRate)
        : m_sampleRate(sampleRate)
    {
    }

    [[maybe_unused]] virtual void setBpm(float bpm)
    {
        m_bpm = bpm;
    }

    [[maybe_unused]] virtual void setSpeed(float speed)
    {
        m_playing = speed == 1.f;
    }

    [[maybe_unused]] virtual void setBeat(float beat)
    {
        m_beat = beat;
    }

    [[maybe_unused]] virtual void setBeatsPerBar(float beatPerBar)
    {
        m_beatsPerBar = beatPerBar;
    }

    [[maybe_unused]] virtual void processMidi(const uint8_t* msg)
    {
        switch (msg[0] & 0xF0)
        {
            case 0x90:
                countNoteOn++;
                break;
            case 0x80:
                countNoteOff++;
                break;
            default:
                break;
        }
    }

    [[nodiscard]] [[maybe_unused]] float sampleRate() const
    {
        return m_sampleRate;
    }

    [[nodiscard]] [[maybe_unused]] float currentBpm() const
    {
        return m_bpm;
    }

    [[nodiscard]] [[maybe_unused]] bool isPlaying() const
    {
        return m_playing;
    }

    [[nodiscard]] [[maybe_unused]] float currentBeat() const
    {
        return m_beat;
    }

    [[nodiscard]] [[maybe_unused]] float currentBeatsPerBar() const
    {
        return m_beatsPerBar;
    }

    [[nodiscard]] [[maybe_unused]] size_t noteOnCount() const
    {
        return countNoteOn;
    }

    [[nodiscard]] [[maybe_unused]] size_t noteOffCount() const
    {
        return countNoteOff;
    }

  private:
    const float m_sampleRate;

    float m_bpm{120.f};
    bool m_playing{false};
    float m_beat{0};
    float m_beatsPerBar{4};

    size_t countNoteOn{0};
    size_t countNoteOff{0};
};
