#pragma once
/*
 * AUTO GENERATED,
 * NOT A GOOD IDEA TO CHANGE STUFF HERE
 * Keep the file readonly
 */

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
        area = area.reduced(static_cast<int>(Constants::Margins::big));

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
        bpmDial.setCcMappable(true, {[this] { processorRef.beginCcLearn(CcTarget::bpm); },
                                     [this] { return processorRef.getCcRange(CcTarget::bpm); },
                                     [this](float lo, float hi) { processorRef.setCcRange(CcTarget::bpm, lo, hi); },
                                     [this] { processorRef.clearCcAssignment(CcTarget::bpm); },
                                     [this] { return processorRef.getCcController(CcTarget::bpm); }});
        addAndMakeVisible(dropBarsDrop);
        dropBarsDrop.addItemList(valueTreeState.getParameter("dropBars")->getAllValueStrings(), 1);
        dropBarsDropAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
            valueTreeState, "dropBars", dropBarsDrop);
        addAndMakeVisible(metroVolumeDial);
        metroVolumeDial.reset(valueTreeState, "metroVolume");
        metroVolumeDial.setLabelText(juce::String::fromUTF8("Metro Volume"));
        metroVolumeDial.setCcMappable(true, {[this] { processorRef.beginCcLearn(CcTarget::metroVolume); },
                                             [this] { return processorRef.getCcRange(CcTarget::metroVolume); },
                                             [this](float lo, float hi)
                                             { processorRef.setCcRange(CcTarget::metroVolume, lo, hi); },
                                             [this] { processorRef.clearCcAssignment(CcTarget::metroVolume); },
                                             [this] { return processorRef.getCcController(CcTarget::metroVolume); }});
        addAndMakeVisible(inputVolumeDial);
        inputVolumeDial.reset(valueTreeState, "inputVolume");
        inputVolumeDial.setLabelText(juce::String::fromUTF8("Input Volume"));
        inputVolumeDial.setCcMappable(true, {[this] { processorRef.beginCcLearn(CcTarget::inputVolume); },
                                             [this] { return processorRef.getCcRange(CcTarget::inputVolume); },
                                             [this](float lo, float hi)
                                             { processorRef.setCcRange(CcTarget::inputVolume, lo, hi); },
                                             [this] { processorRef.clearCcAssignment(CcTarget::inputVolume); },
                                             [this] { return processorRef.getCcController(CcTarget::inputVolume); }});
        addAndMakeVisible(subVolumeDial);
        subVolumeDial.reset(valueTreeState, "subVolume");
        subVolumeDial.setLabelText(juce::String::fromUTF8("Sub Volume"));
        subVolumeDial.setCcMappable(true, {[this] { processorRef.beginCcLearn(CcTarget::subVolume); },
                                           [this] { return processorRef.getCcRange(CcTarget::subVolume); },
                                           [this](float lo, float hi)
                                           { processorRef.setCcRange(CcTarget::subVolume, lo, hi); },
                                           [this] { processorRef.clearCcAssignment(CcTarget::subVolume); },
                                           [this] { return processorRef.getCcController(CcTarget::subVolume); }});
        addAndMakeVisible(onOffSwitch);
        onOffSwitchAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
            valueTreeState, "onOff", onOffSwitch);

        addAndMakeVisible(hostSyncSwitch);
        hostSyncSwitchAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
            valueTreeState, "hostSync", hostSyncSwitch);

        addAndMakeVisible(presetDrop);
        presetDrop.addItemList(valueTreeState.getParameter("preset")->getAllValueStrings(), 1);
        presetDropAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
            valueTreeState, "preset", presetDrop);
        presetDrop.onChange = [this] { updateSwingRatioVisibility(); };
        updateSwingRatioVisibility();
        addChildComponent(swingRatioDial);
        swingRatioDial.reset(valueTreeState, "swingRatio");
        swingRatioDial.setLabelText(juce::String::fromUTF8("Swing"));
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
        return {"Settings"};
    }

    juce::PopupMenu getMenuForIndex(int menuIndex, const juce::String&) override
    {
        juce::PopupMenu menu;
        if (menuIndex == 0)
        {
            juce::PopupMenu themeMenu;
            for (int i = 0; i < Themes::kHueCount; ++i)
            {
                themeMenu.addItem(i + 1, Themes::kHueNames[static_cast<size_t>(i)]);
            }
            menu.addSubMenu("Theme", themeMenu);

            juce::PopupMenu modeMenu;
            modeMenu.addItem(kThemeModeLightId, "Light");
            modeMenu.addItem(kThemeModeDarkId, "Dark");
            menu.addSubMenu("Mode", modeMenu);

            juce::PopupMenu baseMenu;
            baseMenu.addItem(kThemeBaseBichromaticId, "Bichromatic");
            baseMenu.addItem(kThemeBaseTrichromaticId, "Trichromatic");
            menu.addSubMenu("Base", baseMenu);
            menu.addSubMenu("Patches", buildPatchesMenu());
        }
        return menu;
    }

    void menuItemSelected(int menuItemID, int /*topLevelMenuIndex*/) override
    {
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

    juce::PopupMenu buildPatchesMenu()
    {
        m_patchMenuNames = processorRef.listPatchNames();
        const auto currentName = processorRef.getCurrentPatchName();

        juce::PopupMenu loadMenu;
        for (size_t i = 0; i < m_patchMenuNames.size(); ++i)
        {
            loadMenu.addItem(kPatchLoadIdBase + static_cast<int>(i), m_patchMenuNames[i], true,
                             m_patchMenuNames[i] == currentName);
        }

        juce::PopupMenu deleteMenu;
        for (size_t i = 0; i < m_patchMenuNames.size(); ++i)
        {
            deleteMenu.addItem(kPatchDeleteIdBase + static_cast<int>(i), m_patchMenuNames[i]);
        }

        juce::PopupMenu renameMenu;
        for (size_t i = 0; i < m_patchMenuNames.size(); ++i)
        {
            renameMenu.addItem(kPatchRenameIdBase + static_cast<int>(i), m_patchMenuNames[i]);
        }

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
        m_patchNameDialog = std::make_unique<juce::AlertWindow>(
            "Save Patch", "Enter a name for this patch:", juce::MessageBoxIconType::NoIcon);
        m_patchNameDialog->addTextEditor("name", processorRef.getCurrentPatchName());
        m_patchNameDialog->addButton("Save", 1, juce::KeyPress(juce::KeyPress::returnKey));
        m_patchNameDialog->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));
        m_patchNameDialog->enterModalState(true,
                                           juce::ModalCallbackFunction::create(
                                               [this](int result)
                                               {
                                                   const auto name =
                                                       m_patchNameDialog->getTextEditorContents("name").trim();
                                                   m_patchNameDialog.reset();
                                                   if (result != 1 || name.isEmpty())
                                                   {
                                                       return;
                                                   }
                                                   if (processorRef.saveCurrentPatchAs(name))
                                                   {
                                                       m_statusBar.showMessage("Saved '" + name + "'");
                                                   }
                                                   else
                                                   {
                                                       m_statusBar.showMessage("Save failed");
                                                   }
                                               }),
                                           false);
    }

    void promptRename(const juce::String& oldName)
    {
        m_patchNameDialog = std::make_unique<juce::AlertWindow>(
            "Rename Patch", "Enter a new name for \"" + oldName + "\":", juce::MessageBoxIconType::NoIcon);
        m_patchNameDialog->addTextEditor("name", oldName);
        m_patchNameDialog->addButton("Rename", 1, juce::KeyPress(juce::KeyPress::returnKey));
        m_patchNameDialog->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));
        m_patchNameDialog->enterModalState(true,
                                           juce::ModalCallbackFunction::create(
                                               [this, oldName](int result)
                                               {
                                                   const auto newName =
                                                       m_patchNameDialog->getTextEditorContents("name").trim();
                                                   m_patchNameDialog.reset();
                                                   if (result != 1 || newName.isEmpty() || newName == oldName)
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
    juce::Component* m_topLevel{nullptr};
    bool m_boundsRestored{false};
    GuiConstants::Theme m_currentTheme{AppSettings::loadTheme()};
    static constexpr int kThemeModeLightId = 9000;
    static constexpr int kThemeModeDarkId = 9001;
    static constexpr int kThemeBaseBichromaticId = 9002;
    static constexpr int kThemeBaseTrichromaticId = 9003;
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
