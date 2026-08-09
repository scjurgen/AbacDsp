#pragma once
/*
 * AUTO GENERATED,
 * NOT A GOOD IDEA TO CHANGE STUFF HERE
 * Keep the file readonly
 */

#include <map>

#include "MaxdiffuserProcessor.h"
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
            const auto rowHeight = area.getHeight() / 4;
            areas[0] = area.removeFromTop(rowHeight * 1).reduced(Constants::Margins::small);
            areas[1] = area.removeFromTop(rowHeight * 1).reduced(Constants::Margins::small);
            areas[2] = area.reduced(Constants::Margins::small);

            {
                juce::FlexBox box;
                box.flexWrap = juce::FlexBox::Wrap::noWrap;
                box.flexDirection = juce::FlexBox::Direction::row;
                box.justifyContent = juce::FlexBox::JustifyContent::spaceAround;
                box.items.add(juce::FlexItem(dryDial).withFlex(1).withMargin(knobMarginSmall));
                box.items.add(juce::FlexItem(wetDial).withFlex(1).withMargin(knobMarginSmall));
                box.items.add(juce::FlexItem(tapSpanDial).withFlex(1).withMargin(knobMarginSmall));
                box.items.add(juce::FlexItem(feedbackDial).withFlex(1).withMargin(knobMarginSmall));
                box.performLayout(areas[0].toFloat());
            }
            {
                juce::FlexBox box;
                box.flexWrap = juce::FlexBox::Wrap::noWrap;
                box.flexDirection = juce::FlexBox::Direction::row;
                box.justifyContent = juce::FlexBox::JustifyContent::spaceAround;
                box.items.add(juce::FlexItem(fdnMixDial).withFlex(1).withMargin(knobMarginSmall));
                box.items.add(juce::FlexItem(fdnSizeDial).withFlex(1).withMargin(knobMarginSmall));
                box.items.add(juce::FlexItem(fdnDecayDial).withFlex(1).withMargin(knobMarginSmall));
                box.performLayout(areas[1].toFloat());
            }
            {
                juce::FlexBox box;
                box.flexWrap = juce::FlexBox::Wrap::noWrap;
                box.flexDirection = juce::FlexBox::Direction::row;
                box.justifyContent = juce::FlexBox::JustifyContent::spaceAround;
                box.items.add(juce::FlexItem(binsBandsGauge).withFlex(1).withMargin(knobMarginSmall));
                box.performLayout(areas[2].toFloat());
            }
        }
        else
        {
            // auto generated
            // const juce::FlexItem::Margin knobMargin = juce::FlexItem::Margin(Constants::Margins::small);
            const juce::FlexItem::Margin knobMarginSmall = juce::FlexItem::Margin(Constants::Margins::medium);
            std::vector<juce::Rectangle<int>> areas(5);
            const auto rowHeight = area.getHeight() / 12;
            areas[0] = area.removeFromTop(rowHeight * 3).reduced(Constants::Margins::small);
            areas[1] = area.removeFromTop(rowHeight * 2).reduced(Constants::Margins::small);
            areas[2] = area.removeFromTop(rowHeight * 3).reduced(Constants::Margins::small);
            areas[3] = area.removeFromTop(rowHeight * 2).reduced(Constants::Margins::small);
            areas[4] = area.reduced(Constants::Margins::small);

            {
                juce::FlexBox box;
                box.flexWrap = juce::FlexBox::Wrap::noWrap;
                box.flexDirection = juce::FlexBox::Direction::row;
                box.justifyContent = juce::FlexBox::JustifyContent::spaceAround;
                box.items.add(juce::FlexItem(dryDial).withFlex(1).withMargin(knobMarginSmall));
                box.items.add(juce::FlexItem(wetDial).withFlex(1).withMargin(knobMarginSmall));
                box.items.add(juce::FlexItem(preDelayDial).withFlex(1).withMargin(knobMarginSmall));
                box.items.add(juce::FlexItem(driveDial).withFlex(1).withMargin(knobMarginSmall));
                box.items.add(juce::FlexItem(eqInLowDial).withFlex(1).withMargin(knobMarginSmall));
                box.items.add(juce::FlexItem(eqInMidDial).withFlex(1).withMargin(knobMarginSmall));
                box.items.add(juce::FlexItem(eqInHighDial).withFlex(1).withMargin(knobMarginSmall));
                box.items.add(juce::FlexItem(eqOutLowDial).withFlex(1).withMargin(knobMarginSmall));
                box.items.add(juce::FlexItem(eqOutMidDial).withFlex(1).withMargin(knobMarginSmall));
                box.items.add(juce::FlexItem(eqOutHighDial).withFlex(1).withMargin(knobMarginSmall));
                box.items.add(juce::FlexItem(levelDial).withFlex(1).withMargin(knobMarginSmall));
                box.performLayout(areas[0].toFloat());
            }
            {
                juce::FlexBox box;
                box.flexWrap = juce::FlexBox::Wrap::noWrap;
                box.flexDirection = juce::FlexBox::Direction::row;
                box.justifyContent = juce::FlexBox::JustifyContent::spaceAround;
                box.items.add(juce::FlexItem(mixDial).withFlex(1).withMargin(knobMarginSmall));
                box.items.add(juce::FlexItem(pitchDial).withFlex(1).withMargin(knobMarginSmall));
                box.items.add(juce::FlexItem(pitchDelayDial).withFlex(1).withMargin(knobMarginSmall));
                box.items.add(juce::FlexItem(pitch2Dial).withFlex(1).withMargin(knobMarginSmall));
                box.items.add(juce::FlexItem(pitch2DelayDial).withFlex(1).withMargin(knobMarginSmall));
                box.items.add(juce::FlexItem(pitchModeDrop)
                                  .withFlex(1)
                                  .withHeight(Constants::Text::labelHeight)
                                  .withAlignSelf(juce::FlexItem::AlignSelf::center)
                                  .withMargin(knobMarginSmall));
                box.items.add(juce::FlexItem(pitcherShelfLowDial).withFlex(1).withMargin(knobMarginSmall));
                box.items.add(juce::FlexItem(pitcherShelfHighDial).withFlex(1).withMargin(knobMarginSmall));
                box.performLayout(areas[1].toFloat());
            }
            {
                juce::FlexBox box;
                box.flexWrap = juce::FlexBox::Wrap::noWrap;
                box.flexDirection = juce::FlexBox::Direction::row;
                box.justifyContent = juce::FlexBox::JustifyContent::spaceAround;
                box.items.add(juce::FlexItem(elementsDial).withFlex(1).withMargin(knobMarginSmall));
                box.items.add(juce::FlexItem(tapSpanDial).withFlex(1).withMargin(knobMarginSmall));
                box.items.add(juce::FlexItem(feedbackDial).withFlex(1).withMargin(knobMarginSmall));
                box.items.add(juce::FlexItem(bulgeDial).withFlex(1).withMargin(knobMarginSmall));
                box.items.add(juce::FlexItem(bottomSizeDial).withFlex(1).withMargin(knobMarginSmall));
                box.items.add(juce::FlexItem(topSizeDial).withFlex(1).withMargin(knobMarginSmall));
                box.items.add(juce::FlexItem(sizeSpreadDial).withFlex(1).withMargin(knobMarginSmall));
                box.items.add(juce::FlexItem(modulationDepthDial).withFlex(1).withMargin(knobMarginSmall));
                box.items.add(juce::FlexItem(modulationSpeedDial).withFlex(1).withMargin(knobMarginSmall));
                box.items.add(juce::FlexItem(lowPassDial).withFlex(1).withMargin(knobMarginSmall));
                box.items.add(juce::FlexItem(extremeStereoTapSwitch)
                                  .withWidth(Constants::Text::labelWidth)
                                  .withHeight(Constants::Text::labelHeight)
                                  .withAlignSelf(juce::FlexItem::AlignSelf::center)
                                  .withMargin(knobMarginSmall));
                box.items.add(juce::FlexItem(wideDial).withFlex(1).withMargin(knobMarginSmall));
                box.performLayout(areas[2].toFloat());
            }
            {
                juce::FlexBox box;
                box.flexWrap = juce::FlexBox::Wrap::noWrap;
                box.flexDirection = juce::FlexBox::Direction::row;
                box.justifyContent = juce::FlexBox::JustifyContent::spaceAround;
                box.items.add(juce::FlexItem(fdnMixDial).withFlex(1).withMargin(knobMarginSmall));
                box.items.add(juce::FlexItem(fdnSizeDial).withFlex(1).withMargin(knobMarginSmall));
                box.items.add(juce::FlexItem(fdnDecayDial).withFlex(1).withMargin(knobMarginSmall));
                box.items.add(juce::FlexItem(reverbShelfLowDial).withFlex(1).withMargin(knobMarginSmall));
                box.items.add(juce::FlexItem(reverbShelfHighDial).withFlex(1).withMargin(knobMarginSmall));
                box.performLayout(areas[3].toFloat());
            }
            {
                juce::FlexBox box;
                box.flexWrap = juce::FlexBox::Wrap::noWrap;
                box.flexDirection = juce::FlexBox::Direction::row;
                box.justifyContent = juce::FlexBox::JustifyContent::spaceAround;
                box.items.add(juce::FlexItem(sizesGauge).withFlex(1).withMargin(knobMarginSmall));
                box.performLayout(areas[4].toFloat());
            }
        }
    }
