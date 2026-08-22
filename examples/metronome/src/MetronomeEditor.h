#pragma once
/*
 * AUTO GENERATED,
 * NOT A GOOD IDEA TO CHANGE STUFF HERE
 * Keep the file readonly
 */

#include <map>

#include "MetronomeProcessor.h"
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
            std::vector<juce::Rectangle<int>> areas(1);
            areas[0] = area.reduced(Constants::Margins::small);

            {
                juce::FlexBox box;
                box.flexWrap = juce::FlexBox::Wrap::noWrap;
                box.flexDirection = juce::FlexBox::Direction::row;
                box.justifyContent = juce::FlexBox::JustifyContent::spaceAround;
                box.items.add(juce::FlexItem(signalGauge).withFlex(1).withMargin(knobMarginSmall));
                box.items.add(juce::FlexItem(irisGauge).withFlex(1).withMargin(knobMarginSmall));
                box.performLayout(areas[0].toFloat());
            }
        }
        else
        {
            // auto generated
            // const juce::FlexItem::Margin knobMargin = juce::FlexItem::Margin(Constants::Margins::small);
            const juce::FlexItem::Margin knobMarginSmall = juce::FlexItem::Margin(Constants::Margins::medium);
            std::vector<juce::Rectangle<int>> areas(2);
            const auto colWidth = area.getWidth() / 6;
            areas[0] = area.removeFromLeft(colWidth * 1).reduced(Constants::Margins::small);
            areas[1] = area.reduced(Constants::Margins::small);

            {
                juce::FlexBox box;
                box.flexWrap = juce::FlexBox::Wrap::noWrap;
                box.flexDirection = juce::FlexBox::Direction::column;
                box.justifyContent = juce::FlexBox::JustifyContent::spaceAround;
                box.items.add(juce::FlexItem(presetDrop)
                                  .withFlex(0)
                                  .withHeight(Constants::Text::labelHeight)
                                  .withAlignSelf(juce::FlexItem::AlignSelf::stretch)
                                  .withMargin(knobMarginSmall));
                box.items.add(juce::FlexItem(dropBarsDrop)
                                  .withFlex(0)
                                  .withHeight(Constants::Text::labelHeight)
                                  .withAlignSelf(juce::FlexItem::AlignSelf::stretch)
                                  .withMargin(knobMarginSmall));
                if (swingRatioDial.isVisible())
                {
                    box.items.add(juce::FlexItem(swingRatioDial).withFlex(1).withMargin(knobMarginSmall));
                }
                box.items.add(juce::FlexItem(onOffSwitch)
                                  .withFlex(0)
                                  .withHeight(Constants::Text::labelHeight)
                                  .withAlignSelf(juce::FlexItem::AlignSelf::stretch)
                                  .withMargin(knobMarginSmall));
                box.items.add(juce::FlexItem(hostSyncSwitch)
                                  .withFlex(0)
                                  .withHeight(Constants::Text::labelHeight)
                                  .withAlignSelf(juce::FlexItem::AlignSelf::stretch)
                                  .withMargin(knobMarginSmall));
                box.items.add(juce::FlexItem(bpmDial).withFlex(1).withMargin(knobMarginSmall));
                box.items.add(juce::FlexItem(subVolumeDial).withFlex(1).withMargin(knobMarginSmall));
                box.items.add(juce::FlexItem(metroVolumeDial).withFlex(1).withMargin(knobMarginSmall));
                box.items.add(juce::FlexItem(inputVolumeDial).withFlex(1).withMargin(knobMarginSmall));
                box.performLayout(areas[0].toFloat());
            }
            {
                juce::FlexBox box;
                box.flexWrap = juce::FlexBox::Wrap::noWrap;
                box.flexDirection = juce::FlexBox::Direction::column;
                box.justifyContent = juce::FlexBox::JustifyContent::spaceAround;
                box.items.add(juce::FlexItem(signalGauge).withFlex(1).withMargin(knobMarginSmall));
                box.items.add(juce::FlexItem(irisGauge).withFlex(2).withMargin(knobMarginSmall));
                box.performLayout(areas[1].toFloat());
            }
        }
    }
