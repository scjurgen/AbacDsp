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

    struct HostTransport
    {
        double bpm{120.0};
        double ppqPosition{0.0}; // quarter notes since session start
        float beatsPerBar{4.f};  // host time-signature numerator
        bool isPlaying{false};
        // Bumped once per host processBlock(), not per internal sub-block;
        // consumers use it to detect a fresh transport sample.
        uint64_t updateCount{0};
    };

    [[maybe_unused]] virtual void setHostTransport(const HostTransport& transport) noexcept
    {
        m_hostTransport = transport;
    }

    [[nodiscard]] [[maybe_unused]] const HostTransport& hostTransport() const noexcept
    {
        return m_hostTransport;
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

    HostTransport m_hostTransport{};

    size_t countNoteOn{0};
    size_t countNoteOff{0};
};
