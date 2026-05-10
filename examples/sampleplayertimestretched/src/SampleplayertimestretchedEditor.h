#pragma once
/*
 * AUTO GENERATED,
 * NOT A GOOD IDEA TO CHANGE STUFF HERE
 * Keep the file readonly
 */

#include "SampleplayertimestretchedProcessor.h"

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
        std::vector<juce::Rectangle<int>> areas(2);
        const auto colWidth = area.getWidth() / 10;
        areas[0] = area.removeFromLeft(colWidth * 1).reduced(GuiConstants::instance().margins.small);
        areas[1] = area.reduced(GuiConstants::instance().margins.small);

        {
            juce::FlexBox box;
            box.flexWrap = juce::FlexBox::Wrap::noWrap;
            box.flexDirection = juce::FlexBox::Direction::column;
            box.justifyContent = juce::FlexBox::JustifyContent::spaceAround;
            box.items.add(juce::FlexItem(volDial).withFlex(1).withMargin(knobMarginSmall));
            box.items.add(juce::FlexItem(typeDrop)
                              .withWidth(GuiConstants::instance().text.labelWidth)
                              .withHeight(GuiConstants::instance().text.labelHeight)
                              .withMargin(knobMarginSmall));
            box.items.add(juce::FlexItem(subsetDrop)
                              .withWidth(GuiConstants::instance().text.labelWidth)
                              .withHeight(GuiConstants::instance().text.labelHeight)
                              .withMargin(knobMarginSmall));
            box.items.add(juce::FlexItem(positionDial).withFlex(1).withMargin(knobMarginSmall));
            box.items.add(juce::FlexItem(advanceDial).withFlex(1).withMargin(knobMarginSmall));
            box.items.add(juce::FlexItem(levelGauge).withHeight(100).withMargin(knobMarginSmall));
            box.items.add(juce::FlexItem(cpuGauge).withHeight(100).withMargin(knobMarginSmall));
            box.performLayout(areas[0].toFloat());
        }
        {
            juce::FlexBox box;
            box.flexWrap = juce::FlexBox::Wrap::noWrap;
            box.flexDirection = juce::FlexBox::Direction::column;
            box.justifyContent = juce::FlexBox::JustifyContent::spaceAround;
            box.items.add(juce::FlexItem(spectrogramGauge).withFlex(1).withMargin(knobMarginSmall));
            box.performLayout(areas[1].toFloat());
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
        addAndMakeVisible(volDial);
        volDial.reset(valueTreeState, "vol");
        volDial.setLabelText(juce::String::fromUTF8("Vol"));
        addAndMakeVisible(typeDrop);
        typeDrop.addItemList(valueTreeState.getParameter("type")->getAllValueStrings(), 1);
        typeDropAttachment =
            std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(valueTreeState, "type", typeDrop);
        addAndMakeVisible(subsetDrop);
        subsetDrop.addItemList(valueTreeState.getParameter("subset")->getAllValueStrings(), 1);
        subsetDropAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
            valueTreeState, "subset", subsetDrop);
        addAndMakeVisible(positionDial);
        positionDial.reset(valueTreeState, "position");
        positionDial.setLabelText(juce::String::fromUTF8("Position"));
        addAndMakeVisible(advanceDial);
        advanceDial.reset(valueTreeState, "advance");
        advanceDial.setLabelText(juce::String::fromUTF8("Advance"));
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

    CustomRotaryDial volDial{this};
    juce::ComboBox typeDrop{};
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> typeDropAttachment;
    juce::ComboBox subsetDrop{};
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> subsetDropAttachment;
    CustomRotaryDial positionDial{this};
    CustomRotaryDial advanceDial{this};
    CpuGauge cpuGauge{};
    Gauge levelGauge{};
    SpectrogramDisplay spectrogramGauge{};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioPluginAudioProcessorEditor)
};