#pragma GCC diagnostic pop

    void timerCallback() override
    {
        if (processorRef.hasRunner())
        {
            binsBandsGauge.update(processorRef.getProcessingBinBandLevels(),
                                  static_cast<size_t>(valueTreeState.getRawParameterValue("elements")->load()));
            sizesGauge.update(processorRef.getElementSizesInMeters(), processorRef.getProcessingBinBandLevels(),
                              static_cast<size_t>(valueTreeState.getRawParameterValue("elements")->load()));
            processorRef.consumeLastLearnedCc();
        }
    }

    void initWidgets()
    {
        addAndMakeVisible(dryDial);
        dryDial.reset(valueTreeState, "dry");
        dryDial.setLabelText(juce::String::fromUTF8("Dry"));
        dryDial.setCcMappable(true, {[this] { processorRef.beginCcLearn(CcTarget::dry); },
                                     [this] { return processorRef.getCcRange(CcTarget::dry); },
                                     [this](float lo, float hi) { processorRef.setCcRange(CcTarget::dry, lo, hi); },
                                     [this] { processorRef.clearCcAssignment(CcTarget::dry); },
                                     [this] { return processorRef.getCcController(CcTarget::dry); }});
        addAndMakeVisible(wetDial);
        wetDial.reset(valueTreeState, "wet");
        wetDial.setLabelText(juce::String::fromUTF8("Wet"));
        wetDial.setCcMappable(true, {[this] { processorRef.beginCcLearn(CcTarget::wet); },
                                     [this] { return processorRef.getCcRange(CcTarget::wet); },
                                     [this](float lo, float hi) { processorRef.setCcRange(CcTarget::wet, lo, hi); },
                                     [this] { processorRef.clearCcAssignment(CcTarget::wet); },
                                     [this] { return processorRef.getCcController(CcTarget::wet); }});
        addAndMakeVisible(preDelayDial);
        preDelayDial.reset(valueTreeState, "preDelay");
        preDelayDial.setLabelText(juce::String::fromUTF8("Pre Delay"));
        preDelayDial.setCcMappable(true,
                                   {[this] { processorRef.beginCcLearn(CcTarget::preDelay); },
                                    [this] { return processorRef.getCcRange(CcTarget::preDelay); },
                                    [this](float lo, float hi) { processorRef.setCcRange(CcTarget::preDelay, lo, hi); },
                                    [this] { processorRef.clearCcAssignment(CcTarget::preDelay); },
                                    [this] { return processorRef.getCcController(CcTarget::preDelay); }});
        addAndMakeVisible(elementsDial);
        elementsDial.reset(valueTreeState, "elements");
        elementsDial.setLabelText(juce::String::fromUTF8("Elements"));
        elementsDial.setCcMappable(true,
                                   {[this] { processorRef.beginCcLearn(CcTarget::elements); },
                                    [this] { return processorRef.getCcRange(CcTarget::elements); },
                                    [this](float lo, float hi) { processorRef.setCcRange(CcTarget::elements, lo, hi); },
                                    [this] { processorRef.clearCcAssignment(CcTarget::elements); },
                                    [this] { return processorRef.getCcController(CcTarget::elements); }});
        addAndMakeVisible(tapSpanDial);
        tapSpanDial.reset(valueTreeState, "tapSpan");
        tapSpanDial.setLabelText(juce::String::fromUTF8("Tap Span"));
        tapSpanDial.setCcMappable(true,
                                  {[this] { processorRef.beginCcLearn(CcTarget::tapSpan); },
                                   [this] { return processorRef.getCcRange(CcTarget::tapSpan); },
                                   [this](float lo, float hi) { processorRef.setCcRange(CcTarget::tapSpan, lo, hi); },
                                   [this] { processorRef.clearCcAssignment(CcTarget::tapSpan); },
                                   [this] { return processorRef.getCcController(CcTarget::tapSpan); }});
        addAndMakeVisible(feedbackDial);
        feedbackDial.reset(valueTreeState, "feedback");
        feedbackDial.setLabelText(juce::String::fromUTF8("Diffusion"));
        feedbackDial.setCcMappable(true,
                                   {[this] { processorRef.beginCcLearn(CcTarget::feedback); },
                                    [this] { return processorRef.getCcRange(CcTarget::feedback); },
                                    [this](float lo, float hi) { processorRef.setCcRange(CcTarget::feedback, lo, hi); },
                                    [this] { processorRef.clearCcAssignment(CcTarget::feedback); },
                                    [this] { return processorRef.getCcController(CcTarget::feedback); }});
        addAndMakeVisible(bulgeDial);
        bulgeDial.reset(valueTreeState, "bulge");
        bulgeDial.setLabelText(juce::String::fromUTF8("Bulge"));
        bulgeDial.setCcMappable(true, {[this] { processorRef.beginCcLearn(CcTarget::bulge); },
                                       [this] { return processorRef.getCcRange(CcTarget::bulge); },
                                       [this](float lo, float hi) { processorRef.setCcRange(CcTarget::bulge, lo, hi); },
                                       [this] { processorRef.clearCcAssignment(CcTarget::bulge); },
                                       [this] { return processorRef.getCcController(CcTarget::bulge); }});
        addAndMakeVisible(bottomSizeDial);
        bottomSizeDial.reset(valueTreeState, "bottomSize");
        bottomSizeDial.setLabelText(juce::String::fromUTF8("Early Size"));
        bottomSizeDial.setCcMappable(true, {[this] { processorRef.beginCcLearn(CcTarget::bottomSize); },
                                            [this] { return processorRef.getCcRange(CcTarget::bottomSize); },
                                            [this](float lo, float hi)
                                            { processorRef.setCcRange(CcTarget::bottomSize, lo, hi); },
                                            [this] { processorRef.clearCcAssignment(CcTarget::bottomSize); },
                                            [this] { return processorRef.getCcController(CcTarget::bottomSize); }});
        addAndMakeVisible(topSizeDial);
        topSizeDial.reset(valueTreeState, "topSize");
        topSizeDial.setLabelText(juce::String::fromUTF8("Late Size"));
        topSizeDial.setCcMappable(true,
                                  {[this] { processorRef.beginCcLearn(CcTarget::topSize); },
                                   [this] { return processorRef.getCcRange(CcTarget::topSize); },
                                   [this](float lo, float hi) { processorRef.setCcRange(CcTarget::topSize, lo, hi); },
                                   [this] { processorRef.clearCcAssignment(CcTarget::topSize); },
                                   [this] { return processorRef.getCcController(CcTarget::topSize); }});
        addAndMakeVisible(sizeSpreadDial);
        sizeSpreadDial.reset(valueTreeState, "sizeSpread");
        sizeSpreadDial.setLabelText(juce::String::fromUTF8("Size Spread"));
        sizeSpreadDial.setCcMappable(true, {[this] { processorRef.beginCcLearn(CcTarget::sizeSpread); },
                                            [this] { return processorRef.getCcRange(CcTarget::sizeSpread); },
                                            [this](float lo, float hi)
                                            { processorRef.setCcRange(CcTarget::sizeSpread, lo, hi); },
                                            [this] { processorRef.clearCcAssignment(CcTarget::sizeSpread); },
                                            [this] { return processorRef.getCcController(CcTarget::sizeSpread); }});
        addAndMakeVisible(modulationDepthDial);
        modulationDepthDial.reset(valueTreeState, "modulationDepth");
        modulationDepthDial.setLabelText(juce::String::fromUTF8("Mod Depth"));
        modulationDepthDial.setCcMappable(true, {[this] { processorRef.beginCcLearn(CcTarget::modulationDepth); },
                                                 [this] { return processorRef.getCcRange(CcTarget::modulationDepth); },
                                                 [this](float lo, float hi)
                                                 { processorRef.setCcRange(CcTarget::modulationDepth, lo, hi); }, [this]
                                                 { processorRef.clearCcAssignment(CcTarget::modulationDepth); }, [this]
                                                 { return processorRef.getCcController(CcTarget::modulationDepth); }});
        addAndMakeVisible(modulationSpeedDial);
        modulationSpeedDial.reset(valueTreeState, "modulationSpeed");
        modulationSpeedDial.setLabelText(juce::String::fromUTF8("Mod Speed"));
        modulationSpeedDial.setCcMappable(true, {[this] { processorRef.beginCcLearn(CcTarget::modulationSpeed); },
                                                 [this] { return processorRef.getCcRange(CcTarget::modulationSpeed); },
                                                 [this](float lo, float hi)
                                                 { processorRef.setCcRange(CcTarget::modulationSpeed, lo, hi); }, [this]
                                                 { processorRef.clearCcAssignment(CcTarget::modulationSpeed); }, [this]
                                                 { return processorRef.getCcController(CcTarget::modulationSpeed); }});
        addAndMakeVisible(lowPassDial);
        lowPassDial.reset(valueTreeState, "lowPass");
        lowPassDial.setLabelText(juce::String::fromUTF8("Low Pass"));
        lowPassDial.setCcMappable(true,
                                  {[this] { processorRef.beginCcLearn(CcTarget::lowPass); },
                                   [this] { return processorRef.getCcRange(CcTarget::lowPass); },
                                   [this](float lo, float hi) { processorRef.setCcRange(CcTarget::lowPass, lo, hi); },
                                   [this] { processorRef.clearCcAssignment(CcTarget::lowPass); },
                                   [this] { return processorRef.getCcController(CcTarget::lowPass); }});
        addAndMakeVisible(mixDial);
        mixDial.reset(valueTreeState, "mix");
        mixDial.setLabelText(juce::String::fromUTF8("Pitch Mix"));
        mixDial.setCcMappable(true, {[this] { processorRef.beginCcLearn(CcTarget::mix); },
                                     [this] { return processorRef.getCcRange(CcTarget::mix); },
                                     [this](float lo, float hi) { processorRef.setCcRange(CcTarget::mix, lo, hi); },
                                     [this] { processorRef.clearCcAssignment(CcTarget::mix); },
                                     [this] { return processorRef.getCcController(CcTarget::mix); }});
        addAndMakeVisible(pitchDial);
        pitchDial.reset(valueTreeState, "pitch");
        pitchDial.setLabelText(juce::String::fromUTF8("Pitch"));
        pitchDial.setCcMappable(true, {[this] { processorRef.beginCcLearn(CcTarget::pitch); },
                                       [this] { return processorRef.getCcRange(CcTarget::pitch); },
                                       [this](float lo, float hi) { processorRef.setCcRange(CcTarget::pitch, lo, hi); },
                                       [this] { processorRef.clearCcAssignment(CcTarget::pitch); },
                                       [this] { return processorRef.getCcController(CcTarget::pitch); }});
        addAndMakeVisible(pitchDelayDial);
        pitchDelayDial.reset(valueTreeState, "pitchDelay");
        pitchDelayDial.setLabelText(juce::String::fromUTF8("Pitch Delay"));
        pitchDelayDial.setCcMappable(true, {[this] { processorRef.beginCcLearn(CcTarget::pitchDelay); },
                                            [this] { return processorRef.getCcRange(CcTarget::pitchDelay); },
                                            [this](float lo, float hi)
                                            { processorRef.setCcRange(CcTarget::pitchDelay, lo, hi); },
                                            [this] { processorRef.clearCcAssignment(CcTarget::pitchDelay); },
                                            [this] { return processorRef.getCcController(CcTarget::pitchDelay); }});
        addAndMakeVisible(pitch2Dial);
        pitch2Dial.reset(valueTreeState, "pitch2");
        pitch2Dial.setLabelText(juce::String::fromUTF8("Pitch 2"));
        pitch2Dial.setCcMappable(true,
                                 {[this] { processorRef.beginCcLearn(CcTarget::pitch2); },
                                  [this] { return processorRef.getCcRange(CcTarget::pitch2); },
                                  [this](float lo, float hi) { processorRef.setCcRange(CcTarget::pitch2, lo, hi); },
                                  [this] { processorRef.clearCcAssignment(CcTarget::pitch2); },
                                  [this] { return processorRef.getCcController(CcTarget::pitch2); }});
        addAndMakeVisible(pitch2DelayDial);
        pitch2DelayDial.reset(valueTreeState, "pitch2Delay");
        pitch2DelayDial.setLabelText(juce::String::fromUTF8("Pitch 2 Delay"));
        pitch2DelayDial.setCcMappable(true, {[this] { processorRef.beginCcLearn(CcTarget::pitch2Delay); },
                                             [this] { return processorRef.getCcRange(CcTarget::pitch2Delay); },
                                             [this](float lo, float hi)
                                             { processorRef.setCcRange(CcTarget::pitch2Delay, lo, hi); },
                                             [this] { processorRef.clearCcAssignment(CcTarget::pitch2Delay); },
                                             [this] { return processorRef.getCcController(CcTarget::pitch2Delay); }});
        addAndMakeVisible(pitchModeDrop);
        pitchModeDrop.addItemList(valueTreeState.getParameter("pitchMode")->getAllValueStrings(), 1);
        pitchModeDropAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
            valueTreeState, "pitchMode", pitchModeDrop);
        addAndMakeVisible(fdnMixDial);
        fdnMixDial.reset(valueTreeState, "fdnMix");
        fdnMixDial.setLabelText(juce::String::fromUTF8("Reverb Mix"));
        fdnMixDial.setCcMappable(true,
                                 {[this] { processorRef.beginCcLearn(CcTarget::fdnMix); },
                                  [this] { return processorRef.getCcRange(CcTarget::fdnMix); },
                                  [this](float lo, float hi) { processorRef.setCcRange(CcTarget::fdnMix, lo, hi); },
                                  [this] { processorRef.clearCcAssignment(CcTarget::fdnMix); },
                                  [this] { return processorRef.getCcController(CcTarget::fdnMix); }});
        addAndMakeVisible(fdnSizeDial);
        fdnSizeDial.reset(valueTreeState, "fdnSize");
        fdnSizeDial.setLabelText(juce::String::fromUTF8("Reverb Size"));
        fdnSizeDial.setCcMappable(true,
                                  {[this] { processorRef.beginCcLearn(CcTarget::fdnSize); },
                                   [this] { return processorRef.getCcRange(CcTarget::fdnSize); },
                                   [this](float lo, float hi) { processorRef.setCcRange(CcTarget::fdnSize, lo, hi); },
                                   [this] { processorRef.clearCcAssignment(CcTarget::fdnSize); },
                                   [this] { return processorRef.getCcController(CcTarget::fdnSize); }});
        addAndMakeVisible(fdnDecayDial);
        fdnDecayDial.reset(valueTreeState, "fdnDecay");
        fdnDecayDial.setLabelText(juce::String::fromUTF8("Reverb Decay"));
        fdnDecayDial.setCcMappable(true,
                                   {[this] { processorRef.beginCcLearn(CcTarget::fdnDecay); },
                                    [this] { return processorRef.getCcRange(CcTarget::fdnDecay); },
                                    [this](float lo, float hi) { processorRef.setCcRange(CcTarget::fdnDecay, lo, hi); },
                                    [this] { processorRef.clearCcAssignment(CcTarget::fdnDecay); },
                                    [this] { return processorRef.getCcController(CcTarget::fdnDecay); }});
        addAndMakeVisible(driveDial);
        driveDial.reset(valueTreeState, "drive");
        driveDial.setLabelText(juce::String::fromUTF8("Drive"));
        driveDial.setCcMappable(true, {[this] { processorRef.beginCcLearn(CcTarget::drive); },
                                       [this] { return processorRef.getCcRange(CcTarget::drive); },
                                       [this](float lo, float hi) { processorRef.setCcRange(CcTarget::drive, lo, hi); },
                                       [this] { processorRef.clearCcAssignment(CcTarget::drive); },
                                       [this] { return processorRef.getCcController(CcTarget::drive); }});
        addAndMakeVisible(eqInLowDial);
        eqInLowDial.reset(valueTreeState, "eqInLow");
        eqInLowDial.setLabelText(juce::String::fromUTF8("EQ In Low"));
        eqInLowDial.setCcMappable(true,
                                  {[this] { processorRef.beginCcLearn(CcTarget::eqInLow); },
                                   [this] { return processorRef.getCcRange(CcTarget::eqInLow); },
                                   [this](float lo, float hi) { processorRef.setCcRange(CcTarget::eqInLow, lo, hi); },
                                   [this] { processorRef.clearCcAssignment(CcTarget::eqInLow); },
                                   [this] { return processorRef.getCcController(CcTarget::eqInLow); }});
        addAndMakeVisible(eqInMidDial);
        eqInMidDial.reset(valueTreeState, "eqInMid");
        eqInMidDial.setLabelText(juce::String::fromUTF8("EQ In Mid"));
        eqInMidDial.setCcMappable(true,
                                  {[this] { processorRef.beginCcLearn(CcTarget::eqInMid); },
                                   [this] { return processorRef.getCcRange(CcTarget::eqInMid); },
                                   [this](float lo, float hi) { processorRef.setCcRange(CcTarget::eqInMid, lo, hi); },
                                   [this] { processorRef.clearCcAssignment(CcTarget::eqInMid); },
                                   [this] { return processorRef.getCcController(CcTarget::eqInMid); }});
        addAndMakeVisible(eqInHighDial);
        eqInHighDial.reset(valueTreeState, "eqInHigh");
        eqInHighDial.setLabelText(juce::String::fromUTF8("EQ In High"));
        eqInHighDial.setCcMappable(true,
                                   {[this] { processorRef.beginCcLearn(CcTarget::eqInHigh); },
                                    [this] { return processorRef.getCcRange(CcTarget::eqInHigh); },
                                    [this](float lo, float hi) { processorRef.setCcRange(CcTarget::eqInHigh, lo, hi); },
                                    [this] { processorRef.clearCcAssignment(CcTarget::eqInHigh); },
                                    [this] { return processorRef.getCcController(CcTarget::eqInHigh); }});
        addAndMakeVisible(eqOutLowDial);
        eqOutLowDial.reset(valueTreeState, "eqOutLow");
        eqOutLowDial.setLabelText(juce::String::fromUTF8("EQ Out Low"));
        eqOutLowDial.setCcMappable(true,
                                   {[this] { processorRef.beginCcLearn(CcTarget::eqOutLow); },
                                    [this] { return processorRef.getCcRange(CcTarget::eqOutLow); },
                                    [this](float lo, float hi) { processorRef.setCcRange(CcTarget::eqOutLow, lo, hi); },
                                    [this] { processorRef.clearCcAssignment(CcTarget::eqOutLow); },
                                    [this] { return processorRef.getCcController(CcTarget::eqOutLow); }});
        addAndMakeVisible(eqOutMidDial);
        eqOutMidDial.reset(valueTreeState, "eqOutMid");
        eqOutMidDial.setLabelText(juce::String::fromUTF8("EQ Out Mid"));
        eqOutMidDial.setCcMappable(true,
                                   {[this] { processorRef.beginCcLearn(CcTarget::eqOutMid); },
                                    [this] { return processorRef.getCcRange(CcTarget::eqOutMid); },
                                    [this](float lo, float hi) { processorRef.setCcRange(CcTarget::eqOutMid, lo, hi); },
                                    [this] { processorRef.clearCcAssignment(CcTarget::eqOutMid); },
                                    [this] { return processorRef.getCcController(CcTarget::eqOutMid); }});
        addAndMakeVisible(eqOutHighDial);
        eqOutHighDial.reset(valueTreeState, "eqOutHigh");
        eqOutHighDial.setLabelText(juce::String::fromUTF8("EQ Out High"));
        eqOutHighDial.setCcMappable(true, {[this] { processorRef.beginCcLearn(CcTarget::eqOutHigh); },
                                           [this] { return processorRef.getCcRange(CcTarget::eqOutHigh); },
                                           [this](float lo, float hi)
                                           { processorRef.setCcRange(CcTarget::eqOutHigh, lo, hi); },
                                           [this] { processorRef.clearCcAssignment(CcTarget::eqOutHigh); },
                                           [this] { return processorRef.getCcController(CcTarget::eqOutHigh); }});
        addAndMakeVisible(levelDial);
        levelDial.reset(valueTreeState, "level");
        levelDial.setLabelText(juce::String::fromUTF8("Level"));
        levelDial.setCcMappable(true, {[this] { processorRef.beginCcLearn(CcTarget::level); },
                                       [this] { return processorRef.getCcRange(CcTarget::level); },
                                       [this](float lo, float hi) { processorRef.setCcRange(CcTarget::level, lo, hi); },
                                       [this] { processorRef.clearCcAssignment(CcTarget::level); },
                                       [this] { return processorRef.getCcController(CcTarget::level); }});
        addAndMakeVisible(pitcherShelfLowDial);
        pitcherShelfLowDial.reset(valueTreeState, "pitcherShelfLow");
        pitcherShelfLowDial.setLabelText(juce::String::fromUTF8("Pitcher Shelf Low"));
        pitcherShelfLowDial.setCcMappable(true, {[this] { processorRef.beginCcLearn(CcTarget::pitcherShelfLow); },
                                                 [this] { return processorRef.getCcRange(CcTarget::pitcherShelfLow); },
                                                 [this](float lo, float hi)
                                                 { processorRef.setCcRange(CcTarget::pitcherShelfLow, lo, hi); }, [this]
                                                 { processorRef.clearCcAssignment(CcTarget::pitcherShelfLow); }, [this]
                                                 { return processorRef.getCcController(CcTarget::pitcherShelfLow); }});
        addAndMakeVisible(pitcherShelfHighDial);
        pitcherShelfHighDial.reset(valueTreeState, "pitcherShelfHigh");
        pitcherShelfHighDial.setLabelText(juce::String::fromUTF8("Pitcher Shelf High"));
        pitcherShelfHighDial.setCcMappable(
            true, {[this] { processorRef.beginCcLearn(CcTarget::pitcherShelfHigh); },
                   [this] { return processorRef.getCcRange(CcTarget::pitcherShelfHigh); },
                   [this](float lo, float hi) { processorRef.setCcRange(CcTarget::pitcherShelfHigh, lo, hi); },
                   [this] { processorRef.clearCcAssignment(CcTarget::pitcherShelfHigh); },
                   [this] { return processorRef.getCcController(CcTarget::pitcherShelfHigh); }});
        addAndMakeVisible(extremeStereoTapSwitch);
        extremeStereoTapSwitchAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
            valueTreeState, "extremeStereoTap", extremeStereoTapSwitch);

        addAndMakeVisible(wideDial);
        wideDial.reset(valueTreeState, "wide");
        wideDial.setLabelText(juce::String::fromUTF8("Wide"));
        wideDial.setCcMappable(true, {[this] { processorRef.beginCcLearn(CcTarget::wide); },
                                      [this] { return processorRef.getCcRange(CcTarget::wide); },
                                      [this](float lo, float hi) { processorRef.setCcRange(CcTarget::wide, lo, hi); },
                                      [this] { processorRef.clearCcAssignment(CcTarget::wide); },
                                      [this] { return processorRef.getCcController(CcTarget::wide); }});
        addAndMakeVisible(reverbShelfLowDial);
        reverbShelfLowDial.reset(valueTreeState, "reverbShelfLow");
        reverbShelfLowDial.setLabelText(juce::String::fromUTF8("Reverb Shelf Low"));
        reverbShelfLowDial.setCcMappable(true, {[this] { processorRef.beginCcLearn(CcTarget::reverbShelfLow); },
                                                [this] { return processorRef.getCcRange(CcTarget::reverbShelfLow); },
                                                [this](float lo, float hi)
                                                { processorRef.setCcRange(CcTarget::reverbShelfLow, lo, hi); }, [this]
                                                { processorRef.clearCcAssignment(CcTarget::reverbShelfLow); }, [this]
                                                { return processorRef.getCcController(CcTarget::reverbShelfLow); }});
        addAndMakeVisible(reverbShelfHighDial);
        reverbShelfHighDial.reset(valueTreeState, "reverbShelfHigh");
        reverbShelfHighDial.setLabelText(juce::String::fromUTF8("Reverb Shelf High"));
        reverbShelfHighDial.setCcMappable(true, {[this] { processorRef.beginCcLearn(CcTarget::reverbShelfHigh); },
                                                 [this] { return processorRef.getCcRange(CcTarget::reverbShelfHigh); },
                                                 [this](float lo, float hi)
                                                 { processorRef.setCcRange(CcTarget::reverbShelfHigh, lo, hi); }, [this]
                                                 { processorRef.clearCcAssignment(CcTarget::reverbShelfHigh); }, [this]
                                                 { return processorRef.getCcController(CcTarget::reverbShelfHigh); }});
        addAndMakeVisible(binsBandsGauge);
        binsBandsGauge.setLabelText(juce::String::fromUTF8("Bands"));
        addAndMakeVisible(sizesGauge);
        sizesGauge.setLabelText(juce::String::fromUTF8("Sizes"));

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
            dryDial.setVisible(true);
            wetDial.setVisible(true);
            preDelayDial.setVisible(false);
            elementsDial.setVisible(false);
            tapSpanDial.setVisible(true);
            feedbackDial.setVisible(true);
            bulgeDial.setVisible(false);
            bottomSizeDial.setVisible(false);
            topSizeDial.setVisible(false);
            sizeSpreadDial.setVisible(false);
            modulationDepthDial.setVisible(false);
            modulationSpeedDial.setVisible(false);
            lowPassDial.setVisible(false);
            mixDial.setVisible(false);
            pitchDial.setVisible(false);
            pitchDelayDial.setVisible(false);
            pitch2Dial.setVisible(false);
            pitch2DelayDial.setVisible(false);
            pitchModeDrop.setVisible(false);
            fdnMixDial.setVisible(true);
            fdnSizeDial.setVisible(true);
            fdnDecayDial.setVisible(true);
            driveDial.setVisible(false);
            eqInLowDial.setVisible(false);
            eqInMidDial.setVisible(false);
            eqInHighDial.setVisible(false);
            eqOutLowDial.setVisible(false);
            eqOutMidDial.setVisible(false);
            eqOutHighDial.setVisible(false);
            levelDial.setVisible(false);
            pitcherShelfLowDial.setVisible(false);
            pitcherShelfHighDial.setVisible(false);
            extremeStereoTapSwitch.setVisible(false);
            wideDial.setVisible(false);
            reverbShelfLowDial.setVisible(false);
            reverbShelfHighDial.setVisible(false);
            binsBandsGauge.setVisible(true);
            sizesGauge.setVisible(false);
        }
        else
        {
            dryDial.setVisible(true);
            wetDial.setVisible(true);
            preDelayDial.setVisible(true);
            elementsDial.setVisible(true);
            tapSpanDial.setVisible(true);
            feedbackDial.setVisible(true);
            bulgeDial.setVisible(true);
            bottomSizeDial.setVisible(true);
            topSizeDial.setVisible(true);
            sizeSpreadDial.setVisible(true);
            modulationDepthDial.setVisible(true);
            modulationSpeedDial.setVisible(true);
            lowPassDial.setVisible(true);
            mixDial.setVisible(true);
            pitchDial.setVisible(true);
            pitchDelayDial.setVisible(true);
            pitch2Dial.setVisible(true);
            pitch2DelayDial.setVisible(true);
            pitchModeDrop.setVisible(true);
            fdnMixDial.setVisible(true);
            fdnSizeDial.setVisible(true);
            fdnDecayDial.setVisible(true);
            driveDial.setVisible(true);
            eqInLowDial.setVisible(true);
            eqInMidDial.setVisible(true);
            eqInHighDial.setVisible(true);
            eqOutLowDial.setVisible(true);
            eqOutMidDial.setVisible(true);
            eqOutHighDial.setVisible(true);
            levelDial.setVisible(true);
            pitcherShelfLowDial.setVisible(true);
            pitcherShelfHighDial.setVisible(true);
            extremeStereoTapSwitch.setVisible(true);
            wideDial.setVisible(true);
            reverbShelfLowDial.setVisible(true);
            reverbShelfHighDial.setVisible(true);
            binsBandsGauge.setVisible(false);
            sizesGauge.setVisible(true);
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
            "Diffuser delay chain: up to 100 modulated allpass delays in series with pre-delay, pitching, bulge size "
            "distribution and damping.\n\nPart of the AbacDsp project - core DSP library is MIT licensed.\n\nBuilt "
            "with JUCE, licensed under AGPLv3 (or a commercial JUCE licence).\n\nFull third-party license details: "
            "THIRD-PARTY-LICENSES.md in the AbacDsp repository.";

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
        binsBandsGauge.updateColors();
        sizesGauge.updateColors();

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
        m_patchNameDialog->addTextEditor("folder", folder, "Folder (optional):");
        m_patchNameDialog->addTextEditor("name", name, "Name:");
        m_patchNameDialog->addButton("Save", 1, juce::KeyPress(juce::KeyPress::returnKey));
        m_patchNameDialog->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));
        m_patchNameDialog->enterModalState(true,
                                           juce::ModalCallbackFunction::create(
                                               [this](int result)
                                               {
                                                   const auto folderText =
                                                       m_patchNameDialog->getTextEditorContents("folder").trim();
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
        m_patchNameDialog->addTextEditor("folder", folder, "Folder (optional):");
        m_patchNameDialog->addTextEditor("name", name, "Name:");
        m_patchNameDialog->addButton("Rename", 1, juce::KeyPress(juce::KeyPress::returnKey));
        m_patchNameDialog->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));
        m_patchNameDialog->enterModalState(true,
                                           juce::ModalCallbackFunction::create(
                                               [this, oldName](int result)
                                               {
                                                   const auto folderText =
                                                       m_patchNameDialog->getTextEditorContents("folder").trim();
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


    CustomRotaryDial dryDial{this};
    CustomRotaryDial wetDial{this};
    CustomRotaryDial preDelayDial{this};
    CustomRotaryDial elementsDial{this};
    CustomRotaryDial tapSpanDial{this};
    CustomRotaryDial feedbackDial{this};
    CustomRotaryDial bulgeDial{this};
    CustomRotaryDial bottomSizeDial{this};
    CustomRotaryDial topSizeDial{this};
    CustomRotaryDial sizeSpreadDial{this};
    CustomRotaryDial modulationDepthDial{this};
    CustomRotaryDial modulationSpeedDial{this};
    CustomRotaryDial lowPassDial{this};
    CustomRotaryDial mixDial{this};
    CustomRotaryDial pitchDial{this};
    CustomRotaryDial pitchDelayDial{this};
    CustomRotaryDial pitch2Dial{this};
    CustomRotaryDial pitch2DelayDial{this};
    juce::ComboBox pitchModeDrop{};
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> pitchModeDropAttachment;
    CustomRotaryDial fdnMixDial{this};
    CustomRotaryDial fdnSizeDial{this};
    CustomRotaryDial fdnDecayDial{this};
    CustomRotaryDial driveDial{this};
    CustomRotaryDial eqInLowDial{this};
    CustomRotaryDial eqInMidDial{this};
    CustomRotaryDial eqInHighDial{this};
    CustomRotaryDial eqOutLowDial{this};
    CustomRotaryDial eqOutMidDial{this};
    CustomRotaryDial eqOutHighDial{this};
    CustomRotaryDial levelDial{this};
    CustomRotaryDial pitcherShelfLowDial{this};
    CustomRotaryDial pitcherShelfHighDial{this};
    juce::ToggleButton extremeStereoTapSwitch{juce::String::fromUTF8("Extreme Stereo Tap")};
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> extremeStereoTapSwitchAttachment;
    CustomRotaryDial wideDial{this};
    CustomRotaryDial reverbShelfLowDial{this};
    CustomRotaryDial reverbShelfHighDial{this};
    ShowProcessingBinsBands binsBandsGauge{};
    ShowDiffuserSizes sizesGauge{};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioPluginAudioProcessorEditor)
};
