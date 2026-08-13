#pragma once

#include <functional>
#include <juce_gui_basics/juce_gui_basics.h>

// Non-modal popup for viewing/editing a script's plain text (no syntax highlighting),
// hosted by ScriptEditorDialogWindow below rather than juce::DialogWindow::LaunchOptions,
// so the main plugin window stays interactive while this is open. The dropdown picks
// between the current patch script and any installed library script - a library is always
// shown read-only; the patch script's own read-only state is pushed in via setReadOnly()
// (true while LLM-Assist is active, so a live refresh from a watched-folder pull can never
// clobber an in-progress edit). Apply calls onApply and never closes the window itself, so
// the user can keep iterating; a non-empty return is shown inline, an empty one clears any
// prior error. Cancel always closes (via the parent window's closeButtonPressed()). Reset
// replaces the editor's text with onReset()'s skeleton but does not apply or close.
class ScriptEditorWindow final : public juce::Component
{
  public:
    // Returns an error message to display, or an empty string on success; either way the
    // dialog stays open.
    std::function<juce::String(const juce::String&)> onApply;
    // Returns the skeleton text to load into the editor.
    std::function<juce::String()> onReset;

    ScriptEditorWindow()
    {
        m_sourceCombo.addItem(kPatchScriptLabel, 1);
        m_sourceCombo.setSelectedId(1, juce::dontSendNotification);
        m_sourceCombo.onChange = [this] { sourceSelectionChanged(); };
        addAndMakeVisible(m_sourceCombo);

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

    // Caches `text` as the patch script's content; only visibly updates the editor if
    // "Patch Script" is the active dropdown selection, so a live refresh (e.g. from an
    // LLM-Assist pull) never pulls the view away from a library script being inspected.
    void setScriptText(const juce::String& text)
    {
        m_patchScriptText = text;
        if (isShowingPatchScript())
        {
            m_editor.setText(text, juce::dontSendNotification);
        }
    }

    // Remembered as the state to apply whenever "Patch Script" is selected; a library
    // selection is always read-only regardless of this flag.
    void setReadOnly(const bool readOnly)
    {
        m_patchReadOnly = readOnly;
        if (isShowingPatchScript())
        {
            applyReadOnlyState(readOnly);
        }
    }

    // Populates the dropdown with "Patch Script" plus each of `names`; textForName is
    // called lazily, only once a name is actually selected, not all fetched up front.
    void setLibraryScripts(const juce::StringArray& names, std::function<juce::String(const juce::String&)> textForName)
    {
        m_libraryScriptText = std::move(textForName);
        m_sourceCombo.clear(juce::dontSendNotification);
        m_sourceCombo.addItem(kPatchScriptLabel, 1);
        for (int i = 0; i < names.size(); ++i)
        {
            m_sourceCombo.addItem(names[i], i + 2);
        }
        m_sourceCombo.setSelectedId(1, juce::dontSendNotification);
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
        m_sourceCombo.setBounds(area.removeFromTop(24));
        area.removeFromTop(4);
        m_editor.setBounds(area);
    }

  private:
    static constexpr auto kPatchScriptLabel = "Patch Script";

    [[nodiscard]] bool isShowingPatchScript() const
    {
        return m_sourceCombo.getSelectedId() == 1;
    }

    void applyReadOnlyState(const bool readOnly)
    {
        m_editor.setReadOnly(readOnly);
        m_applyButton.setEnabled(!readOnly);
        m_resetButton.setEnabled(!readOnly);
    }

    void sourceSelectionChanged()
    {
        m_errorLabel.setText({}, juce::dontSendNotification);
        if (isShowingPatchScript())
        {
            m_editor.setText(m_patchScriptText, juce::dontSendNotification);
            applyReadOnlyState(m_patchReadOnly);
            return;
        }
        const auto text = m_libraryScriptText ? m_libraryScriptText(m_sourceCombo.getText()) : juce::String{};
        m_editor.setText(text, juce::dontSendNotification);
        applyReadOnlyState(true);
    }

    void apply()
    {
        if (!onApply)
        {
            return;
        }
        const auto error = onApply(m_editor.getText());
        m_errorLabel.setText(error, juce::dontSendNotification);
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
            dw->closeButtonPressed();
        }
    }

    juce::ComboBox m_sourceCombo;
    juce::TextEditor m_editor;
    juce::Label m_errorLabel;
    juce::TextButton m_resetButton;
    juce::TextButton m_applyButton;
    juce::TextButton m_cancelButton;

    juce::String m_patchScriptText;
    bool m_patchReadOnly{false};
    std::function<juce::String(const juce::String&)> m_libraryScriptText;
};

// Non-modal host window for ScriptEditorWindow: setVisible(true) instead of
// enterModalState(), so the main plugin window - in particular any Lua-declared knobs the
// script just applied - stays interactive while this is open. Deletes itself on close (X
// button, Escape, or Cancel/Apply via ScriptEditorWindow::closeParentDialog() above),
// deferred via callAsync so it's safe even when triggered from a child button's own click
// handler mid-dispatch.
class ScriptEditorDialogWindow final : public juce::DialogWindow
{
  public:
    ScriptEditorDialogWindow(const juce::String& title, const juce::Colour& backgroundColour)
        : juce::DialogWindow(title, backgroundColour, true)
    {
    }

    void closeButtonPressed() override
    {
        juce::MessageManager::callAsync([this] { delete this; });
    }
};
