#pragma once

#include <functional>
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_gui_extra/juce_gui_extra.h>

#include "AppSettings.h"
#include "GuiConstants.h"

// Non-modal popup for viewing/editing a script's plain text, with Lua syntax highlighting,
// hosted by ScriptEditorDialogWindow below rather than juce::DialogWindow::LaunchOptions,
// so the main plugin window stays interactive while this is open. The dropdown picks
// between the current patch script and any installed library script - a library is always
// shown read-only; the patch script's own read-only state is pushed in via setReadOnly()
// (true while Authoring Mode is active, banner shown to make that state visible). Apply
// calls onApply and never closes the window itself, so the user can keep iterating; a
// non-empty return is shown inline, an empty one clears any prior error. Cancel always
// closes (via the parent window's closeButtonPressed()). Reset replaces the editor's text
// with onReset()'s skeleton but does not apply or close. Docs calls onOpenDocs and is
// disabled until setDocsAvailable(true) is called.
class ScriptEditorWindow final : public juce::Component
{
  public:
    // Returns an error message to display, or an empty string on success; either way the
    // dialog stays open.
    std::function<juce::String(const juce::String&)> onApply;
    // Returns the skeleton text to load into the editor.
    std::function<juce::String()> onReset;
    // Opens this example's scripting reference (called only while setDocsAvailable(true)).
    std::function<void()> onOpenDocs;

    ScriptEditorWindow()
        : m_editor(m_codeDocument, &m_tokeniser)
    {
        m_sourceCombo.addItem(kPatchScriptLabel, 1);
        m_sourceCombo.setSelectedId(1, juce::dontSendNotification);
        m_sourceCombo.onChange = [this] { sourceSelectionChanged(); };
        addAndMakeVisible(m_sourceCombo);

        m_editor.setTabSize(4, true);
        m_editor.setFont(juce::Font(juce::FontOptions(14.f).withName(juce::Font::getDefaultMonospacedFontName())));
        applyColourScheme();
        addAndMakeVisible(m_editor);

        m_errorLabel.setColour(juce::Label::textColourId, juce::Colours::orangered);
        m_errorLabel.setJustificationType(juce::Justification::centredLeft);
        addAndMakeVisible(m_errorLabel);

        m_authoringBanner.setJustificationType(juce::Justification::centred);
        m_authoringBanner.setFont(juce::Font(juce::FontOptions(13.f, juce::Font::bold)));
        m_authoringBanner.setColour(juce::Label::textColourId, juce::Colours::black);
        m_authoringBanner.setColour(juce::Label::backgroundColourId, juce::Colours::orange);
        addAndMakeVisible(m_authoringBanner);
        m_authoringBanner.setVisible(false);

        m_docsButton.setButtonText("Docs");
        m_docsButton.onClick = [this]
        {
            if (onOpenDocs)
            {
                onOpenDocs();
            }
        };
        m_docsButton.setEnabled(false);
        addAndMakeVisible(m_docsButton);

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
            m_editor.loadContent(text);
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
        refreshAuthoringBanner();
    }

