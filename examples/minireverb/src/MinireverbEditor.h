#pragma once
/*
 * AUTO GENERATED,
 * NOT A GOOD IDEA TO CHANGE STUFF HERE
 * Keep the file readonly
 */

#include <map>

#include "MinireverbProcessor.h"
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
                box.items.add(juce::FlexItem(levelGauge).withFlex(1).withMargin(knobMarginSmall));
                box.performLayout(areas[0].toFloat());
            }
        }
        else
        {
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
                box.items.add(juce::FlexItem(decayDial).withFlex(1).withMargin(knobMarginSmall));
                box.items.add(juce::FlexItem(dryDial).withFlex(1).withMargin(knobMarginSmall));
                box.items.add(juce::FlexItem(wetDial).withFlex(1).withMargin(knobMarginSmall));
                box.items.add(juce::FlexItem(stereoWidthDial).withFlex(1).withMargin(knobMarginSmall));
                box.items.add(juce::FlexItem(spectrogramGauge).withWidth(600).withMargin(knobMarginSmall));
                box.performLayout(areas[1].toFloat());
            }
            {
                juce::FlexBox box;
                box.flexWrap = juce::FlexBox::Wrap::noWrap;
                box.flexDirection = juce::FlexBox::Direction::row;
                box.justifyContent = juce::FlexBox::JustifyContent::spaceAround;
                box.items.add(juce::FlexItem(baseSizeDial).withFlex(1).withMargin(knobMarginSmall));
                box.items.add(juce::FlexItem(sizeFactorDial).withFlex(1).withMargin(knobMarginSmall));
                box.items.add(juce::FlexItem(bulgeDial).withFlex(1).withMargin(knobMarginSmall));
                box.items.add(juce::FlexItem(uniqueDelaySwitch)
                                  .withWidth(Constants::Text::labelWidth)
                                  .withHeight(Constants::Text::labelHeight)
                                  .withAlignSelf(juce::FlexItem::AlignSelf::center)
                                  .withMargin(knobMarginSmall));
                box.performLayout(areas[2].toFloat());
            }
            {
                juce::FlexBox box;
                box.flexWrap = juce::FlexBox::Wrap::noWrap;
                box.flexDirection = juce::FlexBox::Direction::row;
                box.justifyContent = juce::FlexBox::JustifyContent::spaceAround;
                box.items.add(juce::FlexItem(allPassUpDial).withFlex(1).withMargin(knobMarginSmall));
                box.items.add(juce::FlexItem(allPassDownDial).withFlex(1).withMargin(knobMarginSmall));
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
                box.items.add(juce::FlexItem(modulationDepthDial).withFlex(1).withMargin(knobMarginSmall));
                box.items.add(juce::FlexItem(modulationSpeedDial).withFlex(1).withMargin(knobMarginSmall));
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
                box.performLayout(areas[5].toFloat());
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
        }
    }

    void initWidgets()
    {
        addAndMakeVisible(orderDrop);
        orderDrop.addItemList(valueTreeState.getParameter("order")->getAllValueStrings(), 1);
        orderDropAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
            valueTreeState, "order", orderDrop);
        orderDrop.setTooltip(juce::String::fromUTF8("Order (4, 8, 12, 16, 20, 24, 32, 48, 64)"));
        addAndMakeVisible(dryDial);
        dryDial.reset(valueTreeState, "dry");
        dryDial.setLabelText(juce::String::fromUTF8("Dry"));
        dryDial.setTooltip(juce::String::fromUTF8("Dry (-100 to 12 dB)"));
        addAndMakeVisible(wetDial);
        wetDial.reset(valueTreeState, "wet");
        wetDial.setLabelText(juce::String::fromUTF8("Wet"));
        wetDial.setTooltip(juce::String::fromUTF8("Wet (-100 to 12 dB)"));
        addAndMakeVisible(stereoWidthDial);
        stereoWidthDial.reset(valueTreeState, "stereoWidth");
        stereoWidthDial.setLabelText(juce::String::fromUTF8("Stereo Width"));
        stereoWidthDial.setTooltip(juce::String::fromUTF8("Stereo Width (0 to 100)"));
        addAndMakeVisible(baseSizeDial);
        baseSizeDial.reset(valueTreeState, "baseSize");
        baseSizeDial.setLabelText(juce::String::fromUTF8("Base size"));
        baseSizeDial.setTooltip(juce::String::fromUTF8("Base size (1.0 to 600 m)"));
        addAndMakeVisible(sizeFactorDial);
        sizeFactorDial.reset(valueTreeState, "sizeFactor");
        sizeFactorDial.setLabelText(juce::String::fromUTF8("Size Factor"));
        sizeFactorDial.setTooltip(juce::String::fromUTF8("Size Factor (1.0 to 20 x)"));
        addAndMakeVisible(bulgeDial);
        bulgeDial.reset(valueTreeState, "bulge");
        bulgeDial.setLabelText(juce::String::fromUTF8("Bulge"));
        bulgeDial.setTooltip(juce::String::fromUTF8("Bulge (-1 to 1)"));
        addAndMakeVisible(uniqueDelaySwitch);
        uniqueDelaySwitchAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
            valueTreeState, "uniqueDelay", uniqueDelaySwitch);
        uniqueDelaySwitch.setTooltip(juce::String::fromUTF8("Unique delay"));

        addAndMakeVisible(decayDial);
        decayDial.reset(valueTreeState, "decay");
        decayDial.setLabelText(juce::String::fromUTF8("Decay Low"));
        decayDial.setTooltip(juce::String::fromUTF8("Decay Low (0 to 100000 ms)"));
        addAndMakeVisible(allPassUpDial);
        allPassUpDial.reset(valueTreeState, "allPassUp");
        allPassUpDial.setLabelText(juce::String::fromUTF8("All pass First"));
        allPassUpDial.setTooltip(juce::String::fromUTF8("All pass First (20 to 20000 Hz)"));
        addAndMakeVisible(allPassDownDial);
        allPassDownDial.reset(valueTreeState, "allPassDown");
        allPassDownDial.setLabelText(juce::String::fromUTF8("All pass Last"));
        allPassDownDial.setTooltip(juce::String::fromUTF8("All pass Last (20 to 20000 Hz)"));
        addAndMakeVisible(lowPassDial);
        lowPassDial.reset(valueTreeState, "lowPass");
        lowPassDial.setLabelText(juce::String::fromUTF8("Low pass"));
        lowPassDial.setTooltip(juce::String::fromUTF8("Low pass (20 to 20000 Hz)"));
        addAndMakeVisible(lowPassCountDrop);
        lowPassCountDrop.addItemList(valueTreeState.getParameter("lowPassCount")->getAllValueStrings(), 1);
        lowPassCountDropAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
            valueTreeState, "lowPassCount", lowPassCountDrop);
        lowPassCountDrop.setTooltip(juce::String::fromUTF8("Low pass count (none, one, two, 1/4, 1/2, 3/4, All)"));
        addAndMakeVisible(highPassDial);
        highPassDial.reset(valueTreeState, "highPass");
        highPassDial.setLabelText(juce::String::fromUTF8("High pass"));
        highPassDial.setTooltip(juce::String::fromUTF8("High pass (20 to 20000 Hz)"));
        addAndMakeVisible(highPassCountDrop);
        highPassCountDrop.addItemList(valueTreeState.getParameter("highPassCount")->getAllValueStrings(), 1);
        highPassCountDropAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
            valueTreeState, "highPassCount", highPassCountDrop);
        highPassCountDrop.setTooltip(juce::String::fromUTF8("High pass count (none, one, two, 1/4, 1/2, 3/4, All)"));
        addAndMakeVisible(modulationDepthDial);
        modulationDepthDial.reset(valueTreeState, "modulationDepth");
        modulationDepthDial.setLabelText(juce::String::fromUTF8("Mod depth"));
        modulationDepthDial.setTooltip(juce::String::fromUTF8("Mod depth (0 to 1)"));
        addAndMakeVisible(modulationSpeedDial);
        modulationSpeedDial.reset(valueTreeState, "modulationSpeed");
        modulationSpeedDial.setLabelText(juce::String::fromUTF8("Mod speed"));
        modulationSpeedDial.setTooltip(juce::String::fromUTF8("Mod speed (0.01 to 5 Hz)"));
        addAndMakeVisible(reversePitchSwitch);
        reversePitchSwitchAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
            valueTreeState, "reversePitch", reversePitchSwitch);
        reversePitchSwitch.setTooltip(juce::String::fromUTF8("Reverse pitch"));

        addAndMakeVisible(pitchStrengthDial);
        pitchStrengthDial.reset(valueTreeState, "pitchStrength");
        pitchStrengthDial.setLabelText(juce::String::fromUTF8("Pitch Strength"));
        pitchStrengthDial.setTooltip(juce::String::fromUTF8("Pitch Strength (0.0 to 1.0)"));
        addAndMakeVisible(pitch1InplaceDial);
        pitch1InplaceDial.reset(valueTreeState, "pitch1Inplace");
        pitch1InplaceDial.setLabelText(juce::String::fromUTF8("Pitch 1 inplace"));
        pitch1InplaceDial.setTooltip(juce::String::fromUTF8("Pitch 1 inplace (-12 to 12 st)"));
        addAndMakeVisible(pitch2InplaceDial);
        pitch2InplaceDial.reset(valueTreeState, "pitch2Inplace");
        pitch2InplaceDial.setLabelText(juce::String::fromUTF8("Pitch 2 inplace"));
        pitch2InplaceDial.setTooltip(juce::String::fromUTF8("Pitch 2 inplace (-12 to 12 st)"));
        addAndMakeVisible(cpuGauge);
        cpuGauge.setLabelText(juce::String::fromUTF8("CPU"));
        cpuGauge.setTooltip(juce::String::fromUTF8("CPU (0 to 100 %)"));
        addAndMakeVisible(levelGauge);
        levelGauge.setLabelText(juce::String::fromUTF8("Level"));
        levelGauge.setTooltip(juce::String::fromUTF8("Level (0 to 100 %)"));
        addAndMakeVisible(spectrogramGauge);
        spectrogramGauge.setLabelText(juce::String::fromUTF8("Spectrogram"));
        spectrogramGauge.setTooltip(juce::String::fromUTF8("Spectrogram"));

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
            orderDrop.setVisible(false);
            dryDial.setVisible(false);
            wetDial.setVisible(false);
            stereoWidthDial.setVisible(false);
            baseSizeDial.setVisible(false);
            sizeFactorDial.setVisible(false);
            bulgeDial.setVisible(false);
            uniqueDelaySwitch.setVisible(false);
            decayDial.setVisible(false);
            allPassUpDial.setVisible(false);
            allPassDownDial.setVisible(false);
            lowPassDial.setVisible(false);
            lowPassCountDrop.setVisible(false);
            highPassDial.setVisible(false);
            highPassCountDrop.setVisible(false);
            modulationDepthDial.setVisible(false);
            modulationSpeedDial.setVisible(false);
            reversePitchSwitch.setVisible(false);
            pitchStrengthDial.setVisible(false);
            pitch1InplaceDial.setVisible(false);
            pitch2InplaceDial.setVisible(false);
            cpuGauge.setVisible(false);
            levelGauge.setVisible(true);
            spectrogramGauge.setVisible(false);
        }
        else
        {
            orderDrop.setVisible(true);
            dryDial.setVisible(true);
            wetDial.setVisible(true);
            stereoWidthDial.setVisible(true);
            baseSizeDial.setVisible(true);
            sizeFactorDial.setVisible(true);
            bulgeDial.setVisible(true);
            uniqueDelaySwitch.setVisible(true);
            decayDial.setVisible(true);
            allPassUpDial.setVisible(true);
            allPassDownDial.setVisible(true);
            lowPassDial.setVisible(true);
            lowPassCountDrop.setVisible(true);
            highPassDial.setVisible(true);
            highPassCountDrop.setVisible(true);
            modulationDepthDial.setVisible(true);
            modulationSpeedDial.setVisible(true);
            reversePitchSwitch.setVisible(true);
            pitchStrengthDial.setVisible(true);
            pitch1InplaceDial.setVisible(true);
            pitch2InplaceDial.setVisible(true);
            cpuGauge.setVisible(true);
            levelGauge.setVisible(true);
            spectrogramGauge.setVisible(true);
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
            "Just a small Reverb.\n\nPart of the AbacDsp project - core DSP library is MIT licensed.\n\nBuilt with "
            "JUCE, licensed under AGPLv3 (or a commercial JUCE licence).\n\nFull third-party license details: "
            "THIRD-PARTY-LICENSES.md in the AbacDsp repository.";

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
        m_patchNameDialog->addTextEditor("folder", folder, "Folder (optional):");
        m_patchNameDialog->addTextEditor("name", name, "Name:");
        m_patchNameDialog->addButton("Save", 1, juce::KeyPress(juce::KeyPress::returnKey));
        m_patchNameDialog->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));
        m_patchNameDialog->enterModalState(true,
                                           juce::ModalCallbackFunction::create(
                                               [this](int result)
                                               {
                                                   const auto folderText =
                                                       m_patchNameDialog->getTextEditorContents("folder").trim();
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
        m_patchNameDialog->addTextEditor("folder", folder, "Folder (optional):");
        m_patchNameDialog->addTextEditor("name", name, "Name:");
        m_patchNameDialog->addButton("Rename", 1, juce::KeyPress(juce::KeyPress::returnKey));
        m_patchNameDialog->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));
        m_patchNameDialog->enterModalState(true,
                                           juce::ModalCallbackFunction::create(
                                               [this, oldName](int result)
                                               {
                                                   const auto folderText =
                                                       m_patchNameDialog->getTextEditorContents("folder").trim();
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


    juce::ComboBox orderDrop{};
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> orderDropAttachment;
    CustomRotaryDial dryDial{this};
    CustomRotaryDial wetDial{this};
    CustomRotaryDial stereoWidthDial{this};
    CustomRotaryDial baseSizeDial{this};
    CustomRotaryDial sizeFactorDial{this};
    CustomRotaryDial bulgeDial{this};
    juce::ToggleButton uniqueDelaySwitch{juce::String::fromUTF8("Unique delay")};
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> uniqueDelaySwitchAttachment;
    CustomRotaryDial decayDial{this};
    CustomRotaryDial allPassUpDial{this};
    CustomRotaryDial allPassDownDial{this};
    CustomRotaryDial lowPassDial{this};
    juce::ComboBox lowPassCountDrop{};
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> lowPassCountDropAttachment;
    CustomRotaryDial highPassDial{this};
    juce::ComboBox highPassCountDrop{};
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> highPassCountDropAttachment;
    CustomRotaryDial modulationDepthDial{this};
    CustomRotaryDial modulationSpeedDial{this};
    juce::ToggleButton reversePitchSwitch{juce::String::fromUTF8("Reverse pitch")};
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> reversePitchSwitchAttachment;
    CustomRotaryDial pitchStrengthDial{this};
    CustomRotaryDial pitch1InplaceDial{this};
    CustomRotaryDial pitch2InplaceDial{this};
    CpuGauge cpuGauge{};
    Gauge levelGauge{};
    SpectrogramDisplay spectrogramGauge{AppSettings::loadTheme()};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioPluginAudioProcessorEditor)
};
