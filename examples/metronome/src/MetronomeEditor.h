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
        std::vector<juce::Rectangle<int>> areas(2);
                   const auto rowHeight = area.getHeight() / 4;
                                   areas[0] = area.removeFromTop(rowHeight*1).reduced(Constants::Margins::small);
                                   areas[1] = area.reduced(Constants::Margins::small);

{
juce::FlexBox box;
box.flexWrap = juce::FlexBox::Wrap::noWrap;
box.flexDirection = juce::FlexBox::Direction::row;
box.justifyContent = juce::FlexBox::JustifyContent::spaceAround;
box.items.add(juce::FlexItem(subsetDrop).withFlex(0).withWidth(areas[1].toFloat().getWidth()).withHeight(Constants::Text::labelHeight).withMargin(knobMarginSmall));
box.items.add(juce::FlexItem(levelGauge).withWidth(70).withMargin(knobMarginSmall));
box.items.add(juce::FlexItem(onOffSwitch).withWidth(Constants::Text::labelWidth).withHeight(Constants::Text::labelHeight).withMargin(knobMarginSmall));
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
box.items.add(juce::FlexItem(signalGauge).withFlex(1).withMargin(knobMarginSmall));
box.performLayout(areas[1].toFloat());
}

    }
#pragma GCC diagnostic pop

    void timerCallback() override
    {
        if (processorRef.hasRunner())
        {
            levelGauge.update(processorRef.getInputDbLoad(), processorRef.getOutputDbLoad());
signalGauge.update(processorRef.getWaveDataToShow());

        }
    }

    void initWidgets()
    {
        addAndMakeVisible(subsetDrop);
                subsetDrop.addItemList(valueTreeState.getParameter("subset")->getAllValueStrings(), 1);
                subsetDropAttachment = std::make_unique < juce::AudioProcessorValueTreeState::ComboBoxAttachment > (
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
                onOffSwitchAttachment = std::make_unique < juce::AudioProcessorValueTreeState::ButtonAttachment > (
                valueTreeState, "onOff", onOffSwitch);
                
addAndMakeVisible(levelGauge); levelGauge.setLabelText(juce::String::fromUTF8("Level"));
addAndMakeVisible(signalGauge); signalGauge.setLabelText(juce::String::fromUTF8("Beat"));

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
Gauge levelGauge{};
WaveformGauge signalGauge{};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioPluginAudioProcessorEditor)
};