    // Enables the Docs button; left disabled otherwise, so it never invites a click that
    // silently does nothing (e.g. when this example's scripting.html isn't available - see
    // onOpenDocs and the <Module>_SCRIPTING_DOCS_FILE compile definition).
    void setDocsAvailable(const bool available)
    {
        m_docsButton.setEnabled(available);
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
        buttonRow.removeFromLeft(8);
        m_docsButton.setBounds(buttonRow.removeFromLeft(90));
        area.removeFromBottom(4);
        m_errorLabel.setBounds(area.removeFromBottom(20));
        area.removeFromBottom(4);
        if (m_authoringBanner.isVisible())
        {
            m_authoringBanner.setBounds(area.removeFromTop(22));
            area.removeFromTop(4);
        }
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

    // Fixed syntax-highlighting palette, not derived from the app's accent theme: readers
    // expect a code editor's colours to be the familiar constant ones (these match VS
    // Code's Dark+/Light+ defaults), not something that shifts with the plugin's own hue.
    struct SyntaxPalette
    {
        juce::Colour background, lineNumberBackground, lineNumberText, selection, text, comment, keyword, string,
            number;
    };

    [[nodiscard]] static SyntaxPalette darkPalette()
    {
        return {juce::Colour(0xff1e1e1e), juce::Colour(0xff333333), juce::Colour(0xff858585),
                juce::Colour(0xff264f78), juce::Colour(0xffd4d4d4), juce::Colour(0xff6a9955),
                juce::Colour(0xff569cd6), juce::Colour(0xffce9178), juce::Colour(0xffb5cea8)};
    }

    [[nodiscard]] static SyntaxPalette lightPalette()
    {
        return {juce::Colour(0xffffffff), juce::Colour(0xfff3f3f3), juce::Colour(0xff237893),
                juce::Colour(0xffadd6ff), juce::Colour(0xff000000), juce::Colour(0xff008000),
                juce::Colour(0xff0000ff), juce::Colour(0xffa31515), juce::Colour(0xff098658)};
    }

    // One-time; no live theme-switch hook, so a reopen picks up a light/dark change rather
    // than an open instance updating in place. Dark/light comes from the app's own
    // background lightness, not the raw theme enum, so nothing else needs plumbing in.
    void applyColourScheme()
    {
        const bool isDark = juce::Colour(GuiConstants::instance().colors.background).getPerceivedBrightness() < 0.5f;
        const auto p = isDark ? darkPalette() : lightPalette();

        m_editor.setColour(juce::CodeEditorComponent::backgroundColourId, p.background);
        m_editor.setColour(juce::CodeEditorComponent::highlightColourId, p.selection);
        m_editor.setColour(juce::CodeEditorComponent::defaultTextColourId, p.text);
        m_editor.setColour(juce::CodeEditorComponent::lineNumberBackgroundId, p.lineNumberBackground);
        m_editor.setColour(juce::CodeEditorComponent::lineNumberTextId, p.lineNumberText);

        juce::CodeEditorComponent::ColourScheme scheme;
        scheme.set("Error", juce::Colours::red);
        scheme.set("Comment", p.comment);
        scheme.set("Keyword", p.keyword);
        scheme.set("Operator", p.text);
        scheme.set("Identifier", p.text);
        scheme.set("Integer", p.number);
        scheme.set("Float", p.number);
        scheme.set("String", p.string);
        scheme.set("Bracket", p.text);
        scheme.set("Punctuation", p.text);
        m_editor.setColourScheme(scheme);
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
            m_editor.loadContent(m_patchScriptText);
            applyReadOnlyState(m_patchReadOnly);
            refreshAuthoringBanner();
            return;
        }
        const auto text = m_libraryScriptText ? m_libraryScriptText(m_sourceCombo.getText()) : juce::String{};
        m_editor.loadContent(text);
        applyReadOnlyState(true);
        refreshAuthoringBanner();
    }

    // Distinct from a library script's always-read-only state: this banner only appears
    // when the patch script itself is view-only because Authoring Mode is running.
    void refreshAuthoringBanner()
    {
        const bool active = isShowingPatchScript() && m_patchReadOnly;
        if (m_authoringBanner.isVisible() == active)
        {
            return;
        }
        m_authoringBanner.setText(active ? "Authoring Mode active - script is view-only" : juce::String{},
                                  juce::dontSendNotification);
        m_authoringBanner.setVisible(active);
        resized();
    }

    void apply()
    {
        if (!onApply)
        {
            return;
        }
        const auto error = onApply(m_codeDocument.getAllContent());
        m_errorLabel.setText(error, juce::dontSendNotification);
    }

    void reset()
    {
        if (!onReset)
        {
            return;
        }
        m_editor.loadContent(onReset());
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
    juce::CodeDocument m_codeDocument;
    juce::LuaTokeniser m_tokeniser;
    juce::CodeEditorComponent m_editor;
    juce::Label m_errorLabel;
    juce::Label m_authoringBanner;
    juce::TextButton m_docsButton;
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
// handler mid-dispatch. Persists its own position/size (see AppSettings::save/
// loadScriptEditorBounds) independently of the main plugin window's bounds, restored the
// next time this dialog is opened - even across app restarts.
class ScriptEditorDialogWindow final : public juce::DialogWindow, private juce::ComponentListener
{
  public:
    ScriptEditorDialogWindow(const juce::String& title, const juce::Colour& backgroundColour)
        : juce::DialogWindow(title, backgroundColour, true)
    {
        addComponentListener(this);
    }

    void closeButtonPressed() override
    {
        juce::MessageManager::callAsync([this] { delete this; });
    }

    // setContentOwned()'s auto-fit-to-content and setVisible()'s own on-screen settling
    // both move/resize this window and would otherwise be saved as if the user had done
    // it, clobbering the real last-dragged position before it's ever read back on the next
    // open. Call once setup (including setVisible) is fully done.
    void armBoundsPersistence()
    {
        m_persistBounds = true;
    }

  private:
    void componentMovedOrResized(juce::Component&, bool, bool) override
    {
        if (!m_persistBounds)
        {
            return;
        }
        AppSettings::saveScriptEditorBounds(getBounds());
    }

    bool m_persistBounds{false};
};
