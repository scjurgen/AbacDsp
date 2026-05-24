#pragma once
/*
 * AUTO GENERATED,
 * NOT A GOOD IDEA TO CHANGE STUFF HERE
 * Keep the file readonly
 */

#include "MetronomeProcessor.h"
#include "UiElements.h"

//==============================================================================
class AudioPluginAudioProcessorEditor : public juce::AudioProcessorEditor, juce::Timer
{
  public:
    explicit AudioPluginAudioProcessorEditor(AudioPluginAudioProcessor& p, juce::AudioProcessorValueTreeState& vts)
        : AudioProcessorEditor(&p)
        , processorRef(p)
        , valueTreeState(vts)
        , backgroundApp(juce::Colour(GuiConstants::instance().colors.bg_App))
    {
        setLookAndFeel(&m_laf);
        initWidgets();
        setResizable(true, true);
        setResizeLimits(GuiConstants::instance().init.WindowWidth, GuiConstants::instance().init.WindowHeight, 4000,
                        3000);
        setSize(GuiConstants::instance().init.WindowWidth, GuiConstants::instance().init.WindowHeight);
        startTimerHz(GuiConstants::instance().init.TimerHertz);
    }

    ~AudioPluginAudioProcessorEditor() override
    {
        stopTimer();
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
        auto area = getLocalBounds().reduced(static_cast<int>(Constants::Margins::big));

        // auto generated
        // const juce::FlexItem::Margin knobMargin = juce::FlexItem::Margin(Constants::Margins::small);
        const juce::FlexItem::Margin knobMarginSmall = juce::FlexItem::Margin(Constants::Margins::medium);
        std::vector<juce::Rectangle<int>> areas(3);
        const auto rowHeight = area.getHeight() / 6;
        areas[0] = area.removeFromTop(rowHeight).reduced(Constants::Margins::small);
        areas[1] = area.removeFromTop(rowHeight).reduced(Constants::Margins::small);
        areas[2] = area.reduced(Constants::Margins::small);

        {
            juce::FlexBox box;
            box.flexWrap = juce::FlexBox::Wrap::noWrap;
            box.flexDirection = juce::FlexBox::Direction::row;
            box.justifyContent = juce::FlexBox::JustifyContent::spaceAround;
            box.items.add(juce::FlexItem(subsetDrop)
                              .withFlex(0)
                              .withWidth(120)
                              .withHeight(Constants::Text::labelHeight)
                              .withAlignSelf(juce::FlexItem::AlignSelf::center)
                              .withMargin(knobMarginSmall));
            box.items.add(juce::FlexItem(levelGauge).withWidth(70).withMargin(knobMarginSmall));
            box.items.add(juce::FlexItem(onOffSwitch)
                              .withWidth(Constants::Text::labelWidth)
                              .withHeight(Constants::Text::labelHeight)
                              .withAlignSelf(juce::FlexItem::AlignSelf::center)
                              .withMargin(knobMarginSmall));
            box.items.add(juce::FlexItem(bpmDial).withFlex(1).withMargin(knobMarginSmall));
            box.items.add(juce::FlexItem(metroVolumeDial).withFlex(1).withMargin(knobMarginSmall));
            box.items.add(juce::FlexItem(inputVolumeDial).withFlex(1).withMargin(knobMarginSmall));
            box.performLayout(areas[0].toFloat());
        }
        {
            juce::FlexBox box;
            box.flexWrap = juce::FlexBox::Wrap::noWrap;
            box.flexDirection = juce::FlexBox::Direction::row;
            box.justifyContent = juce::FlexBox::JustifyContent::spaceAround;
            box.items.add(juce::FlexItem(timeSigDrop)
                              .withFlex(0)
                              .withWidth(160)
                              .withHeight(Constants::Text::labelHeight)
                              .withAlignSelf(juce::FlexItem::AlignSelf::center)
                              .withMargin(knobMarginSmall));
            box.items.add(juce::FlexItem(subdivisionDrop)
                              .withFlex(0)
                              .withWidth(160)
                              .withHeight(Constants::Text::labelHeight)
                              .withAlignSelf(juce::FlexItem::AlignSelf::center)
                              .withMargin(knobMarginSmall));
            box.items.add(juce::FlexItem(subVolumeDial).withFlex(1).withMargin(knobMarginSmall));
            box.performLayout(areas[1].toFloat());
        }
        {
            juce::FlexBox box;
            box.flexWrap = juce::FlexBox::Wrap::noWrap;
            box.flexDirection = juce::FlexBox::Direction::row;
            box.justifyContent = juce::FlexBox::JustifyContent::spaceAround;
            box.items.add(juce::FlexItem(signalGauge).withFlex(1).withMargin(knobMarginSmall));
            box.performLayout(areas[2].toFloat());
        }
    }
#pragma GCC diagnostic pop

