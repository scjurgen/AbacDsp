#pragma once
/*
 * AUTO GENERATED,
 * NOT A GOOD IDEA TO CHANGE STUFF HERE
 * Keep the file readonly
 */

#include "TapedelayProcessor.h"
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

        std::vector<juce::Rectangle<int>> areas(6);
        const auto colWidth = area.getWidth() / 13;
        const auto rowHeight = area.getHeight() / 6;
        areas[0] = area.removeFromLeft(colWidth * 1).reduced(Constants::Margins::small);
        areas[1] = area.removeFromTop(rowHeight * 1).reduced(Constants::Margins::small);
        areas[2] = area.removeFromTop(rowHeight * 1).reduced(Constants::Margins::small);
        areas[3] = area.removeFromTop(rowHeight * 1).reduced(Constants::Margins::small);
        areas[4] = area.removeFromTop(rowHeight * 1).reduced(Constants::Margins::small);
        areas[5] = area.reduced(Constants::Margins::small);

        {
            juce::FlexBox box;
            box.flexWrap = juce::FlexBox::Wrap::noWrap;
            box.flexDirection = juce::FlexBox::Direction::column;
            box.justifyContent = juce::FlexBox::JustifyContent::spaceAround;
            box.items.add(juce::FlexItem(feedGainDial).withFlex(1).withMargin(knobMarginSmall));
            box.items.add(juce::FlexItem(levelGauge).withHeight(500).withMargin(knobMarginSmall));
            box.items.add(juce::FlexItem(cpuGauge).withHeight(200).withMargin(knobMarginSmall));
            box.performLayout(areas[0].toFloat());
        }
        {
            juce::FlexBox box;
            box.flexWrap = juce::FlexBox::Wrap::noWrap;
            box.flexDirection = juce::FlexBox::Direction::row;
            box.justifyContent = juce::FlexBox::JustifyContent::spaceAround;
            box.items.add(juce::FlexItem(tapeSpeedDial).withFlex(1).withMargin(knobMarginSmall));
            box.items.add(juce::FlexItem(wowDial).withFlex(1).withMargin(knobMarginSmall));
            box.items.add(juce::FlexItem(saturationDial).withFlex(1).withMargin(knobMarginSmall));
            box.performLayout(areas[1].toFloat());
        }
        {
            juce::FlexBox box;
            box.flexWrap = juce::FlexBox::Wrap::noWrap;
            box.flexDirection = juce::FlexBox::Direction::row;
            box.justifyContent = juce::FlexBox::JustifyContent::spaceAround;
            box.items.add(juce::FlexItem(hysteresisDial).withFlex(1).withMargin(knobMarginSmall));
            box.items.add(juce::FlexItem(noiseFloorDial).withFlex(1).withMargin(knobMarginSmall));
            box.items.add(juce::FlexItem(noiseDistributionDial).withFlex(1).withMargin(knobMarginSmall));
            box.performLayout(areas[2].toFloat());
        }
        {
            juce::FlexBox box;
            box.flexWrap = juce::FlexBox::Wrap::noWrap;
            box.flexDirection = juce::FlexBox::Direction::row;
            box.justifyContent = juce::FlexBox::JustifyContent::spaceAround;
            box.items.add(juce::FlexItem(delayTime1Dial).withFlex(1).withMargin(knobMarginSmall));
            box.items.add(juce::FlexItem(delayLevel1Dial).withFlex(1).withMargin(knobMarginSmall));
            box.items.add(juce::FlexItem(feedback1Dial).withFlex(1).withMargin(knobMarginSmall));
            box.items.add(juce::FlexItem(delayTime2Dial).withFlex(1).withMargin(knobMarginSmall));
            box.items.add(juce::FlexItem(delayLevel2Dial).withFlex(1).withMargin(knobMarginSmall));
            box.items.add(juce::FlexItem(feedback2Dial).withFlex(1).withMargin(knobMarginSmall));
            box.performLayout(areas[3].toFloat());
        }
        {
            juce::FlexBox box;
            box.flexWrap = juce::FlexBox::Wrap::noWrap;
            box.flexDirection = juce::FlexBox::Direction::row;
            box.justifyContent = juce::FlexBox::JustifyContent::spaceAround;
            box.items.add(juce::FlexItem(delayTime3Dial).withFlex(1).withMargin(knobMarginSmall));
            box.items.add(juce::FlexItem(delayLevel3Dial).withFlex(1).withMargin(knobMarginSmall));
            box.items.add(juce::FlexItem(feedback3Dial).withFlex(1).withMargin(knobMarginSmall));
            box.items.add(juce::FlexItem(delayTime4Dial).withFlex(1).withMargin(knobMarginSmall));
            box.items.add(juce::FlexItem(delayLevel4Dial).withFlex(1).withMargin(knobMarginSmall));
            box.items.add(juce::FlexItem(feedback4Dial).withFlex(1).withMargin(knobMarginSmall));
            box.performLayout(areas[4].toFloat());
        }
        {
            juce::FlexBox box;
            box.flexWrap = juce::FlexBox::Wrap::noWrap;
            box.flexDirection = juce::FlexBox::Direction::row;
            box.justifyContent = juce::FlexBox::JustifyContent::spaceAround;
            box.items.add(juce::FlexItem(spectrogramGauge).withFlex(1).withMargin(knobMarginSmall));
            box.items.add(juce::FlexItem(signalGauge).withFlex(1).withMargin(knobMarginSmall));
            box.performLayout(areas[5].toFloat());
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
            signalGauge.update(processorRef.getWaveDataToShow());
        }
    }

    void initWidgets()
    {
        addAndMakeVisible(feedGainDial);
        feedGainDial.reset(valueTreeState, "feedGain");
        feedGainDial.setLabelText(juce::String::fromUTF8("Feed"));
        addAndMakeVisible(tapeSpeedDial);
        tapeSpeedDial.reset(valueTreeState, "tapeSpeed");
        tapeSpeedDial.setLabelText(juce::String::fromUTF8("Tape speed"));
        addAndMakeVisible(wowDial);
        wowDial.reset(valueTreeState, "wow");
        wowDial.setLabelText(juce::String::fromUTF8("WOW"));
        addAndMakeVisible(hysteresisDial);
        hysteresisDial.reset(valueTreeState, "hysteresis");
        hysteresisDial.setLabelText(juce::String::fromUTF8("Hysteresis"));
        addAndMakeVisible(saturationDial);
        saturationDial.reset(valueTreeState, "saturation");
        saturationDial.setLabelText(juce::String::fromUTF8("Saturation"));
        addAndMakeVisible(noiseFloorDial);
        noiseFloorDial.reset(valueTreeState, "noiseFloor");
        noiseFloorDial.setLabelText(juce::String::fromUTF8("Noise floor"));
        addAndMakeVisible(noiseDistributionDial);
        noiseDistributionDial.reset(valueTreeState, "noiseDistribution");
        noiseDistributionDial.setLabelText(juce::String::fromUTF8("Noise distribution"));
        addAndMakeVisible(delayTime1Dial);
        delayTime1Dial.reset(valueTreeState, "delayTime1");
        delayTime1Dial.setLabelText(juce::String::fromUTF8("Delay time 1"));
        addAndMakeVisible(delayTime2Dial);
        delayTime2Dial.reset(valueTreeState, "delayTime2");
        delayTime2Dial.setLabelText(juce::String::fromUTF8("Delay time 2"));
        addAndMakeVisible(delayTime3Dial);
        delayTime3Dial.reset(valueTreeState, "delayTime3");
        delayTime3Dial.setLabelText(juce::String::fromUTF8("Delay time 3"));
        addAndMakeVisible(delayTime4Dial);
        delayTime4Dial.reset(valueTreeState, "delayTime4");
        delayTime4Dial.setLabelText(juce::String::fromUTF8("Tape time 4"));
        addAndMakeVisible(delayLevel1Dial);
        delayLevel1Dial.reset(valueTreeState, "delayLevel1");
        delayLevel1Dial.setLabelText(juce::String::fromUTF8("Delay level 1"));
        addAndMakeVisible(delayLevel2Dial);
        delayLevel2Dial.reset(valueTreeState, "delayLevel2");
        delayLevel2Dial.setLabelText(juce::String::fromUTF8("Delay level 2"));
        addAndMakeVisible(delayLevel3Dial);
        delayLevel3Dial.reset(valueTreeState, "delayLevel3");
        delayLevel3Dial.setLabelText(juce::String::fromUTF8("Delay level 3"));
        addAndMakeVisible(delayLevel4Dial);
        delayLevel4Dial.reset(valueTreeState, "delayLevel4");
        delayLevel4Dial.setLabelText(juce::String::fromUTF8("Delay level 4"));
        addAndMakeVisible(feedback1Dial);
        feedback1Dial.reset(valueTreeState, "feedback1");
        feedback1Dial.setLabelText(juce::String::fromUTF8("Feedback 1"));
        addAndMakeVisible(feedback2Dial);
        feedback2Dial.reset(valueTreeState, "feedback2");
        feedback2Dial.setLabelText(juce::String::fromUTF8("Feedback 2"));
        addAndMakeVisible(feedback3Dial);
        feedback3Dial.reset(valueTreeState, "feedback3");
        feedback3Dial.setLabelText(juce::String::fromUTF8("Feedback 3"));
        addAndMakeVisible(feedback4Dial);
        feedback4Dial.reset(valueTreeState, "feedback4");
        feedback4Dial.setLabelText(juce::String::fromUTF8("Feedback 4"));
        addAndMakeVisible(cpuGauge);
        cpuGauge.setLabelText(juce::String::fromUTF8("CPU"));
        addAndMakeVisible(levelGauge);
        levelGauge.setLabelText(juce::String::fromUTF8("Level"));
        addAndMakeVisible(spectrogramGauge);
        spectrogramGauge.setLabelText(juce::String::fromUTF8("Spectrogram"));
        addAndMakeVisible(signalGauge);
        signalGauge.setLabelText(juce::String::fromUTF8("Signal"));
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
        cpuGauge.updateColors();
        levelGauge.updateColors();
        spectrogramGauge.setGradientPreset(preset);
        signalGauge.updateColors();

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


    CustomRotaryDial feedGainDial{this};
    CustomRotaryDial tapeSpeedDial{this};
    CustomRotaryDial wowDial{this};
    CustomRotaryDial hysteresisDial{this};
    CustomRotaryDial saturationDial{this};
    CustomRotaryDial noiseFloorDial{this};
    CustomRotaryDial noiseDistributionDial{this};
    CustomRotaryDial delayTime1Dial{this};
    CustomRotaryDial delayTime2Dial{this};
    CustomRotaryDial delayTime3Dial{this};
    CustomRotaryDial delayTime4Dial{this};
    CustomRotaryDial delayLevel1Dial{this};
    CustomRotaryDial delayLevel2Dial{this};
    CustomRotaryDial delayLevel3Dial{this};
    CustomRotaryDial delayLevel4Dial{this};
    CustomRotaryDial feedback1Dial{this};
    CustomRotaryDial feedback2Dial{this};
    CustomRotaryDial feedback3Dial{this};
    CustomRotaryDial feedback4Dial{this};
    CpuGauge cpuGauge{};
    Gauge levelGauge{};
    SpectrogramDisplay spectrogramGauge{AppSettings::loadTheme()};
    WaveformGauge signalGauge{};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioPluginAudioProcessorEditor)
};
