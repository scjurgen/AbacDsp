#pragma once
/*
 * AUTO GENERATED,
 * NOT A GOOD IDEA TO CHANGE STUFF HERE
 * Keep the file readonly
 */

#include "SamplePlayerProcessor.h"
#include "UiElements.h"


//==============================================================================
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

        std::vector<juce::Rectangle<int>> areas(5);
        const auto colWidth = area.getWidth() / 10;
        const auto rowHeight = area.getHeight() / 6;
        areas[0] = area.removeFromLeft(colWidth * 1).reduced(Constants::Margins::small);
        areas[1] = area.removeFromTop(rowHeight * 1).reduced(Constants::Margins::small);
        areas[2] = area.removeFromTop(rowHeight * 1).reduced(Constants::Margins::small);
        areas[3] = area.removeFromTop(rowHeight * 1).reduced(Constants::Margins::small);
        areas[4] = area.reduced(Constants::Margins::small);

        {
            juce::FlexBox box;
            box.flexWrap = juce::FlexBox::Wrap::noWrap;
            box.flexDirection = juce::FlexBox::Direction::column;
            box.justifyContent = juce::FlexBox::JustifyContent::spaceAround;
            box.items.add(juce::FlexItem(syncSwitch)
                              .withFlex(0)
                              .withHeight(Constants::Text::labelHeight)
                              .withAlignSelf(juce::FlexItem::AlignSelf::stretch)
                              .withMargin(knobMarginSmall));
            box.items.add(juce::FlexItem(typeDrop)
                              .withFlex(0)
                              .withHeight(Constants::Text::labelHeight)
                              .withAlignSelf(juce::FlexItem::AlignSelf::stretch)
                              .withMargin(knobMarginSmall));
            box.items.add(juce::FlexItem(soloDrop)
                              .withFlex(0)
                              .withHeight(Constants::Text::labelHeight)
                              .withAlignSelf(juce::FlexItem::AlignSelf::stretch)
                              .withMargin(knobMarginSmall));
            box.items.add(juce::FlexItem(volDial).withFlex(1).withMargin(knobMarginSmall));
            box.items.add(juce::FlexItem(reverbLevelWetDial).withFlex(1).withMargin(knobMarginSmall));
            box.items.add(juce::FlexItem(reverbDecayDial).withFlex(1).withMargin(knobMarginSmall));
            box.items.add(juce::FlexItem(reverbShimmerDial).withFlex(1).withMargin(knobMarginSmall));
            box.items.add(juce::FlexItem(levelGauge).withHeight(100).withMargin(knobMarginSmall));
            box.items.add(juce::FlexItem(cpuGauge).withHeight(100).withMargin(knobMarginSmall));
            box.performLayout(areas[0].toFloat());
        }
        {
            juce::FlexBox box;
            box.flexWrap = juce::FlexBox::Wrap::noWrap;
            box.flexDirection = juce::FlexBox::Direction::row;
            box.justifyContent = juce::FlexBox::JustifyContent::spaceAround;
            box.items.add(juce::FlexItem(vol1Dial).withFlex(1).withMargin(knobMarginSmall));
            box.items.add(juce::FlexItem(vol2Dial).withFlex(1).withMargin(knobMarginSmall));
            box.items.add(juce::FlexItem(vol3Dial).withFlex(1).withMargin(knobMarginSmall));
            box.items.add(juce::FlexItem(vol4Dial).withFlex(1).withMargin(knobMarginSmall));
            box.items.add(juce::FlexItem(vol5Dial).withFlex(1).withMargin(knobMarginSmall));
            box.items.add(juce::FlexItem(vol6Dial).withFlex(1).withMargin(knobMarginSmall));
            box.items.add(juce::FlexItem(vol7Dial).withFlex(1).withMargin(knobMarginSmall));
            box.items.add(juce::FlexItem(vol8Dial).withFlex(1).withMargin(knobMarginSmall));
            box.items.add(juce::FlexItem(vol9Dial).withFlex(1).withMargin(knobMarginSmall));
            box.items.add(juce::FlexItem(vol10Dial).withFlex(1).withMargin(knobMarginSmall));
            box.performLayout(areas[1].toFloat());
        }
        {
            juce::FlexBox box;
            box.flexWrap = juce::FlexBox::Wrap::noWrap;
            box.flexDirection = juce::FlexBox::Direction::row;
            box.justifyContent = juce::FlexBox::JustifyContent::spaceAround;
            box.items.add(juce::FlexItem(pitch1Dial).withFlex(1).withMargin(knobMarginSmall));
            box.items.add(juce::FlexItem(pitch2Dial).withFlex(1).withMargin(knobMarginSmall));
            box.items.add(juce::FlexItem(pitch3Dial).withFlex(1).withMargin(knobMarginSmall));
            box.items.add(juce::FlexItem(pitch4Dial).withFlex(1).withMargin(knobMarginSmall));
            box.items.add(juce::FlexItem(pitch5Dial).withFlex(1).withMargin(knobMarginSmall));
            box.items.add(juce::FlexItem(pitch6Dial).withFlex(1).withMargin(knobMarginSmall));
            box.items.add(juce::FlexItem(pitch7Dial).withFlex(1).withMargin(knobMarginSmall));
            box.items.add(juce::FlexItem(pitch8Dial).withFlex(1).withMargin(knobMarginSmall));
            box.items.add(juce::FlexItem(pitch9Dial).withFlex(1).withMargin(knobMarginSmall));
            box.items.add(juce::FlexItem(pitch10Dial).withFlex(1).withMargin(knobMarginSmall));
            box.performLayout(areas[2].toFloat());
        }
        {
            juce::FlexBox box;
            box.flexWrap = juce::FlexBox::Wrap::noWrap;
            box.flexDirection = juce::FlexBox::Direction::row;
            box.justifyContent = juce::FlexBox::JustifyContent::spaceAround;
            box.items.add(juce::FlexItem(revFeed1Dial).withFlex(1).withMargin(knobMarginSmall));
            box.items.add(juce::FlexItem(revFeed2Dial).withFlex(1).withMargin(knobMarginSmall));
            box.items.add(juce::FlexItem(revFeed3Dial).withFlex(1).withMargin(knobMarginSmall));
            box.items.add(juce::FlexItem(revFeed4Dial).withFlex(1).withMargin(knobMarginSmall));
            box.items.add(juce::FlexItem(revFeed5Dial).withFlex(1).withMargin(knobMarginSmall));
            box.items.add(juce::FlexItem(revFeed6Dial).withFlex(1).withMargin(knobMarginSmall));
            box.items.add(juce::FlexItem(revFeed7Dial).withFlex(1).withMargin(knobMarginSmall));
            box.items.add(juce::FlexItem(revFeed8Dial).withFlex(1).withMargin(knobMarginSmall));
            box.items.add(juce::FlexItem(revFeed9Dial).withFlex(1).withMargin(knobMarginSmall));
            box.items.add(juce::FlexItem(revFeed10Dial).withFlex(1).withMargin(knobMarginSmall));
            box.performLayout(areas[3].toFloat());
        }
        {
            juce::FlexBox box;
            box.flexWrap = juce::FlexBox::Wrap::noWrap;
            box.flexDirection = juce::FlexBox::Direction::row;
            box.justifyContent = juce::FlexBox::JustifyContent::spaceAround;
            box.items.add(juce::FlexItem(spectrogramGauge).withFlex(1).withMargin(knobMarginSmall));
            box.performLayout(areas[4].toFloat());
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
        addAndMakeVisible(syncSwitch);
        syncSwitchAttachment =
            std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(valueTreeState, "sync", syncSwitch);

        addAndMakeVisible(typeDrop);
        typeDrop.addItemList(valueTreeState.getParameter("type")->getAllValueStrings(), 1);
        typeDropAttachment =
            std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(valueTreeState, "type", typeDrop);
        addAndMakeVisible(soloDrop);
        soloDrop.addItemList(valueTreeState.getParameter("solo")->getAllValueStrings(), 1);
        soloDropAttachment =
            std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(valueTreeState, "solo", soloDrop);
        addAndMakeVisible(volDial);
        volDial.reset(valueTreeState, "vol");
        volDial.setLabelText(juce::String::fromUTF8("Vol"));
        addAndMakeVisible(reverbLevelWetDial);
        reverbLevelWetDial.reset(valueTreeState, "reverbLevelWet");
        reverbLevelWetDial.setLabelText(juce::String::fromUTF8("Reverb Wet"));
        addAndMakeVisible(reverbDecayDial);
        reverbDecayDial.reset(valueTreeState, "reverbDecay");
        reverbDecayDial.setLabelText(juce::String::fromUTF8("Rev Decay"));
        addAndMakeVisible(reverbShimmerDial);
        reverbShimmerDial.reset(valueTreeState, "reverbShimmer");
        reverbShimmerDial.setLabelText(juce::String::fromUTF8("Rev Shimmer"));
        addAndMakeVisible(cpuGauge);
        cpuGauge.setLabelText(juce::String::fromUTF8("CPU"));
        addAndMakeVisible(levelGauge);
        levelGauge.setLabelText(juce::String::fromUTF8("Level"));
        addAndMakeVisible(spectrogramGauge);
        spectrogramGauge.setLabelText(juce::String::fromUTF8("Spectrogram"));
        addAndMakeVisible(vol1Dial);
        vol1Dial.reset(valueTreeState, "vol1");
        vol1Dial.setLabelText(juce::String::fromUTF8("Vol 1"));
        addAndMakeVisible(vol2Dial);
        vol2Dial.reset(valueTreeState, "vol2");
        vol2Dial.setLabelText(juce::String::fromUTF8("Vol 2"));
        addAndMakeVisible(vol3Dial);
        vol3Dial.reset(valueTreeState, "vol3");
        vol3Dial.setLabelText(juce::String::fromUTF8("Vol 3"));
        addAndMakeVisible(vol4Dial);
        vol4Dial.reset(valueTreeState, "vol4");
        vol4Dial.setLabelText(juce::String::fromUTF8("Vol 4"));
        addAndMakeVisible(vol5Dial);
        vol5Dial.reset(valueTreeState, "vol5");
        vol5Dial.setLabelText(juce::String::fromUTF8("Vol 5"));
        addAndMakeVisible(vol6Dial);
        vol6Dial.reset(valueTreeState, "vol6");
        vol6Dial.setLabelText(juce::String::fromUTF8("Vol 6"));
        addAndMakeVisible(vol7Dial);
        vol7Dial.reset(valueTreeState, "vol7");
        vol7Dial.setLabelText(juce::String::fromUTF8("Vol 7"));
        addAndMakeVisible(vol8Dial);
        vol8Dial.reset(valueTreeState, "vol8");
        vol8Dial.setLabelText(juce::String::fromUTF8("Vol 8"));
        addAndMakeVisible(vol9Dial);
        vol9Dial.reset(valueTreeState, "vol9");
        vol9Dial.setLabelText(juce::String::fromUTF8("Vol 9"));
        addAndMakeVisible(vol10Dial);
        vol10Dial.reset(valueTreeState, "vol10");
        vol10Dial.setLabelText(juce::String::fromUTF8("Vol 10"));
        addAndMakeVisible(revFeed1Dial);
        revFeed1Dial.reset(valueTreeState, "revFeed1");
        revFeed1Dial.setLabelText(juce::String::fromUTF8("Rev 1"));
        addAndMakeVisible(revFeed2Dial);
        revFeed2Dial.reset(valueTreeState, "revFeed2");
        revFeed2Dial.setLabelText(juce::String::fromUTF8("Rev 2"));
        addAndMakeVisible(revFeed3Dial);
        revFeed3Dial.reset(valueTreeState, "revFeed3");
        revFeed3Dial.setLabelText(juce::String::fromUTF8("Rev 3"));
        addAndMakeVisible(revFeed4Dial);
        revFeed4Dial.reset(valueTreeState, "revFeed4");
        revFeed4Dial.setLabelText(juce::String::fromUTF8("Rev 4"));
        addAndMakeVisible(revFeed5Dial);
        revFeed5Dial.reset(valueTreeState, "revFeed5");
        revFeed5Dial.setLabelText(juce::String::fromUTF8("Rev 5"));
        addAndMakeVisible(revFeed6Dial);
        revFeed6Dial.reset(valueTreeState, "revFeed6");
        revFeed6Dial.setLabelText(juce::String::fromUTF8("Rev 6"));
        addAndMakeVisible(revFeed7Dial);
        revFeed7Dial.reset(valueTreeState, "revFeed7");
        revFeed7Dial.setLabelText(juce::String::fromUTF8("Rev 7"));
        addAndMakeVisible(revFeed8Dial);
        revFeed8Dial.reset(valueTreeState, "revFeed8");
        revFeed8Dial.setLabelText(juce::String::fromUTF8("Rev 8"));
        addAndMakeVisible(revFeed9Dial);
        revFeed9Dial.reset(valueTreeState, "revFeed9");
        revFeed9Dial.setLabelText(juce::String::fromUTF8("Rev 9"));
        addAndMakeVisible(revFeed10Dial);
        revFeed10Dial.reset(valueTreeState, "revFeed10");
        revFeed10Dial.setLabelText(juce::String::fromUTF8("Rev 10"));
        addAndMakeVisible(pitch1Dial);
        pitch1Dial.reset(valueTreeState, "pitch1");
        pitch1Dial.setLabelText(juce::String::fromUTF8("Pch 1"));
        addAndMakeVisible(pitch2Dial);
        pitch2Dial.reset(valueTreeState, "pitch2");
        pitch2Dial.setLabelText(juce::String::fromUTF8("Pch 2"));
        addAndMakeVisible(pitch3Dial);
        pitch3Dial.reset(valueTreeState, "pitch3");
        pitch3Dial.setLabelText(juce::String::fromUTF8("Pch 3"));
        addAndMakeVisible(pitch4Dial);
        pitch4Dial.reset(valueTreeState, "pitch4");
        pitch4Dial.setLabelText(juce::String::fromUTF8("Pch 4"));
        addAndMakeVisible(pitch5Dial);
        pitch5Dial.reset(valueTreeState, "pitch5");
        pitch5Dial.setLabelText(juce::String::fromUTF8("Pch 5"));
        addAndMakeVisible(pitch6Dial);
        pitch6Dial.reset(valueTreeState, "pitch6");
        pitch6Dial.setLabelText(juce::String::fromUTF8("Pch 6"));
        addAndMakeVisible(pitch7Dial);
        pitch7Dial.reset(valueTreeState, "pitch7");
        pitch7Dial.setLabelText(juce::String::fromUTF8("Pch 7"));
        addAndMakeVisible(pitch8Dial);
        pitch8Dial.reset(valueTreeState, "pitch8");
        pitch8Dial.setLabelText(juce::String::fromUTF8("Pch 8"));
        addAndMakeVisible(pitch9Dial);
        pitch9Dial.reset(valueTreeState, "pitch9");
        pitch9Dial.setLabelText(juce::String::fromUTF8("Pch 9"));
        addAndMakeVisible(pitch10Dial);
        pitch10Dial.reset(valueTreeState, "pitch10");
        pitch10Dial.setLabelText(juce::String::fromUTF8("Pch 10"));
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
            for (size_t i = 0; i < Themes::kThemes.size(); ++i)
            {
                themeMenu.addItem(static_cast<int>(i) + 1, Themes::kThemes[i].name);
            }
            menu.addSubMenu("Theme", themeMenu);
            menu.addSubMenu("Patches", buildPatchesMenu());
        }
        return menu;
    }

    void menuItemSelected(int menuItemID, int /*topLevelMenuIndex*/) override
    {
        if (menuItemID >= 1 && menuItemID <= static_cast<int>(Themes::kThemes.size()))
        {
            applyTheme(static_cast<GuiConstants::Theme>(menuItemID - 1));
            return;
        }
        handlePatchMenuSelection(menuItemID);
    }

    void applyTheme(GuiConstants::Theme preset)
    {
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
    static constexpr int kPatchSaveId = 1000;
    static constexpr int kPatchSaveAsId = 1001;
    static constexpr int kPatchLoadIdBase = 2000;
    static constexpr int kPatchDeleteIdBase = 3000;
    static constexpr int kPatchRenameIdBase = 4000;
    std::unique_ptr<juce::AlertWindow> m_patchNameDialog;
    std::vector<juce::String> m_patchMenuNames;

    juce::ToggleButton syncSwitch{juce::String::fromUTF8("Sync")};
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> syncSwitchAttachment;
    juce::ComboBox typeDrop{};
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> typeDropAttachment;
    juce::ComboBox soloDrop{};
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> soloDropAttachment;
    CustomRotaryDial volDial{this};
    CustomRotaryDial reverbLevelWetDial{this};
    CustomRotaryDial reverbDecayDial{this};
    CustomRotaryDial reverbShimmerDial{this};
    CpuGauge cpuGauge{};
    Gauge levelGauge{};
    SpectrogramDisplay spectrogramGauge{AppSettings::loadTheme()};
    CustomRotaryDial vol1Dial{this};
    CustomRotaryDial vol2Dial{this};
    CustomRotaryDial vol3Dial{this};
    CustomRotaryDial vol4Dial{this};
    CustomRotaryDial vol5Dial{this};
    CustomRotaryDial vol6Dial{this};
    CustomRotaryDial vol7Dial{this};
    CustomRotaryDial vol8Dial{this};
    CustomRotaryDial vol9Dial{this};
    CustomRotaryDial vol10Dial{this};
    CustomRotaryDial revFeed1Dial{this};
    CustomRotaryDial revFeed2Dial{this};
    CustomRotaryDial revFeed3Dial{this};
    CustomRotaryDial revFeed4Dial{this};
    CustomRotaryDial revFeed5Dial{this};
    CustomRotaryDial revFeed6Dial{this};
    CustomRotaryDial revFeed7Dial{this};
    CustomRotaryDial revFeed8Dial{this};
    CustomRotaryDial revFeed9Dial{this};
    CustomRotaryDial revFeed10Dial{this};
    CustomRotaryDial pitch1Dial{this};
    CustomRotaryDial pitch2Dial{this};
    CustomRotaryDial pitch3Dial{this};
    CustomRotaryDial pitch4Dial{this};
    CustomRotaryDial pitch5Dial{this};
    CustomRotaryDial pitch6Dial{this};
    CustomRotaryDial pitch7Dial{this};
    CustomRotaryDial pitch8Dial{this};
    CustomRotaryDial pitch9Dial{this};
    CustomRotaryDial pitch10Dial{this};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioPluginAudioProcessorEditor)
};
