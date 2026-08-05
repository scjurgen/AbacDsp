#pragma once

#include <cmath>
#include <cstdint>

#include "Filters/SvfResoBP.h"

namespace AbacDsp
{

/// @ingroup generators
/// @brief Which of the three click voices a beat triggers, or none.
enum class ClickAccent : uint8_t
{
    None,
    Sub,
    Beat,
    Downbeat
};

/// @ingroup generators
/// @brief Pitches for the three accents and their shared decay.
/// Accents are distinguished by pitch rather than level, which survives being played quietly.
struct ClickVoiceConfig
{
    float downbeatFrequencyHz{400.f};
    float beatFrequencyHz{800.f};
    float subFrequencyHz{1600.f};
    float decaySeconds{0.04f};
};

/**
 * @ingroup generators
 * @brief Three damped-sine click voices sharing one output.
 *
 * A damped sine is the whole click: an exponentially decaying sinusoid has a
 * compact envelope and a single spectral peak, so it reads as a transient
 * without the broadband spray of a filtered impulse and without needing a
 * sample.
 */
class ClickGenerator
{
  public:
    static constexpr float kBoostDb{24.f};
    static constexpr float kSubDefaultOffsetDb{-9.f};

    explicit ClickGenerator(const float sampleRate, const ClickVoiceConfig& config = {})
        : m_downbeatFilter(sampleRate)
        , m_beatFilter(sampleRate)
        , m_subFilter(sampleRate)
    {
        m_downbeatFilter.setByDecay(0, config.downbeatFrequencyHz, config.decaySeconds);
        m_beatFilter.setByDecay(0, config.beatFrequencyHz, config.decaySeconds);
        m_subFilter.setByDecay(0, config.subFrequencyHz, config.decaySeconds);
    }

    void setVolumeDb(const float valueDb) noexcept
    {
        m_gain = dbToGain(valueDb + kBoostDb);
    }

    void setSubVolumeDb(const float valueDb) noexcept
    {
        m_subGain = dbToGain(valueDb + kBoostDb);
    }

    void trigger(const ClickAccent accent) noexcept
    {
        switch (accent)
        {
            case ClickAccent::Downbeat:
                m_downbeatFilter.reset(0.f, m_gain);
                break;
            case ClickAccent::Beat:
                m_beatFilter.reset(0.f, m_gain);
                break;
            case ClickAccent::Sub:
                m_subFilter.reset(0.f, m_subGain);
                break;
            case ClickAccent::None:
                break;
        }
    }

    void triggerSub() noexcept
    {
        m_subFilter.reset(0.f, m_subGain);
    }

    [[nodiscard]] float step0() noexcept
    {
        return m_downbeatFilter.step0() + m_beatFilter.step0() + m_subFilter.step0();
    }

    void reset() noexcept
    {
        m_downbeatFilter.reset();
        m_beatFilter.reset();
        m_subFilter.reset();
    }

  private:
    [[nodiscard]] static float dbToGain(const float valueDb) noexcept
    {
        return std::pow(10.f, valueDb / 20.f);
    }

    SvfResoBP m_downbeatFilter;
    SvfResoBP m_beatFilter;
    SvfResoBP m_subFilter;
    float m_gain{dbToGain(-6.f + kBoostDb)};
    float m_subGain{dbToGain(-6.f + kSubDefaultOffsetDb + kBoostDb)};
};

}