    void timerCallback() override
    {
        if (processorRef.hasRunner())
        {
            levelGauge.update(processorRef.getInputDbLoad(), processorRef.getOutputDbLoad());
            const float sr = static_cast<float>(processorRef.getSampleRate());
            const float bpm = static_cast<float>(valueTreeState.getParameterAsValue("bpm").getValue());
            signalGauge.setSampleRate(sr);
            signalGauge.setSamplesPerBeat(static_cast<size_t>(sr * 60.f / bpm));
            signalGauge.setSubdivisionType(
                static_cast<int>(valueTreeState.getParameterAsValue("subdivision").getValue()));
            signalGauge.update(processorRef.getWaveDataToShow());
        }
    }

    void initWidgets()
    {
        addAndMakeVisible(subsetDrop);
        subsetDrop.addItemList(valueTreeState.getParameter("subset")->getAllValueStrings(), 1);
        subsetDropAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
            valueTreeState, "subset", subsetDrop);
        addAndMakeVisible(bpmDial);
        bpmDial.reset(valueTreeState, "bpm");
        bpmDial.setLabelText(juce::String::fromUTF8("BPM"));
        addAndMakeVisible(metroVolumeDial);
        metroVolumeDial.reset(valueTreeState, "metroVolume");
        metroVolumeDial.setLabelText(juce::String::fromUTF8("Metro Volume"));
        addAndMakeVisible(inputVolumeDial);
        inputVolumeDial.reset(valueTreeState, "inputVolume");
        inputVolumeDial.setLabelText(juce::String::fromUTF8("Input Volume"));
        addAndMakeVisible(onOffSwitch);
        onOffSwitchAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
            valueTreeState, "onOff", onOffSwitch);

        addAndMakeVisible(timeSigDrop);
        timeSigDrop.addItemList(valueTreeState.getParameter("timeSig")->getAllValueStrings(), 1);
        timeSigDropAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
            valueTreeState, "timeSig", timeSigDrop);
        addAndMakeVisible(subdivisionDrop);
        subdivisionDrop.addItemList(valueTreeState.getParameter("subdivision")->getAllValueStrings(), 1);
        subdivisionDropAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
            valueTreeState, "subdivision", subdivisionDrop);
        addAndMakeVisible(subVolumeDial);
        subVolumeDial.reset(valueTreeState, "subVolume");
        subVolumeDial.setLabelText(juce::String::fromUTF8("Sub Volume"));

        addAndMakeVisible(levelGauge);
        levelGauge.setLabelText(juce::String::fromUTF8("Level"));
        addAndMakeVisible(signalGauge);
        signalGauge.setLabelText(juce::String::fromUTF8("Beat"));
    }

  private:
    AudioPluginAudioProcessor& processorRef;
    juce::AudioProcessorValueTreeState& valueTreeState;
    GuiLookAndFeel m_laf;
    juce::Colour backgroundApp;

    juce::ComboBox subsetDrop{};
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> subsetDropAttachment;
    CustomRotaryDial bpmDial{this};
    CustomRotaryDial metroVolumeDial{this};
    CustomRotaryDial inputVolumeDial{this};
    juce::ToggleButton onOffSwitch{juce::String::fromUTF8("Start")};
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> onOffSwitchAttachment;
    juce::ComboBox timeSigDrop{};
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> timeSigDropAttachment;
    juce::ComboBox subdivisionDrop{};
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> subdivisionDropAttachment;
    CustomRotaryDial subVolumeDial{this};
    Gauge levelGauge{};
    MetronomeWaveDisplay signalGauge{};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioPluginAudioProcessorEditor)
};
