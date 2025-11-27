#pragma once
/*
 * AUTO GENERATED,
 * NOT A GOOD IDEA TO CHANGE STUFF HERE
 * Keep the file readonly
 */

#include "GuisandboxProcessor.h"

#include "UiElements.h"

//==============================================================================
class AudioPluginAudioProcessorEditor : public juce::AudioProcessorEditor, juce::Timer
{
  public:
    explicit AudioPluginAudioProcessorEditor(AudioPluginAudioProcessor& p, juce::AudioProcessorValueTreeState& vts)
        : AudioProcessorEditor(&p)
        , processorRef(p)
        , valueTreeState(vts)
        , backgroundApp(juce::Colour(Constants::Colors::bg_App))
    {
        setLookAndFeel(&m_laf);
        initWidgets();
        setResizable(true, true);
        setResizeLimits(Constants::InitJuce::WindowWidth, Constants::InitJuce::WindowHeight, 4000, 3000);
        setSize(Constants::InitJuce::WindowWidth, Constants::InitJuce::WindowHeight);
        startTimerHz(Constants::InitJuce::TimerHertz);
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
        std::vector<juce::Rectangle<int>> areas(4);
        const auto colWidth = area.getWidth() / 13;
        areas[0] = area.removeFromLeft(colWidth * 1).reduced(Constants::Margins::small);
        areas[1] = area.removeFromLeft(colWidth * 2).reduced(Constants::Margins::small);
        areas[2] = area.removeFromLeft(colWidth * 2).reduced(Constants::Margins::small);
        areas[3] = area.reduced(Constants::Margins::small);

        {
            juce::FlexBox box;
            box.flexWrap = juce::FlexBox::Wrap::noWrap;
            box.flexDirection = juce::FlexBox::Direction::column;
            box.justifyContent = juce::FlexBox::JustifyContent::spaceAround;
            box.items.add(juce::FlexItem(levelGauge).withHeight(2400).withMargin(knobMarginSmall));
            box.items.add(juce::FlexItem(cpuGauge).withHeight(2400).withMargin(knobMarginSmall));
            box.performLayout(areas[0].toFloat());
        }
        {
            juce::FlexBox box;
            box.flexWrap = juce::FlexBox::Wrap::noWrap;
            box.flexDirection = juce::FlexBox::Direction::column;
            box.justifyContent = juce::FlexBox::JustifyContent::spaceAround;
            box.items.add(juce::FlexItem(onOffSwitch)
                              .withWidth(Constants::Text::labelWidth)
                              .withHeight(Constants::Text::labelHeight)
                              .withMargin(knobMarginSmall));
            box.items.add(juce::FlexItem(mixDial).withFlex(1).withMargin(knobMarginSmall));
            box.items.add(juce::FlexItem(modulationDepthDial).withFlex(1).withMargin(knobMarginSmall));
            box.items.add(juce::FlexItem(kneeDial).withFlex(1).withMargin(knobMarginSmall));
            box.performLayout(areas[1].toFloat());
        }
        {
            juce::FlexBox box;
            box.flexWrap = juce::FlexBox::Wrap::noWrap;
            box.flexDirection = juce::FlexBox::Direction::column;
            box.justifyContent = juce::FlexBox::JustifyContent::spaceAround;
            box.items.add(juce::FlexItem(inputDial).withFlex(1).withMargin(knobMarginSmall));
            box.items.add(juce::FlexItem(div1Label)
                              .withWidth(Constants::Text::labelWidth)
                              .withHeight(Constants::Text::labelHeight)
                              .withMargin(knobMarginSmall));
            box.items.add(juce::FlexItem(densityDial).withFlex(1).withMargin(knobMarginSmall));
            box.items.add(juce::FlexItem(thresholdDial).withFlex(1).withMargin(knobMarginSmall));
            box.performLayout(areas[2].toFloat());
        }
        {
            juce::FlexBox box;
            box.flexWrap = juce::FlexBox::Wrap::noWrap;
            box.flexDirection = juce::FlexBox::Direction::column;
            box.justifyContent = juce::FlexBox::JustifyContent::spaceAround;
            box.items.add(juce::FlexItem(spectrogramGauge).withHeight(2000).withMargin(knobMarginSmall));
            box.performLayout(areas[3].toFloat());
        }
    }
#pragma GCC diagnostic pop

    void timerCallback() override
    {
        cpuGauge.update(processorRef.getCpuLoad());
        levelGauge.update(processorRef.getInputDbLoad(), processorRef.getOutputDbLoad());
        spectrogramGauge.update(processorRef.getSpectrogram());
    }

    void initWidgets()
    {
        addAndMakeVisible(onOffSwitch);
        onOffSwitchAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
            valueTreeState, "onOff", onOffSwitch);

        addAndMakeVisible(inputDial);
        inputDial.reset(valueTreeState, "input");
        inputDial.setLabelText(juce::String::fromUTF8("Input"));
        addAndMakeVisible(modulationDepthDial);
        modulationDepthDial.reset(valueTreeState, "modulationDepth");
        modulationDepthDial.setLabelText(juce::String::fromUTF8("Depth"));
        addAndMakeVisible(mixDial);
        mixDial.reset(valueTreeState, "mix");
        mixDial.setLabelText(juce::String::fromUTF8("Mix"));
        addAndMakeVisible(densityDial);
        densityDial.reset(valueTreeState, "density");
        densityDial.setLabelText(juce::String::fromUTF8("Density"));
        addAndMakeVisible(thresholdDial);
        thresholdDial.reset(valueTreeState, "threshold");
        thresholdDial.setLabelText(juce::String::fromUTF8("Threshold"));
        addAndMakeVisible(kneeDial);
        kneeDial.reset(valueTreeState, "knee");
        kneeDial.setLabelText(juce::String::fromUTF8("Knee"));
        addAndMakeVisible(div1Label);
        div1Label.setText(juce::String::fromUTF8("-- drop this too ----"), juce::dontSendNotification);
        addAndMakeVisible(div2Label);
        div2Label.setText(juce::String::fromUTF8("DropIt!"), juce::dontSendNotification);
        addAndMakeVisible(dropItDrop);
        dropItDrop.addItemList(valueTreeState.getParameter("dropIt")->getAllValueStrings(), 1);
        dropItDropAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
            valueTreeState, "dropIt", dropItDrop);
        addAndMakeVisible(cpuGauge);
        cpuGauge.setLabelText(juce::String::fromUTF8("CPU"));
        addAndMakeVisible(levelGauge);
        levelGauge.setLabelText(juce::String::fromUTF8("Level"));
        addAndMakeVisible(spectrogramGauge);
        spectrogramGauge.setLabelText(juce::String::fromUTF8("Spectrogram"));
    }

  private:
    AudioPluginAudioProcessor& processorRef;
    juce::AudioProcessorValueTreeState& valueTreeState;
    GuiLookAndFeel m_laf;
    juce::Colour backgroundApp;

    juce::ToggleButton onOffSwitch{juce::String::fromUTF8("✨")};
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> onOffSwitchAttachment;
    CustomRotaryDial inputDial{this};
    CustomRotaryDial modulationDepthDial{this};
    CustomRotaryDial mixDial{this};
    CustomRotaryDial densityDial{this};
    CustomRotaryDial thresholdDial{this};
    CustomRotaryDial kneeDial{this};
    juce::Label div1Label{};
    juce::Label div2Label{};
    juce::ComboBox dropItDrop{};
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> dropItDropAttachment;
    CpuGauge cpuGauge{};
    Gauge levelGauge{};
    SpectrogramDisplay spectrogramGauge{};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioPluginAudioProcessorEditor)
};
