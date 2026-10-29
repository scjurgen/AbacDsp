#pragma once
/*
 * AUTO GENERATED,
 * NOT A GOOD IDEA TO CHANGE STUFF HERE
 * Keep the file readonly
 */

#include <map>

#include "ListenandplayProcessor.h"
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
        const int pageButtonWidth = pageSwitchArea.getWidth() / static_cast<int>(m_pageButtons.size());
        for (size_t i = 0; i < m_pageButtons.size(); ++i)
        {
            const bool isLast = i + 1 == m_pageButtons.size();
            m_pageButtons[i].setBounds(isLast ? pageSwitchArea : pageSwitchArea.removeFromLeft(pageButtonWidth));
        }
        area = area.reduced(static_cast<int>(Constants::Margins::big));
        {
            switch (m_currentPage)
            {
                case 0:
                {
                    // auto generated
                    // const juce::FlexItem::Margin knobMargin = juce::FlexItem::Margin(Constants::Margins::small);
                    const juce::FlexItem::Margin knobMarginSmall = juce::FlexItem::Margin(Constants::Margins::medium);
                    std::vector<juce::Rectangle<int>> areas(2);
                    const auto rowHeight = area.getHeight() / 10;
                    areas[0] = area.removeFromTop(rowHeight * 1).reduced(Constants::Margins::small);
                    areas[1] = area.reduced(Constants::Margins::small);

                    {
                        juce::FlexBox box;
                        box.flexWrap = juce::FlexBox::Wrap::noWrap;
                        box.flexDirection = juce::FlexBox::Direction::row;
                        box.justifyContent = juce::FlexBox::JustifyContent::spaceAround;
                        box.items.add(juce::FlexItem(instrumentDrop)
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
                        box.items.add(juce::FlexItem(lessonsGauge).withFlex(1).withMargin(knobMarginSmall));
                        box.performLayout(areas[1].toFloat());
                    }
                    break;
                }
                case 1:
                {
                    // auto generated
                    // const juce::FlexItem::Margin knobMargin = juce::FlexItem::Margin(Constants::Margins::small);
                    const juce::FlexItem::Margin knobMarginSmall = juce::FlexItem::Margin(Constants::Margins::medium);
                    std::vector<juce::Rectangle<int>> areas(2);
                    const auto colWidth = area.getWidth() / 5;
                    areas[0] = area.removeFromLeft(colWidth * 1).reduced(Constants::Margins::small);
                    areas[1] = area.reduced(Constants::Margins::small);

                    {
                        juce::FlexBox box;
                        box.flexWrap = juce::FlexBox::Wrap::noWrap;
                        box.flexDirection = juce::FlexBox::Direction::column;
                        box.justifyContent = juce::FlexBox::JustifyContent::spaceAround;
                        box.items.add(juce::FlexItem(instrumentDrop)
                                          .withFlex(0)
                                          .withHeight(Constants::Text::labelHeight)
                                          .withAlignSelf(juce::FlexItem::AlignSelf::stretch)
                                          .withMargin(knobMarginSmall));
                        box.items.add(juce::FlexItem(bpmDial).withFlex(1).withMargin(knobMarginSmall));
                        box.items.add(juce::FlexItem(clickOnSwitch)
                                          .withFlex(0)
                                          .withHeight(Constants::Text::labelHeight)
                                          .withAlignSelf(juce::FlexItem::AlignSelf::stretch)
                                          .withMargin(knobMarginSmall));
                        box.items.add(juce::FlexItem(instrumentLevelDial).withFlex(1).withMargin(knobMarginSmall));
                        box.items.add(juce::FlexItem(clickLevelDial).withFlex(1).withMargin(knobMarginSmall));
                        box.items.add(juce::FlexItem(saveTakesSwitch)
                                          .withFlex(0)
                                          .withHeight(Constants::Text::labelHeight)
                                          .withAlignSelf(juce::FlexItem::AlignSelf::stretch)
                                          .withMargin(knobMarginSmall));
                        box.items.add(juce::FlexItem(playSwitch)
                                          .withFlex(0)
                                          .withHeight(Constants::Text::labelHeight)
                                          .withAlignSelf(juce::FlexItem::AlignSelf::stretch)
                                          .withMargin(knobMarginSmall));
                        box.items.add(juce::FlexItem(referenceSwitch)
                                          .withFlex(0)
                                          .withHeight(Constants::Text::labelHeight)
                                          .withAlignSelf(juce::FlexItem::AlignSelf::stretch)
                                          .withMargin(knobMarginSmall));
                        box.items.add(juce::FlexItem(stopSwitch)
                                          .withFlex(0)
                                          .withHeight(Constants::Text::labelHeight)
                                          .withAlignSelf(juce::FlexItem::AlignSelf::stretch)
                                          .withMargin(knobMarginSmall));
                        box.items.add(juce::FlexItem(nextSwitch)
                                          .withFlex(0)
                                          .withHeight(Constants::Text::labelHeight)
                                          .withAlignSelf(juce::FlexItem::AlignSelf::stretch)
                                          .withMargin(knobMarginSmall));
                        box.performLayout(areas[0].toFloat());
                    }
                    {
                        juce::FlexBox box;
                        box.flexWrap = juce::FlexBox::Wrap::noWrap;
                        box.flexDirection = juce::FlexBox::Direction::column;
                        box.justifyContent = juce::FlexBox::JustifyContent::spaceAround;
                        box.items.add(juce::FlexItem(rollGauge).withFlex(1).withMargin(knobMarginSmall));
                        box.performLayout(areas[1].toFloat());
                    }
                    break;
                }
                case 2:
                {
                    // auto generated
                    // const juce::FlexItem::Margin knobMargin = juce::FlexItem::Margin(Constants::Margins::small);
                    const juce::FlexItem::Margin knobMarginSmall = juce::FlexItem::Margin(Constants::Margins::medium);
                    std::vector<juce::Rectangle<int>> areas(2);
                    const auto colWidth = area.getWidth() / 5;
                    areas[0] = area.removeFromLeft(colWidth * 1).reduced(Constants::Margins::small);
                    areas[1] = area.reduced(Constants::Margins::small);

                    {
                        juce::FlexBox box;
                        box.flexWrap = juce::FlexBox::Wrap::noWrap;
                        box.flexDirection = juce::FlexBox::Direction::column;
                        box.justifyContent = juce::FlexBox::JustifyContent::spaceAround;
                        box.items.add(juce::FlexItem(playSwitch)
                                          .withFlex(0)
                                          .withHeight(Constants::Text::labelHeight)
                                          .withAlignSelf(juce::FlexItem::AlignSelf::stretch)
                                          .withMargin(knobMarginSmall));
                        box.items.add(juce::FlexItem(listenSwitch)
                                          .withFlex(0)
                                          .withHeight(Constants::Text::labelHeight)
                                          .withAlignSelf(juce::FlexItem::AlignSelf::stretch)
                                          .withMargin(knobMarginSmall));
                        box.items.add(juce::FlexItem(replaySwitch)
                                          .withFlex(0)
                                          .withHeight(Constants::Text::labelHeight)
                                          .withAlignSelf(juce::FlexItem::AlignSelf::stretch)
                                          .withMargin(knobMarginSmall));
                        box.items.add(juce::FlexItem(referenceSwitch)
                                          .withFlex(0)
                                          .withHeight(Constants::Text::labelHeight)
                                          .withAlignSelf(juce::FlexItem::AlignSelf::stretch)
                                          .withMargin(knobMarginSmall));
                        box.items.add(juce::FlexItem(nextSwitch)
                                          .withFlex(0)
                                          .withHeight(Constants::Text::labelHeight)
                                          .withAlignSelf(juce::FlexItem::AlignSelf::stretch)
                                          .withMargin(knobMarginSmall));
                        box.items.add(juce::FlexItem(stopSwitch)
                                          .withFlex(0)
                                          .withHeight(Constants::Text::labelHeight)
                                          .withAlignSelf(juce::FlexItem::AlignSelf::stretch)
                                          .withMargin(knobMarginSmall));
                        box.items.add(juce::FlexItem(saveTakesSwitch)
                                          .withFlex(0)
                                          .withHeight(Constants::Text::labelHeight)
                                          .withAlignSelf(juce::FlexItem::AlignSelf::stretch)
                                          .withMargin(knobMarginSmall));
                        box.performLayout(areas[0].toFloat());
                    }
                    {
                        juce::FlexBox box;
                        box.flexWrap = juce::FlexBox::Wrap::noWrap;
                        box.flexDirection = juce::FlexBox::Direction::column;
                        box.justifyContent = juce::FlexBox::JustifyContent::spaceAround;
                        box.items.add(juce::FlexItem(scoreGauge).withHeight(230).withMargin(knobMarginSmall));
                        box.items.add(juce::FlexItem(rollGauge).withFlex(1).withMargin(knobMarginSmall));
                        box.performLayout(areas[1].toFloat());
                    }
                    break;
                }
                case 3:
                {
                    // auto generated
                    // const juce::FlexItem::Margin knobMargin = juce::FlexItem::Margin(Constants::Margins::small);
                    const juce::FlexItem::Margin knobMarginSmall = juce::FlexItem::Margin(Constants::Margins::medium);
                    std::vector<juce::Rectangle<int>> areas(2);
                    const auto colWidth = area.getWidth() / 2;
                    areas[0] = area.removeFromLeft(colWidth * 1).reduced(Constants::Margins::small);
                    areas[1] = area.reduced(Constants::Margins::small);

                    {
                        juce::FlexBox box;
                        box.flexWrap = juce::FlexBox::Wrap::noWrap;
                        box.flexDirection = juce::FlexBox::Direction::column;
                        box.justifyContent = juce::FlexBox::JustifyContent::spaceAround;
                        box.items.add(juce::FlexItem(inputDrop)
                                          .withFlex(0)
                                          .withHeight(Constants::Text::labelHeight)
                                          .withAlignSelf(juce::FlexItem::AlignSelf::stretch)
                                          .withMargin(knobMarginSmall));
                        box.items.add(juce::FlexItem(clickOnSwitch)
                                          .withFlex(0)
                                          .withHeight(Constants::Text::labelHeight)
                                          .withAlignSelf(juce::FlexItem::AlignSelf::stretch)
                                          .withMargin(knobMarginSmall));
                        box.items.add(juce::FlexItem(inputGainDial).withFlex(1).withMargin(knobMarginSmall));
                        box.performLayout(areas[0].toFloat());
                    }
                    {
                        juce::FlexBox box;
                        box.flexWrap = juce::FlexBox::Wrap::noWrap;
                        box.flexDirection = juce::FlexBox::Direction::column;
                        box.justifyContent = juce::FlexBox::JustifyContent::spaceAround;
                        box.items.add(juce::FlexItem(latencyCompDial).withFlex(1).withMargin(knobMarginSmall));
                        box.items.add(juce::FlexItem(outputVolumeDial).withFlex(1).withMargin(knobMarginSmall));
                        box.items.add(juce::FlexItem(instrumentLevelDial).withFlex(1).withMargin(knobMarginSmall));
                        box.items.add(juce::FlexItem(clickLevelDial).withFlex(1).withMargin(knobMarginSmall));
                        box.performLayout(areas[1].toFloat());
                    }
                    break;
                }
                default:
                    break;
            }
        }
    }
