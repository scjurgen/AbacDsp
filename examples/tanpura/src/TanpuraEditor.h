#pragma once
/*
 * AUTO GENERATED,
 * NOT A GOOD IDEA TO CHANGE STUFF HERE
 * Keep the file readonly
 */

#include <map>

#include "TanpuraProcessor.h"
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
                box.items.add(juce::FlexItem(bpmDial).withFlex(1).withMargin(knobMarginSmall));
                box.items.add(juce::FlexItem(levelDial).withFlex(1).withMargin(knobMarginSmall));
                box.items.add(juce::FlexItem(reverbDryDial).withFlex(1).withMargin(knobMarginSmall));
                box.items.add(juce::FlexItem(reverbWetDial).withFlex(1).withMargin(knobMarginSmall));
                box.items.add(juce::FlexItem(reverbSizeDial).withFlex(1).withMargin(knobMarginSmall));
                box.items.add(juce::FlexItem(reverbDecayDial).withFlex(1).withMargin(knobMarginSmall));
                box.performLayout(areas[0].toFloat());
            }
        }
        else
        {
            // auto generated
            // const juce::FlexItem::Margin knobMargin = juce::FlexItem::Margin(Constants::Margins::small);
            const juce::FlexItem::Margin knobMarginSmall = juce::FlexItem::Margin(Constants::Margins::medium);
            std::vector<juce::Rectangle<int>> areas(5);
            const auto rowHeight = area.getHeight() / 5;
            areas[0] = area.removeFromTop(rowHeight * 1).reduced(Constants::Margins::small);
            areas[1] = area.removeFromTop(rowHeight * 1).reduced(Constants::Margins::small);
            areas[2] = area.removeFromTop(rowHeight * 1).reduced(Constants::Margins::small);
            areas[3] = area.removeFromTop(rowHeight * 1).reduced(Constants::Margins::small);
            areas[4] = area.reduced(Constants::Margins::small);

            {
                juce::FlexBox box;
                box.flexWrap = juce::FlexBox::Wrap::noWrap;
                box.flexDirection = juce::FlexBox::Direction::row;
                box.justifyContent = juce::FlexBox::JustifyContent::spaceAround;
                box.items.add(juce::FlexItem(keyDrop)
                                  .withFlex(1)
                                  .withHeight(Constants::Text::labelHeight)
                                  .withAlignSelf(juce::FlexItem::AlignSelf::center)
                                  .withMargin(knobMarginSmall));
                box.items.add(juce::FlexItem(levelDial).withFlex(1).withMargin(knobMarginSmall));
                box.items.add(juce::FlexItem(tuningDial).withFlex(1).withMargin(knobMarginSmall));
                box.items.add(juce::FlexItem(detuneDial).withFlex(1).withMargin(knobMarginSmall));
                box.performLayout(areas[0].toFloat());
            }
            {
                juce::FlexBox box;
                box.flexWrap = juce::FlexBox::Wrap::noWrap;
                box.flexDirection = juce::FlexBox::Direction::row;
                box.justifyContent = juce::FlexBox::JustifyContent::spaceAround;
                box.items.add(juce::FlexItem(patternDrop)
                                  .withFlex(1)
                                  .withHeight(Constants::Text::labelHeight)
                                  .withAlignSelf(juce::FlexItem::AlignSelf::center)
                                  .withMargin(knobMarginSmall));
                box.items.add(juce::FlexItem(slideDial).withFlex(1).withMargin(knobMarginSmall));
                box.items.add(juce::FlexItem(slideTimeDial).withFlex(1).withMargin(knobMarginSmall));
                box.items.add(juce::FlexItem(harmonicFirstDrop)
                                  .withFlex(1)
                                  .withHeight(Constants::Text::labelHeight)
                                  .withAlignSelf(juce::FlexItem::AlignSelf::center)
                                  .withMargin(knobMarginSmall));
                box.items.add(juce::FlexItem(harmonicSecondDrop)
                                  .withFlex(1)
                                  .withHeight(Constants::Text::labelHeight)
                                  .withAlignSelf(juce::FlexItem::AlignSelf::center)
                                  .withMargin(knobMarginSmall));
                box.items.add(juce::FlexItem(playStopSwitch)
                                  .withWidth(Constants::Text::labelWidth)
                                  .withHeight(Constants::Text::labelHeight)
                                  .withAlignSelf(juce::FlexItem::AlignSelf::center)
                                  .withMargin(knobMarginSmall));
                box.items.add(juce::FlexItem(humanizeTimingDial).withFlex(1).withMargin(knobMarginSmall));
                box.items.add(juce::FlexItem(humanizeLevelDial).withFlex(1).withMargin(knobMarginSmall));
                box.performLayout(areas[1].toFloat());
            }
            {
                juce::FlexBox box;
                box.flexWrap = juce::FlexBox::Wrap::noWrap;
                box.flexDirection = juce::FlexBox::Direction::row;
                box.justifyContent = juce::FlexBox::JustifyContent::spaceAround;
                box.items.add(juce::FlexItem(bpmDial).withFlex(1).withMargin(knobMarginSmall));
                box.items.add(juce::FlexItem(hostSyncSwitch)
                                  .withWidth(Constants::Text::labelWidth)
                                  .withHeight(Constants::Text::labelHeight)
                                  .withAlignSelf(juce::FlexItem::AlignSelf::center)
                                  .withMargin(knobMarginSmall));
                box.items.add(juce::FlexItem(pluckDivisionDrop)
                                  .withFlex(1)
                                  .withHeight(Constants::Text::labelHeight)
                                  .withAlignSelf(juce::FlexItem::AlignSelf::center)
                                  .withMargin(knobMarginSmall));
                box.items.add(juce::FlexItem(pauseDivisionDrop)
                                  .withFlex(1)
                                  .withHeight(Constants::Text::labelHeight)
                                  .withAlignSelf(juce::FlexItem::AlignSelf::center)
                                  .withMargin(knobMarginSmall));
                box.items.add(juce::FlexItem(attackDial).withFlex(1).withMargin(knobMarginSmall));
                box.items.add(juce::FlexItem(decayDial).withFlex(1).withMargin(knobMarginSmall));
                box.items.add(juce::FlexItem(levelSustainDial).withFlex(1).withMargin(knobMarginSmall));
                box.items.add(juce::FlexItem(sustainHumanizeDial).withFlex(1).withMargin(knobMarginSmall));
                box.items.add(juce::FlexItem(lfoDepthDial).withFlex(1).withMargin(knobMarginSmall));
                box.items.add(juce::FlexItem(lfoSpeedDial).withFlex(1).withMargin(knobMarginSmall));
                box.items.add(juce::FlexItem(lfoSpeedVariationDial).withFlex(1).withMargin(knobMarginSmall));
                box.performLayout(areas[2].toFloat());
            }
            {
                juce::FlexBox box;
                box.flexWrap = juce::FlexBox::Wrap::noWrap;
                box.flexDirection = juce::FlexBox::Direction::row;
                box.justifyContent = juce::FlexBox::JustifyContent::spaceAround;
                box.items.add(juce::FlexItem(attackFilterDial).withFlex(1).withMargin(knobMarginSmall));
                box.items.add(juce::FlexItem(decayFilterDial).withFlex(1).withMargin(knobMarginSmall));
                box.items.add(juce::FlexItem(levelSustainFilterDial).withFlex(1).withMargin(knobMarginSmall));
                box.items.add(juce::FlexItem(filterCutoffDial).withFlex(1).withMargin(knobMarginSmall));
                box.items.add(juce::FlexItem(filterResonanceDial).withFlex(1).withMargin(knobMarginSmall));
                box.items.add(juce::FlexItem(contourFilterDial).withFlex(1).withMargin(knobMarginSmall));
                box.performLayout(areas[3].toFloat());
            }
            {
                juce::FlexBox box;
                box.flexWrap = juce::FlexBox::Wrap::noWrap;
                box.flexDirection = juce::FlexBox::Direction::row;
                box.justifyContent = juce::FlexBox::JustifyContent::spaceAround;
                box.items.add(juce::FlexItem(reverbDryDial).withFlex(1).withMargin(knobMarginSmall));
                box.items.add(juce::FlexItem(reverbWetDial).withFlex(1).withMargin(knobMarginSmall));
                box.items.add(juce::FlexItem(reverbSizeDial).withFlex(1).withMargin(knobMarginSmall));
                box.items.add(juce::FlexItem(reverbDecayDial).withFlex(1).withMargin(knobMarginSmall));
                box.items.add(juce::FlexItem(reverbShelfLowDial).withFlex(1).withMargin(knobMarginSmall));
                box.items.add(juce::FlexItem(reverbShelfHighDial).withFlex(1).withMargin(knobMarginSmall));
                box.performLayout(areas[4].toFloat());
            }
        }
    }
