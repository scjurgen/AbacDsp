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
        initLlmAssist();
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
            std::vector<juce::Rectangle<int>> areas(6);
            const auto colWidth = area.getWidth() / 6;
            areas[0] = area.removeFromLeft(colWidth * 1).reduced(Constants::Margins::small);
            areas[1] = area.removeFromLeft(colWidth * 1).reduced(Constants::Margins::small);
            areas[2] = area.removeFromLeft(colWidth * 1).reduced(Constants::Margins::small);
            areas[3] = area.removeFromLeft(colWidth * 1).reduced(Constants::Margins::small);
            areas[4] = area.removeFromLeft(colWidth * 1).reduced(Constants::Margins::small);
            areas[5] = area.reduced(Constants::Margins::small);

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
                box.items.add(juce::FlexItem(trackGainADial).withFlex(1).withMargin(knobMarginSmall));
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
                box.items.add(juce::FlexItem(trackGainBDial).withFlex(1).withMargin(knobMarginSmall));
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
                box.items.add(juce::FlexItem(trackGainCDial).withFlex(1).withMargin(knobMarginSmall));
                box.items.add(juce::FlexItem(wowDepthCDial).withFlex(1).withMargin(knobMarginSmall));
                box.items.add(juce::FlexItem(wowRateCDial).withFlex(1).withMargin(knobMarginSmall));
                box.items.add(juce::FlexItem(wowDriftCDial).withFlex(1).withMargin(knobMarginSmall));
                box.items.add(juce::FlexItem(flutterDepthCDial).withFlex(1).withMargin(knobMarginSmall));
                box.items.add(juce::FlexItem(flutterRateCDial).withFlex(1).withMargin(knobMarginSmall));
                box.performLayout(areas[4].toFloat());
            }
            {
                juce::FlexBox box;
                box.flexWrap = juce::FlexBox::Wrap::noWrap;
                box.flexDirection = juce::FlexBox::Direction::column;
                box.justifyContent = juce::FlexBox::JustifyContent::spaceAround;
                box.items.add(juce::FlexItem(luaControlsLuaControlArea).withFlex(1).withMargin(knobMarginSmall));
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
            pollScriptError();
            pollLlmAssistWatcher();
            if (processorRef.hasRunner())
            {
                luaControlsLuaControlArea.refresh(toLuaControlDescriptors(processorRef.getLuaUiParamSlots()),
                                                  valueTreeState);
            }
            processorRef.consumeLastLearnedCc();
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
        addAndMakeVisible(trackGainADial);
        trackGainADial.reset(valueTreeState, "trackGainA");
        trackGainADial.setLabelText(juce::String::fromUTF8("Track Gain A"));
        trackGainADial.setTooltip(juce::String::fromUTF8("Track Gain A (-60 to 12 dB)"));
        addAndMakeVisible(trackGainBDial);
        trackGainBDial.reset(valueTreeState, "trackGainB");
        trackGainBDial.setLabelText(juce::String::fromUTF8("Track Gain B"));
        trackGainBDial.setTooltip(juce::String::fromUTF8("Track Gain B (-60 to 12 dB)"));
        addAndMakeVisible(trackGainCDial);
        trackGainCDial.reset(valueTreeState, "trackGainC");
        trackGainCDial.setLabelText(juce::String::fromUTF8("Track Gain C"));
        trackGainCDial.setTooltip(juce::String::fromUTF8("Track Gain C (-60 to 12 dB)"));
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
        addAndMakeVisible(luaControlsLuaControlArea);
        addAndMakeVisible(luaParam1Dial);
        luaParam1Dial.reset(valueTreeState, "luaParam1");
        luaParam1Dial.setLabelText(juce::String::fromUTF8("Lua Param 1"));
        luaParam1Dial.setTooltip(juce::String::fromUTF8("Lua Param 1 (0 to 1)"));
        luaParam1Dial.setCcMappable(true, {[this] { processorRef.beginCcLearn(CcTarget::luaParam1); },
                                           [this] { return processorRef.getCcRange(CcTarget::luaParam1); },
                                           [this](float lo, float hi)
                                           { processorRef.setCcRange(CcTarget::luaParam1, lo, hi); },
                                           [this] { processorRef.clearCcAssignment(CcTarget::luaParam1); },
                                           [this] { return processorRef.getCcController(CcTarget::luaParam1); }});
        addAndMakeVisible(luaParam2Dial);
        luaParam2Dial.reset(valueTreeState, "luaParam2");
        luaParam2Dial.setLabelText(juce::String::fromUTF8("Lua Param 2"));
        luaParam2Dial.setTooltip(juce::String::fromUTF8("Lua Param 2 (0 to 1)"));
        luaParam2Dial.setCcMappable(true, {[this] { processorRef.beginCcLearn(CcTarget::luaParam2); },
                                           [this] { return processorRef.getCcRange(CcTarget::luaParam2); },
                                           [this](float lo, float hi)
                                           { processorRef.setCcRange(CcTarget::luaParam2, lo, hi); },
                                           [this] { processorRef.clearCcAssignment(CcTarget::luaParam2); },
                                           [this] { return processorRef.getCcController(CcTarget::luaParam2); }});
        addAndMakeVisible(luaParam3Dial);
        luaParam3Dial.reset(valueTreeState, "luaParam3");
        luaParam3Dial.setLabelText(juce::String::fromUTF8("Lua Param 3"));
        luaParam3Dial.setTooltip(juce::String::fromUTF8("Lua Param 3 (0 to 1)"));
        luaParam3Dial.setCcMappable(true, {[this] { processorRef.beginCcLearn(CcTarget::luaParam3); },
                                           [this] { return processorRef.getCcRange(CcTarget::luaParam3); },
                                           [this](float lo, float hi)
                                           { processorRef.setCcRange(CcTarget::luaParam3, lo, hi); },
                                           [this] { processorRef.clearCcAssignment(CcTarget::luaParam3); },
                                           [this] { return processorRef.getCcController(CcTarget::luaParam3); }});
        addAndMakeVisible(luaParam4Dial);
        luaParam4Dial.reset(valueTreeState, "luaParam4");
        luaParam4Dial.setLabelText(juce::String::fromUTF8("Lua Param 4"));
        luaParam4Dial.setTooltip(juce::String::fromUTF8("Lua Param 4 (0 to 1)"));
        luaParam4Dial.setCcMappable(true, {[this] { processorRef.beginCcLearn(CcTarget::luaParam4); },
                                           [this] { return processorRef.getCcRange(CcTarget::luaParam4); },
                                           [this](float lo, float hi)
                                           { processorRef.setCcRange(CcTarget::luaParam4, lo, hi); },
                                           [this] { processorRef.clearCcAssignment(CcTarget::luaParam4); },
                                           [this] { return processorRef.getCcController(CcTarget::luaParam4); }});
        addAndMakeVisible(luaParam5Dial);
        luaParam5Dial.reset(valueTreeState, "luaParam5");
        luaParam5Dial.setLabelText(juce::String::fromUTF8("Lua Param 5"));
        luaParam5Dial.setTooltip(juce::String::fromUTF8("Lua Param 5 (0 to 1)"));
        luaParam5Dial.setCcMappable(true, {[this] { processorRef.beginCcLearn(CcTarget::luaParam5); },
                                           [this] { return processorRef.getCcRange(CcTarget::luaParam5); },
                                           [this](float lo, float hi)
                                           { processorRef.setCcRange(CcTarget::luaParam5, lo, hi); },
                                           [this] { processorRef.clearCcAssignment(CcTarget::luaParam5); },
                                           [this] { return processorRef.getCcController(CcTarget::luaParam5); }});
        addAndMakeVisible(luaParam6Dial);
        luaParam6Dial.reset(valueTreeState, "luaParam6");
        luaParam6Dial.setLabelText(juce::String::fromUTF8("Lua Param 6"));
        luaParam6Dial.setTooltip(juce::String::fromUTF8("Lua Param 6 (0 to 1)"));
        luaParam6Dial.setCcMappable(true, {[this] { processorRef.beginCcLearn(CcTarget::luaParam6); },
                                           [this] { return processorRef.getCcRange(CcTarget::luaParam6); },
                                           [this](float lo, float hi)
                                           { processorRef.setCcRange(CcTarget::luaParam6, lo, hi); },
                                           [this] { processorRef.clearCcAssignment(CcTarget::luaParam6); },
                                           [this] { return processorRef.getCcController(CcTarget::luaParam6); }});
        addAndMakeVisible(luaParam7Dial);
        luaParam7Dial.reset(valueTreeState, "luaParam7");
        luaParam7Dial.setLabelText(juce::String::fromUTF8("Lua Param 7"));
        luaParam7Dial.setTooltip(juce::String::fromUTF8("Lua Param 7 (0 to 1)"));
        luaParam7Dial.setCcMappable(true, {[this] { processorRef.beginCcLearn(CcTarget::luaParam7); },
                                           [this] { return processorRef.getCcRange(CcTarget::luaParam7); },
                                           [this](float lo, float hi)
                                           { processorRef.setCcRange(CcTarget::luaParam7, lo, hi); },
                                           [this] { processorRef.clearCcAssignment(CcTarget::luaParam7); },
                                           [this] { return processorRef.getCcController(CcTarget::luaParam7); }});
        addAndMakeVisible(luaParam8Dial);
        luaParam8Dial.reset(valueTreeState, "luaParam8");
        luaParam8Dial.setLabelText(juce::String::fromUTF8("Lua Param 8"));
        luaParam8Dial.setTooltip(juce::String::fromUTF8("Lua Param 8 (0 to 1)"));
        luaParam8Dial.setCcMappable(true, {[this] { processorRef.beginCcLearn(CcTarget::luaParam8); },
                                           [this] { return processorRef.getCcRange(CcTarget::luaParam8); },
                                           [this](float lo, float hi)
                                           { processorRef.setCcRange(CcTarget::luaParam8, lo, hi); },
                                           [this] { processorRef.clearCcAssignment(CcTarget::luaParam8); },
                                           [this] { return processorRef.getCcController(CcTarget::luaParam8); }});
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
        if (menuName == "Scripts")
        {
            return buildScriptsMenu();
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
            "MIT licensed.\n\nBuilt with JUCE, licensed under AGPLv3 (or a commercial JUCE licence).\n\nScripting "
            "powered by Lua and sol2 (both MIT licensed).\n\nFull third-party license details: THIRD-PARTY-LICENSES.md "
            "in the AbacDsp repository.";

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
        // While LLM-Assist is active, a manual edit could race with (and silently lose
        // to) a script the watcher applies from the folder - view-only instead of blocked.
        editorComponent->setReadOnly(m_llmAssistActive);
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
        scripts.addSubMenu("LLM-Assist", buildLlmAssistMenu());
        return scripts;
    }

    juce::PopupMenu buildLlmAssistMenu()
    {
        juce::PopupMenu menu;
        menu.addItem(kLlmAssistToggleId, m_llmAssistActive ? "Disable" : "Enable", true, m_llmAssistActive);
        menu.addItem(kLlmAssistChooseFolderId, "Choose Folder...");
        return menu;
    }

    void handleScriptMenuSelection(int menuItemID)
    {
        if (menuItemID == kLlmAssistToggleId)
        {
            toggleLlmAssist();
        }
        else if (menuItemID == kLlmAssistChooseFolderId)
        {
            chooseLlmAssistFolder(false);
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

    void initLlmAssist()
    {
        m_llmAssistWatcher.applyScriptText = [this](const juce::String& text)
        { return processorRef.applyScriptText(text); };
        m_llmAssistWatcher.scriptErrorMessage = [this] { return juce::String(processorRef.scriptErrorMessage()); };
        m_llmAssistWatcher.currentPatchName = [this] { return processorRef.getCurrentPatchName(); };
        m_llmAssistWatcher.currentScriptText = [this] { return processorRef.getScriptText(); };
        m_llmAssistWatcher.saveUserLibraryScript = [this](const juce::String& name, const juce::String& content)
        { return processorRef.saveUserLibraryScript(name, content); };
    }

    void toggleLlmAssist()
    {
        if (m_llmAssistActive)
        {
            m_llmAssistActive = false;
            m_statusBar.showMessage("LLM-Assist disabled");
            updateScriptEditorReadOnlyState();
            return;
        }
        if (m_llmAssistFolder.isEmpty())
        {
            chooseLlmAssistFolder(true);
            return;
        }
        m_llmAssistActive = true;
        m_statusBar.showMessage("LLM-Assist watching " + m_llmAssistFolder);
        updateScriptEditorReadOnlyState();
    }

    // Pushed into an already-open editor whenever LLM-Assist toggles, so its read-only
    // state always reflects whether a watched-folder pull could currently race an edit.
    void updateScriptEditorReadOnlyState()
    {
        if (m_scriptEditorContent != nullptr)
        {
            m_scriptEditorContent->setReadOnly(m_llmAssistActive);
        }
    }

    void chooseLlmAssistFolder(bool activateOnPick)
    {
        m_llmAssistFolderChooser =
            std::make_unique<juce::FileChooser>("Choose LLM-Assist Folder", juce::File(m_llmAssistFolder));
        m_llmAssistFolderChooser->launchAsync(juce::FileBrowserComponent::openMode |
                                                  juce::FileBrowserComponent::canSelectDirectories,
                                              [this, activateOnPick](const juce::FileChooser& fc)
                                              {
                                                  const auto result = fc.getResult();
                                                  if (!result.isDirectory())
                                                  {
                                                      return;
                                                  }
                                                  m_llmAssistFolder = result.getFullPathName();
                                                  AppSettings::saveLlmAssistFolder(m_llmAssistFolder);
                                                  m_llmAssistActive = m_llmAssistActive || activateOnPick;
                                                  m_statusBar.showMessage("LLM-Assist folder: " + m_llmAssistFolder);
                                                  updateScriptEditorReadOnlyState();
                                              });
    }

    // Runs at a fraction of the timer rate (see kLlmAssistPollEveryNTicks) - a folder
    // scan every tick is unnecessary for a workflow driven by an LLM/human editing text.
    void pollLlmAssistWatcher()
    {
        if (!m_llmAssistActive)
        {
            return;
        }
        if (++m_llmAssistPollCounter % kLlmAssistPollEveryNTicks != 0)
        {
            return;
        }
        const auto result = m_llmAssistWatcher.poll(juce::File(m_llmAssistFolder));
        if (!result)
        {
            return;
        }
        if (result->kind == LlmAssistResultKind::Library)
        {
            m_statusBar.showMessage(result->compiled
                                        ? "LLM-Assist library '" + result->scriptName + "' saved, current script OK"
                                        : "LLM-Assist library '" + result->scriptName +
                                              "' saved, current script error: " + result->error,
                                    true);
        }
        else if (result->compiled)
        {
            m_statusBar.showMessage("LLM-Assist applied '" + result->scriptName + "'", true);
            if (m_scriptEditorContent != nullptr)
            {
                m_scriptEditorContent->setScriptText(processorRef.getScriptText());
            }
        }
        else
        {
            m_statusBar.showMessage("LLM-Assist error in '" + result->scriptName + "': " + result->error, true);
        }
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
    // Points at the window's content component, so pollLlmAssistWatcher()/toggleLlmAssist()
    // can push a refresh/read-only update without reaching into ScriptEditorDialogWindow.
    juce::Component::SafePointer<ScriptEditorWindow> m_scriptEditorContent;
    static constexpr int kLlmAssistToggleId = 15000;
    static constexpr int kLlmAssistChooseFolderId = 15001;
    static constexpr int kLlmAssistPollEveryNTicks = 15;
    bool m_llmAssistActive{false};
    int m_llmAssistPollCounter{0};
    juce::String m_llmAssistFolder{AppSettings::loadLlmAssistFolder()};
    LlmAssistWatcher m_llmAssistWatcher;
    std::unique_ptr<juce::FileChooser> m_llmAssistFolderChooser;

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
    CustomRotaryDial trackGainADial{this};
    CustomRotaryDial trackGainBDial{this};
    CustomRotaryDial trackGainCDial{this};
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
    LuaControlArea luaControlsLuaControlArea{};
    CustomRotaryDial luaParam1Dial{this};
    CustomRotaryDial luaParam2Dial{this};
    CustomRotaryDial luaParam3Dial{this};
    CustomRotaryDial luaParam4Dial{this};
    CustomRotaryDial luaParam5Dial{this};
    CustomRotaryDial luaParam6Dial{this};
    CustomRotaryDial luaParam7Dial{this};
    CustomRotaryDial luaParam8Dial{this};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioPluginAudioProcessorEditor)
};
