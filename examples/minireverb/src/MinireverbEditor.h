#pragma once
/*
 * AUTO GENERATED,
 * NOT A GOOD IDEA TO CHANGE STUFF HERE
 * Keep the file readonly
 */

#include "MinireverbProcessor.h"

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
        const auto colWidth = area.getWidth() / 6;
        const auto rowHeight = area.getHeight() / 3;
        areas[0] = area.removeFromLeft(colWidth * 1).reduced(Constants::Margins::small);
        auto keepArea = area;
        areas[1] = area.removeFromTop(rowHeight * 1).reduced(Constants::Margins::small);
        areas[2] = area.removeFromTop(rowHeight * 1).reduced(Constants::Margins::small);
        areas[3] = area.reduced(Constants::Margins::small);

        {
            juce::FlexBox box;
            box.flexWrap = juce::FlexBox::Wrap::noWrap;
            box.flexDirection = juce::FlexBox::Direction::column;
            box.justifyContent = juce::FlexBox::JustifyContent::spaceAround;
            box.items.add(juce::FlexItem(dryDial).withFlex(1).withMargin(knobMarginSmall));
            box.items.add(juce::FlexItem(wetDial).withFlex(1).withMargin(knobMarginSmall));
            box.items.add(juce::FlexItem(levelGauge).withHeight(80).withMargin(knobMarginSmall));
            box.items.add(juce::FlexItem(cpuGauge).withHeight(80).withMargin(knobMarginSmall));
            box.performLayout(areas[0].toFloat());
        }
        {
            juce::FlexBox box;
            box.flexWrap = juce::FlexBox::Wrap::noWrap;
            box.flexDirection = juce::FlexBox::Direction::row;
            box.justifyContent = juce::FlexBox::JustifyContent::spaceAround;
            box.items.add(juce::FlexItem(spectrogramGauge).withWidth(400).withMargin(knobMarginSmall));
            box.items.add(juce::FlexItem(lowPassDial).withFlex(1).withMargin(knobMarginSmall));
            box.items.add(juce::FlexItem(highPassDial).withFlex(1).withMargin(knobMarginSmall));
            box.performLayout(areas[1].toFloat());
        }
        {
            juce::FlexBox box;
            box.flexWrap = juce::FlexBox::Wrap::noWrap;
            box.flexDirection = juce::FlexBox::Direction::row;
            box.justifyContent = juce::FlexBox::JustifyContent::spaceAround;
            box.items.add(juce::FlexItem(decayDial).withFlex(1).withMargin(knobMarginSmall));
            box.items.add(juce::FlexItem(baseSizeDial).withFlex(1).withMargin(knobMarginSmall));
            box.items.add(juce::FlexItem(sizeFactorDial).withFlex(1).withMargin(knobMarginSmall));
            box.items.add(juce::FlexItem(bulgeDial).withFlex(1).withMargin(knobMarginSmall));
            box.performLayout(areas[2].toFloat());
        }
        {
            juce::FlexBox box;
            box.flexWrap = juce::FlexBox::Wrap::noWrap;
            box.flexDirection = juce::FlexBox::Direction::row;
            box.justifyContent = juce::FlexBox::JustifyContent::spaceAround;
            box.items.add(juce::FlexItem(modulationDepthDial).withFlex(1).withMargin(knobMarginSmall));
            box.items.add(juce::FlexItem(modulationSpeedDial).withFlex(1).withMargin(knobMarginSmall));
            box.items.add(juce::FlexItem(pitchStrengthDial).withFlex(1).withMargin(knobMarginSmall));
            box.items.add(juce::FlexItem(pitch1InplaceDial).withFlex(1).withMargin(knobMarginSmall));
            box.items.add(juce::FlexItem(pitch2InplaceDial).withFlex(1).withMargin(knobMarginSmall));
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
        addAndMakeVisible(dryDial);
        dryDial.reset(valueTreeState, "dry");
        dryDial.setLabelText(juce::String::fromUTF8("Dry"));
        addAndMakeVisible(wetDial);
        wetDial.reset(valueTreeState, "wet");
        wetDial.setLabelText(juce::String::fromUTF8("Wet"));
        addAndMakeVisible(baseSizeDial);
        baseSizeDial.reset(valueTreeState, "baseSize");
        baseSizeDial.setLabelText(juce::String::fromUTF8("Base size"));
        addAndMakeVisible(sizeFactorDial);
        sizeFactorDial.reset(valueTreeState, "sizeFactor");
        sizeFactorDial.setLabelText(juce::String::fromUTF8("Size Factor"));
        addAndMakeVisible(bulgeDial);
        bulgeDial.reset(valueTreeState, "bulge");
        bulgeDial.setLabelText(juce::String::fromUTF8("Bulge"));
        addAndMakeVisible(decayDial);
        decayDial.reset(valueTreeState, "decay");
        decayDial.setLabelText(juce::String::fromUTF8("Decay"));
        addAndMakeVisible(lowPassDial);
        lowPassDial.reset(valueTreeState, "lowPass");
        lowPassDial.setLabelText(juce::String::fromUTF8("Low pass"));
        addAndMakeVisible(highPassDial);
        highPassDial.reset(valueTreeState, "highPass");
        highPassDial.setLabelText(juce::String::fromUTF8("High pass"));
        addAndMakeVisible(modulationDepthDial);
        modulationDepthDial.reset(valueTreeState, "modulationDepth");
        modulationDepthDial.setLabelText(juce::String::fromUTF8("Mod depth"));
        addAndMakeVisible(modulationSpeedDial);
        modulationSpeedDial.reset(valueTreeState, "modulationSpeed");
        modulationSpeedDial.setLabelText(juce::String::fromUTF8("Mod speed"));
        addAndMakeVisible(pitchStrengthDial);
        pitchStrengthDial.reset(valueTreeState, "pitchStrength");
        pitchStrengthDial.setLabelText(juce::String::fromUTF8("Pitch Strength"));
        addAndMakeVisible(pitch1InplaceDial);
        pitch1InplaceDial.reset(valueTreeState, "pitch1Inplace");
        pitch1InplaceDial.setLabelText(juce::String::fromUTF8("Pitch 1 inplace"));
        addAndMakeVisible(pitch2InplaceDial);
        pitch2InplaceDial.reset(valueTreeState, "pitch2Inplace");
        pitch2InplaceDial.setLabelText(juce::String::fromUTF8("Pitch 2 inplace"));
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

    CustomRotaryDial dryDial{this};
    CustomRotaryDial wetDial{this};
    CustomRotaryDial baseSizeDial{this};
    CustomRotaryDial sizeFactorDial{this};
    CustomRotaryDial bulgeDial{this};
    CustomRotaryDial decayDial{this};
    CustomRotaryDial lowPassDial{this};
    CustomRotaryDial highPassDial{this};
    CustomRotaryDial modulationDepthDial{this};
    CustomRotaryDial modulationSpeedDial{this};
    CustomRotaryDial pitchStrengthDial{this};
    CustomRotaryDial pitch1InplaceDial{this};
    CustomRotaryDial pitch2InplaceDial{this};
    CpuGauge cpuGauge{};
    Gauge levelGauge{};
    SpectrogramDisplay spectrogramGauge{};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioPluginAudioProcessorEditor)
};