#pragma GCC diagnostic pop

    void timerCallback() override
    {
        if (processorRef.hasRunner())
        {
            bpmDial.setEnabled(!processorRef.isHostSynced());
            if (processorRef.isHostSynced())
            {
                bpmDial.setValue(processorRef.getCurrentBpm());
            }
            playStopSwitch.setEnabled(!processorRef.isHostSynced());
            if (processorRef.isHostSynced())
            {
                playStopSwitch.setToggleState(processorRef.getIsEffectivelyPlaying(), juce::dontSendNotification);
            }
        }
    }

    void initWidgets()
    {
        addAndMakeVisible(keyDrop);
        keyDrop.addItemList(valueTreeState.getParameter("key")->getAllValueStrings(), 1);
        keyDropAttachment =
            std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(valueTreeState, "key", keyDrop);
        addAndMakeVisible(levelDial);
        levelDial.reset(valueTreeState, "level");
        levelDial.setLabelText(juce::String::fromUTF8("Level"));
        addAndMakeVisible(tuningDial);
        tuningDial.reset(valueTreeState, "tuning");
        tuningDial.setLabelText(juce::String::fromUTF8("Tuning"));
        addAndMakeVisible(detuneDial);
        detuneDial.reset(valueTreeState, "detune");
        detuneDial.setLabelText(juce::String::fromUTF8("Detune"));
        addAndMakeVisible(reverbDryDial);
        reverbDryDial.reset(valueTreeState, "reverbDry");
        reverbDryDial.setLabelText(juce::String::fromUTF8("Reverb Dry"));
        addAndMakeVisible(reverbWetDial);
        reverbWetDial.reset(valueTreeState, "reverbWet");
        reverbWetDial.setLabelText(juce::String::fromUTF8("Reverb Wet"));
        addAndMakeVisible(reverbSizeDial);
        reverbSizeDial.reset(valueTreeState, "reverbSize");
        reverbSizeDial.setLabelText(juce::String::fromUTF8("Reverb Size"));
        addAndMakeVisible(reverbDecayDial);
        reverbDecayDial.reset(valueTreeState, "reverbDecay");
        reverbDecayDial.setLabelText(juce::String::fromUTF8("Reverb Decay"));
        addAndMakeVisible(reverbShelfLowDial);
        reverbShelfLowDial.reset(valueTreeState, "reverbShelfLow");
        reverbShelfLowDial.setLabelText(juce::String::fromUTF8("Reverb Shelf Low"));
        addAndMakeVisible(reverbShelfHighDial);
        reverbShelfHighDial.reset(valueTreeState, "reverbShelfHigh");
        reverbShelfHighDial.setLabelText(juce::String::fromUTF8("Reverb Shelf High"));
        addAndMakeVisible(patternDrop);
        patternDrop.addItemList(valueTreeState.getParameter("pattern")->getAllValueStrings(), 1);
        patternDropAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
            valueTreeState, "pattern", patternDrop);
        addAndMakeVisible(slideDial);
        slideDial.reset(valueTreeState, "slide");
        slideDial.setLabelText(juce::String::fromUTF8("Slide"));
        addAndMakeVisible(slideTimeDial);
        slideTimeDial.reset(valueTreeState, "slideTime");
        slideTimeDial.setLabelText(juce::String::fromUTF8("Slide Time"));
        addAndMakeVisible(harmonicFirstDrop);
        harmonicFirstDrop.addItemList(valueTreeState.getParameter("harmonicFirst")->getAllValueStrings(), 1);
        harmonicFirstDropAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
            valueTreeState, "harmonicFirst", harmonicFirstDrop);
        addAndMakeVisible(harmonicSecondDrop);
        harmonicSecondDrop.addItemList(valueTreeState.getParameter("harmonicSecond")->getAllValueStrings(), 1);
        harmonicSecondDropAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
            valueTreeState, "harmonicSecond", harmonicSecondDrop);
        addAndMakeVisible(playStopSwitch);
        playStopSwitchAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
            valueTreeState, "playStop", playStopSwitch);

        addAndMakeVisible(humanizeTimingDial);
        humanizeTimingDial.reset(valueTreeState, "humanizeTiming");
        humanizeTimingDial.setLabelText(juce::String::fromUTF8("Humanize Timing"));
        addAndMakeVisible(humanizeLevelDial);
        humanizeLevelDial.reset(valueTreeState, "humanizeLevel");
        humanizeLevelDial.setLabelText(juce::String::fromUTF8("Humanize Level"));
        addAndMakeVisible(bpmDial);
        bpmDial.reset(valueTreeState, "bpm");
        bpmDial.setLabelText(juce::String::fromUTF8("BPM"));
        addAndMakeVisible(hostSyncSwitch);
        hostSyncSwitchAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
            valueTreeState, "hostSync", hostSyncSwitch);

        addAndMakeVisible(pluckDivisionDrop);
        pluckDivisionDrop.addItemList(valueTreeState.getParameter("pluckDivision")->getAllValueStrings(), 1);
        pluckDivisionDropAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
            valueTreeState, "pluckDivision", pluckDivisionDrop);
        addAndMakeVisible(pauseDivisionDrop);
        pauseDivisionDrop.addItemList(valueTreeState.getParameter("pauseDivision")->getAllValueStrings(), 1);
        pauseDivisionDropAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
            valueTreeState, "pauseDivision", pauseDivisionDrop);
        addAndMakeVisible(attackDial);
        attackDial.reset(valueTreeState, "attack");
        attackDial.setLabelText(juce::String::fromUTF8("Attack"));
        addAndMakeVisible(decayDial);
        decayDial.reset(valueTreeState, "decay");
        decayDial.setLabelText(juce::String::fromUTF8("Decay"));
        addAndMakeVisible(levelSustainDial);
        levelSustainDial.reset(valueTreeState, "levelSustain");
        levelSustainDial.setLabelText(juce::String::fromUTF8("Sustain"));
        addAndMakeVisible(sustainHumanizeDial);
        sustainHumanizeDial.reset(valueTreeState, "sustainHumanize");
        sustainHumanizeDial.setLabelText(juce::String::fromUTF8("Sustain Humanize"));
        addAndMakeVisible(lfoDepthDial);
        lfoDepthDial.reset(valueTreeState, "lfoDepth");
        lfoDepthDial.setLabelText(juce::String::fromUTF8("Filter LFO Depth"));
        addAndMakeVisible(lfoSpeedDial);
        lfoSpeedDial.reset(valueTreeState, "lfoSpeed");
        lfoSpeedDial.setLabelText(juce::String::fromUTF8("Filter LFO Speed"));
        addAndMakeVisible(lfoSpeedVariationDial);
        lfoSpeedVariationDial.reset(valueTreeState, "lfoSpeedVariation");
        lfoSpeedVariationDial.setLabelText(juce::String::fromUTF8("Filter LFO Variation"));
        addAndMakeVisible(attackFilterDial);
        attackFilterDial.reset(valueTreeState, "attackFilter");
        attackFilterDial.setLabelText(juce::String::fromUTF8("Filter Attack"));
        addAndMakeVisible(decayFilterDial);
        decayFilterDial.reset(valueTreeState, "decayFilter");
        decayFilterDial.setLabelText(juce::String::fromUTF8("Filter Decay"));
        addAndMakeVisible(levelSustainFilterDial);
        levelSustainFilterDial.reset(valueTreeState, "levelSustainFilter");
        levelSustainFilterDial.setLabelText(juce::String::fromUTF8("Filter Sustain"));
        addAndMakeVisible(filterCutoffDial);
        filterCutoffDial.reset(valueTreeState, "filterCutoff");
        filterCutoffDial.setLabelText(juce::String::fromUTF8("Filter Cutoff"));
        addAndMakeVisible(filterResonanceDial);
        filterResonanceDial.reset(valueTreeState, "filterResonance");
        filterResonanceDial.setLabelText(juce::String::fromUTF8("Filter Resonance"));
        addAndMakeVisible(contourFilterDial);
        contourFilterDial.reset(valueTreeState, "contourFilter");
        contourFilterDial.setLabelText(juce::String::fromUTF8("Contour F"));

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
            keyDrop.setVisible(false);
            levelDial.setVisible(true);
            tuningDial.setVisible(false);
            detuneDial.setVisible(false);
            reverbDryDial.setVisible(true);
            reverbWetDial.setVisible(true);
            reverbSizeDial.setVisible(true);
            reverbDecayDial.setVisible(true);
            reverbShelfLowDial.setVisible(false);
            reverbShelfHighDial.setVisible(false);
            patternDrop.setVisible(false);
            slideDial.setVisible(false);
            slideTimeDial.setVisible(false);
            harmonicFirstDrop.setVisible(false);
            harmonicSecondDrop.setVisible(false);
            playStopSwitch.setVisible(false);
            humanizeTimingDial.setVisible(false);
            humanizeLevelDial.setVisible(false);
            bpmDial.setVisible(true);
            hostSyncSwitch.setVisible(false);
            pluckDivisionDrop.setVisible(false);
            pauseDivisionDrop.setVisible(false);
            attackDial.setVisible(false);
            decayDial.setVisible(false);
            levelSustainDial.setVisible(false);
            sustainHumanizeDial.setVisible(false);
            lfoDepthDial.setVisible(false);
            lfoSpeedDial.setVisible(false);
            lfoSpeedVariationDial.setVisible(false);
            attackFilterDial.setVisible(false);
            decayFilterDial.setVisible(false);
            levelSustainFilterDial.setVisible(false);
            filterCutoffDial.setVisible(false);
            filterResonanceDial.setVisible(false);
            contourFilterDial.setVisible(false);
        }
        else
        {
            keyDrop.setVisible(true);
            levelDial.setVisible(true);
            tuningDial.setVisible(true);
            detuneDial.setVisible(true);
            reverbDryDial.setVisible(true);
            reverbWetDial.setVisible(true);
            reverbSizeDial.setVisible(true);
            reverbDecayDial.setVisible(true);
            reverbShelfLowDial.setVisible(true);
            reverbShelfHighDial.setVisible(true);
            patternDrop.setVisible(true);
            slideDial.setVisible(true);
            slideTimeDial.setVisible(true);
            harmonicFirstDrop.setVisible(true);
            harmonicSecondDrop.setVisible(true);
            playStopSwitch.setVisible(true);
            humanizeTimingDial.setVisible(true);
            humanizeLevelDial.setVisible(true);
            bpmDial.setVisible(true);
            hostSyncSwitch.setVisible(true);
            pluckDivisionDrop.setVisible(true);
            pauseDivisionDrop.setVisible(true);
            attackDial.setVisible(true);
            decayDial.setVisible(true);
            levelSustainDial.setVisible(true);
            sustainHumanizeDial.setVisible(true);
            lfoDepthDial.setVisible(true);
            lfoSpeedDial.setVisible(true);
            lfoSpeedVariationDial.setVisible(true);
            attackFilterDial.setVisible(true);
            decayFilterDial.setVisible(true);
            levelSustainFilterDial.setVisible(true);
            filterCutoffDial.setVisible(true);
            filterResonanceDial.setVisible(true);
            contourFilterDial.setVisible(true);
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
    static constexpr int kPatchSaveId = 1000;
    static constexpr int kPatchSaveAsId = 1001;
    static constexpr int kPatchLoadIdBase = 2000;
    static constexpr int kPatchDeleteIdBase = 3000;
    static constexpr int kPatchRenameIdBase = 4000;
    std::unique_ptr<juce::AlertWindow> m_patchNameDialog;
    std::vector<juce::String> m_patchMenuNames;


    juce::ComboBox keyDrop{};
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> keyDropAttachment;
    CustomRotaryDial levelDial{this};
    CustomRotaryDial tuningDial{this};
    CustomRotaryDial detuneDial{this};
    CustomRotaryDial reverbDryDial{this};
    CustomRotaryDial reverbWetDial{this};
    CustomRotaryDial reverbSizeDial{this};
    CustomRotaryDial reverbDecayDial{this};
    CustomRotaryDial reverbShelfLowDial{this};
    CustomRotaryDial reverbShelfHighDial{this};
    juce::ComboBox patternDrop{};
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> patternDropAttachment;
    CustomRotaryDial slideDial{this};
    CustomRotaryDial slideTimeDial{this};
    juce::ComboBox harmonicFirstDrop{};
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> harmonicFirstDropAttachment;
    juce::ComboBox harmonicSecondDrop{};
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> harmonicSecondDropAttachment;
    juce::ToggleButton playStopSwitch{juce::String::fromUTF8("Play")};
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> playStopSwitchAttachment;
    CustomRotaryDial humanizeTimingDial{this};
    CustomRotaryDial humanizeLevelDial{this};
    CustomRotaryDial bpmDial{this};
    juce::ToggleButton hostSyncSwitch{juce::String::fromUTF8("Host Sync")};
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> hostSyncSwitchAttachment;
    juce::ComboBox pluckDivisionDrop{};
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> pluckDivisionDropAttachment;
    juce::ComboBox pauseDivisionDrop{};
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> pauseDivisionDropAttachment;
    CustomRotaryDial attackDial{this};
    CustomRotaryDial decayDial{this};
    CustomRotaryDial levelSustainDial{this};
    CustomRotaryDial sustainHumanizeDial{this};
    CustomRotaryDial lfoDepthDial{this};
    CustomRotaryDial lfoSpeedDial{this};
    CustomRotaryDial lfoSpeedVariationDial{this};
    CustomRotaryDial attackFilterDial{this};
    CustomRotaryDial decayFilterDial{this};
    CustomRotaryDial levelSustainFilterDial{this};
    CustomRotaryDial filterCutoffDial{this};
    CustomRotaryDial filterResonanceDial{this};
    CustomRotaryDial contourFilterDial{this};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioPluginAudioProcessorEditor)
};
