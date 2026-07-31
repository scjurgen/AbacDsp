#pragma once
/*
 * AUTO GENERATED,
 * NOT A GOOD IDEA TO CHANGE STUFF HERE
 * Keep the file readonly
 */

#include "MaxdiffuserProcessor.h"
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
        std::vector<juce::Rectangle<int>> areas(5);
        const auto rowHeight = area.getHeight() / 10;
        areas[0] = area.removeFromTop(rowHeight * 2).reduced(Constants::Margins::small);
        areas[1] = area.removeFromTop(rowHeight * 2).reduced(Constants::Margins::small);
        areas[2] = area.removeFromTop(rowHeight * 2).reduced(Constants::Margins::small);
        areas[3] = area.removeFromTop(rowHeight * 2).reduced(Constants::Margins::small);
        areas[4] = area.reduced(Constants::Margins::small);

        {
            juce::FlexBox box;
            box.flexWrap = juce::FlexBox::Wrap::noWrap;
            box.flexDirection = juce::FlexBox::Direction::row;
            box.justifyContent = juce::FlexBox::JustifyContent::spaceAround;
            box.items.add(juce::FlexItem(dryDial).withFlex(1).withMargin(knobMarginSmall));
            box.items.add(juce::FlexItem(wetDial).withFlex(1).withMargin(knobMarginSmall));
            box.items.add(juce::FlexItem(preDelayDial).withFlex(1).withMargin(knobMarginSmall));
            box.items.add(juce::FlexItem(mixDial).withFlex(1).withMargin(knobMarginSmall));
            box.items.add(juce::FlexItem(pitchDial).withFlex(1).withMargin(knobMarginSmall));
            box.items.add(juce::FlexItem(pitchModeDrop)
                              .withFlex(1)
                              .withHeight(Constants::Text::labelHeight)
                              .withAlignSelf(juce::FlexItem::AlignSelf::center)
                              .withMargin(knobMarginSmall));
            box.performLayout(areas[0].toFloat());
        }
        {
            juce::FlexBox box;
            box.flexWrap = juce::FlexBox::Wrap::noWrap;
            box.flexDirection = juce::FlexBox::Direction::row;
            box.justifyContent = juce::FlexBox::JustifyContent::spaceAround;
            box.items.add(juce::FlexItem(elementsDial).withFlex(1).withMargin(knobMarginSmall));
            box.items.add(juce::FlexItem(feedbackDial).withFlex(1).withMargin(knobMarginSmall));
            box.items.add(juce::FlexItem(bulgeDial).withFlex(1).withMargin(knobMarginSmall));
            box.items.add(juce::FlexItem(bottomSizeDial).withFlex(1).withMargin(knobMarginSmall));
            box.items.add(juce::FlexItem(topSizeDial).withFlex(1).withMargin(knobMarginSmall));
            box.performLayout(areas[1].toFloat());
        }
        {
            juce::FlexBox box;
            box.flexWrap = juce::FlexBox::Wrap::noWrap;
            box.flexDirection = juce::FlexBox::Direction::row;
            box.justifyContent = juce::FlexBox::JustifyContent::spaceAround;
            box.items.add(juce::FlexItem(modulationDepthDial).withFlex(1).withMargin(knobMarginSmall));
            box.items.add(juce::FlexItem(modulationSpeedDial).withFlex(1).withMargin(knobMarginSmall));
            box.items.add(juce::FlexItem(lowPassDial).withFlex(1).withMargin(knobMarginSmall));
            box.performLayout(areas[2].toFloat());
        }
        {
            juce::FlexBox box;
            box.flexWrap = juce::FlexBox::Wrap::noWrap;
            box.flexDirection = juce::FlexBox::Direction::row;
            box.justifyContent = juce::FlexBox::JustifyContent::spaceAround;
            box.items.add(juce::FlexItem(fdnMixDial).withFlex(1).withMargin(knobMarginSmall));
            box.items.add(juce::FlexItem(fdnSizeDial).withFlex(1).withMargin(knobMarginSmall));
            box.items.add(juce::FlexItem(fdnDecayDial).withFlex(1).withMargin(knobMarginSmall));
            box.performLayout(areas[3].toFloat());
        }
        {
            juce::FlexBox box;
            box.flexWrap = juce::FlexBox::Wrap::noWrap;
            box.flexDirection = juce::FlexBox::Direction::row;
            box.justifyContent = juce::FlexBox::JustifyContent::spaceAround;
            box.items.add(juce::FlexItem(binsGauge).withFlex(1).withMargin(knobMarginSmall));
            box.items.add(juce::FlexItem(binsBandsGauge).withFlex(1).withMargin(knobMarginSmall));
            box.performLayout(areas[4].toFloat());
        }
    }
