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
            std::vector<juce::Rectangle<int>> areas(3);
            const auto rowHeight = area.getHeight() / 3;
            areas[0] = area.removeFromTop(rowHeight * 1).reduced(Constants::Margins::small);
            areas[1] = area.removeFromTop(rowHeight * 1).reduced(Constants::Margins::small);
            areas[2] = area.reduced(Constants::Margins::small);

            {
                juce::FlexBox box;
                box.flexWrap = juce::FlexBox::Wrap::noWrap;
                box.flexDirection = juce::FlexBox::Direction::row;
                box.justifyContent = juce::FlexBox::JustifyContent::spaceAround;
                box.items.add(juce::FlexItem(bpmDial).withFlex(1).withMargin(knobMarginSmall));
                box.items.add(juce::FlexItem(playStopSwitch)
                                  .withWidth(Constants::Text::labelWidth)
                                  .withHeight(Constants::Text::labelHeight)
                                  .withAlignSelf(juce::FlexItem::AlignSelf::center)
                                  .withMargin(knobMarginSmall));
                box.items.add(juce::FlexItem(levelDial).withFlex(1).withMargin(knobMarginSmall));
                box.items.add(juce::FlexItem(keyDrop)
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
                box.items.add(juce::FlexItem(lfoDepthDial).withFlex(1).withMargin(knobMarginSmall));
                box.items.add(juce::FlexItem(filterResonanceDial).withFlex(1).withMargin(knobMarginSmall));
                box.items.add(juce::FlexItem(filterCutoffDial).withFlex(1).withMargin(knobMarginSmall));
                box.performLayout(areas[1].toFloat());
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
                box.items.add(juce::FlexItem(spectrogramGauge).withFlex(1).withMargin(knobMarginSmall));
                box.performLayout(areas[2].toFloat());
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
                box.items.add(juce::FlexItem(decayOctaveDial).withFlex(1).withMargin(knobMarginSmall));
                box.items.add(juce::FlexItem(damperDial).withFlex(1).withMargin(knobMarginSmall));
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
            spectrogramGauge.update(processorRef.getSpectrogram());

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
        keyDrop.setTooltip(juce::String::fromUTF8(
            "Key (C0, C#/Db0, D0, D#/Eb0, E0, F0, F#/Gb0, G0, G#/Ab0, A0, Bb0, B0, C1, C#/Db1, D1, D#/Eb1, E1, F1, "
            "F#/Gb1, G1, G#/Ab1, A1, Bb1, B1, C2, C#/Db2, D2, D#/Eb2, E2, F2, F#/Gb2, G2, G#/Ab2, A2, Bb2, B2, C3)"));
        addAndMakeVisible(levelDial);
        levelDial.reset(valueTreeState, "level");
        levelDial.setLabelText(juce::String::fromUTF8("Level"));
        levelDial.setTooltip(juce::String::fromUTF8("Level (-80 to 0 dB)"));
        addAndMakeVisible(tuningDial);
        tuningDial.reset(valueTreeState, "tuning");
        tuningDial.setLabelText(juce::String::fromUTF8("Tuning"));
        tuningDial.setTooltip(juce::String::fromUTF8("Tuning (400 to 800 Hz)"));
        addAndMakeVisible(detuneDial);
        detuneDial.reset(valueTreeState, "detune");
        detuneDial.setLabelText(juce::String::fromUTF8("Detune"));
        detuneDial.setTooltip(juce::String::fromUTF8("Detune (0 to 100 ct)"));
        addAndMakeVisible(reverbDryDial);
        reverbDryDial.reset(valueTreeState, "reverbDry");
        reverbDryDial.setLabelText(juce::String::fromUTF8("Reverb Dry"));
        reverbDryDial.setTooltip(juce::String::fromUTF8("Reverb Dry (-100 to 12 dB)"));
        addAndMakeVisible(reverbWetDial);
        reverbWetDial.reset(valueTreeState, "reverbWet");
        reverbWetDial.setLabelText(juce::String::fromUTF8("Reverb Wet"));
        reverbWetDial.setTooltip(juce::String::fromUTF8("Reverb Wet (-100 to 12 dB)"));
        addAndMakeVisible(reverbSizeDial);
        reverbSizeDial.reset(valueTreeState, "reverbSize");
        reverbSizeDial.setLabelText(juce::String::fromUTF8("Reverb Size"));
        reverbSizeDial.setTooltip(juce::String::fromUTF8("Reverb Size (1 to 330 m)"));
        addAndMakeVisible(reverbDecayDial);
        reverbDecayDial.reset(valueTreeState, "reverbDecay");
        reverbDecayDial.setLabelText(juce::String::fromUTF8("Reverb Decay"));
        reverbDecayDial.setTooltip(juce::String::fromUTF8("Reverb Decay (1 to 100000 ms)"));
        addAndMakeVisible(reverbShelfLowDial);
        reverbShelfLowDial.reset(valueTreeState, "reverbShelfLow");
        reverbShelfLowDial.setLabelText(juce::String::fromUTF8("Reverb Shelf Low"));
        reverbShelfLowDial.setTooltip(juce::String::fromUTF8("Reverb Shelf Low (-18 to 18 dB)"));
        addAndMakeVisible(reverbShelfHighDial);
        reverbShelfHighDial.reset(valueTreeState, "reverbShelfHigh");
        reverbShelfHighDial.setLabelText(juce::String::fromUTF8("Reverb Shelf High"));
        reverbShelfHighDial.setTooltip(juce::String::fromUTF8("Reverb Shelf High (-18 to 18 dB)"));
        addAndMakeVisible(patternDrop);
        patternDrop.addItemList(valueTreeState.getParameter("pattern")->getAllValueStrings(), 1);
        patternDropAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
            valueTreeState, "pattern", patternDrop);
        patternDrop.setTooltip(
            juce::String::fromUTF8("Pattern (H1 H2 1 -, H1 H2 8 1 -, H1 H2 8 8 1 -, H1 H2 - 8 8 1 -)"));
        addAndMakeVisible(slideDial);
        slideDial.reset(valueTreeState, "slide");
        slideDial.setLabelText(juce::String::fromUTF8("Slide"));
        slideDial.setTooltip(juce::String::fromUTF8("Slide (0 to 100 %)"));
        addAndMakeVisible(slideTimeDial);
        slideTimeDial.reset(valueTreeState, "slideTime");
        slideTimeDial.setLabelText(juce::String::fromUTF8("Slide Time"));
        slideTimeDial.setTooltip(juce::String::fromUTF8("Slide Time (1 to 3000 ms)"));
        addAndMakeVisible(harmonicFirstDrop);
        harmonicFirstDrop.addItemList(valueTreeState.getParameter("harmonicFirst")->getAllValueStrings(), 1);
        harmonicFirstDropAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
            valueTreeState, "harmonicFirst", harmonicFirstDrop);
        harmonicFirstDrop.setTooltip(juce::String::fromUTF8(
            "Set Harmonic 1 (-12 sā सा, -11, -10 re र, -9, -8 ga ग, -7 ma म, -6, -5 pa प, -4, -3 dha ध, -2, -1 ni नी, "
            "0 Sā सा, 1, 2 re र, 3, 4 ga ग, 5 ma म, 6, 7 pa प, 8, 9 dha ध, 10, 11 ni नी, 12 Sā सा)"));
        addAndMakeVisible(harmonicSecondDrop);
        harmonicSecondDrop.addItemList(valueTreeState.getParameter("harmonicSecond")->getAllValueStrings(), 1);
        harmonicSecondDropAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
            valueTreeState, "harmonicSecond", harmonicSecondDrop);
        harmonicSecondDrop.setTooltip(juce::String::fromUTF8(
            "Set Harmonic 2 (-12 sā सा, -11, -10 re र, -9, -8 ga ग, -7 ma म, -6, -5 pa प, -4, -3 dha ध, -2, -1 ni नी, "
            "0 Sā सा, 1, 2 re र, 3, 4 ga ग, 5 ma म, 6, 7 pa प, 8, 9 dha ध, 10, 11 ni नी, 12 Sā सा)"));
        addAndMakeVisible(playStopSwitch);
        playStopSwitchAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
            valueTreeState, "playStop", playStopSwitch);
        playStopSwitch.setTooltip(juce::String::fromUTF8("Play"));

        addAndMakeVisible(humanizeTimingDial);
        humanizeTimingDial.reset(valueTreeState, "humanizeTiming");
        humanizeTimingDial.setLabelText(juce::String::fromUTF8("Humanize Timing"));
        humanizeTimingDial.setTooltip(juce::String::fromUTF8("Humanize Timing (0 to 100 %)"));
        addAndMakeVisible(humanizeLevelDial);
        humanizeLevelDial.reset(valueTreeState, "humanizeLevel");
        humanizeLevelDial.setLabelText(juce::String::fromUTF8("Humanize Level"));
        humanizeLevelDial.setTooltip(juce::String::fromUTF8("Humanize Level (0 to 100 %)"));
        addAndMakeVisible(bpmDial);
        bpmDial.reset(valueTreeState, "bpm");
        bpmDial.setLabelText(juce::String::fromUTF8("BPM"));
        bpmDial.setTooltip(juce::String::fromUTF8("BPM (40 to 250 BPM)"));
        addAndMakeVisible(hostSyncSwitch);
        hostSyncSwitchAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
            valueTreeState, "hostSync", hostSyncSwitch);
        hostSyncSwitch.setTooltip(juce::String::fromUTF8("Host Sync"));

        addAndMakeVisible(pluckDivisionDrop);
        pluckDivisionDrop.addItemList(valueTreeState.getParameter("pluckDivision")->getAllValueStrings(), 1);
        pluckDivisionDropAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
            valueTreeState, "pluckDivision", pluckDivisionDrop);
        pluckDivisionDrop.setTooltip(juce::String::fromUTF8(
            "Pluck Division (1/1, 1/2, 1/2., 1/2T, 1/4, 1/4., 1/4T, 1/8, 1/8., 1/8T, 1/16, 1/16., 1/16T)"));
        addAndMakeVisible(pauseDivisionDrop);
        pauseDivisionDrop.addItemList(valueTreeState.getParameter("pauseDivision")->getAllValueStrings(), 1);
        pauseDivisionDropAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
            valueTreeState, "pauseDivision", pauseDivisionDrop);
        pauseDivisionDrop.setTooltip(juce::String::fromUTF8(
            "Pause Division (1/1, 1/2, 1/2., 1/2T, 1/4, 1/4., 1/4T, 1/8, 1/8., 1/8T, 1/16, 1/16., 1/16T)"));
        addAndMakeVisible(attackDial);
        attackDial.reset(valueTreeState, "attack");
        attackDial.setLabelText(juce::String::fromUTF8("Attack"));
        attackDial.setTooltip(juce::String::fromUTF8("Attack (1 to 3000 ms)"));
        addAndMakeVisible(decayDial);
        decayDial.reset(valueTreeState, "decay");
        decayDial.setLabelText(juce::String::fromUTF8("Decay"));
        decayDial.setTooltip(juce::String::fromUTF8("Decay (1 to 100000 ms)"));
        addAndMakeVisible(decayOctaveDial);
        decayOctaveDial.reset(valueTreeState, "decayOctave");
        decayOctaveDial.setLabelText(juce::String::fromUTF8("Decay Octave"));
        decayOctaveDial.setTooltip(juce::String::fromUTF8("Decay Octave (0 to 2)"));
        addAndMakeVisible(damperDial);
        damperDial.reset(valueTreeState, "damper");
        damperDial.setLabelText(juce::String::fromUTF8("Damper"));
        damperDial.setTooltip(juce::String::fromUTF8("Damper (0 to 1)"));
        addAndMakeVisible(levelSustainDial);
        levelSustainDial.reset(valueTreeState, "levelSustain");
        levelSustainDial.setLabelText(juce::String::fromUTF8("Sustain"));
        levelSustainDial.setTooltip(juce::String::fromUTF8("Sustain (0 to 1)"));
        addAndMakeVisible(sustainHumanizeDial);
        sustainHumanizeDial.reset(valueTreeState, "sustainHumanize");
        sustainHumanizeDial.setLabelText(juce::String::fromUTF8("Sustain Humanize"));
        sustainHumanizeDial.setTooltip(juce::String::fromUTF8("Sustain Humanize (0 to 100 %)"));
        addAndMakeVisible(lfoDepthDial);
        lfoDepthDial.reset(valueTreeState, "lfoDepth");
        lfoDepthDial.setLabelText(juce::String::fromUTF8("Filter LFO Depth"));
        lfoDepthDial.setTooltip(juce::String::fromUTF8("Filter LFO Depth (0 to 2)"));
        addAndMakeVisible(lfoSpeedDial);
        lfoSpeedDial.reset(valueTreeState, "lfoSpeed");
        lfoSpeedDial.setLabelText(juce::String::fromUTF8("Filter LFO Speed"));
        lfoSpeedDial.setTooltip(juce::String::fromUTF8("Filter LFO Speed (0.01 to 20 Hz)"));
        addAndMakeVisible(lfoSpeedVariationDial);
        lfoSpeedVariationDial.reset(valueTreeState, "lfoSpeedVariation");
        lfoSpeedVariationDial.setLabelText(juce::String::fromUTF8("Filter LFO Variation"));
        lfoSpeedVariationDial.setTooltip(juce::String::fromUTF8("Filter LFO Variation (0 to 100 %)"));
        addAndMakeVisible(attackFilterDial);
        attackFilterDial.reset(valueTreeState, "attackFilter");
        attackFilterDial.setLabelText(juce::String::fromUTF8("Filter Attack"));
        attackFilterDial.setTooltip(juce::String::fromUTF8("Filter Attack (1 to 3000 ms)"));
        addAndMakeVisible(decayFilterDial);
        decayFilterDial.reset(valueTreeState, "decayFilter");
        decayFilterDial.setLabelText(juce::String::fromUTF8("Filter Decay"));
        decayFilterDial.setTooltip(juce::String::fromUTF8("Filter Decay (1 to 30000 ms)"));
        addAndMakeVisible(levelSustainFilterDial);
        levelSustainFilterDial.reset(valueTreeState, "levelSustainFilter");
        levelSustainFilterDial.setLabelText(juce::String::fromUTF8("Filter Sustain"));
        levelSustainFilterDial.setTooltip(juce::String::fromUTF8("Filter Sustain (0 to 1)"));
        addAndMakeVisible(filterCutoffDial);
        filterCutoffDial.reset(valueTreeState, "filterCutoff");
        filterCutoffDial.setLabelText(juce::String::fromUTF8("Filter Cutoff"));
        filterCutoffDial.setTooltip(juce::String::fromUTF8("Filter Cutoff (-60 to 48 st)"));
        addAndMakeVisible(filterResonanceDial);
        filterResonanceDial.reset(valueTreeState, "filterResonance");
        filterResonanceDial.setLabelText(juce::String::fromUTF8("Filter Resonance"));
        filterResonanceDial.setTooltip(juce::String::fromUTF8("Filter Resonance (0 to 1.2)"));
        addAndMakeVisible(contourFilterDial);
        contourFilterDial.reset(valueTreeState, "contourFilter");
        contourFilterDial.setLabelText(juce::String::fromUTF8("Contour F"));
        contourFilterDial.setTooltip(juce::String::fromUTF8("Contour F (-4 to 4 oct)"));
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
            keyDrop.setVisible(true);
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
            playStopSwitch.setVisible(true);
            humanizeTimingDial.setVisible(false);
            humanizeLevelDial.setVisible(false);
            bpmDial.setVisible(true);
            hostSyncSwitch.setVisible(false);
            pluckDivisionDrop.setVisible(false);
            pauseDivisionDrop.setVisible(false);
            attackDial.setVisible(false);
            decayDial.setVisible(false);
            decayOctaveDial.setVisible(false);
            damperDial.setVisible(false);
            levelSustainDial.setVisible(false);
            sustainHumanizeDial.setVisible(false);
            lfoDepthDial.setVisible(true);
            lfoSpeedDial.setVisible(false);
            lfoSpeedVariationDial.setVisible(false);
            attackFilterDial.setVisible(false);
            decayFilterDial.setVisible(false);
            levelSustainFilterDial.setVisible(false);
            filterCutoffDial.setVisible(true);
            filterResonanceDial.setVisible(true);
            contourFilterDial.setVisible(false);
            spectrogramGauge.setVisible(true);
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
            decayOctaveDial.setVisible(true);
            damperDial.setVisible(true);
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
            spectrogramGauge.setVisible(false);
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
            "Tanpura drone synth - plucked-string simulation of the Indian drone instrument.\n\nPart of the AbacDsp "
            "project - core DSP library is MIT licensed.\n\nBuilt with JUCE, licensed under AGPLv3 (or a commercial "
            "JUCE licence).\n\nFull third-party license details: THIRD-PARTY-LICENSES.md in the AbacDsp repository.";

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
    CustomRotaryDial decayOctaveDial{this};
    CustomRotaryDial damperDial{this};
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
    SpectrogramDisplay spectrogramGauge{AppSettings::loadTheme()};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioPluginAudioProcessorEditor)
};
