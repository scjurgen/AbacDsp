#pragma once
/*
 * AUTO GENERATED,
 * NOT A GOOD IDEA TO CHANGE STUFF HERE
 * Keep the file readonly
 */

#include "PlaingainProcessor.h"

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
        auto area = getLocalBounds().reduced(static_cast<int>(GuiConstants::instance().margins.big));

        // auto generated
        // const juce::FlexItem::Margin knobMargin = juce::FlexItem::Margin(GuiConstants::instance().margins.small);
        const juce::FlexItem::Margin knobMarginSmall = juce::FlexItem::Margin(GuiConstants::instance().margins.medium);
        std::vector<juce::Rectangle<int>> areas(3);
        const auto colWidth = area.getWidth() / 7;
        areas[0] = area.removeFromLeft(colWidth * 1).reduced(GuiConstants::instance().margins.small);
        areas[1] = area.removeFromLeft(colWidth * 1).reduced(GuiConstants::instance().margins.small);
        areas[2] = area.reduced(GuiConstants::instance().margins.small);

        {
            juce::FlexBox box;
            box.flexWrap = juce::FlexBox::Wrap::noWrap;
            box.flexDirection = juce::FlexBox::Direction::column;
            box.justifyContent = juce::FlexBox::JustifyContent::spaceAround;
            box.items.add(juce::FlexItem(subsetDrop)
                              .withWidth(GuiConstants::instance().text.labelWidth)
                              .withHeight(GuiConstants::instance().text.labelHeight)
                              .withMargin(knobMarginSmall));
            box.items.add(juce::FlexItem(latencyDial).withFlex(1).withMargin(knobMarginSmall));
            box.items.add(juce::FlexItem(levelGauge).withHeight(400).withMargin(knobMarginSmall));
            box.performLayout(areas[0].toFloat());
        }
        {
            juce::FlexBox box;
            box.flexWrap = juce::FlexBox::Wrap::noWrap;
            box.flexDirection = juce::FlexBox::Direction::column;
            box.justifyContent = juce::FlexBox::JustifyContent::spaceAround;
            box.items.add(juce::FlexItem(gainDial).withFlex(1).withMargin(knobMarginSmall));
            box.items.add(juce::FlexItem(lowShelvingDial).withFlex(1).withMargin(knobMarginSmall));
            box.items.add(juce::FlexItem(highShelvingDial).withFlex(1).withMargin(knobMarginSmall));
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
        levelGauge.update(processorRef.getInputDbLoad(), processorRef.getOutputDbLoad());
        spectrogramGauge.update(processorRef.getSpectrogram());
        signalGauge.update(processorRef.getWaveDataToShow());
    }

    void initWidgets()
    {
        addAndMakeVisible(subsetDrop);
        subsetDrop.addItemList(valueTreeState.getParameter("subset")->getAllValueStrings(), 1);
        subsetDropAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
            valueTreeState, "subset", subsetDrop);
        addAndMakeVisible(gainDial);
        gainDial.reset(valueTreeState, "gain");
        gainDial.setLabelText(juce::String::fromUTF8("Gain"));
        addAndMakeVisible(lowShelvingDial);
        lowShelvingDial.reset(valueTreeState, "lowShelving");
        lowShelvingDial.setLabelText(juce::String::fromUTF8("Low"));
        addAndMakeVisible(highShelvingDial);
        highShelvingDial.reset(valueTreeState, "highShelving");
        highShelvingDial.setLabelText(juce::String::fromUTF8("High"));
        addAndMakeVisible(latencyDial);
        latencyDial.reset(valueTreeState, "latency");
        latencyDial.setLabelText(juce::String::fromUTF8("Latency"));
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

    juce::ComboBox subsetDrop{};
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> subsetDropAttachment;
    CustomRotaryDial gainDial{this};
    CustomRotaryDial lowShelvingDial{this};
    CustomRotaryDial highShelvingDial{this};
    CustomRotaryDial latencyDial{this};
    Gauge levelGauge{};
    SpectrogramDisplay spectrogramGauge{};
    WaveformGauge signalGauge{};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioPluginAudioProcessorEditor)
};
