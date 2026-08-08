#pragma once

#include <functional>
#include <juce_gui_basics/juce_gui_basics.h>

// Modal popup for editing a script's plain text (no syntax highlighting). Launched via
// juce::DialogWindow::LaunchOptions, which owns this component and destroys it when the
// dialog closes. Apply calls onApply and, only on success (empty returned string), closes
// the dialog itself; a non-empty return is shown inline and the dialog stays open so the
// user can fix a compile error without losing their edits. Cancel always just closes.
// Reset replaces the editor's text with onReset()'s skeleton but does not apply or close -
// it's still just an edit, undoable by Cancel, until Apply is clicked.
class ScriptEditorWindow final : public juce::Component
{
  public:
    // Returns an error message to display (and keep the dialog open), or an empty
    // string on success (closes the dialog).
    std::function<juce::String(const juce::String&)> onApply;
    // Returns the skeleton text to load into the editor.
    std::function<juce::String()> onReset;

    ScriptEditorWindow()
    {
        m_editor.setMultiLine(true, false);
        m_editor.setReturnKeyStartsNewLine(true);
        m_editor.setTabKeyUsedAsCharacter(true);
        m_editor.setFont(juce::Font(juce::FontOptions(14.f).withName(juce::Font::getDefaultMonospacedFontName())));
        addAndMakeVisible(m_editor);

        m_errorLabel.setColour(juce::Label::textColourId, juce::Colours::orangered);
        m_errorLabel.setJustificationType(juce::Justification::centredLeft);
        addAndMakeVisible(m_errorLabel);

        m_resetButton.setButtonText("Reset");
        m_resetButton.onClick = [this] { reset(); };
        addAndMakeVisible(m_resetButton);

        m_applyButton.setButtonText("Apply");
        m_applyButton.onClick = [this] { apply(); };
        addAndMakeVisible(m_applyButton);

        m_cancelButton.setButtonText("Cancel");
        m_cancelButton.onClick = [this] { closeParentDialog(); };
        addAndMakeVisible(m_cancelButton);

        setSize(800, 600);
    }

    void setScriptText(const juce::String& text)
    {
        m_editor.setText(text, juce::dontSendNotification);
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced(8);
        auto buttonRow = area.removeFromBottom(32);
        m_cancelButton.setBounds(buttonRow.removeFromRight(90));
        buttonRow.removeFromRight(8);
        m_applyButton.setBounds(buttonRow.removeFromRight(90));
        m_resetButton.setBounds(buttonRow.removeFromLeft(90));
        area.removeFromBottom(4);
        m_errorLabel.setBounds(area.removeFromBottom(20));
        area.removeFromBottom(4);
        m_editor.setBounds(area);
    }

  private:
    void apply()
    {
        if (!onApply)
        {
            return;
        }
        const auto error = onApply(m_editor.getText());
        if (error.isEmpty())
        {
            closeParentDialog();
        }
        else
        {
            m_errorLabel.setText(error, juce::dontSendNotification);
        }
    }

    void reset()
    {
        if (!onReset)
        {
            return;
        }
        m_editor.setText(onReset(), juce::dontSendNotification);
        m_errorLabel.setText({}, juce::dontSendNotification);
    }

    void closeParentDialog()
    {
        if (auto* dw = findParentComponentOfClass<juce::DialogWindow>())
        {
            dw->exitModalState(0);
        }
    }

    juce::TextEditor m_editor;
    juce::Label m_errorLabel;
    juce::TextButton m_resetButton;
    juce::TextButton m_applyButton;
    juce::TextButton m_cancelButton;
};
