#pragma once
/*
 * AUTO GENERATED,
 * NOT A GOOD IDEA TO CHANGE STUFF HERE
 * Keep the file readonly
 */

#include "SamplePlayerProcessor.h"

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

        std::vector<juce::Rectangle<int>> areas(5);
        const auto colWidth = area.getWidth() / 10;
        const auto rowHeight = area.getHeight() / 6;
        areas[0] = area.removeFromLeft(colWidth * 1).reduced(Constants::Margins::small);
        auto keepArea = area;
        areas[1] = area.removeFromTop(rowHeight * 1).reduced(Constants::Margins::small);
        areas[2] = area.removeFromTop(rowHeight * 1).reduced(Constants::Margins::small);
        areas[3] = area.removeFromTop(rowHeight * 1).reduced(Constants::Margins::small);
        areas[4] = area.reduced(Constants::Margins::small);

        {
            juce::FlexBox box;
            box.flexWrap = juce::FlexBox::Wrap::noWrap;
            box.flexDirection = juce::FlexBox::Direction::column;
            box.justifyContent = juce::FlexBox::JustifyContent::spaceAround;
            box.items.add(juce::FlexItem(subsetDrop)
                              .withWidth(Constants::Text::labelWidth)
                              .withHeight(Constants::Text::labelHeight)
                              .withMargin(knobMarginSmall));
            box.items.add(juce::FlexItem(syncSwitch)
                              .withWidth(Constants::Text::labelWidth)
                              .withHeight(Constants::Text::labelHeight)
                              .withMargin(knobMarginSmall));
            box.items.add(juce::FlexItem(typeDrop)
                              .withWidth(Constants::Text::labelWidth)
                              .withHeight(Constants::Text::labelHeight)
                              .withMargin(knobMarginSmall));
            box.items.add(juce::FlexItem(soloDrop)
                              .withWidth(Constants::Text::labelWidth)
                              .withHeight(Constants::Text::labelHeight)
                              .withMargin(knobMarginSmall));
            box.items.add(juce::FlexItem(volDial).withFlex(1).withMargin(knobMarginSmall));
            box.items.add(juce::FlexItem(reverbLevelWetDial).withFlex(1).withMargin(knobMarginSmall));
            box.items.add(juce::FlexItem(reverbDecayDial).withFlex(1).withMargin(knobMarginSmall));
            box.items.add(juce::FlexItem(reverbShimmerDial).withFlex(1).withMargin(knobMarginSmall));
            box.items.add(juce::FlexItem(levelGauge).withHeight(100).withMargin(knobMarginSmall));
            box.items.add(juce::FlexItem(cpuGauge).withHeight(100).withMargin(knobMarginSmall));
            box.performLayout(areas[0].toFloat());
        }
        {
            juce::FlexBox box;
            box.flexWrap = juce::FlexBox::Wrap::noWrap;
            box.flexDirection = juce::FlexBox::Direction::row;
            box.justifyContent = juce::FlexBox::JustifyContent::spaceAround;
            box.items.add(juce::FlexItem(vol1Dial).withFlex(1).withMargin(knobMarginSmall));
            box.items.add(juce::FlexItem(vol2Dial).withFlex(1).withMargin(knobMarginSmall));
            box.items.add(juce::FlexItem(vol3Dial).withFlex(1).withMargin(knobMarginSmall));
            box.items.add(juce::FlexItem(vol4Dial).withFlex(1).withMargin(knobMarginSmall));
            box.items.add(juce::FlexItem(vol5Dial).withFlex(1).withMargin(knobMarginSmall));
            box.items.add(juce::FlexItem(vol6Dial).withFlex(1).withMargin(knobMarginSmall));
            box.items.add(juce::FlexItem(vol7Dial).withFlex(1).withMargin(knobMarginSmall));
            box.items.add(juce::FlexItem(vol8Dial).withFlex(1).withMargin(knobMarginSmall));
            box.items.add(juce::FlexItem(vol9Dial).withFlex(1).withMargin(knobMarginSmall));
            box.items.add(juce::FlexItem(vol10Dial).withFlex(1).withMargin(knobMarginSmall));
            box.performLayout(areas[1].toFloat());
        }
        {
            juce::FlexBox box;
            box.flexWrap = juce::FlexBox::Wrap::noWrap;
            box.flexDirection = juce::FlexBox::Direction::row;
            box.justifyContent = juce::FlexBox::JustifyContent::spaceAround;
            box.items.add(juce::FlexItem(pitch1Dial).withFlex(1).withMargin(knobMarginSmall));
            box.items.add(juce::FlexItem(pitch2Dial).withFlex(1).withMargin(knobMarginSmall));
            box.items.add(juce::FlexItem(pitch3Dial).withFlex(1).withMargin(knobMarginSmall));
            box.items.add(juce::FlexItem(pitch4Dial).withFlex(1).withMargin(knobMarginSmall));
            box.items.add(juce::FlexItem(pitch5Dial).withFlex(1).withMargin(knobMarginSmall));
            box.items.add(juce::FlexItem(pitch6Dial).withFlex(1).withMargin(knobMarginSmall));
            box.items.add(juce::FlexItem(pitch7Dial).withFlex(1).withMargin(knobMarginSmall));
            box.items.add(juce::FlexItem(pitch8Dial).withFlex(1).withMargin(knobMarginSmall));
            box.items.add(juce::FlexItem(pitch9Dial).withFlex(1).withMargin(knobMarginSmall));
            box.items.add(juce::FlexItem(pitch10Dial).withFlex(1).withMargin(knobMarginSmall));
            box.performLayout(areas[2].toFloat());
        }
        {
            juce::FlexBox box;
            box.flexWrap = juce::FlexBox::Wrap::noWrap;
            box.flexDirection = juce::FlexBox::Direction::row;
            box.justifyContent = juce::FlexBox::JustifyContent::spaceAround;
            box.items.add(juce::FlexItem(revFeed1Dial).withFlex(1).withMargin(knobMarginSmall));
            box.items.add(juce::FlexItem(revFeed2Dial).withFlex(1).withMargin(knobMarginSmall));
            box.items.add(juce::FlexItem(revFeed3Dial).withFlex(1).withMargin(knobMarginSmall));
            box.items.add(juce::FlexItem(revFeed4Dial).withFlex(1).withMargin(knobMarginSmall));
            box.items.add(juce::FlexItem(revFeed5Dial).withFlex(1).withMargin(knobMarginSmall));
            box.items.add(juce::FlexItem(revFeed6Dial).withFlex(1).withMargin(knobMarginSmall));
            box.items.add(juce::FlexItem(revFeed7Dial).withFlex(1).withMargin(knobMarginSmall));
            box.items.add(juce::FlexItem(revFeed8Dial).withFlex(1).withMargin(knobMarginSmall));
            box.items.add(juce::FlexItem(revFeed9Dial).withFlex(1).withMargin(knobMarginSmall));
            box.items.add(juce::FlexItem(revFeed10Dial).withFlex(1).withMargin(knobMarginSmall));
            box.performLayout(areas[3].toFloat());
        }
        {
            juce::FlexBox box;
            box.flexWrap = juce::FlexBox::Wrap::noWrap;
            box.flexDirection = juce::FlexBox::Direction::row;
            box.justifyContent = juce::FlexBox::JustifyContent::spaceAround;
            box.items.add(juce::FlexItem(spectrogramGauge).withFlex(1).withMargin(knobMarginSmall));
            box.performLayout(areas[4].toFloat());
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
        addAndMakeVisible(syncSwitch);
        syncSwitchAttachment =
            std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(valueTreeState, "sync", syncSwitch);

        addAndMakeVisible(typeDrop);
        typeDrop.addItemList(valueTreeState.getParameter("type")->getAllValueStrings(), 1);
        typeDropAttachment =
            std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(valueTreeState, "type", typeDrop);
        addAndMakeVisible(subsetDrop);
        subsetDrop.addItemList(valueTreeState.getParameter("subset")->getAllValueStrings(), 1);
        subsetDropAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
            valueTreeState, "subset", subsetDrop);
        addAndMakeVisible(soloDrop);
        soloDrop.addItemList(valueTreeState.getParameter("solo")->getAllValueStrings(), 1);
        soloDropAttachment =
            std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(valueTreeState, "solo", soloDrop);
        addAndMakeVisible(volDial);
        volDial.reset(valueTreeState, "vol");
        volDial.setLabelText(juce::String::fromUTF8("Vol"));
        addAndMakeVisible(reverbLevelWetDial);
        reverbLevelWetDial.reset(valueTreeState, "reverbLevelWet");
        reverbLevelWetDial.setLabelText(juce::String::fromUTF8("Reverb Wet"));
        addAndMakeVisible(reverbDecayDial);
        reverbDecayDial.reset(valueTreeState, "reverbDecay");
        reverbDecayDial.setLabelText(juce::String::fromUTF8("Rev Decay"));
        addAndMakeVisible(reverbShimmerDial);
        reverbShimmerDial.reset(valueTreeState, "reverbShimmer");
        reverbShimmerDial.setLabelText(juce::String::fromUTF8("Rev Shimmer"));
        addAndMakeVisible(cpuGauge);
        cpuGauge.setLabelText(juce::String::fromUTF8("CPU"));
        addAndMakeVisible(levelGauge);
        levelGauge.setLabelText(juce::String::fromUTF8("Level"));
        addAndMakeVisible(spectrogramGauge);
        spectrogramGauge.setLabelText(juce::String::fromUTF8("Spectrogram"));
        addAndMakeVisible(vol1Dial);
        vol1Dial.reset(valueTreeState, "vol1");
        vol1Dial.setLabelText(juce::String::fromUTF8("Vol 1"));
        addAndMakeVisible(vol2Dial);
        vol2Dial.reset(valueTreeState, "vol2");
        vol2Dial.setLabelText(juce::String::fromUTF8("Vol 2"));
        addAndMakeVisible(vol3Dial);
        vol3Dial.reset(valueTreeState, "vol3");
        vol3Dial.setLabelText(juce::String::fromUTF8("Vol 3"));
        addAndMakeVisible(vol4Dial);
        vol4Dial.reset(valueTreeState, "vol4");
        vol4Dial.setLabelText(juce::String::fromUTF8("Vol 4"));
        addAndMakeVisible(vol5Dial);
        vol5Dial.reset(valueTreeState, "vol5");
        vol5Dial.setLabelText(juce::String::fromUTF8("Vol 5"));
        addAndMakeVisible(vol6Dial);
        vol6Dial.reset(valueTreeState, "vol6");
        vol6Dial.setLabelText(juce::String::fromUTF8("Vol 6"));
        addAndMakeVisible(vol7Dial);
        vol7Dial.reset(valueTreeState, "vol7");
        vol7Dial.setLabelText(juce::String::fromUTF8("Vol 7"));
        addAndMakeVisible(vol8Dial);
        vol8Dial.reset(valueTreeState, "vol8");
        vol8Dial.setLabelText(juce::String::fromUTF8("Vol 8"));
        addAndMakeVisible(vol9Dial);
        vol9Dial.reset(valueTreeState, "vol9");
        vol9Dial.setLabelText(juce::String::fromUTF8("Vol 9"));
        addAndMakeVisible(vol10Dial);
        vol10Dial.reset(valueTreeState, "vol10");
        vol10Dial.setLabelText(juce::String::fromUTF8("Vol 10"));
        addAndMakeVisible(revFeed1Dial);
        revFeed1Dial.reset(valueTreeState, "revFeed1");
        revFeed1Dial.setLabelText(juce::String::fromUTF8("Rev 1"));
        addAndMakeVisible(revFeed2Dial);
        revFeed2Dial.reset(valueTreeState, "revFeed2");
        revFeed2Dial.setLabelText(juce::String::fromUTF8("Rev 2"));
        addAndMakeVisible(revFeed3Dial);
        revFeed3Dial.reset(valueTreeState, "revFeed3");
        revFeed3Dial.setLabelText(juce::String::fromUTF8("Rev 3"));
        addAndMakeVisible(revFeed4Dial);
        revFeed4Dial.reset(valueTreeState, "revFeed4");
        revFeed4Dial.setLabelText(juce::String::fromUTF8("Rev 4"));
        addAndMakeVisible(revFeed5Dial);
        revFeed5Dial.reset(valueTreeState, "revFeed5");
        revFeed5Dial.setLabelText(juce::String::fromUTF8("Rev 5"));
        addAndMakeVisible(revFeed6Dial);
        revFeed6Dial.reset(valueTreeState, "revFeed6");
        revFeed6Dial.setLabelText(juce::String::fromUTF8("Rev 6"));
        addAndMakeVisible(revFeed7Dial);
        revFeed7Dial.reset(valueTreeState, "revFeed7");
        revFeed7Dial.setLabelText(juce::String::fromUTF8("Rev 7"));
        addAndMakeVisible(revFeed8Dial);
        revFeed8Dial.reset(valueTreeState, "revFeed8");
        revFeed8Dial.setLabelText(juce::String::fromUTF8("Rev 8"));
        addAndMakeVisible(revFeed9Dial);
        revFeed9Dial.reset(valueTreeState, "revFeed9");
        revFeed9Dial.setLabelText(juce::String::fromUTF8("Rev 9"));
        addAndMakeVisible(revFeed10Dial);
        revFeed10Dial.reset(valueTreeState, "revFeed10");
        revFeed10Dial.setLabelText(juce::String::fromUTF8("Rev 10"));
        addAndMakeVisible(pitch1Dial);
        pitch1Dial.reset(valueTreeState, "pitch1");
        pitch1Dial.setLabelText(juce::String::fromUTF8("Pch 1"));
        addAndMakeVisible(pitch2Dial);
        pitch2Dial.reset(valueTreeState, "pitch2");
        pitch2Dial.setLabelText(juce::String::fromUTF8("Pch 2"));
        addAndMakeVisible(pitch3Dial);
        pitch3Dial.reset(valueTreeState, "pitch3");
        pitch3Dial.setLabelText(juce::String::fromUTF8("Pch 3"));
        addAndMakeVisible(pitch4Dial);
        pitch4Dial.reset(valueTreeState, "pitch4");
        pitch4Dial.setLabelText(juce::String::fromUTF8("Pch 4"));
        addAndMakeVisible(pitch5Dial);
        pitch5Dial.reset(valueTreeState, "pitch5");
        pitch5Dial.setLabelText(juce::String::fromUTF8("Pch 5"));
        addAndMakeVisible(pitch6Dial);
        pitch6Dial.reset(valueTreeState, "pitch6");
        pitch6Dial.setLabelText(juce::String::fromUTF8("Pch 6"));
        addAndMakeVisible(pitch7Dial);
        pitch7Dial.reset(valueTreeState, "pitch7");
        pitch7Dial.setLabelText(juce::String::fromUTF8("Pch 7"));
        addAndMakeVisible(pitch8Dial);
        pitch8Dial.reset(valueTreeState, "pitch8");
        pitch8Dial.setLabelText(juce::String::fromUTF8("Pch 8"));
        addAndMakeVisible(pitch9Dial);
        pitch9Dial.reset(valueTreeState, "pitch9");
        pitch9Dial.setLabelText(juce::String::fromUTF8("Pch 9"));
        addAndMakeVisible(pitch10Dial);
        pitch10Dial.reset(valueTreeState, "pitch10");
        pitch10Dial.setLabelText(juce::String::fromUTF8("Pch 10"));
    }

  private:
    AudioPluginAudioProcessor& processorRef;
    juce::AudioProcessorValueTreeState& valueTreeState;
    GuiLookAndFeel m_laf;
    juce::Colour backgroundApp;

    juce::ToggleButton syncSwitch{juce::String::fromUTF8("Sync")};
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> syncSwitchAttachment;
    juce::ComboBox typeDrop{};
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> typeDropAttachment;
    juce::ComboBox subsetDrop{};
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> subsetDropAttachment;
    juce::ComboBox soloDrop{};
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> soloDropAttachment;
    CustomRotaryDial volDial{this};
    CustomRotaryDial reverbLevelWetDial{this};
    CustomRotaryDial reverbDecayDial{this};
    CustomRotaryDial reverbShimmerDial{this};
    CpuGauge cpuGauge{};
    Gauge levelGauge{};
    SpectrogramDisplay spectrogramGauge{};
    CustomRotaryDial vol1Dial{this};
    CustomRotaryDial vol2Dial{this};
    CustomRotaryDial vol3Dial{this};
    CustomRotaryDial vol4Dial{this};
    CustomRotaryDial vol5Dial{this};
    CustomRotaryDial vol6Dial{this};
    CustomRotaryDial vol7Dial{this};
    CustomRotaryDial vol8Dial{this};
    CustomRotaryDial vol9Dial{this};
    CustomRotaryDial vol10Dial{this};
    CustomRotaryDial revFeed1Dial{this};
    CustomRotaryDial revFeed2Dial{this};
    CustomRotaryDial revFeed3Dial{this};
    CustomRotaryDial revFeed4Dial{this};
    CustomRotaryDial revFeed5Dial{this};
    CustomRotaryDial revFeed6Dial{this};
    CustomRotaryDial revFeed7Dial{this};
    CustomRotaryDial revFeed8Dial{this};
    CustomRotaryDial revFeed9Dial{this};
    CustomRotaryDial revFeed10Dial{this};
    CustomRotaryDial pitch1Dial{this};
    CustomRotaryDial pitch2Dial{this};
    CustomRotaryDial pitch3Dial{this};
    CustomRotaryDial pitch4Dial{this};
    CustomRotaryDial pitch5Dial{this};
    CustomRotaryDial pitch6Dial{this};
    CustomRotaryDial pitch7Dial{this};
    CustomRotaryDial pitch8Dial{this};
    CustomRotaryDial pitch9Dial{this};
    CustomRotaryDial pitch10Dial{this};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioPluginAudioProcessorEditor)
};
