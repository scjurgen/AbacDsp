#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

// Transient status line: shows a short message (e.g. from PatchBrowser::onStatus), then
// fades back to empty on its own. Purely a display widget; callers just call showMessage().
class StatusBar final : public juce::Component, private juce::Timer
{
  public:
    StatusBar()
    {
        addAndMakeVisible(m_label);
        m_label.setJustificationType(juce::Justification::centredLeft);
    }

    // Kept for interface parity with the generated gauge/dial widgets, which all get a
    // setLabelText() call from the generator; this bar has no separate title of its own.
    void setLabelText(const juce::String& /*label*/) noexcept {}

    void showMessage(const juce::String& message, bool isError = false)
    {
        m_label.setText(message, juce::dontSendNotification);
        m_label.setColour(juce::Label::textColourId, isError ? juce::Colours::orangered : juce::Colours::lightgreen);
        startTimer(2500);
    }

    void resized() override
    {
        m_label.setBounds(getLocalBounds());
    }

  private:
    void timerCallback() override
    {
        stopTimer();
        m_label.setText({}, juce::dontSendNotification);
    }

    juce::Label m_label;
};
