#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

// Transient status line: shows a short message (e.g. from the Settings > Patches menu), then
// fades back to empty on its own. Paints itself directly rather than hosting a juce::Label,
// since GuiLookAndFeel::drawLabel hardcodes centred justification for every label in the app,
// which this bar's left-aligned text doesn't want.
class StatusBar final : public juce::Component, private juce::Timer
{
  public:
    // Kept for interface parity with the generated gauge/dial widgets, which all get a
    // setLabelText() call from the generator; this bar has no separate title of its own.
    void setLabelText(const juce::String& /*label*/) noexcept {}

    // persistent messages skip the auto-fade timer entirely - they stay until the next
    // showMessage() call of either kind replaces them.
    void showMessage(const juce::String& message, bool persistent = false)
    {
        m_message = message;
        stopTimer();
        if (!persistent)
        {
            startTimer(60000);
        }
        repaint();
    }

    void paint(juce::Graphics& g) override
    {
        g.setColour(juce::Colour(GuiConstants::instance().colors.labelColour));
        g.setFont(Constants::Text::fontHeight);
        g.drawFittedText(m_message, getLocalBounds().reduced(static_cast<int>(Constants::Margins::medium), 0),
                         juce::Justification::centredLeft, 1);
    }

  private:
    void timerCallback() override
    {
        stopTimer();
        m_message.clear();
        repaint();
    }

    juce::String m_message;
};