#pragma GCC diagnostic pop

    void timerCallback() override
    {
        if (processorRef.hasRunner())
        {
            rollGauge.setSnapshot(processorRef.getPianoRollSnapshot());
            scoreGauge.setScore(processorRef.getScoreSnapshot());
            listenSwitch.tickFlash();
            playSwitch.tickFlash();
            replaySwitch.tickFlash();
            stopSwitch.tickFlash();
            nextSwitch.tickFlash();
            referenceSwitch.tickFlash();

            processorRef.pollLesson();
            processorRef.pollResult();
            if (processorRef.consumeResultArrived())
            {
                switchPage(2);
            }
            lessonsGauge.setRows(processorRef.lessonRowsVersion(), [this] { return processorRef.lessonRows(); });
            if (!lessonsGauge.onLoad)
            {
                lessonsGauge.onLoad = [this](const juce::String& stem)
                {
                    if (processorRef.loadLesson(stem))
                    {
                        switchPage(1);
                    }
                };
            }
            if (const auto status = processorRef.pollStatusText())
            {
                m_statusBar.showMessage(*status, status->isNotEmpty());
            }
            pollScriptError();
        }
    }

    void initWidgets()
    {
        addAndMakeVisible(bpmDial);
        bpmDial.reset(valueTreeState, "bpm");
        bpmDial.setLabelText(juce::String::fromUTF8("BPM"));
        bpmDial.setTooltip(juce::String::fromUTF8("BPM (40 to 200 BPM)"));
        addAndMakeVisible(clickOnSwitch);
        clickOnSwitchAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
            valueTreeState, "clickOn", clickOnSwitch);
        clickOnSwitch.setTooltip(juce::String::fromUTF8("Click"));

        addAndMakeVisible(latencyCompDial);
        latencyCompDial.reset(valueTreeState, "latencyComp");
        latencyCompDial.setLabelText(juce::String::fromUTF8("Latency Comp"));
        latencyCompDial.setTooltip(juce::String::fromUTF8("Latency Comp (-50 to 150 ms)"));
        addAndMakeVisible(instrumentDrop);
        instrumentDrop.addItemList(valueTreeState.getParameter("instrument")->getAllValueStrings(), 1);
        instrumentDropAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
            valueTreeState, "instrument", instrumentDrop);
        instrumentDrop.setTooltip(juce::String::fromUTF8("Instrument (Guitar, Bass, Harp, Nylon, Ukulele, Music box)"));
        addAndMakeVisible(inputDrop);
        inputDrop.addItemList(valueTreeState.getParameter("input")->getAllValueStrings(), 1);
        inputDropAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
            valueTreeState, "input", inputDrop);
        inputDrop.setTooltip(juce::String::fromUTF8("Input (Audio, MIDI)"));
        addAndMakeVisible(outputVolumeDial);
        outputVolumeDial.reset(valueTreeState, "outputVolume");
        outputVolumeDial.setLabelText(juce::String::fromUTF8("Volume"));
        outputVolumeDial.setTooltip(juce::String::fromUTF8("Volume (-60 to 12 dB)"));
        addAndMakeVisible(inputGainDial);
        inputGainDial.reset(valueTreeState, "inputGain");
        inputGainDial.setLabelText(juce::String::fromUTF8("Input Gain"));
        inputGainDial.setTooltip(juce::String::fromUTF8("Input Gain (-24 to 24 dB)"));
        addAndMakeVisible(instrumentLevelDial);
        instrumentLevelDial.reset(valueTreeState, "instrumentLevel");
        instrumentLevelDial.setLabelText(juce::String::fromUTF8("Instr. Level"));
        instrumentLevelDial.setTooltip(juce::String::fromUTF8("Instr. Level (-24 to 12 dB)"));
        addAndMakeVisible(clickLevelDial);
        clickLevelDial.reset(valueTreeState, "clickLevel");
        clickLevelDial.setLabelText(juce::String::fromUTF8("Click Level"));
        clickLevelDial.setTooltip(juce::String::fromUTF8("Click Level (-24 to 12 dB)"));
        addAndMakeVisible(listenSwitch);
        listenSwitchAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
            valueTreeState, "listen", listenSwitch);
        listenSwitch.setTooltip(juce::String::fromUTF8("Listen"));

        addAndMakeVisible(playSwitch);
        playSwitchAttachment =
            std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(valueTreeState, "play", playSwitch);
        playSwitch.setTooltip(juce::String::fromUTF8("Play"));

        addAndMakeVisible(replaySwitch);
        replaySwitchAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
            valueTreeState, "replay", replaySwitch);
        replaySwitch.setTooltip(juce::String::fromUTF8("Replay Take"));

        addAndMakeVisible(stopSwitch);
        stopSwitchAttachment =
            std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(valueTreeState, "stop", stopSwitch);
        stopSwitch.setTooltip(juce::String::fromUTF8("Stop"));

        addAndMakeVisible(nextSwitch);
        nextSwitchAttachment =
            std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(valueTreeState, "next", nextSwitch);
        nextSwitch.setTooltip(juce::String::fromUTF8("Next Phrase"));

        addAndMakeVisible(referenceSwitch);
        referenceSwitchAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
            valueTreeState, "reference", referenceSwitch);
        referenceSwitch.setTooltip(juce::String::fromUTF8("Reference"));

        addAndMakeVisible(saveTakesSwitch);
        saveTakesSwitchAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
            valueTreeState, "saveTakes", saveTakesSwitch);
        saveTakesSwitch.setTooltip(juce::String::fromUTF8("Save Takes"));

        addAndMakeVisible(rollGauge);
        rollGauge.setLabelText(juce::String::fromUTF8("Phrase"));
        addAndMakeVisible(scoreGauge);
        scoreGauge.setLabelText(juce::String::fromUTF8("Result"));
        addAndMakeVisible(lessonsGauge);
        lessonsGauge.setLabelText(juce::String::fromUTF8("Lessons"));

        for (size_t i = 0; i < m_pageButtons.size(); ++i)
        {
            m_pageButtons[i].setButtonText(kPageNames[i]);
            m_pageButtons[i].onClick = [this, i] { switchPage(i); };
            addAndMakeVisible(m_pageButtons[i]);
        }
        switchPage(0);
    }

    void switchPage(const size_t page)
    {
        m_currentPage = page;
        for (size_t i = 0; i < m_pageButtons.size(); ++i)
        {
            m_pageButtons[i].setToggleState(i == page, juce::dontSendNotification);
        }
        bpmDial.setVisible(page == 1);
        clickOnSwitch.setVisible(page == 1 || page == 3);
        latencyCompDial.setVisible(page == 3);
        instrumentDrop.setVisible(page == 0 || page == 1);
        inputDrop.setVisible(page == 3);
        outputVolumeDial.setVisible(page == 3);
        inputGainDial.setVisible(page == 3);
        instrumentLevelDial.setVisible(page == 1 || page == 3);
        clickLevelDial.setVisible(page == 1 || page == 3);
        listenSwitch.setVisible(page == 2);
        playSwitch.setVisible(page == 1 || page == 2);
        replaySwitch.setVisible(page == 2);
        stopSwitch.setVisible(page == 1 || page == 2);
        nextSwitch.setVisible(page == 1 || page == 2);
        referenceSwitch.setVisible(page == 1 || page == 2);
        saveTakesSwitch.setVisible(page == 1 || page == 2);
        rollGauge.setVisible(page == 1 || page == 2);
        scoreGauge.setVisible(page == 2);
        lessonsGauge.setVisible(page == 0);

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
        names.add("Lessons");

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
        if (menuName == "Lessons")
        {
            return buildLessonsMenu();
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
            "Listen and play: repeat a played phrase on your instrument, scored on notes, rhythm and dynamics\n\nPart "
            "of the AbacDsp project - core DSP library is MIT licensed.\n\nBuilt with JUCE, licensed under AGPLv3 (or "
            "a commercial JUCE licence).\n\nScripting powered by Lua and sol2 (both MIT licensed).\n\nFull third-party "
            "license details: THIRD-PARTY-LICENSES.md in the AbacDsp repository.";

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
        handleLessonsMenuSelection(menuItemID);
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
#ifdef LISTENANDPLAY_SCRIPTING_DOCS_FILE
        editorComponent->onOpenDocs = []
        { juce::URL(juce::File(LISTENANDPLAY_SCRIPTING_DOCS_FILE)).launchInDefaultBrowser(); };
        editorComponent->setDocsAvailable(true);
#endif

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
        menu.addItem(kAuthoringModeToggleId, active ? "Enabled" : "Enable", true, active);
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


    juce::PopupMenu buildLessonsMenu()
    {
        juce::PopupMenu menu;
        menu.addItem(20000 + 1, "Open next suggested lesson");
        menu.addItem(20000 + 2, "Reload lessons");
        menu.addSeparator();
        menu.addItem(20000 + 3, "Open last take report in browser");
        return menu;
    }

    void handleLessonsMenuSelection(const int menuItemID)
    {
        if (menuItemID == 20000 + 1)
        {
            if (processorRef.loadRecommendedLesson())
            {
                switchPage(1);
            }
        }
        else if (menuItemID == 20000 + 2)
        {
            processorRef.reloadLessons();
        }
        else if (menuItemID == 20000 + 3)
        {
            processorRef.openLastReport();
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
    static constexpr auto kPageNames = std::to_array<const char*>({"Lessons", "Practice", "Result", "Settings"});
    std::array<juce::TextButton, kPageNames.size()> m_pageButtons;
    size_t m_currentPage{0};
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


    CustomRotaryDial bpmDial{this};
    juce::ToggleButton clickOnSwitch{juce::String::fromUTF8("Click")};
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> clickOnSwitchAttachment;
    CustomRotaryDial latencyCompDial{this};
    juce::ComboBox instrumentDrop{};
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> instrumentDropAttachment;
    juce::ComboBox inputDrop{};
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> inputDropAttachment;
    CustomRotaryDial outputVolumeDial{this};
    CustomRotaryDial inputGainDial{this};
    CustomRotaryDial instrumentLevelDial{this};
    CustomRotaryDial clickLevelDial{this};
    MomentaryToggleButton listenSwitch{juce::String::fromUTF8("Listen")};
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> listenSwitchAttachment;
    MomentaryToggleButton playSwitch{juce::String::fromUTF8("Play")};
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> playSwitchAttachment;
    MomentaryToggleButton replaySwitch{juce::String::fromUTF8("Replay Take")};
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> replaySwitchAttachment;
    MomentaryToggleButton stopSwitch{juce::String::fromUTF8("Stop")};
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> stopSwitchAttachment;
    MomentaryToggleButton nextSwitch{juce::String::fromUTF8("Next Phrase")};
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> nextSwitchAttachment;
    MomentaryToggleButton referenceSwitch{juce::String::fromUTF8("Reference")};
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> referenceSwitchAttachment;
    juce::ToggleButton saveTakesSwitch{juce::String::fromUTF8("Save Takes")};
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> saveTakesSwitchAttachment;
    PianoRollDisplay rollGauge{};
    ScoreDisplay scoreGauge{};
    LessonBrowserDisplay lessonsGauge{};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioPluginAudioProcessorEditor)
};
