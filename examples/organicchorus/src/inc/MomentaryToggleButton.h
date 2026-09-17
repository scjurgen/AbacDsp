#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

// Momentary pulse switch: holds its visual on-state for a minimum time
// regardless of trigger source (click, preset load, automation), since the
// bound boolean can revert within a single timer tick. The editor calls
// tickFlash() each frame to release it once that time has elapsed.
class MomentaryToggleButton : public juce::ToggleButton
{
  public:
    using juce::ToggleButton::ToggleButton;

    void tickFlash()
    {
        if (getToggleState() && juce::Time::getMillisecondCounter() >= m_flashUntilMs)
        {
            setToggleState(false, juce::sendNotification);
        }
    }

  protected:
    // Fires for ButtonAttachment-driven changes too, not just clicks.
    void buttonStateChanged() override
    {
        juce::ToggleButton::buttonStateChanged();
        const bool isOn = getToggleState();
        if (isOn && !m_wasOn)
        {
            m_flashUntilMs = juce::Time::getMillisecondCounter() + kMinimumFlashMs;
        }
        m_wasOn = isOn;
    }

  private:
    static constexpr juce::uint32 kMinimumFlashMs{300};
    juce::uint32 m_flashUntilMs{0};
    bool m_wasOn{false};
};