#pragma GCC diagnostic pop

    void timerCallback() override
    {
        if (processorRef.hasRunner())
        {
            binsGauge.update(processorRef.getProcessingBinLevels(),
                             static_cast<size_t>(valueTreeState.getRawParameterValue("elements")->load()));
            binsBandsGauge.update(processorRef.getProcessingBinBandLevels(),
                                  static_cast<size_t>(valueTreeState.getRawParameterValue("elements")->load()));
            processorRef.consumeLastLearnedCc();
        }
    }

    void initWidgets()
    {
        addAndMakeVisible(dryDial);
        dryDial.reset(valueTreeState, "dry");
        dryDial.setLabelText(juce::String::fromUTF8("Dry"));
        dryDial.setCcMappable(true, {[this] { processorRef.beginCcLearn(CcTarget::dry); },
                                     [this] { return processorRef.getCcRange(CcTarget::dry); },
                                     [this](float lo, float hi) { processorRef.setCcRange(CcTarget::dry, lo, hi); },
                                     [this] { processorRef.clearCcAssignment(CcTarget::dry); },
                                     [this] { return processorRef.getCcController(CcTarget::dry); }});
        addAndMakeVisible(wetDial);
        wetDial.reset(valueTreeState, "wet");
        wetDial.setLabelText(juce::String::fromUTF8("Wet"));
        wetDial.setCcMappable(true, {[this] { processorRef.beginCcLearn(CcTarget::wet); },
                                     [this] { return processorRef.getCcRange(CcTarget::wet); },
                                     [this](float lo, float hi) { processorRef.setCcRange(CcTarget::wet, lo, hi); },
                                     [this] { processorRef.clearCcAssignment(CcTarget::wet); },
                                     [this] { return processorRef.getCcController(CcTarget::wet); }});
        addAndMakeVisible(preDelayDial);
        preDelayDial.reset(valueTreeState, "preDelay");
        preDelayDial.setLabelText(juce::String::fromUTF8("Pre Delay"));
        preDelayDial.setCcMappable(true,
                                   {[this] { processorRef.beginCcLearn(CcTarget::preDelay); },
                                    [this] { return processorRef.getCcRange(CcTarget::preDelay); },
                                    [this](float lo, float hi) { processorRef.setCcRange(CcTarget::preDelay, lo, hi); },
                                    [this] { processorRef.clearCcAssignment(CcTarget::preDelay); },
                                    [this] { return processorRef.getCcController(CcTarget::preDelay); }});
        addAndMakeVisible(elementsDial);
        elementsDial.reset(valueTreeState, "elements");
        elementsDial.setLabelText(juce::String::fromUTF8("Elements"));
        elementsDial.setCcMappable(true,
                                   {[this] { processorRef.beginCcLearn(CcTarget::elements); },
                                    [this] { return processorRef.getCcRange(CcTarget::elements); },
                                    [this](float lo, float hi) { processorRef.setCcRange(CcTarget::elements, lo, hi); },
                                    [this] { processorRef.clearCcAssignment(CcTarget::elements); },
                                    [this] { return processorRef.getCcController(CcTarget::elements); }});
        addAndMakeVisible(feedbackDial);
        feedbackDial.reset(valueTreeState, "feedback");
        feedbackDial.setLabelText(juce::String::fromUTF8("Diffusion"));
        feedbackDial.setCcMappable(true,
                                   {[this] { processorRef.beginCcLearn(CcTarget::feedback); },
                                    [this] { return processorRef.getCcRange(CcTarget::feedback); },
                                    [this](float lo, float hi) { processorRef.setCcRange(CcTarget::feedback, lo, hi); },
                                    [this] { processorRef.clearCcAssignment(CcTarget::feedback); },
                                    [this] { return processorRef.getCcController(CcTarget::feedback); }});
        addAndMakeVisible(bulgeDial);
        bulgeDial.reset(valueTreeState, "bulge");
        bulgeDial.setLabelText(juce::String::fromUTF8("Bulge"));
        bulgeDial.setCcMappable(true, {[this] { processorRef.beginCcLearn(CcTarget::bulge); },
                                       [this] { return processorRef.getCcRange(CcTarget::bulge); },
                                       [this](float lo, float hi) { processorRef.setCcRange(CcTarget::bulge, lo, hi); },
                                       [this] { processorRef.clearCcAssignment(CcTarget::bulge); },
                                       [this] { return processorRef.getCcController(CcTarget::bulge); }});
        addAndMakeVisible(bottomSizeDial);
        bottomSizeDial.reset(valueTreeState, "bottomSize");
        bottomSizeDial.setLabelText(juce::String::fromUTF8("Bottom Size"));
        bottomSizeDial.setCcMappable(true, {[this] { processorRef.beginCcLearn(CcTarget::bottomSize); },
                                            [this] { return processorRef.getCcRange(CcTarget::bottomSize); },
                                            [this](float lo, float hi)
                                            { processorRef.setCcRange(CcTarget::bottomSize, lo, hi); },
                                            [this] { processorRef.clearCcAssignment(CcTarget::bottomSize); },
                                            [this] { return processorRef.getCcController(CcTarget::bottomSize); }});
        addAndMakeVisible(topSizeDial);
        topSizeDial.reset(valueTreeState, "topSize");
        topSizeDial.setLabelText(juce::String::fromUTF8("Top Size"));
        topSizeDial.setCcMappable(true,
                                  {[this] { processorRef.beginCcLearn(CcTarget::topSize); },
                                   [this] { return processorRef.getCcRange(CcTarget::topSize); },
                                   [this](float lo, float hi) { processorRef.setCcRange(CcTarget::topSize, lo, hi); },
                                   [this] { processorRef.clearCcAssignment(CcTarget::topSize); },
                                   [this] { return processorRef.getCcController(CcTarget::topSize); }});
        addAndMakeVisible(modulationDepthDial);
        modulationDepthDial.reset(valueTreeState, "modulationDepth");
        modulationDepthDial.setLabelText(juce::String::fromUTF8("Mod Depth"));
        modulationDepthDial.setCcMappable(true, {[this] { processorRef.beginCcLearn(CcTarget::modulationDepth); },
                                                 [this] { return processorRef.getCcRange(CcTarget::modulationDepth); },
                                                 [this](float lo, float hi)
                                                 { processorRef.setCcRange(CcTarget::modulationDepth, lo, hi); }, [this]
                                                 { processorRef.clearCcAssignment(CcTarget::modulationDepth); }, [this]
                                                 { return processorRef.getCcController(CcTarget::modulationDepth); }});
        addAndMakeVisible(modulationSpeedDial);
        modulationSpeedDial.reset(valueTreeState, "modulationSpeed");
        modulationSpeedDial.setLabelText(juce::String::fromUTF8("Mod Speed"));
        modulationSpeedDial.setCcMappable(true, {[this] { processorRef.beginCcLearn(CcTarget::modulationSpeed); },
                                                 [this] { return processorRef.getCcRange(CcTarget::modulationSpeed); },
                                                 [this](float lo, float hi)
                                                 { processorRef.setCcRange(CcTarget::modulationSpeed, lo, hi); }, [this]
                                                 { processorRef.clearCcAssignment(CcTarget::modulationSpeed); }, [this]
                                                 { return processorRef.getCcController(CcTarget::modulationSpeed); }});
        addAndMakeVisible(lowPassDial);
        lowPassDial.reset(valueTreeState, "lowPass");
        lowPassDial.setLabelText(juce::String::fromUTF8("Low Pass"));
        lowPassDial.setCcMappable(true,
                                  {[this] { processorRef.beginCcLearn(CcTarget::lowPass); },
                                   [this] { return processorRef.getCcRange(CcTarget::lowPass); },
                                   [this](float lo, float hi) { processorRef.setCcRange(CcTarget::lowPass, lo, hi); },
                                   [this] { processorRef.clearCcAssignment(CcTarget::lowPass); },
                                   [this] { return processorRef.getCcController(CcTarget::lowPass); }});
        addAndMakeVisible(mixDial);
        mixDial.reset(valueTreeState, "mix");
        mixDial.setLabelText(juce::String::fromUTF8("Pitch Mix"));
        mixDial.setCcMappable(true, {[this] { processorRef.beginCcLearn(CcTarget::mix); },
                                     [this] { return processorRef.getCcRange(CcTarget::mix); },
                                     [this](float lo, float hi) { processorRef.setCcRange(CcTarget::mix, lo, hi); },
                                     [this] { processorRef.clearCcAssignment(CcTarget::mix); },
                                     [this] { return processorRef.getCcController(CcTarget::mix); }});
        addAndMakeVisible(pitchDial);
        pitchDial.reset(valueTreeState, "pitch");
        pitchDial.setLabelText(juce::String::fromUTF8("Pitch"));
        pitchDial.setCcMappable(true, {[this] { processorRef.beginCcLearn(CcTarget::pitch); },
                                       [this] { return processorRef.getCcRange(CcTarget::pitch); },
                                       [this](float lo, float hi) { processorRef.setCcRange(CcTarget::pitch, lo, hi); },
                                       [this] { processorRef.clearCcAssignment(CcTarget::pitch); },
                                       [this] { return processorRef.getCcController(CcTarget::pitch); }});
        addAndMakeVisible(pitchModeDrop);
        pitchModeDrop.addItemList(valueTreeState.getParameter("pitchMode")->getAllValueStrings(), 1);
        pitchModeDropAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
            valueTreeState, "pitchMode", pitchModeDrop);
        addAndMakeVisible(fdnMixDial);
        fdnMixDial.reset(valueTreeState, "fdnMix");
        fdnMixDial.setLabelText(juce::String::fromUTF8("FDN Mix"));
        fdnMixDial.setCcMappable(true,
                                 {[this] { processorRef.beginCcLearn(CcTarget::fdnMix); },
                                  [this] { return processorRef.getCcRange(CcTarget::fdnMix); },
                                  [this](float lo, float hi) { processorRef.setCcRange(CcTarget::fdnMix, lo, hi); },
                                  [this] { processorRef.clearCcAssignment(CcTarget::fdnMix); },
                                  [this] { return processorRef.getCcController(CcTarget::fdnMix); }});
        addAndMakeVisible(fdnSizeDial);
        fdnSizeDial.reset(valueTreeState, "fdnSize");
        fdnSizeDial.setLabelText(juce::String::fromUTF8("FDN Size"));
        fdnSizeDial.setCcMappable(true,
                                  {[this] { processorRef.beginCcLearn(CcTarget::fdnSize); },
                                   [this] { return processorRef.getCcRange(CcTarget::fdnSize); },
                                   [this](float lo, float hi) { processorRef.setCcRange(CcTarget::fdnSize, lo, hi); },
                                   [this] { processorRef.clearCcAssignment(CcTarget::fdnSize); },
                                   [this] { return processorRef.getCcController(CcTarget::fdnSize); }});
        addAndMakeVisible(fdnDecayDial);
        fdnDecayDial.reset(valueTreeState, "fdnDecay");
        fdnDecayDial.setLabelText(juce::String::fromUTF8("FDN Decay"));
        fdnDecayDial.setCcMappable(true,
                                   {[this] { processorRef.beginCcLearn(CcTarget::fdnDecay); },
                                    [this] { return processorRef.getCcRange(CcTarget::fdnDecay); },
                                    [this](float lo, float hi) { processorRef.setCcRange(CcTarget::fdnDecay, lo, hi); },
                                    [this] { processorRef.clearCcAssignment(CcTarget::fdnDecay); },
                                    [this] { return processorRef.getCcController(CcTarget::fdnDecay); }});
        addAndMakeVisible(binsGauge);
        binsGauge.setLabelText(juce::String::fromUTF8("Bins"));
        addAndMakeVisible(binsBandsGauge);
        binsBandsGauge.setLabelText(juce::String::fromUTF8("Bands"));
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
        binsGauge.updateColors();
        binsBandsGauge.updateColors();

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


    CustomRotaryDial dryDial{this};
    CustomRotaryDial wetDial{this};
    CustomRotaryDial preDelayDial{this};
    CustomRotaryDial elementsDial{this};
    CustomRotaryDial feedbackDial{this};
    CustomRotaryDial bulgeDial{this};
    CustomRotaryDial bottomSizeDial{this};
    CustomRotaryDial topSizeDial{this};
    CustomRotaryDial modulationDepthDial{this};
    CustomRotaryDial modulationSpeedDial{this};
    CustomRotaryDial lowPassDial{this};
    CustomRotaryDial mixDial{this};
    CustomRotaryDial pitchDial{this};
    juce::ComboBox pitchModeDrop{};
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> pitchModeDropAttachment;
    CustomRotaryDial fdnMixDial{this};
    CustomRotaryDial fdnSizeDial{this};
    CustomRotaryDial fdnDecayDial{this};
    ShowProcessingBins<BinsDisplayMode::ShowContinuousLine, LevelUnit::Decibel> binsGauge{};
    ShowProcessingBinsBands binsBandsGauge{};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioPluginAudioProcessorEditor)
};
