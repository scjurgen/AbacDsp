#pragma once
/*
 * AUTO GENERATED,
 * NOT A GOOD IDEA TO CHANGE STUFF HERE
 * Keep the file readonly
 */

#include <map>

#include "PingsynthProcessor.h"
#include "UiElements.h"


class AudioPluginAudioProcessorEditor : public juce::AudioProcessorEditor,
                                        juce::Timer,
                                        juce::MenuBarModel,
                                        juce::ComponentListener
{
  public:
    explicit AudioPluginAudioProcessorEditor(AudioPluginAudioProcessor& p, juce::AudioProcessorValueTreeState& vts)
        : AudioProcessorEditor(&p)
        , processorRef(p)
        , valueTreeState(vts)
        , backgroundApp(juce::Colour(GuiConstants::instance().colors.background))
        , m_menuBar(this)
    {
        m_laf = std::make_unique<GuiLookAndFeel>();
        setLookAndFeel(m_laf.get());
        juce::LookAndFeel::setDefaultLookAndFeel(m_laf.get());
        addAndMakeVisible(m_menuBar);
        addAndMakeVisible(m_statusBar);
        initWidgets();
        setResizable(true, true);
        setResizeLimits(GuiConstants::instance().init.WindowWidth, GuiConstants::instance().init.WindowHeight, 4000,
                        3000);
        // Saved window bounds (position in particular) only make sense for the
        // Standalone app's own OS window. Applying a remembered on-screen X/Y to
        // a hosted plugin editor's top-level component can push its native peer
        // to coordinates outside any connected display, leaving the host with
        // an empty content area even though the editor itself constructed fine.
        if (processorRef.wrapperType == juce::AudioProcessor::wrapperType_Standalone)
        {
            const auto saved = AppSettings::loadWindowBounds(GuiConstants::instance().init.WindowWidth,
                                                             GuiConstants::instance().init.WindowHeight);
            setSize(saved.getWidth(), saved.getHeight());
        }
        else
        {
            setSize(GuiConstants::instance().init.WindowWidth, GuiConstants::instance().init.WindowHeight);
        }
        startTimerHz(GuiConstants::instance().init.TimerHertz);
    }

    ~AudioPluginAudioProcessorEditor() override
    {
        if (m_topLevel != nullptr)
        {
            m_topLevel->removeComponentListener(this);
        }
        stopTimer();
        juce::LookAndFeel::setDefaultLookAndFeel(nullptr);
        setLookAndFeel(nullptr);
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(backgroundApp);
    }

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wfloat-conversion"
    void resized() override
    {
        auto area = getLocalBounds();
        m_menuBar.setBounds(area.removeFromTop(getLookAndFeel().getDefaultMenuBarHeight()));
        m_statusBar.setBounds(area.removeFromBottom(static_cast<int>(Constants::Text::labelHeight)));
        auto pageSwitchArea = area.removeFromTop(static_cast<int>(Constants::Text::labelHeight));
        m_pagePerformanceButton.setBounds(pageSwitchArea.removeFromLeft(pageSwitchArea.getWidth() / 2));
        m_pageSettingsButton.setBounds(pageSwitchArea);
        area = area.reduced(static_cast<int>(Constants::Margins::big));
        if (m_currentPage == Page::Performance)
        {
            // auto generated
            // const juce::FlexItem::Margin knobMargin = juce::FlexItem::Margin(Constants::Margins::small);
            const juce::FlexItem::Margin knobMarginSmall = juce::FlexItem::Margin(Constants::Margins::medium);
            std::vector<juce::Rectangle<int>> areas(2);
            const auto rowHeight = area.getHeight() / 1;
            areas[0] = area.removeFromTop(rowHeight * 1).reduced(Constants::Margins::small);
            areas[1] = area.reduced(Constants::Margins::small);

            {
                juce::FlexBox box;
                box.flexWrap = juce::FlexBox::Wrap::noWrap;
                box.flexDirection = juce::FlexBox::Direction::row;
                box.justifyContent = juce::FlexBox::JustifyContent::spaceAround;
                box.items.add(juce::FlexItem(levelGauge).withFlex(1).withMargin(knobMarginSmall));
                box.performLayout(areas[0].toFloat());
            }
            {
                juce::FlexBox box;
                box.flexWrap = juce::FlexBox::Wrap::noWrap;
                box.flexDirection = juce::FlexBox::Direction::row;
                box.justifyContent = juce::FlexBox::JustifyContent::spaceAround;
                box.items.add(juce::FlexItem(luaControlsLuaControlArea).withFlex(1).withMargin(knobMarginSmall));
                box.performLayout(areas[1].toFloat());
            }
        }
        else
        {
            // auto generated
            // const juce::FlexItem::Margin knobMargin = juce::FlexItem::Margin(Constants::Margins::small);
            const juce::FlexItem::Margin knobMarginSmall = juce::FlexItem::Margin(Constants::Margins::medium);

            std::vector<juce::Rectangle<int>> areas(3);
            const auto colWidth = area.getWidth() / 13;
            const auto rowHeight = area.getHeight() / 2;
            areas[0] = area.removeFromLeft(colWidth * 1).reduced(Constants::Margins::small);
            areas[1] = area.removeFromTop(rowHeight * 1).reduced(Constants::Margins::small);
            areas[2] = area.reduced(Constants::Margins::small);

            {
                juce::FlexBox box;
                box.flexWrap = juce::FlexBox::Wrap::noWrap;
                box.flexDirection = juce::FlexBox::Direction::column;
                box.justifyContent = juce::FlexBox::JustifyContent::spaceAround;
                box.items.add(juce::FlexItem(cpuGauge).withHeight(200).withMargin(knobMarginSmall));
                box.items.add(juce::FlexItem(levelGauge).withHeight(500).withMargin(knobMarginSmall));
                box.performLayout(areas[0].toFloat());
            }
            {
                juce::FlexBox box;
                box.flexWrap = juce::FlexBox::Wrap::noWrap;
                box.flexDirection = juce::FlexBox::Direction::row;
                box.justifyContent = juce::FlexBox::JustifyContent::spaceAround;
                box.items.add(juce::FlexItem(modeDrop)
                                  .withFlex(1)
                                  .withHeight(Constants::Text::labelHeight)
                                  .withAlignSelf(juce::FlexItem::AlignSelf::center)
                                  .withMargin(knobMarginSmall));
                box.items.add(juce::FlexItem(volDial).withFlex(1).withMargin(knobMarginSmall));
                box.items.add(juce::FlexItem(reverbLevelDial).withFlex(1).withMargin(knobMarginSmall));
                box.items.add(juce::FlexItem(spectrogramGauge).withWidth(600).withMargin(knobMarginSmall));
                box.performLayout(areas[1].toFloat());
            }
            {
                juce::FlexBox box;
                box.flexWrap = juce::FlexBox::Wrap::noWrap;
                box.flexDirection = juce::FlexBox::Direction::row;
                box.justifyContent = juce::FlexBox::JustifyContent::spaceAround;
                box.items.add(juce::FlexItem(luaControlsLuaControlArea).withFlex(1).withMargin(knobMarginSmall));
                box.performLayout(areas[2].toFloat());
            }
        }
    }
#pragma GCC diagnostic pop

    void timerCallback() override
    {
        if (processorRef.hasRunner())
        {
            cpuGauge.update(processorRef.getCpuLoad());
            levelGauge.update(processorRef.getInputDbLoad(), processorRef.getOutputDbLoad());
            spectrogramGauge.update(processorRef.getSpectrogram());

            pollScriptError();
            if (processorRef.hasRunner())
            {
                luaControlsLuaControlArea.refresh(toLuaControlDescriptors(processorRef.getLuaUiParamSlots()),
                                                  valueTreeState);
            }
            processorRef.consumeLastLearnedCc();
        }
    }

    void initWidgets()
    {
        addAndMakeVisible(modeDrop);
        modeDrop.addItemList(valueTreeState.getParameter("mode")->getAllValueStrings(), 1);
        modeDropAttachment =
            std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(valueTreeState, "mode", modeDrop);
        modeDrop.setTooltip(juce::String::fromUTF8("Mode (polyphonic, mpe)"));
        addAndMakeVisible(volDial);
        volDial.reset(valueTreeState, "vol");
        volDial.setLabelText(juce::String::fromUTF8("Vol"));
        volDial.setTooltip(juce::String::fromUTF8("Vol (-100 to 12 dB)"));
        addAndMakeVisible(reverbLevelDial);
        reverbLevelDial.reset(valueTreeState, "reverbLevel");
        reverbLevelDial.setLabelText(juce::String::fromUTF8("Reverb"));
        reverbLevelDial.setTooltip(juce::String::fromUTF8("Reverb (-120 to 0 dB)"));
        addAndMakeVisible(cpuGauge);
        cpuGauge.setLabelText(juce::String::fromUTF8("CPU"));
        cpuGauge.setTooltip(juce::String::fromUTF8("CPU (0 to 100 %)"));
        addAndMakeVisible(levelGauge);
        levelGauge.setLabelText(juce::String::fromUTF8("Level"));
        levelGauge.setTooltip(juce::String::fromUTF8("Level (0 to 100 %)"));
        addAndMakeVisible(spectrogramGauge);
        spectrogramGauge.setLabelText(juce::String::fromUTF8("Spectrogram"));
        spectrogramGauge.setTooltip(juce::String::fromUTF8("Spectrogram"));
        addAndMakeVisible(luaControlsLuaControlArea);
        addAndMakeVisible(luaParam1Dial);
        luaParam1Dial.reset(valueTreeState, "luaParam1");
        luaParam1Dial.setLabelText(juce::String::fromUTF8("Lua Param 1"));
        luaParam1Dial.setTooltip(juce::String::fromUTF8("Lua Param 1 (0 to 1)"));
        luaParam1Dial.setCcMappable(true, {[this] { processorRef.beginCcLearn(CcTarget::luaParam1); },
                                           [this] { return processorRef.getCcRange(CcTarget::luaParam1); },
                                           [this](float lo, float hi)
                                           { processorRef.setCcRange(CcTarget::luaParam1, lo, hi); },
                                           [this] { processorRef.clearCcAssignment(CcTarget::luaParam1); },
                                           [this] { return processorRef.getCcController(CcTarget::luaParam1); }});
        addAndMakeVisible(luaParam2Dial);
        luaParam2Dial.reset(valueTreeState, "luaParam2");
        luaParam2Dial.setLabelText(juce::String::fromUTF8("Lua Param 2"));
        luaParam2Dial.setTooltip(juce::String::fromUTF8("Lua Param 2 (0 to 1)"));
        luaParam2Dial.setCcMappable(true, {[this] { processorRef.beginCcLearn(CcTarget::luaParam2); },
                                           [this] { return processorRef.getCcRange(CcTarget::luaParam2); },
                                           [this](float lo, float hi)
                                           { processorRef.setCcRange(CcTarget::luaParam2, lo, hi); },
                                           [this] { processorRef.clearCcAssignment(CcTarget::luaParam2); },
                                           [this] { return processorRef.getCcController(CcTarget::luaParam2); }});
        addAndMakeVisible(luaParam3Dial);
        luaParam3Dial.reset(valueTreeState, "luaParam3");
        luaParam3Dial.setLabelText(juce::String::fromUTF8("Lua Param 3"));
        luaParam3Dial.setTooltip(juce::String::fromUTF8("Lua Param 3 (0 to 1)"));
        luaParam3Dial.setCcMappable(true, {[this] { processorRef.beginCcLearn(CcTarget::luaParam3); },
                                           [this] { return processorRef.getCcRange(CcTarget::luaParam3); },
                                           [this](float lo, float hi)
                                           { processorRef.setCcRange(CcTarget::luaParam3, lo, hi); },
                                           [this] { processorRef.clearCcAssignment(CcTarget::luaParam3); },
                                           [this] { return processorRef.getCcController(CcTarget::luaParam3); }});
        addAndMakeVisible(luaParam4Dial);
        luaParam4Dial.reset(valueTreeState, "luaParam4");
        luaParam4Dial.setLabelText(juce::String::fromUTF8("Lua Param 4"));
        luaParam4Dial.setTooltip(juce::String::fromUTF8("Lua Param 4 (0 to 1)"));
        luaParam4Dial.setCcMappable(true, {[this] { processorRef.beginCcLearn(CcTarget::luaParam4); },
                                           [this] { return processorRef.getCcRange(CcTarget::luaParam4); },
                                           [this](float lo, float hi)
                                           { processorRef.setCcRange(CcTarget::luaParam4, lo, hi); },
                                           [this] { processorRef.clearCcAssignment(CcTarget::luaParam4); },
                                           [this] { return processorRef.getCcController(CcTarget::luaParam4); }});
        addAndMakeVisible(luaParam5Dial);
        luaParam5Dial.reset(valueTreeState, "luaParam5");
        luaParam5Dial.setLabelText(juce::String::fromUTF8("Lua Param 5"));
        luaParam5Dial.setTooltip(juce::String::fromUTF8("Lua Param 5 (0 to 1)"));
        luaParam5Dial.setCcMappable(true, {[this] { processorRef.beginCcLearn(CcTarget::luaParam5); },
                                           [this] { return processorRef.getCcRange(CcTarget::luaParam5); },
                                           [this](float lo, float hi)
                                           { processorRef.setCcRange(CcTarget::luaParam5, lo, hi); },
                                           [this] { processorRef.clearCcAssignment(CcTarget::luaParam5); },
                                           [this] { return processorRef.getCcController(CcTarget::luaParam5); }});
        addAndMakeVisible(luaParam6Dial);
        luaParam6Dial.reset(valueTreeState, "luaParam6");
        luaParam6Dial.setLabelText(juce::String::fromUTF8("Lua Param 6"));
        luaParam6Dial.setTooltip(juce::String::fromUTF8("Lua Param 6 (0 to 1)"));
        luaParam6Dial.setCcMappable(true, {[this] { processorRef.beginCcLearn(CcTarget::luaParam6); },
                                           [this] { return processorRef.getCcRange(CcTarget::luaParam6); },
                                           [this](float lo, float hi)
                                           { processorRef.setCcRange(CcTarget::luaParam6, lo, hi); },
                                           [this] { processorRef.clearCcAssignment(CcTarget::luaParam6); },
                                           [this] { return processorRef.getCcController(CcTarget::luaParam6); }});
        addAndMakeVisible(luaParam7Dial);
        luaParam7Dial.reset(valueTreeState, "luaParam7");
        luaParam7Dial.setLabelText(juce::String::fromUTF8("Lua Param 7"));
        luaParam7Dial.setTooltip(juce::String::fromUTF8("Lua Param 7 (0 to 1)"));
        luaParam7Dial.setCcMappable(true, {[this] { processorRef.beginCcLearn(CcTarget::luaParam7); },
                                           [this] { return processorRef.getCcRange(CcTarget::luaParam7); },
                                           [this](float lo, float hi)
                                           { processorRef.setCcRange(CcTarget::luaParam7, lo, hi); },
                                           [this] { processorRef.clearCcAssignment(CcTarget::luaParam7); },
                                           [this] { return processorRef.getCcController(CcTarget::luaParam7); }});
        addAndMakeVisible(luaParam8Dial);
        luaParam8Dial.reset(valueTreeState, "luaParam8");
        luaParam8Dial.setLabelText(juce::String::fromUTF8("Lua Param 8"));
        luaParam8Dial.setTooltip(juce::String::fromUTF8("Lua Param 8 (0 to 1)"));
        luaParam8Dial.setCcMappable(true, {[this] { processorRef.beginCcLearn(CcTarget::luaParam8); },
                                           [this] { return processorRef.getCcRange(CcTarget::luaParam8); },
                                           [this](float lo, float hi)
                                           { processorRef.setCcRange(CcTarget::luaParam8, lo, hi); },
                                           [this] { processorRef.clearCcAssignment(CcTarget::luaParam8); },
                                           [this] { return processorRef.getCcController(CcTarget::luaParam8); }});

        addAndMakeVisible(m_pagePerformanceButton);
        addAndMakeVisible(m_pageSettingsButton);
        m_pagePerformanceButton.onClick = [this] { switchPage(Page::Performance); };
        m_pageSettingsButton.onClick = [this] { switchPage(Page::Settings); };
        switchPage(Page::Performance);
    }

    enum class Page
    {
        Performance,
        Settings
    };

    void switchPage(Page page)
    {
        m_currentPage = page;
        m_pagePerformanceButton.setToggleState(page == Page::Performance, juce::dontSendNotification);
        m_pageSettingsButton.setToggleState(page == Page::Settings, juce::dontSendNotification);
        if (page == Page::Performance)
        {
            modeDrop.setVisible(false);
            volDial.setVisible(false);
            reverbLevelDial.setVisible(false);
            cpuGauge.setVisible(false);
            levelGauge.setVisible(true);
            spectrogramGauge.setVisible(false);
            luaControlsLuaControlArea.setVisible(true);
            luaParam1Dial.setVisible(false);
            luaParam2Dial.setVisible(false);
            luaParam3Dial.setVisible(false);
            luaParam4Dial.setVisible(false);
            luaParam5Dial.setVisible(false);
            luaParam6Dial.setVisible(false);
            luaParam7Dial.setVisible(false);
            luaParam8Dial.setVisible(false);
        }
        else
        {
            modeDrop.setVisible(true);
            volDial.setVisible(true);
            reverbLevelDial.setVisible(true);
            cpuGauge.setVisible(true);
            levelGauge.setVisible(true);
            spectrogramGauge.setVisible(true);
            luaControlsLuaControlArea.setVisible(true);
            luaParam1Dial.setVisible(false);
            luaParam2Dial.setVisible(false);
            luaParam3Dial.setVisible(false);
            luaParam4Dial.setVisible(false);
            luaParam5Dial.setVisible(false);
            luaParam6Dial.setVisible(false);
            luaParam7Dial.setVisible(false);
            luaParam8Dial.setVisible(false);
        }
        resized();
    }

    void parentHierarchyChanged() override
    {
        auto* top = getTopLevelComponent();
        if (top == this)
        {
            return;
        }

        if (m_topLevel != top)
        {
            if (m_topLevel != nullptr)
            {
                m_topLevel->removeComponentListener(this);
            }
            m_topLevel = top;
            m_topLevel->addComponentListener(this);
        }

        if (processorRef.wrapperType == juce::AudioProcessor::wrapperType_Standalone && !m_boundsRestored &&
            m_topLevel->isOnDesktop())
        {
            const auto saved = AppSettings::loadWindowBounds(getWidth(), getHeight());
            m_topLevel->setTopLeftPosition(saved.getX(), saved.getY());
            m_boundsRestored = true;
        }
    }

    void componentMovedOrResized(juce::Component& component, bool /*wasMoved*/, bool /*wasResized*/) override
    {
        if (m_boundsRestored)
        {
            AppSettings::saveWindowBounds(component.getScreenBounds());
        }
    }

    juce::StringArray getMenuBarNames() override
    {
        juce::StringArray names{"Theme"};
        names.add("Patches");
        names.add("Scripts");

        names.add("About");
        return names;
    }

    juce::PopupMenu getMenuForIndex(int /*menuIndex*/, const juce::String& menuName) override
    {
        if (menuName == "Theme")
        {
            return buildThemeMenu();
        }
        if (menuName == "Patches")
        {
            return buildPatchesMenu();
        }
        if (menuName == "Scripts")
        {
            return buildScriptsMenu();
        }

        if (menuName == "About")
        {
            return buildAboutMenu();
        }
        return {};
    }

    juce::PopupMenu buildThemeMenu()
    {
        juce::PopupMenu colorMenu;
        for (int i = 0; i < Themes::kHueCount; ++i)
        {
            colorMenu.addItem(i + 1, Themes::kHueNames[static_cast<size_t>(i)], true,
                              i == Themes::hueIndex(m_currentTheme));
        }

        juce::PopupMenu baseMenu;
        baseMenu.addItem(kThemeBaseBichromaticId, "Bichromatic", true,
                         Themes::family(m_currentTheme) == ui::ThemeFamily::Bichromatic);
        baseMenu.addItem(kThemeBaseTrichromaticId, "Trichromatic", true,
                         Themes::family(m_currentTheme) == ui::ThemeFamily::Trichromatic);

        juce::PopupMenu modeMenu;
        modeMenu.addItem(kThemeModeLightId, "Light", true, !Themes::isDark(m_currentTheme));
        modeMenu.addItem(kThemeModeDarkId, "Dark", true, Themes::isDark(m_currentTheme));

        juce::PopupMenu menu;
        menu.addSubMenu("Color", colorMenu);
        menu.addSubMenu("Base", baseMenu);
        menu.addSubMenu("Mode", modeMenu);
        return menu;
    }

    juce::PopupMenu buildAboutMenu()
    {
        juce::PopupMenu menu;
        menu.addItem(kAboutShowInfoId, "License Info...");
        return menu;
    }

    void showAboutDialog()
    {
        const juce::String header = juce::String(JucePlugin_Name) + " v" + JucePlugin_VersionString;
        const juce::String body =
            juce::String(JucePlugin_Manufacturer) +
            "\n\n"
            "Lua-scripted resonator synth: a patch's script builds an arbitrary harmonic list per note and fires it "
            "into a bank of ringing resonators.\n\nPart of the AbacDsp project - core DSP library is MIT "
            "licensed.\n\nBuilt with JUCE, licensed under AGPLv3 (or a commercial JUCE licence).\n\nScripting powered "
            "by Lua and sol2 (both MIT licensed).\n\nFull third-party license details: THIRD-PARTY-LICENSES.md in the "
            "AbacDsp repository.";

        auto* aboutComponent = new AboutWindow();
        aboutComponent->setAboutText(body);

        juce::DialogWindow::LaunchOptions options;
        options.content.setOwned(aboutComponent);
        options.dialogTitle = header;
        options.dialogBackgroundColour = juce::Colour(GuiConstants::instance().colors.background);
        options.escapeKeyTriggersCloseButton = true;
        options.useNativeTitleBar = true;
        options.resizable = true;
        options.launchAsync();
    }

    void menuItemSelected(int menuItemID, int /*topLevelMenuIndex*/) override
    {
        if (menuItemID == kAboutShowInfoId)
        {
            showAboutDialog();
            return;
        }
        if (menuItemID >= 1 && menuItemID <= Themes::kHueCount)
        {
            applyTheme(Themes::withHue(m_currentTheme, menuItemID - 1));
            return;
        }
        if (menuItemID == kThemeModeLightId || menuItemID == kThemeModeDarkId)
        {
            applyTheme(Themes::withMode(m_currentTheme, menuItemID == kThemeModeDarkId));
            return;
        }
        if (menuItemID == kThemeBaseBichromaticId || menuItemID == kThemeBaseTrichromaticId)
        {
            const auto family =
                menuItemID == kThemeBaseTrichromaticId ? ui::ThemeFamily::Trichromatic : ui::ThemeFamily::Bichromatic;
            applyTheme(Themes::withFamily(m_currentTheme, family));
            return;
        }
        handlePatchMenuSelection(menuItemID);
        handleScriptMenuSelection(menuItemID);
    }

    void applyTheme(GuiConstants::Theme preset)
    {
        m_currentTheme = preset;
        AppSettings::saveTheme(preset);
        GuiConstants::setPreset(preset);
        setLookAndFeel(nullptr);
        m_laf = std::make_unique<GuiLookAndFeel>();
        setLookAndFeel(m_laf.get());
        juce::LookAndFeel::setDefaultLookAndFeel(m_laf.get());
        backgroundApp = juce::Colour(GuiConstants::instance().colors.background);
        cpuGauge.updateColors();
        levelGauge.updateColors();
        spectrogramGauge.setGradientPreset(preset);

        repaint();
    }

    // AlertWindow::addTextEditor() copies ComboBox::outlineColourId onto the editor
    // (transparent in this LookAndFeel), leaving it invisible until it gains focus;
    // restore a visible outline and hand it keyboard focus so typing works immediately.
    void focusNameEditor(juce::AlertWindow& dialog)
    {
        if (auto* editor = dialog.getTextEditor("name"))
        {
            editor->setColour(juce::TextEditor::outlineColourId,
                              juce::Colour(GuiConstants::instance().colors.statusOutline));
            editor->selectAll();
            editor->grabKeyboardFocus();
        }
    }

    // Joins a folder ("" means root) and a leaf name into the "folder/leaf" form
    // FileIo/LoopStorageService use on disk.
    [[nodiscard]] static juce::String combineFolderAndName(const juce::String& folder, const juce::String& name)
    {
        return folder.isEmpty() ? name : folder + "/" + name;
    }

    // Splits "folder/sub/leaf" back into {"folder/sub", "leaf"} to prefill a rename
    // dialog's two fields; folder is empty for a root-level name.
    [[nodiscard]] static std::pair<juce::String, juce::String> splitFolderAndName(const juce::String& fullName)
    {
        const int slashIndex = fullName.lastIndexOfChar('/');
        if (slashIndex < 0)
        {
            return {juce::String(), fullName};
        }
        return {fullName.substring(0, slashIndex), fullName.substring(slashIndex + 1)};
    }

    // The unique, sorted set of folder prefixes already used by an existing name list
    // (each entry "folder/leaf" or a root-level "leaf"), for a Save/Rename dialog's
    // folder dropdown.
    [[nodiscard]] static juce::StringArray collectFolderNames(const std::vector<juce::String>& names)
    {
        juce::StringArray folders;
        for (const auto& fullName : names)
        {
            const int slashIndex = fullName.lastIndexOfChar('/');
            if (slashIndex >= 0)
            {
                folders.addIfNotAlreadyThere(fullName.substring(0, slashIndex));
            }
        }
        folders.sort(false);
        return folders;
    }

    // Adds an editable folder dropdown to a Save/Rename dialog: existing folders to
    // pick from, or type a new one in the same box. currentValue empty means root.
    static void addFolderComboBox(juce::AlertWindow& dialog, const std::vector<juce::String>& existingNames,
                                  const juce::String& currentValue)
    {
        juce::StringArray items{"(none)"};
        items.addArray(collectFolderNames(existingNames));
        dialog.addComboBox("folder", items, "Folder:");
        if (auto* combo = dialog.getComboBoxComponent("folder"))
        {
            combo->setEditableText(true);
            combo->setText(currentValue.isEmpty() ? "(none)" : currentValue, juce::dontSendNotification);
        }
    }

    // Reads back addFolderComboBox()'s current value (picked or freely typed),
    // mapping the "(none)" placeholder back to root/empty.
    [[nodiscard]] static juce::String readFolderComboBox(const juce::AlertWindow& dialog)
    {
        if (auto* combo = dialog.getComboBoxComponent("folder"))
        {
            const auto text = combo->getText().trim();
            return text == "(none)" ? juce::String() : text;
        }
        return {};
    }

    // A "/" in a name (e.g. "chorus/classic tri chorus") groups it under a folder
    // submenu; root-level entries stay directly in the returned menu. Shared by the
    // patches and (when present) loops menus.
    juce::PopupMenu buildGroupedMenu(const std::vector<juce::String>& names, int idBase,
                                     const juce::String& tickedName = {})
    {
        juce::PopupMenu rootMenu;
        std::map<juce::String, juce::PopupMenu> folderMenus;
        for (size_t i = 0; i < names.size(); ++i)
        {
            const auto& fullName = names[i];
            const int itemId = idBase + static_cast<int>(i);
            const int slashIndex = fullName.lastIndexOfChar('/');
            if (slashIndex < 0)
            {
                rootMenu.addItem(itemId, fullName, true, fullName == tickedName);
            }
            else
            {
                const auto folder = fullName.substring(0, slashIndex);
                const auto leaf = fullName.substring(slashIndex + 1);
                folderMenus[folder].addItem(itemId, leaf, true, fullName == tickedName);
            }
        }
        for (auto& [folder, menu] : folderMenus)
        {
            rootMenu.addSubMenu(folder, menu);
        }
        return rootMenu;
    }

    juce::PopupMenu buildGroupedPatchMenu(int idBase, const juce::String& tickedName = {})
    {
        return buildGroupedMenu(m_patchMenuNames, idBase, tickedName);
    }

    juce::PopupMenu buildPatchesMenu()
    {
        m_patchMenuNames = processorRef.listPatchNames();
        const auto currentName = processorRef.getCurrentPatchName();

        auto loadMenu = buildGroupedPatchMenu(kPatchLoadIdBase, currentName);
        auto deleteMenu = buildGroupedPatchMenu(kPatchDeleteIdBase);
        auto renameMenu = buildGroupedPatchMenu(kPatchRenameIdBase);

        juce::PopupMenu patches;
        patches.addSubMenu("Load", loadMenu, !m_patchMenuNames.empty());
        patches.addItem(kPatchSaveId, "Save");
        patches.addItem(kPatchSaveAsId, "Save As...");
        patches.addSubMenu("Delete", deleteMenu, !m_patchMenuNames.empty());
        patches.addSubMenu("Rename", renameMenu, !m_patchMenuNames.empty());
        return patches;
    }

    void handlePatchMenuSelection(int menuItemID)
    {
        if (menuItemID == kPatchSaveId)
        {
            savePatchWithPrompt();
        }
        else if (menuItemID == kPatchSaveAsId)
        {
            promptSaveAs();
        }
        else if (menuItemID >= kPatchLoadIdBase &&
                 menuItemID < kPatchLoadIdBase + static_cast<int>(m_patchMenuNames.size()))
        {
            const auto& name = m_patchMenuNames[static_cast<size_t>(menuItemID - kPatchLoadIdBase)];
            processorRef.requestLoadPatch(name);
            m_statusBar.showMessage("Loaded '" + name + "'");
        }
        else if (menuItemID >= kPatchDeleteIdBase &&
                 menuItemID < kPatchDeleteIdBase + static_cast<int>(m_patchMenuNames.size()))
        {
            confirmAndDeletePatch(m_patchMenuNames[static_cast<size_t>(menuItemID - kPatchDeleteIdBase)]);
        }
        else if (menuItemID >= kPatchRenameIdBase &&
                 menuItemID < kPatchRenameIdBase + static_cast<int>(m_patchMenuNames.size()))
        {
            promptRename(m_patchMenuNames[static_cast<size_t>(menuItemID - kPatchRenameIdBase)]);
        }
    }

    void savePatchWithPrompt()
    {
        const auto currentName = processorRef.getCurrentPatchName();
        if (currentName.isEmpty())
        {
            promptSaveAs();
            return;
        }
        if (processorRef.saveCurrentPatchAs(currentName))
        {
            m_statusBar.showMessage("Saved '" + currentName + "'");
        }
        else
        {
            m_statusBar.showMessage("Save failed");
        }
    }

    void promptSaveAs()
    {
        const auto [folder, name] = splitFolderAndName(processorRef.getCurrentPatchName());
        m_patchNameDialog =
            std::make_unique<juce::AlertWindow>("Save Patch", juce::String(), juce::MessageBoxIconType::NoIcon);
        addFolderComboBox(*m_patchNameDialog, m_patchMenuNames, folder);
        m_patchNameDialog->addTextEditor("name", name, "Name:");
        m_patchNameDialog->addButton("Save", 1, juce::KeyPress(juce::KeyPress::returnKey));
        m_patchNameDialog->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));
        m_patchNameDialog->enterModalState(true,
                                           juce::ModalCallbackFunction::create(
                                               [this](int result)
                                               {
                                                   const auto folderText = readFolderComboBox(*m_patchNameDialog);
                                                   const auto nameText =
                                                       m_patchNameDialog->getTextEditorContents("name").trim();
                                                   m_patchNameDialog.reset();
                                                   if (result != 1 || nameText.isEmpty())
                                                   {
                                                       return;
                                                   }
                                                   const auto fullName = combineFolderAndName(folderText, nameText);
                                                   if (processorRef.saveCurrentPatchAs(fullName))
                                                   {
                                                       m_statusBar.showMessage("Saved '" + fullName + "'");
                                                   }
                                                   else
                                                   {
                                                       m_statusBar.showMessage("Save failed");
                                                   }
                                               }),
                                           false);
        focusNameEditor(*m_patchNameDialog);
    }

    void promptRename(const juce::String& oldName)
    {
        const auto [folder, name] = splitFolderAndName(oldName);
        m_patchNameDialog = std::make_unique<juce::AlertWindow>("Rename Patch \"" + oldName + "\"", juce::String(),
                                                                juce::MessageBoxIconType::NoIcon);
        addFolderComboBox(*m_patchNameDialog, m_patchMenuNames, folder);
        m_patchNameDialog->addTextEditor("name", name, "Name:");
        m_patchNameDialog->addButton("Rename", 1, juce::KeyPress(juce::KeyPress::returnKey));
        m_patchNameDialog->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));
        m_patchNameDialog->enterModalState(true,
                                           juce::ModalCallbackFunction::create(
                                               [this, oldName](int result)
                                               {
                                                   const auto folderText = readFolderComboBox(*m_patchNameDialog);
                                                   const auto nameText =
                                                       m_patchNameDialog->getTextEditorContents("name").trim();
                                                   m_patchNameDialog.reset();
                                                   if (result != 1 || nameText.isEmpty())
                                                   {
                                                       return;
                                                   }
                                                   const auto newName = combineFolderAndName(folderText, nameText);
                                                   if (newName == oldName)
                                                   {
                                                       return;
                                                   }
                                                   if (processorRef.renamePatch(oldName, newName))
                                                   {
                                                       m_statusBar.showMessage("Renamed to '" + newName + "'");
                                                   }
                                                   else
                                                   {
                                                       m_statusBar.showMessage("Rename failed");
                                                   }
                                               }),
                                           false);
        focusNameEditor(*m_patchNameDialog);
    }

    void confirmAndDeletePatch(const juce::String& name)
    {
        juce::NativeMessageBox::showAsync(juce::MessageBoxOptions()
                                              .withIconType(juce::MessageBoxIconType::WarningIcon)
                                              .withTitle("Delete Patch")
                                              .withMessage("Delete patch \"" + name + "\"?")
                                              .withButton("Yes")
                                              .withButton("No"),
                                          [this, name](int result)
                                          {
                                              if (result != 0)
                                              {
                                                  return;
                                              }
                                              if (processorRef.deletePatchNamed(name))
                                              {
                                                  m_statusBar.showMessage("Deleted '" + name + "'");
                                              }
                                              else
                                              {
                                                  m_statusBar.showMessage("Delete failed");
                                              }
                                          });
    }


    // Reuses buildGroupedMenu() from the PRESETBROWSER section above, same as
    // LOOPBROWSER does - a blueprint with a script port but no patches would need that
    // helper pulled out of its guard.
    void openScriptEditor()
    {
        // Non-modal (see ScriptEditorDialogWindow), so this can already be open - just
        // bring it forward rather than spawning a second editor.
        if (m_scriptEditorWindow != nullptr)
        {
            m_scriptEditorWindow->toFront(true);
            return;
        }

        juce::StringArray libraryScriptNames;
        for (const auto& n : processorRef.getLibraryScriptNames())
        {
            libraryScriptNames.add(n);
        }

        auto* editorComponent = new ScriptEditorWindow();
        editorComponent->setScriptText(processorRef.getScriptText());
        // While Authoring Mode is active, a manual edit could race with (and silently
        // lose to) a script an HTTP POST /script call applies - view-only instead of blocked.
        editorComponent->setReadOnly(processorRef.isAuthoringModeEnabled());
        editorComponent->setLibraryScripts(libraryScriptNames, [this](const juce::String& name)
                                           { return processorRef.getLibraryScriptText(name); });
        editorComponent->onApply = [this](const juce::String& text) -> juce::String
        {
            if (processorRef.applyScriptText(text))
            {
                m_statusBar.showMessage("Script applied");
                return {};
            }
            return juce::String(processorRef.scriptErrorMessage());
        };
        editorComponent->onReset = [this] { return juce::String(processorRef.getScriptSkeleton()); };

        auto* dialogWindow =
            new ScriptEditorDialogWindow("Edit Script", juce::Colour(GuiConstants::instance().colors.background));
        dialogWindow->setContentOwned(editorComponent, true);
        dialogWindow->setUsingNativeTitleBar(true);
        dialogWindow->setResizable(true, false);
        const auto savedBounds = AppSettings::loadScriptEditorBounds();
        if (savedBounds)
        {
            dialogWindow->setBounds(*savedBounds);
        }
        else
        {
            dialogWindow->centreAroundComponent(nullptr, dialogWindow->getWidth(), dialogWindow->getHeight());
        }
        dialogWindow->setVisible(true);
        // setVisible() can itself shift the window once the OS actually places it on
        // screen - reapply so it lands exactly where it was left, not off by that shift.
        if (savedBounds)
        {
            dialogWindow->setBounds(*savedBounds);
        }
        dialogWindow->armBoundsPersistence();
        m_scriptEditorWindow = dialogWindow;
        m_scriptEditorContent = editorComponent;
    }

    // Apply-time only catches errors the script hits while its top-level chunk runs
    // (i.e. at load); a script that compiles fine but errors when NextNotes()/OnTiming()
    // are actually called later (on the audio thread, once real data flows through it)
    // has nowhere else to surface that - poll for it instead. Called every timer tick
    // (see extra_timer_callbacks); tracks the last-shown message so a persistent error
    // doesn't keep resetting the status bar's fade timer forever.
    void pollScriptError()
    {
        if (!processorRef.hasScriptError())
        {
            m_lastScriptErrorShown.clear();
            return;
        }
        const auto message = juce::String(processorRef.scriptErrorMessage());
        if (message == m_lastScriptErrorShown)
        {
            return;
        }
        m_lastScriptErrorShown = message;
        m_statusBar.showMessage("Script error: " + message);
    }

    juce::PopupMenu buildScriptsMenu()
    {
        m_scriptMenuNames = processorRef.listScriptNames();
        const auto currentName = processorRef.getCurrentScriptName();

        auto loadMenu = buildGroupedMenu(m_scriptMenuNames, kScriptLoadIdBase, currentName);
        auto deleteMenu = buildGroupedMenu(m_scriptMenuNames, kScriptDeleteIdBase);
        auto renameMenu = buildGroupedMenu(m_scriptMenuNames, kScriptRenameIdBase);

        juce::PopupMenu scripts;
        scripts.addItem(kScriptEditId, "Edit...");
        scripts.addSubMenu("Load", loadMenu, !m_scriptMenuNames.empty());
        scripts.addItem(kScriptSaveAsId, "Save As...");
        scripts.addSubMenu("Delete", deleteMenu, !m_scriptMenuNames.empty());
        scripts.addSubMenu("Rename", renameMenu, !m_scriptMenuNames.empty());
        scripts.addSeparator();
        scripts.addSubMenu("Authoring Mode", buildAuthoringModeMenu());
        return scripts;
    }

    juce::PopupMenu buildAuthoringModeMenu()
    {
        const bool active = processorRef.isAuthoringModeEnabled();
        juce::PopupMenu menu;
        menu.addItem(kAuthoringModeToggleId, active ? "Disable" : "Enable", true, active);
        menu.addItem(kAuthoringModeOpenBrowserId, "Open in Browser", active);
        return menu;
    }

    void handleScriptMenuSelection(int menuItemID)
    {
        if (menuItemID == kAuthoringModeToggleId)
        {
            toggleAuthoringMode();
        }
        else if (menuItemID == kAuthoringModeOpenBrowserId)
        {
            processorRef.authoringDashboardUrl().launchInDefaultBrowser();
        }
        else if (menuItemID == kScriptEditId)
        {
            openScriptEditor();
        }
        else if (menuItemID == kScriptSaveAsId)
        {
            promptSaveScriptAs();
        }
        else if (menuItemID >= kScriptLoadIdBase &&
                 menuItemID < kScriptLoadIdBase + static_cast<int>(m_scriptMenuNames.size()))
        {
            const auto& name = m_scriptMenuNames[static_cast<size_t>(menuItemID - kScriptLoadIdBase)];
            if (processorRef.requestLoadScript(name))
            {
                m_statusBar.showMessage("Loaded '" + name + "'");
            }
            else
            {
                m_statusBar.showMessage("Load failed");
            }
        }
        else if (menuItemID >= kScriptDeleteIdBase &&
                 menuItemID < kScriptDeleteIdBase + static_cast<int>(m_scriptMenuNames.size()))
        {
            confirmAndDeleteScript(m_scriptMenuNames[static_cast<size_t>(menuItemID - kScriptDeleteIdBase)]);
        }
        else if (menuItemID >= kScriptRenameIdBase &&
                 menuItemID < kScriptRenameIdBase + static_cast<int>(m_scriptMenuNames.size()))
        {
            promptRenameScript(m_scriptMenuNames[static_cast<size_t>(menuItemID - kScriptRenameIdBase)]);
        }
    }

    void promptSaveScriptAs()
    {
        m_scriptNameDialog =
            std::make_unique<juce::AlertWindow>("Save Script", juce::String(), juce::MessageBoxIconType::NoIcon);
        addFolderComboBox(*m_scriptNameDialog, m_scriptMenuNames, "");
        m_scriptNameDialog->addTextEditor("name", "", "Name:");
        m_scriptNameDialog->addButton("Save", 1, juce::KeyPress(juce::KeyPress::returnKey));
        m_scriptNameDialog->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));
        m_scriptNameDialog->enterModalState(true,
                                            juce::ModalCallbackFunction::create(
                                                [this](int result)
                                                {
                                                    const auto folderText = readFolderComboBox(*m_scriptNameDialog);
                                                    const auto nameText =
                                                        m_scriptNameDialog->getTextEditorContents("name").trim();
                                                    m_scriptNameDialog.reset();
                                                    if (result != 1 || nameText.isEmpty())
                                                    {
                                                        return;
                                                    }
                                                    const auto fullName = combineFolderAndName(folderText, nameText);
                                                    if (processorRef.saveCurrentScriptAs(fullName))
                                                    {
                                                        m_statusBar.showMessage("Saved '" + fullName + "'");
                                                    }
                                                    else
                                                    {
                                                        m_statusBar.showMessage("Save failed");
                                                    }
                                                }),
                                            false);
        focusNameEditor(*m_scriptNameDialog);
    }

    void promptRenameScript(const juce::String& oldName)
    {
        const auto [folder, name] = splitFolderAndName(oldName);
        m_scriptNameDialog = std::make_unique<juce::AlertWindow>("Rename Script \"" + oldName + "\"", juce::String(),
                                                                 juce::MessageBoxIconType::NoIcon);
        addFolderComboBox(*m_scriptNameDialog, m_scriptMenuNames, folder);
        m_scriptNameDialog->addTextEditor("name", name, "Name:");
        m_scriptNameDialog->addButton("Rename", 1, juce::KeyPress(juce::KeyPress::returnKey));
        m_scriptNameDialog->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));
        m_scriptNameDialog->enterModalState(true,
                                            juce::ModalCallbackFunction::create(
                                                [this, oldName](int result)
                                                {
                                                    const auto folderText = readFolderComboBox(*m_scriptNameDialog);
                                                    const auto nameText =
                                                        m_scriptNameDialog->getTextEditorContents("name").trim();
                                                    m_scriptNameDialog.reset();
                                                    if (result != 1 || nameText.isEmpty())
                                                    {
                                                        return;
                                                    }
                                                    const auto newName = combineFolderAndName(folderText, nameText);
                                                    if (newName == oldName)
                                                    {
                                                        return;
                                                    }
                                                    if (processorRef.renameScript(oldName, newName))
                                                    {
                                                        m_statusBar.showMessage("Renamed to '" + newName + "'");
                                                    }
                                                    else
                                                    {
                                                        m_statusBar.showMessage("Rename failed");
                                                    }
                                                }),
                                            false);
        focusNameEditor(*m_scriptNameDialog);
    }

    void confirmAndDeleteScript(const juce::String& name)
    {
        juce::NativeMessageBox::showAsync(juce::MessageBoxOptions()
                                              .withIconType(juce::MessageBoxIconType::WarningIcon)
                                              .withTitle("Delete Script")
                                              .withMessage("Delete script \"" + name + "\"?")
                                              .withButton("Yes")
                                              .withButton("No"),
                                          [this, name](int result)
                                          {
                                              if (result != 0)
                                              {
                                                  return;
                                              }
                                              if (processorRef.deleteScriptNamed(name))
                                              {
                                                  m_statusBar.showMessage("Deleted '" + name + "'");
                                              }
                                              else
                                              {
                                                  m_statusBar.showMessage("Delete failed");
                                              }
                                          });
    }

    void toggleAuthoringMode()
    {
        const bool nowEnabled = processorRef.setAuthoringModeEnabled(!processorRef.isAuthoringModeEnabled());
        if (nowEnabled)
        {
            const auto url = processorRef.authoringDashboardUrl();
            m_statusBar.showMessage("Authoring Mode listening on 127.0.0.1:" + juce::String(url.getPort()));
            url.launchInDefaultBrowser();
        }
        else
        {
            m_statusBar.showMessage("Authoring Mode disabled");
        }
        updateScriptEditorReadOnlyState();
    }

    // Pushed into an already-open editor whenever Authoring Mode toggles, so its
    // read-only state always reflects whether an HTTP POST /script call could race an edit.
    void updateScriptEditorReadOnlyState()
    {
        if (m_scriptEditorContent != nullptr)
        {
            m_scriptEditorContent->setReadOnly(processorRef.isAuthoringModeEnabled());
        }
    }


  private:
    AudioPluginAudioProcessor& processorRef;
    juce::AudioProcessorValueTreeState& valueTreeState;
    std::unique_ptr<GuiLookAndFeel> m_laf;
    juce::Colour backgroundApp;
    juce::MenuBarComponent m_menuBar;
    StatusBar m_statusBar;
    juce::TooltipWindow m_tooltipWindow{this};
    juce::Component* m_topLevel{nullptr};
    bool m_boundsRestored{false};
    GuiConstants::Theme m_currentTheme{AppSettings::loadTheme()};
    Page m_currentPage{Page::Performance};
    juce::TextButton m_pagePerformanceButton{"Performance"};
    juce::TextButton m_pageSettingsButton{"Settings"};
    static constexpr int kThemeModeLightId = 9000;
    static constexpr int kThemeModeDarkId = 9001;
    static constexpr int kThemeBaseBichromaticId = 9002;
    static constexpr int kThemeBaseTrichromaticId = 9003;
    static constexpr int kAboutShowInfoId = 14000;
    static constexpr int kPatchSaveId = 1000;
    static constexpr int kPatchSaveAsId = 1001;
    static constexpr int kPatchLoadIdBase = 2000;
    static constexpr int kPatchDeleteIdBase = 3000;
    static constexpr int kPatchRenameIdBase = 4000;
    std::unique_ptr<juce::AlertWindow> m_patchNameDialog;
    std::vector<juce::String> m_patchMenuNames;


    static constexpr int kScriptEditId = 9004;
    static constexpr int kScriptSaveAsId = 10000;
    static constexpr int kScriptLoadIdBase = 11000;
    static constexpr int kScriptDeleteIdBase = 12000;
    static constexpr int kScriptRenameIdBase = 13000;
    std::unique_ptr<juce::AlertWindow> m_scriptNameDialog;
    std::vector<juce::String> m_scriptMenuNames;
    juce::String m_lastScriptErrorShown;
    // Non-modal; deletes itself on close (see ScriptEditorDialogWindow), hence SafePointer
    // rather than an owning pointer here.
    juce::Component::SafePointer<ScriptEditorDialogWindow> m_scriptEditorWindow;
    // Points at the window's content component, so toggleAuthoringMode() can push a
    // read-only update without reaching into ScriptEditorDialogWindow.
    juce::Component::SafePointer<ScriptEditorWindow> m_scriptEditorContent;
    static constexpr int kAuthoringModeToggleId = 15000;
    static constexpr int kAuthoringModeOpenBrowserId = 15001;

    juce::ComboBox modeDrop{};
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> modeDropAttachment;
    CustomRotaryDial volDial{this};
    CustomRotaryDial reverbLevelDial{this};
    CpuGauge cpuGauge{};
    Gauge levelGauge{};
    SpectrogramDisplay spectrogramGauge{AppSettings::loadTheme()};
    LuaControlArea luaControlsLuaControlArea{};
    CustomRotaryDial luaParam1Dial{this};
    CustomRotaryDial luaParam2Dial{this};
    CustomRotaryDial luaParam3Dial{this};
    CustomRotaryDial luaParam4Dial{this};
    CustomRotaryDial luaParam5Dial{this};
    CustomRotaryDial luaParam6Dial{this};
    CustomRotaryDial luaParam7Dial{this};
    CustomRotaryDial luaParam8Dial{this};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioPluginAudioProcessorEditor)
};
