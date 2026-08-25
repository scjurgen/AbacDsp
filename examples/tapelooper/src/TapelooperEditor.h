#pragma once
/*
 * AUTO GENERATED,
 * NOT A GOOD IDEA TO CHANGE STUFF HERE
 * Keep the file readonly
 */

#include <map>

#include "TapelooperProcessor.h"
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
        {
            // auto generated
            // const juce::FlexItem::Margin knobMargin = juce::FlexItem::Margin(Constants::Margins::small);
            const juce::FlexItem::Margin knobMarginSmall = juce::FlexItem::Margin(Constants::Margins::medium);
            std::vector<juce::Rectangle<int>> areas(5);
            const auto colWidth = area.getWidth() / 5;
            areas[0] = area.removeFromLeft(colWidth * 1).reduced(Constants::Margins::small);
            areas[1] = area.removeFromLeft(colWidth * 1).reduced(Constants::Margins::small);
            areas[2] = area.removeFromLeft(colWidth * 1).reduced(Constants::Margins::small);
            areas[3] = area.removeFromLeft(colWidth * 1).reduced(Constants::Margins::small);
            areas[4] = area.reduced(Constants::Margins::small);

            {
                juce::FlexBox box;
                box.flexWrap = juce::FlexBox::Wrap::noWrap;
                box.flexDirection = juce::FlexBox::Direction::column;
                box.justifyContent = juce::FlexBox::JustifyContent::spaceAround;
                box.items.add(juce::FlexItem(tapeSpeedDial).withFlex(1).withMargin(knobMarginSmall));
                box.items.add(juce::FlexItem(barsDial).withFlex(1).withMargin(knobMarginSmall));
                box.items.add(juce::FlexItem(inputGainDial).withFlex(1).withMargin(knobMarginSmall));
                box.items.add(juce::FlexItem(levelGauge).withHeight(100).withMargin(knobMarginSmall));
                box.items.add(juce::FlexItem(cpuGauge).withHeight(100).withMargin(knobMarginSmall));
                box.performLayout(areas[0].toFloat());
            }
            {
                juce::FlexBox box;
                box.flexWrap = juce::FlexBox::Wrap::noWrap;
                box.flexDirection = juce::FlexBox::Direction::column;
                box.justifyContent = juce::FlexBox::JustifyContent::spaceAround;
                box.items.add(juce::FlexItem(groovePlaySwitch)
                                  .withFlex(0)
                                  .withHeight(Constants::Text::labelHeight)
                                  .withAlignSelf(juce::FlexItem::AlignSelf::stretch)
                                  .withMargin(knobMarginSmall));
                box.items.add(juce::FlexItem(bpmDial).withFlex(1).withMargin(knobMarginSmall));
                box.items.add(juce::FlexItem(grooveVariationDial).withFlex(1).withMargin(knobMarginSmall));
                box.items.add(juce::FlexItem(grooveLevelDial).withFlex(1).withMargin(knobMarginSmall));
                box.performLayout(areas[1].toFloat());
            }
            {
                juce::FlexBox box;
                box.flexWrap = juce::FlexBox::Wrap::noWrap;
                box.flexDirection = juce::FlexBox::Direction::column;
                box.justifyContent = juce::FlexBox::JustifyContent::spaceAround;
                box.items.add(juce::FlexItem(recordASwitch)
                                  .withFlex(0)
                                  .withHeight(Constants::Text::labelHeight)
                                  .withAlignSelf(juce::FlexItem::AlignSelf::stretch)
                                  .withMargin(knobMarginSmall));
                box.items.add(juce::FlexItem(playASwitch)
                                  .withFlex(0)
                                  .withHeight(Constants::Text::labelHeight)
                                  .withAlignSelf(juce::FlexItem::AlignSelf::stretch)
                                  .withMargin(knobMarginSmall));
                box.items.add(juce::FlexItem(clearASwitch)
                                  .withFlex(0)
                                  .withHeight(Constants::Text::labelHeight)
                                  .withAlignSelf(juce::FlexItem::AlignSelf::stretch)
                                  .withMargin(knobMarginSmall));
                box.items.add(juce::FlexItem(wowDepthADial).withFlex(1).withMargin(knobMarginSmall));
                box.items.add(juce::FlexItem(wowRateADial).withFlex(1).withMargin(knobMarginSmall));
                box.items.add(juce::FlexItem(wowDriftADial).withFlex(1).withMargin(knobMarginSmall));
                box.items.add(juce::FlexItem(flutterDepthADial).withFlex(1).withMargin(knobMarginSmall));
                box.items.add(juce::FlexItem(flutterRateADial).withFlex(1).withMargin(knobMarginSmall));
                box.performLayout(areas[2].toFloat());
            }
            {
                juce::FlexBox box;
                box.flexWrap = juce::FlexBox::Wrap::noWrap;
                box.flexDirection = juce::FlexBox::Direction::column;
                box.justifyContent = juce::FlexBox::JustifyContent::spaceAround;
                box.items.add(juce::FlexItem(recordBSwitch)
                                  .withFlex(0)
                                  .withHeight(Constants::Text::labelHeight)
                                  .withAlignSelf(juce::FlexItem::AlignSelf::stretch)
                                  .withMargin(knobMarginSmall));
                box.items.add(juce::FlexItem(playBSwitch)
                                  .withFlex(0)
                                  .withHeight(Constants::Text::labelHeight)
                                  .withAlignSelf(juce::FlexItem::AlignSelf::stretch)
                                  .withMargin(knobMarginSmall));
                box.items.add(juce::FlexItem(clearBSwitch)
                                  .withFlex(0)
                                  .withHeight(Constants::Text::labelHeight)
                                  .withAlignSelf(juce::FlexItem::AlignSelf::stretch)
                                  .withMargin(knobMarginSmall));
                box.items.add(juce::FlexItem(wowDepthBDial).withFlex(1).withMargin(knobMarginSmall));
                box.items.add(juce::FlexItem(wowRateBDial).withFlex(1).withMargin(knobMarginSmall));
                box.items.add(juce::FlexItem(wowDriftBDial).withFlex(1).withMargin(knobMarginSmall));
                box.items.add(juce::FlexItem(flutterDepthBDial).withFlex(1).withMargin(knobMarginSmall));
                box.items.add(juce::FlexItem(flutterRateBDial).withFlex(1).withMargin(knobMarginSmall));
                box.performLayout(areas[3].toFloat());
            }
            {
                juce::FlexBox box;
                box.flexWrap = juce::FlexBox::Wrap::noWrap;
                box.flexDirection = juce::FlexBox::Direction::column;
                box.justifyContent = juce::FlexBox::JustifyContent::spaceAround;
                box.items.add(juce::FlexItem(recordCSwitch)
                                  .withFlex(0)
                                  .withHeight(Constants::Text::labelHeight)
                                  .withAlignSelf(juce::FlexItem::AlignSelf::stretch)
                                  .withMargin(knobMarginSmall));
                box.items.add(juce::FlexItem(playCSwitch)
                                  .withFlex(0)
                                  .withHeight(Constants::Text::labelHeight)
                                  .withAlignSelf(juce::FlexItem::AlignSelf::stretch)
                                  .withMargin(knobMarginSmall));
                box.items.add(juce::FlexItem(clearCSwitch)
                                  .withFlex(0)
                                  .withHeight(Constants::Text::labelHeight)
                                  .withAlignSelf(juce::FlexItem::AlignSelf::stretch)
                                  .withMargin(knobMarginSmall));
                box.items.add(juce::FlexItem(wowDepthCDial).withFlex(1).withMargin(knobMarginSmall));
                box.items.add(juce::FlexItem(wowRateCDial).withFlex(1).withMargin(knobMarginSmall));
                box.items.add(juce::FlexItem(wowDriftCDial).withFlex(1).withMargin(knobMarginSmall));
                box.items.add(juce::FlexItem(flutterDepthCDial).withFlex(1).withMargin(knobMarginSmall));
                box.items.add(juce::FlexItem(flutterRateCDial).withFlex(1).withMargin(knobMarginSmall));
                box.performLayout(areas[4].toFloat());
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
            clearASwitch.tickFlash();
            clearBSwitch.tickFlash();
            clearCSwitch.tickFlash();
            groovePlaySwitch.setButtonText(processorRef.isGroovePlaying() ? juce::String::fromUTF8("Stop")
                                                                          : juce::String::fromUTF8("Play"));
            bpmDial.setEnabled(processorRef.canEditBpm());

            if (const auto info = processorRef.consumeGrooveInfoText(); info.isNotEmpty())
            {
                m_statusBar.showMessage(info);
            }
        }
    }

    void initWidgets()
    {
        addAndMakeVisible(tapeSpeedDial);
        tapeSpeedDial.reset(valueTreeState, "tapeSpeed");
        tapeSpeedDial.setLabelText(juce::String::fromUTF8("Tape Speed"));
        tapeSpeedDial.setTooltip(juce::String::fromUTF8("Tape Speed (0.25 to 4.0 x)"));
        addAndMakeVisible(barsDial);
        barsDial.reset(valueTreeState, "bars");
        barsDial.setLabelText(juce::String::fromUTF8("Bars"));
        barsDial.setTooltip(juce::String::fromUTF8("Bars (1 to 32)"));
        addAndMakeVisible(inputGainDial);
        inputGainDial.reset(valueTreeState, "inputGain");
        inputGainDial.setLabelText(juce::String::fromUTF8("Input Level"));
        inputGainDial.setTooltip(juce::String::fromUTF8("Input Level (-60 to 12 dB)"));
        addAndMakeVisible(grooveLevelDial);
        grooveLevelDial.reset(valueTreeState, "grooveLevel");
        grooveLevelDial.setLabelText(juce::String::fromUTF8("Groove Level"));
        grooveLevelDial.setTooltip(juce::String::fromUTF8("Groove Level (-60 to 12 dB)"));
        addAndMakeVisible(recordASwitch);
        recordASwitchAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
            valueTreeState, "recordA", recordASwitch);
        recordASwitch.setTooltip(juce::String::fromUTF8("Rec A"));

        addAndMakeVisible(playASwitch);
        playASwitchAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
            valueTreeState, "playA", playASwitch);
        playASwitch.setTooltip(juce::String::fromUTF8("Play A"));

        addAndMakeVisible(clearASwitch);
        clearASwitchAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
            valueTreeState, "clearA", clearASwitch);
        clearASwitch.setTooltip(juce::String::fromUTF8("Clear A"));

        addAndMakeVisible(recordBSwitch);
        recordBSwitchAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
            valueTreeState, "recordB", recordBSwitch);
        recordBSwitch.setTooltip(juce::String::fromUTF8("Rec B"));

        addAndMakeVisible(playBSwitch);
        playBSwitchAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
            valueTreeState, "playB", playBSwitch);
        playBSwitch.setTooltip(juce::String::fromUTF8("Play B"));

        addAndMakeVisible(clearBSwitch);
        clearBSwitchAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
            valueTreeState, "clearB", clearBSwitch);
        clearBSwitch.setTooltip(juce::String::fromUTF8("Clear B"));

        addAndMakeVisible(recordCSwitch);
        recordCSwitchAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
            valueTreeState, "recordC", recordCSwitch);
        recordCSwitch.setTooltip(juce::String::fromUTF8("Rec C"));

        addAndMakeVisible(playCSwitch);
        playCSwitchAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
            valueTreeState, "playC", playCSwitch);
        playCSwitch.setTooltip(juce::String::fromUTF8("Play C"));

        addAndMakeVisible(clearCSwitch);
        clearCSwitchAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
            valueTreeState, "clearC", clearCSwitch);
        clearCSwitch.setTooltip(juce::String::fromUTF8("Clear C"));

        addAndMakeVisible(groovePlaySwitch);
        groovePlaySwitchAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
            valueTreeState, "groovePlay", groovePlaySwitch);
        groovePlaySwitch.setTooltip(juce::String::fromUTF8("Groove"));

        addAndMakeVisible(bpmDial);
        bpmDial.reset(valueTreeState, "bpm");
        bpmDial.setLabelText(juce::String::fromUTF8("BPM"));
        bpmDial.setTooltip(juce::String::fromUTF8("BPM (50 to 250 BPM)"));
        addAndMakeVisible(grooveVariationDial);
        grooveVariationDial.reset(valueTreeState, "grooveVariation");
        grooveVariationDial.setLabelText(juce::String::fromUTF8("Groove Var"));
        grooveVariationDial.setTooltip(juce::String::fromUTF8("Groove Var (0 to 31)"));
        addAndMakeVisible(wowDepthADial);
        wowDepthADial.reset(valueTreeState, "wowDepthA");
        wowDepthADial.setLabelText(juce::String::fromUTF8("Wow Depth A"));
        wowDepthADial.setTooltip(juce::String::fromUTF8("Wow Depth A (0 to 1)"));
        addAndMakeVisible(wowRateADial);
        wowRateADial.reset(valueTreeState, "wowRateA");
        wowRateADial.setLabelText(juce::String::fromUTF8("Wow Rate A"));
        wowRateADial.setTooltip(juce::String::fromUTF8("Wow Rate A (0 to 3 Hz)"));
        addAndMakeVisible(wowDriftADial);
        wowDriftADial.reset(valueTreeState, "wowDriftA");
        wowDriftADial.setLabelText(juce::String::fromUTF8("Wow Drift A"));
        wowDriftADial.setTooltip(juce::String::fromUTF8("Wow Drift A (0 to 1)"));
        addAndMakeVisible(wowDepthBDial);
        wowDepthBDial.reset(valueTreeState, "wowDepthB");
        wowDepthBDial.setLabelText(juce::String::fromUTF8("Wow Depth B"));
        wowDepthBDial.setTooltip(juce::String::fromUTF8("Wow Depth B (0 to 1)"));
        addAndMakeVisible(wowRateBDial);
        wowRateBDial.reset(valueTreeState, "wowRateB");
        wowRateBDial.setLabelText(juce::String::fromUTF8("Wow Rate B"));
        wowRateBDial.setTooltip(juce::String::fromUTF8("Wow Rate B (0 to 3 Hz)"));
        addAndMakeVisible(wowDriftBDial);
        wowDriftBDial.reset(valueTreeState, "wowDriftB");
        wowDriftBDial.setLabelText(juce::String::fromUTF8("Wow Drift B"));
        wowDriftBDial.setTooltip(juce::String::fromUTF8("Wow Drift B (0 to 1)"));
        addAndMakeVisible(wowDepthCDial);
        wowDepthCDial.reset(valueTreeState, "wowDepthC");
        wowDepthCDial.setLabelText(juce::String::fromUTF8("Wow Depth C"));
        wowDepthCDial.setTooltip(juce::String::fromUTF8("Wow Depth C (0 to 1)"));
        addAndMakeVisible(wowRateCDial);
        wowRateCDial.reset(valueTreeState, "wowRateC");
        wowRateCDial.setLabelText(juce::String::fromUTF8("Wow Rate C"));
        wowRateCDial.setTooltip(juce::String::fromUTF8("Wow Rate C (0 to 3 Hz)"));
        addAndMakeVisible(wowDriftCDial);
        wowDriftCDial.reset(valueTreeState, "wowDriftC");
        wowDriftCDial.setLabelText(juce::String::fromUTF8("Wow Drift C"));
        wowDriftCDial.setTooltip(juce::String::fromUTF8("Wow Drift C (0 to 1)"));
        addAndMakeVisible(flutterDepthADial);
        flutterDepthADial.reset(valueTreeState, "flutterDepthA");
        flutterDepthADial.setLabelText(juce::String::fromUTF8("Flutter Depth A"));
        flutterDepthADial.setTooltip(juce::String::fromUTF8("Flutter Depth A (0 to 1)"));
        addAndMakeVisible(flutterRateADial);
        flutterRateADial.reset(valueTreeState, "flutterRateA");
        flutterRateADial.setLabelText(juce::String::fromUTF8("Flutter Rate A"));
        flutterRateADial.setTooltip(juce::String::fromUTF8("Flutter Rate A (0 to 10 Hz)"));
        addAndMakeVisible(flutterDepthBDial);
        flutterDepthBDial.reset(valueTreeState, "flutterDepthB");
        flutterDepthBDial.setLabelText(juce::String::fromUTF8("Flutter Depth B"));
        flutterDepthBDial.setTooltip(juce::String::fromUTF8("Flutter Depth B (0 to 1)"));
        addAndMakeVisible(flutterRateBDial);
        flutterRateBDial.reset(valueTreeState, "flutterRateB");
        flutterRateBDial.setLabelText(juce::String::fromUTF8("Flutter Rate B"));
        flutterRateBDial.setTooltip(juce::String::fromUTF8("Flutter Rate B (0 to 10 Hz)"));
        addAndMakeVisible(flutterDepthCDial);
        flutterDepthCDial.reset(valueTreeState, "flutterDepthC");
        flutterDepthCDial.setLabelText(juce::String::fromUTF8("Flutter Depth C"));
        flutterDepthCDial.setTooltip(juce::String::fromUTF8("Flutter Depth C (0 to 1)"));
        addAndMakeVisible(flutterRateCDial);
        flutterRateCDial.reset(valueTreeState, "flutterRateC");
        flutterRateCDial.setLabelText(juce::String::fromUTF8("Flutter Rate C"));
        flutterRateCDial.setTooltip(juce::String::fromUTF8("Flutter Rate C (0 to 10 Hz)"));
        addAndMakeVisible(cpuGauge);
        cpuGauge.setLabelText(juce::String::fromUTF8("CPU"));
        cpuGauge.setTooltip(juce::String::fromUTF8("CPU"));
        addAndMakeVisible(levelGauge);
        levelGauge.setLabelText(juce::String::fromUTF8("Level"));
        levelGauge.setTooltip(juce::String::fromUTF8("Level (0 to 100 %)"));
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
        names.add("Groove");

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
        if (menuName == "Groove")
        {
            return buildGrooveMenu();
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
            "Varispeed 3-track tape recorder (A, B, C) plus a parallel MIDI-groove track.  Each tape track records and "
            "plays back independently, at a shared tape speed.\n\nPart of the AbacDsp project - core DSP library is "
            "MIT licensed.\n\nBuilt with JUCE, licensed under AGPLv3 (or a commercial JUCE licence).\n\nFull "
            "third-party license details: THIRD-PARTY-LICENSES.md in the AbacDsp repository.";

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
        handleGrooveMenuSelection(menuItemID);
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


    juce::PopupMenu buildGrooveMenu()
    {
        const auto names = processorRef.listGrooveNames();
        juce::String currentStyle = processorRef.getCurrentGrooveName();
        const int lastSlash = currentStyle.lastIndexOfChar('/');
        const int vPos = currentStyle.lastIndexOf("_v");
        if (vPos > lastSlash)
        {
            currentStyle = currentStyle.substring(0, vPos);
        }
        return buildGroupedMenu(names, 20000, currentStyle);
    }

    void handleGrooveMenuSelection(int menuItemID)
    {
        const auto names = processorRef.listGrooveNames();
        if (menuItemID >= 20000 && menuItemID < 20000 + static_cast<int>(names.size()))
        {
            const auto& style = names[static_cast<size_t>(menuItemID - 20000)];
            processorRef.requestLoadGroove(style, 0);
            m_statusBar.showMessage("Loading groove '" + style + "'...");
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


    CustomRotaryDial tapeSpeedDial{this};
    CustomRotaryDial barsDial{this};
    CustomRotaryDial inputGainDial{this};
    CustomRotaryDial grooveLevelDial{this};
    juce::ToggleButton recordASwitch{juce::String::fromUTF8("Rec A")};
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> recordASwitchAttachment;
    juce::ToggleButton playASwitch{juce::String::fromUTF8("Play A")};
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> playASwitchAttachment;
    MomentaryToggleButton clearASwitch{juce::String::fromUTF8("Clear A")};
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> clearASwitchAttachment;
    juce::ToggleButton recordBSwitch{juce::String::fromUTF8("Rec B")};
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> recordBSwitchAttachment;
    juce::ToggleButton playBSwitch{juce::String::fromUTF8("Play B")};
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> playBSwitchAttachment;
    MomentaryToggleButton clearBSwitch{juce::String::fromUTF8("Clear B")};
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> clearBSwitchAttachment;
    juce::ToggleButton recordCSwitch{juce::String::fromUTF8("Rec C")};
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> recordCSwitchAttachment;
    juce::ToggleButton playCSwitch{juce::String::fromUTF8("Play C")};
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> playCSwitchAttachment;
    MomentaryToggleButton clearCSwitch{juce::String::fromUTF8("Clear C")};
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> clearCSwitchAttachment;
    juce::ToggleButton groovePlaySwitch{juce::String::fromUTF8("Groove")};
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> groovePlaySwitchAttachment;
    CustomRotaryDial bpmDial{this};
    CustomRotaryDial grooveVariationDial{this};
    CustomRotaryDial wowDepthADial{this};
    CustomRotaryDial wowRateADial{this};
    CustomRotaryDial wowDriftADial{this};
    CustomRotaryDial wowDepthBDial{this};
    CustomRotaryDial wowRateBDial{this};
    CustomRotaryDial wowDriftBDial{this};
    CustomRotaryDial wowDepthCDial{this};
    CustomRotaryDial wowRateCDial{this};
    CustomRotaryDial wowDriftCDial{this};
    CustomRotaryDial flutterDepthADial{this};
    CustomRotaryDial flutterRateADial{this};
    CustomRotaryDial flutterDepthBDial{this};
    CustomRotaryDial flutterRateBDial{this};
    CustomRotaryDial flutterDepthCDial{this};
    CustomRotaryDial flutterRateCDial{this};
    CpuGauge cpuGauge{};
    Gauge levelGauge{};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioPluginAudioProcessorEditor)
};
