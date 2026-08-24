#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

// Read-only license/attribution popup. juce::AlertWindow::showMessageBoxAsync() assumes
// a short one-to-three-line message and re-flows anything longer with its own "balanced
// line lengths" heuristic, which garbles multi-paragraph text - a plain multi-line
// TextEditor lays out and wraps long text correctly instead. Launched via
// juce::DialogWindow::LaunchOptions, which owns this component and destroys it when the
// dialog closes.
class AboutWindow final : public juce::Component
{
  public:
    AboutWindow()
    {
        m_text.setMultiLine(true, true);
        m_text.setReadOnly(true);
        m_text.setCaretVisible(false);
        m_text.setScrollbarsShown(true);
        addAndMakeVisible(m_text);

        m_closeButton.setButtonText("Close");
        m_closeButton.onClick = [this] { closeParentDialog(); };
        addAndMakeVisible(m_closeButton);

        setSize(480, 360);
    }

    void setAboutText(const juce::String& text)
    {
        m_text.setText(text, juce::dontSendNotification);
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced(8);
        auto buttonRow = area.removeFromBottom(32);
        m_closeButton.setBounds(buttonRow.removeFromRight(90));
        area.removeFromBottom(4);
        m_text.setBounds(area);
    }

  private:
    void closeParentDialog()
    {
        if (auto* dw = findParentComponentOfClass<juce::DialogWindow>())
        {
            dw->exitModalState(0);
        }
    }

    juce::TextEditor m_text;
    juce::TextButton m_closeButton;
};