#pragma GCC diagnostic pop

    void timerCallback() override
    {
        if (processorRef.hasRunner())
        {
            signalGauge.update(processorRef.getWaveDataToShow());
            signalGauge.setSampleRate(static_cast<float>(processorRef.getSampleRate()));
            signalGauge.setBeatIndex(processorRef.getWaveDataBeatIndex());
            signalGauge.setSubdivisionPositions(processorRef.getSubdivisionPositions());
            irisGauge.setSampleRate(static_cast<float>(processorRef.getSampleRate()));
            irisGauge.setBarBeats(processorRef.getBarBeats());
            irisGauge.setBarPhase(processorRef.getBarPhase());
            irisGauge.setSubdivisionPositions(processorRef.getSubdivisionPositions());
            irisGauge.update(processorRef.getInputSpectrogram());

            {
                const float sr = static_cast<float>(processorRef.getSampleRate());
                const float bpm = processorRef.getCurrentClickBpm();
                const size_t spb = static_cast<size_t>(sr * 60.f / bpm);
                bpmDial.setEnabled(!processorRef.isHostSynced());
                if (processorRef.isHostSynced())
                {
                    bpmDial.setValue(bpm);
                }
                onOffSwitch.setEnabled(!processorRef.isHostSynced());
                signalGauge.setSamplesPerBeat(spb);
                irisGauge.setSamplesPerBeat(spb);
            }
            processorRef.consumeLastLearnedCc();
        }
    }

    void initWidgets()
    {
        addAndMakeVisible(bpmDial);
        bpmDial.reset(valueTreeState, "bpm");
        bpmDial.setLabelText(juce::String::fromUTF8("BPM"));
        bpmDial.setTooltip(juce::String::fromUTF8("BPM (40 to 250 BPM)"));
        bpmDial.setCcMappable(true, {[this] { processorRef.beginCcLearn(CcTarget::bpm); },
                                     [this] { return processorRef.getCcRange(CcTarget::bpm); },
                                     [this](float lo, float hi) { processorRef.setCcRange(CcTarget::bpm, lo, hi); },
                                     [this] { processorRef.clearCcAssignment(CcTarget::bpm); },
                                     [this] { return processorRef.getCcController(CcTarget::bpm); }});
        addAndMakeVisible(dropBarsDrop);
        dropBarsDrop.addItemList(valueTreeState.getParameter("dropBars")->getAllValueStrings(), 1);
        dropBarsDropAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
            valueTreeState, "dropBars", dropBarsDrop);
        dropBarsDrop.setTooltip(juce::String::fromUTF8(
            "Drop Bars (Drop none, Play 1 Drop 1, Play 3 Drop 1, Play 2 Drop 2, Play 1 Drop 3)"));
        addAndMakeVisible(metroVolumeDial);
        metroVolumeDial.reset(valueTreeState, "metroVolume");
        metroVolumeDial.setLabelText(juce::String::fromUTF8("Metro Volume"));
        metroVolumeDial.setTooltip(juce::String::fromUTF8("Metro Volume (-60 to 0 dB)"));
        metroVolumeDial.setCcMappable(true, {[this] { processorRef.beginCcLearn(CcTarget::metroVolume); },
                                             [this] { return processorRef.getCcRange(CcTarget::metroVolume); },
                                             [this](float lo, float hi)
                                             { processorRef.setCcRange(CcTarget::metroVolume, lo, hi); },
                                             [this] { processorRef.clearCcAssignment(CcTarget::metroVolume); },
                                             [this] { return processorRef.getCcController(CcTarget::metroVolume); }});
        addAndMakeVisible(inputVolumeDial);
        inputVolumeDial.reset(valueTreeState, "inputVolume");
        inputVolumeDial.setLabelText(juce::String::fromUTF8("Input Volume"));
        inputVolumeDial.setTooltip(juce::String::fromUTF8("Input Volume (-60 to 12 dB)"));
        inputVolumeDial.setCcMappable(true, {[this] { processorRef.beginCcLearn(CcTarget::inputVolume); },
                                             [this] { return processorRef.getCcRange(CcTarget::inputVolume); },
                                             [this](float lo, float hi)
                                             { processorRef.setCcRange(CcTarget::inputVolume, lo, hi); },
                                             [this] { processorRef.clearCcAssignment(CcTarget::inputVolume); },
                                             [this] { return processorRef.getCcController(CcTarget::inputVolume); }});
        addAndMakeVisible(subVolumeDial);
        subVolumeDial.reset(valueTreeState, "subVolume");
        subVolumeDial.setLabelText(juce::String::fromUTF8("Sub Volume"));
        subVolumeDial.setTooltip(juce::String::fromUTF8("Sub Volume (-60 to 0 dB)"));
        subVolumeDial.setCcMappable(true, {[this] { processorRef.beginCcLearn(CcTarget::subVolume); },
                                           [this] { return processorRef.getCcRange(CcTarget::subVolume); },
                                           [this](float lo, float hi)
                                           { processorRef.setCcRange(CcTarget::subVolume, lo, hi); },
                                           [this] { processorRef.clearCcAssignment(CcTarget::subVolume); },
                                           [this] { return processorRef.getCcController(CcTarget::subVolume); }});
        addAndMakeVisible(onOffSwitch);
        onOffSwitchAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
            valueTreeState, "onOff", onOffSwitch);
        onOffSwitch.setTooltip(juce::String::fromUTF8("Start"));

        addAndMakeVisible(hostSyncSwitch);
        hostSyncSwitchAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
            valueTreeState, "hostSync", hostSyncSwitch);
        hostSyncSwitch.setTooltip(juce::String::fromUTF8("Host Sync"));

        addAndMakeVisible(presetDrop);
        presetDrop.addItemList(valueTreeState.getParameter("preset")->getAllValueStrings(), 1);
        presetDropAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
            valueTreeState, "preset", presetDrop);
        presetDrop.setTooltip(
            juce::String::fromUTF8("Preset (3/4, 3/4 8th, 3/4 16th, 3/4 shuffle, 3/4 triplet, 4/4, 4/4 8th, 4/4 16th, "
                                   "4/4 shuffle, 4/4 triplet, 4/4 swing, 5/4 (3+2), 5/4 8th (3+2), 5/4 (2+3), 5/4 8th "
                                   "(2+3), 6/8 in-2, 6/8 in-6, 7/8 (2+2+3), 7/8 (2+3+2), 7/8 (3+2+2), 9/8 in-3, 9/8 "
                                   "in-9, 11/8 (3+3+3+2), 11/8 (3+3+2+3), 13/8 (3+3+3+2+2), 13/8 (3+4+3+3))"));
        presetDrop.onChange = [this] { updateSwingRatioVisibility(); };
        updateSwingRatioVisibility();
        addChildComponent(swingRatioDial);
        swingRatioDial.reset(valueTreeState, "swingRatio");
        swingRatioDial.setLabelText(juce::String::fromUTF8("Swing"));
        swingRatioDial.setTooltip(juce::String::fromUTF8("Swing (1.0 to 2.0)"));
        swingRatioDial.setCcMappable(true, {[this] { processorRef.beginCcLearn(CcTarget::swingRatio); },
                                            [this] { return processorRef.getCcRange(CcTarget::swingRatio); },
                                            [this](float lo, float hi)
                                            { processorRef.setCcRange(CcTarget::swingRatio, lo, hi); },
                                            [this] { processorRef.clearCcAssignment(CcTarget::swingRatio); },
                                            [this] { return processorRef.getCcController(CcTarget::swingRatio); }});
        addAndMakeVisible(signalGauge);
        signalGauge.setLabelText(juce::String::fromUTF8("Beat"));
        addAndMakeVisible(irisGauge);
        irisGauge.setLabelText(juce::String::fromUTF8("Spectrum Iris"));

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
            bpmDial.setVisible(false);
            dropBarsDrop.setVisible(false);
            metroVolumeDial.setVisible(false);
            inputVolumeDial.setVisible(false);
            subVolumeDial.setVisible(false);
            onOffSwitch.setVisible(false);
            hostSyncSwitch.setVisible(false);
            presetDrop.setVisible(false);
            signalGauge.setVisible(true);
            irisGauge.setVisible(true);
        }
        else
        {
            bpmDial.setVisible(true);
            dropBarsDrop.setVisible(true);
            metroVolumeDial.setVisible(true);
            inputVolumeDial.setVisible(true);
            subVolumeDial.setVisible(true);
            onOffSwitch.setVisible(true);
            hostSyncSwitch.setVisible(true);
            presetDrop.setVisible(true);
            signalGauge.setVisible(true);
            irisGauge.setVisible(true);
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
            "Metronome with settable BPM, start/stop and damped sine tick\n\nPart of the AbacDsp project - core DSP "
            "library is MIT licensed.\n\nBuilt with JUCE, licensed under AGPLv3 (or a commercial JUCE "
            "licence).\n\nFull third-party license details: THIRD-PARTY-LICENSES.md in the AbacDsp repository.";

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
        signalGauge.updateColors();
        irisGauge.setGradientPreset(preset);

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


    void updateSwingRatioVisibility()
    {
        swingRatioDial.setVisible(processorRef.presetHasSwing(presetDrop.getSelectedItemIndex()));
        resized();
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


    CustomRotaryDial bpmDial{this};
    juce::ComboBox dropBarsDrop{};
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> dropBarsDropAttachment;
    CustomRotaryDial metroVolumeDial{this};
    CustomRotaryDial inputVolumeDial{this};
    CustomRotaryDial subVolumeDial{this};
    juce::ToggleButton onOffSwitch{juce::String::fromUTF8("Start")};
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> onOffSwitchAttachment;
    juce::ToggleButton hostSyncSwitch{juce::String::fromUTF8("Host Sync")};
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> hostSyncSwitchAttachment;
    juce::ComboBox presetDrop{};
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> presetDropAttachment;
    CustomRotaryDial swingRatioDial{this};
    CircularBeatDisplay signalGauge{};
    CircularSpectrogramDisplay irisGauge{};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioPluginAudioProcessorEditor)
};
