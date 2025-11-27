#pragma once
/*
 * AUTO GENERATED,
 * NOT A GOOD IDEA TO CHANGE STUFF HERE
 * Keep the file readonly
 */

#include "JuliasynthProcessor.h"

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

        std::vector<juce::Rectangle<int>> areas(3);
        const auto colWidth = area.getWidth() / 10;
        const auto rowHeight = area.getHeight() / 4;
        areas[0] = area.removeFromLeft(colWidth * 1).reduced(Constants::Margins::small);
        auto keepArea = area;
        areas[1] = area.removeFromTop(rowHeight * 1).reduced(Constants::Margins::small);
        areas[2] = area.reduced(Constants::Margins::small);

        {
            juce::FlexBox box;
            box.flexWrap = juce::FlexBox::Wrap::noWrap;
            box.flexDirection = juce::FlexBox::Direction::column;
            box.justifyContent = juce::FlexBox::JustifyContent::spaceAround;
            box.items.add(juce::FlexItem(volDial).withFlex(1).withMargin(knobMarginSmall));
            box.items.add(juce::FlexItem(levelGauge).withHeight(200).withMargin(knobMarginSmall));
            box.items.add(juce::FlexItem(cpuGauge).withHeight(200).withMargin(knobMarginSmall));
            box.performLayout(areas[0].toFloat());
        }
        {
            juce::FlexBox box;
            box.flexWrap = juce::FlexBox::Wrap::noWrap;
            box.flexDirection = juce::FlexBox::Direction::row;
            box.justifyContent = juce::FlexBox::JustifyContent::spaceAround;
            box.items.add(juce::FlexItem(cxDial).withFlex(1).withMargin(knobMarginSmall));
            box.items.add(juce::FlexItem(cyDial).withFlex(1).withMargin(knobMarginSmall));
            box.items.add(juce::FlexItem(jxDial).withFlex(1).withMargin(knobMarginSmall));
            box.items.add(juce::FlexItem(jyDial).withFlex(1).withMargin(knobMarginSmall));
            box.items.add(juce::FlexItem(jrxDial).withFlex(1).withMargin(knobMarginSmall));
            box.items.add(juce::FlexItem(jryDial).withFlex(1).withMargin(knobMarginSmall));
            box.performLayout(areas[1].toFloat());
        }
        {
            juce::FlexBox box;
            box.flexWrap = juce::FlexBox::Wrap::noWrap;
            box.flexDirection = juce::FlexBox::Direction::column;
            box.justifyContent = juce::FlexBox::JustifyContent::spaceAround;
            box.items.add(juce::FlexItem(spectrogramGauge).withFlex(1).withMargin(knobMarginSmall));
            box.items.add(juce::FlexItem(signalGauge).withFlex(1).withMargin(knobMarginSmall));
            box.performLayout(areas[2].toFloat());
        }
    }
#pragma GCC diagnostic pop

    void timerCallback() override
    {
        cpuGauge.update(processorRef.getCpuLoad());
        levelGauge.update(processorRef.getInputDbLoad(), processorRef.getOutputDbLoad());
        spectrogramGauge.update(processorRef.getSpectrogram());
        signalGauge.update(processorRef.getWaveDataToShow());
    }

    void initWidgets()
    {
        addAndMakeVisible(volDial);
        volDial.reset(valueTreeState, "vol");
        volDial.setLabelText(juce::String::fromUTF8("Vol"));
        addAndMakeVisible(powerDial);
        powerDial.reset(valueTreeState, "power");
        powerDial.setLabelText(juce::String::fromUTF8("Power"));
        addAndMakeVisible(cxDial);
        cxDial.reset(valueTreeState, "cx");
        cxDial.setLabelText(juce::String::fromUTF8("CX"));
        addAndMakeVisible(cyDial);
        cyDial.reset(valueTreeState, "cy");
        cyDial.setLabelText(juce::String::fromUTF8("CY"));
        addAndMakeVisible(jxDial);
        jxDial.reset(valueTreeState, "jx");
        jxDial.setLabelText(juce::String::fromUTF8("JX"));
        addAndMakeVisible(jyDial);
        jyDial.reset(valueTreeState, "jy");
        jyDial.setLabelText(juce::String::fromUTF8("JY"));
        addAndMakeVisible(jrxDial);
        jrxDial.reset(valueTreeState, "jrx");
        jrxDial.setLabelText(juce::String::fromUTF8("JR-X"));
        addAndMakeVisible(jryDial);
        jryDial.reset(valueTreeState, "jry");
        jryDial.setLabelText(juce::String::fromUTF8("JR-Y"));
        addAndMakeVisible(cpuGauge);
        cpuGauge.setLabelText(juce::String::fromUTF8("CPU"));
        addAndMakeVisible(levelGauge);
        levelGauge.setLabelText(juce::String::fromUTF8("Level"));
        addAndMakeVisible(spectrogramGauge);
        spectrogramGauge.setLabelText(juce::String::fromUTF8("Spectrogram"));
        addAndMakeVisible(signalGauge);
        signalGauge.setLabelText(juce::String::fromUTF8("Signal"));
    }

  private:
    AudioPluginAudioProcessor& processorRef;
    juce::AudioProcessorValueTreeState& valueTreeState;
    GuiLookAndFeel m_laf;
    juce::Colour backgroundApp;

    CustomRotaryDial volDial{this};
    CustomRotaryDial powerDial{this};
    CustomRotaryDial cxDial{this};
    CustomRotaryDial cyDial{this};
    CustomRotaryDial jxDial{this};
    CustomRotaryDial jyDial{this};
    CustomRotaryDial jrxDial{this};
    CustomRotaryDial jryDial{this};
    CpuGauge cpuGauge{};
    Gauge levelGauge{};
    SpectrogramDisplay spectrogramGauge{};
    WaveformGauge signalGauge{};
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioPluginAudioProcessorEditor)
};
