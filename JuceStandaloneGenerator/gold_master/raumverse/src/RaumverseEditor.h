#pragma once
/*
 * AUTO GENERATED,
 * NOT A GOOD IDEA TO CHANGE STUFF HERE
 * Keep the file readonly
 */

#include "RaumverseProcessor.h"
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
        areas[1] = area.removeFromTop(rowHeight * 2).reduced(Constants::Margins::small);
        areas[2] = area.removeFromTop(rowHeight * 1).reduced(Constants::Margins::small);
        areas[3] = area.removeFromTop(rowHeight * 1).reduced(Constants::Margins::small);
        areas[4] = area.removeFromTop(rowHeight * 1).reduced(Constants::Margins::small);
        areas[5] = area.reduced(Constants::Margins::small);

        {
            juce::FlexBox box;
            box.flexWrap = juce::FlexBox::Wrap::noWrap;
            box.flexDirection = juce::FlexBox::Direction::column;
            box.justifyContent = juce::FlexBox::JustifyContent::spaceAround;
            box.items.add(juce::FlexItem(levelGauge).withHeight(500).withMargin(knobMarginSmall));
            box.items.add(juce::FlexItem(cpuGauge).withHeight(200).withMargin(knobMarginSmall));
            box.performLayout(areas[0].toFloat());
        }
        {
            juce::FlexBox box;
            box.flexWrap = juce::FlexBox::Wrap::noWrap;
            box.flexDirection = juce::FlexBox::Direction::row;
            box.justifyContent = juce::FlexBox::JustifyContent::spaceAround;
            box.items.add(juce::FlexItem(decayLowDial).withFlex(1).withMargin(knobMarginSmall));
            box.items.add(juce::FlexItem(crossOverDial).withFlex(1).withMargin(knobMarginSmall));
            box.items.add(juce::FlexItem(decayHighDial).withFlex(1).withMargin(knobMarginSmall));
            box.items.add(juce::FlexItem(spectrogramGauge).withWidth(600).withMargin(knobMarginSmall));
            box.performLayout(areas[1].toFloat());
        }
        {
            juce::FlexBox box;
            box.flexWrap = juce::FlexBox::Wrap::noWrap;
            box.flexDirection = juce::FlexBox::Direction::row;
            box.justifyContent = juce::FlexBox::JustifyContent::spaceAround;
            box.items.add(juce::FlexItem(lowSizeDial).withFlex(1).withMargin(knobMarginSmall));
            box.items.add(juce::FlexItem(highSizeDial).withFlex(1).withMargin(knobMarginSmall));
            box.items.add(juce::FlexItem(uniqueDelaySwitch)
                              .withWidth(Constants::Text::labelWidth)
                              .withHeight(Constants::Text::labelHeight)
                              .withAlignSelf(juce::FlexItem::AlignSelf::center)
                              .withMargin(knobMarginSmall));
            box.items.add(juce::FlexItem(bulgeDial).withFlex(1).withMargin(knobMarginSmall));
            box.performLayout(areas[2].toFloat());
        }
        {
            juce::FlexBox box;
            box.flexWrap = juce::FlexBox::Wrap::noWrap;
            box.flexDirection = juce::FlexBox::Direction::row;
            box.justifyContent = juce::FlexBox::JustifyContent::spaceAround;
            box.items.add(juce::FlexItem(allPassUpDial).withFlex(1).withMargin(knobMarginSmall));
            box.items.add(juce::FlexItem(allPassDownDial).withFlex(1).withMargin(knobMarginSmall));
            box.items.add(juce::FlexItem(allPassCountDrop)
                              .withFlex(1)
                              .withHeight(Constants::Text::labelHeight)
                              .withAlignSelf(juce::FlexItem::AlignSelf::center)
                              .withMargin(knobMarginSmall));
            box.items.add(juce::FlexItem(lowPassDial).withFlex(1).withMargin(knobMarginSmall));
            box.items.add(juce::FlexItem(lowPassCountDrop)
                              .withFlex(1)
                              .withHeight(Constants::Text::labelHeight)
                              .withAlignSelf(juce::FlexItem::AlignSelf::center)
                              .withMargin(knobMarginSmall));
            box.items.add(juce::FlexItem(highPassDial).withFlex(1).withMargin(knobMarginSmall));
            box.items.add(juce::FlexItem(highPassCountDrop)
                              .withFlex(1)
                              .withHeight(Constants::Text::labelHeight)
                              .withAlignSelf(juce::FlexItem::AlignSelf::center)
                              .withMargin(knobMarginSmall));
            box.performLayout(areas[3].toFloat());
        }
        {
            juce::FlexBox box;
            box.flexWrap = juce::FlexBox::Wrap::noWrap;
            box.flexDirection = juce::FlexBox::Direction::row;
            box.justifyContent = juce::FlexBox::JustifyContent::spaceAround;
            box.items.add(juce::FlexItem(orderDrop)
                              .withFlex(1)
                              .withHeight(Constants::Text::labelHeight)
                              .withAlignSelf(juce::FlexItem::AlignSelf::center)
                              .withMargin(knobMarginSmall));
            box.items.add(juce::FlexItem(dryDial).withFlex(1).withMargin(knobMarginSmall));
            box.items.add(juce::FlexItem(wetDial).withFlex(1).withMargin(knobMarginSmall));
            box.items.add(juce::FlexItem(modulationDepthDial).withFlex(1).withMargin(knobMarginSmall));
            box.items.add(juce::FlexItem(modulationSpeedDial).withFlex(1).withMargin(knobMarginSmall));
            box.items.add(juce::FlexItem(modulationCountDrop)
                              .withFlex(1)
                              .withHeight(Constants::Text::labelHeight)
                              .withAlignSelf(juce::FlexItem::AlignSelf::center)
                              .withMargin(knobMarginSmall));
            box.performLayout(areas[4].toFloat());
        }
        {
            juce::FlexBox box;
            box.flexWrap = juce::FlexBox::Wrap::noWrap;
            box.flexDirection = juce::FlexBox::Direction::row;
            box.justifyContent = juce::FlexBox::JustifyContent::spaceAround;
            box.items.add(juce::FlexItem(reversePitchSwitch)
                              .withWidth(Constants::Text::labelWidth)
                              .withHeight(Constants::Text::labelHeight)
                              .withAlignSelf(juce::FlexItem::AlignSelf::center)
                              .withMargin(knobMarginSmall));
            box.items.add(juce::FlexItem(pitchStrengthDial).withFlex(1).withMargin(knobMarginSmall));
            box.items.add(juce::FlexItem(pitch1InplaceDial).withFlex(1).withMargin(knobMarginSmall));
            box.items.add(juce::FlexItem(pitch2InplaceDial).withFlex(1).withMargin(knobMarginSmall));
            box.items.add(juce::FlexItem(pitchSizeDial).withFlex(1).withMargin(knobMarginSmall));
            box.items.add(juce::FlexItem(pitch1Dial).withFlex(1).withMargin(knobMarginSmall));
            box.items.add(juce::FlexItem(pitch2Dial).withFlex(1).withMargin(knobMarginSmall));
            box.items.add(juce::FlexItem(pitch3Dial).withFlex(1).withMargin(knobMarginSmall));
            box.items.add(juce::FlexItem(pitch4Dial).withFlex(1).withMargin(knobMarginSmall));
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
        }
    }

    void initWidgets()
    {
        addAndMakeVisible(orderDrop);
        orderDrop.addItemList(valueTreeState.getParameter("order")->getAllValueStrings(), 1);
        orderDropAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
            valueTreeState, "order", orderDrop);
        addAndMakeVisible(dryDial);
        dryDial.reset(valueTreeState, "dry");
        dryDial.setLabelText(juce::String::fromUTF8("Dry"));
        addAndMakeVisible(wetDial);
        wetDial.reset(valueTreeState, "wet");
        wetDial.setLabelText(juce::String::fromUTF8("Wet"));
        addAndMakeVisible(lowSizeDial);
        lowSizeDial.reset(valueTreeState, "lowSize");
        lowSizeDial.setLabelText(juce::String::fromUTF8("Smallest size"));
        addAndMakeVisible(highSizeDial);
        highSizeDial.reset(valueTreeState, "highSize");
        highSizeDial.setLabelText(juce::String::fromUTF8("Largest size"));
        addAndMakeVisible(uniqueDelaySwitch);
        uniqueDelaySwitchAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
            valueTreeState, "uniqueDelay", uniqueDelaySwitch);

        addAndMakeVisible(bulgeDial);
        bulgeDial.reset(valueTreeState, "bulge");
        bulgeDial.setLabelText(juce::String::fromUTF8("Bulge"));
        addAndMakeVisible(decayLowDial);
        decayLowDial.reset(valueTreeState, "decayLow");
        decayLowDial.setLabelText(juce::String::fromUTF8("Decay Low"));
        addAndMakeVisible(crossOverDial);
        crossOverDial.reset(valueTreeState, "crossOver");
        crossOverDial.setLabelText(juce::String::fromUTF8("Cross Over"));
        addAndMakeVisible(decayHighDial);
        decayHighDial.reset(valueTreeState, "decayHigh");
        decayHighDial.setLabelText(juce::String::fromUTF8("Decay High"));
        addAndMakeVisible(div1Label);
        div1Label.setText(juce::String::fromUTF8("Dispersion"), juce::dontSendNotification);
        addAndMakeVisible(allPassUpDial);
        allPassUpDial.reset(valueTreeState, "allPassUp");
        allPassUpDial.setLabelText(juce::String::fromUTF8("All pass up"));
        addAndMakeVisible(allPassDownDial);
        allPassDownDial.reset(valueTreeState, "allPassDown");
        allPassDownDial.setLabelText(juce::String::fromUTF8("All pass down"));
        addAndMakeVisible(allPassCountDrop);
        allPassCountDrop.addItemList(valueTreeState.getParameter("allPassCount")->getAllValueStrings(), 1);
        allPassCountDropAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
            valueTreeState, "allPassCount", allPassCountDrop);
        addAndMakeVisible(div2Label);
        div2Label.setText(juce::String::fromUTF8("Filters"), juce::dontSendNotification);
        addAndMakeVisible(lowPassDial);
        lowPassDial.reset(valueTreeState, "lowPass");
        lowPassDial.setLabelText(juce::String::fromUTF8("Low pass"));
        addAndMakeVisible(lowPassCountDrop);
        lowPassCountDrop.addItemList(valueTreeState.getParameter("lowPassCount")->getAllValueStrings(), 1);
        lowPassCountDropAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
            valueTreeState, "lowPassCount", lowPassCountDrop);
        addAndMakeVisible(highPassDial);
        highPassDial.reset(valueTreeState, "highPass");
        highPassDial.setLabelText(juce::String::fromUTF8("High pass"));
        addAndMakeVisible(highPassCountDrop);
        highPassCountDrop.addItemList(valueTreeState.getParameter("highPassCount")->getAllValueStrings(), 1);
        highPassCountDropAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
            valueTreeState, "highPassCount", highPassCountDrop);
        addAndMakeVisible(modulationDepthDial);
        modulationDepthDial.reset(valueTreeState, "modulationDepth");
        modulationDepthDial.setLabelText(juce::String::fromUTF8("Mod depth"));
        addAndMakeVisible(modulationSpeedDial);
        modulationSpeedDial.reset(valueTreeState, "modulationSpeed");
        modulationSpeedDial.setLabelText(juce::String::fromUTF8("Mod speed"));
        addAndMakeVisible(modulationCountDrop);
        modulationCountDrop.addItemList(valueTreeState.getParameter("modulationCount")->getAllValueStrings(), 1);
        modulationCountDropAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
            valueTreeState, "modulationCount", modulationCountDrop);
        addAndMakeVisible(reversePitchSwitch);
        reversePitchSwitchAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
            valueTreeState, "reversePitch", reversePitchSwitch);

        addAndMakeVisible(pitchStrengthDial);
        pitchStrengthDial.reset(valueTreeState, "pitchStrength");
        pitchStrengthDial.setLabelText(juce::String::fromUTF8("Pitch Strength"));
        addAndMakeVisible(pitch1InplaceDial);
        pitch1InplaceDial.reset(valueTreeState, "pitch1Inplace");
        pitch1InplaceDial.setLabelText(juce::String::fromUTF8("Pitch 1 inplace"));
        addAndMakeVisible(pitch2InplaceDial);
        pitch2InplaceDial.reset(valueTreeState, "pitch2Inplace");
        pitch2InplaceDial.setLabelText(juce::String::fromUTF8("Pitch 2 inplace"));
        addAndMakeVisible(pitchSizeDial);
        pitchSizeDial.reset(valueTreeState, "pitchSize");
        pitchSizeDial.setLabelText(juce::String::fromUTF8("Pitchsize"));
        addAndMakeVisible(pitch1Dial);
        pitch1Dial.reset(valueTreeState, "pitch1");
        pitch1Dial.setLabelText(juce::String::fromUTF8("Pitch 1"));
        addAndMakeVisible(pitch2Dial);
        pitch2Dial.reset(valueTreeState, "pitch2");
        pitch2Dial.setLabelText(juce::String::fromUTF8("Pitch 2"));
        addAndMakeVisible(pitch3Dial);
        pitch3Dial.reset(valueTreeState, "pitch3");
        pitch3Dial.setLabelText(juce::String::fromUTF8("Pitch 3"));
        addAndMakeVisible(pitch4Dial);
        pitch4Dial.reset(valueTreeState, "pitch4");
        pitch4Dial.setLabelText(juce::String::fromUTF8("Pitch 4"));
        addAndMakeVisible(cpuGauge);
        cpuGauge.setLabelText(juce::String::fromUTF8("CPU"));
        addAndMakeVisible(levelGauge);
        levelGauge.setLabelText(juce::String::fromUTF8("Level"));
        addAndMakeVisible(spectrogramGauge);
        spectrogramGauge.setLabelText(juce::String::fromUTF8("Spectrogram"));
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


    juce::ComboBox orderDrop{};
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> orderDropAttachment;
    CustomRotaryDial dryDial{this};
    CustomRotaryDial wetDial{this};
    CustomRotaryDial lowSizeDial{this};
    CustomRotaryDial highSizeDial{this};
    juce::ToggleButton uniqueDelaySwitch{juce::String::fromUTF8("Unique delay")};
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> uniqueDelaySwitchAttachment;
    CustomRotaryDial bulgeDial{this};
    CustomRotaryDial decayLowDial{this};
    CustomRotaryDial crossOverDial{this};
    CustomRotaryDial decayHighDial{this};
    juce::Label div1Label{};
    CustomRotaryDial allPassUpDial{this};
    CustomRotaryDial allPassDownDial{this};
    juce::ComboBox allPassCountDrop{};
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> allPassCountDropAttachment;
    juce::Label div2Label{};
    CustomRotaryDial lowPassDial{this};
    juce::ComboBox lowPassCountDrop{};
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> lowPassCountDropAttachment;
    CustomRotaryDial highPassDial{this};
    juce::ComboBox highPassCountDrop{};
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> highPassCountDropAttachment;
    CustomRotaryDial modulationDepthDial{this};
    CustomRotaryDial modulationSpeedDial{this};
    juce::ComboBox modulationCountDrop{};
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> modulationCountDropAttachment;
    juce::ToggleButton reversePitchSwitch{juce::String::fromUTF8("Reverse pitch")};
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> reversePitchSwitchAttachment;
    CustomRotaryDial pitchStrengthDial{this};
    CustomRotaryDial pitch1InplaceDial{this};
    CustomRotaryDial pitch2InplaceDial{this};
    CustomRotaryDial pitchSizeDial{this};
    CustomRotaryDial pitch1Dial{this};
    CustomRotaryDial pitch2Dial{this};
    CustomRotaryDial pitch3Dial{this};
    CustomRotaryDial pitch4Dial{this};
    CpuGauge cpuGauge{};
    Gauge levelGauge{};
    SpectrogramDisplay spectrogramGauge{AppSettings::loadTheme()};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioPluginAudioProcessorEditor)
};
