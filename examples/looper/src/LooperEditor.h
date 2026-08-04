#pragma once
/*
 * AUTO GENERATED,
 * NOT A GOOD IDEA TO CHANGE STUFF HERE
 * Keep the file readonly
 */

#include <map>

#include "LooperProcessor.h"
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
            std::vector<juce::Rectangle<int>> areas(2);
            const auto colWidth = area.getWidth() / 4;
            areas[0] = area.removeFromLeft(colWidth * 3).reduced(Constants::Margins::small);
            areas[1] = area.reduced(Constants::Margins::small);

            {
                juce::FlexBox box;
                box.flexWrap = juce::FlexBox::Wrap::noWrap;
                box.flexDirection = juce::FlexBox::Direction::column;
                box.justifyContent = juce::FlexBox::JustifyContent::spaceAround;
                box.items.add(juce::FlexItem(beatGauge).withFlex(1).withMargin(knobMarginSmall));
                box.performLayout(areas[0].toFloat());
            }
            {
                juce::FlexBox box;
                box.flexWrap = juce::FlexBox::Wrap::noWrap;
                box.flexDirection = juce::FlexBox::Direction::column;
                box.justifyContent = juce::FlexBox::JustifyContent::spaceAround;
                box.items.add(juce::FlexItem(recordSwitch)
                                  .withFlex(0)
                                  .withHeight(Constants::Text::labelHeight)
                                  .withAlignSelf(juce::FlexItem::AlignSelf::stretch)
                                  .withMargin(knobMarginSmall));
                box.items.add(juce::FlexItem(playSwitch)
                                  .withFlex(0)
                                  .withHeight(Constants::Text::labelHeight)
                                  .withAlignSelf(juce::FlexItem::AlignSelf::stretch)
                                  .withMargin(knobMarginSmall));
                box.items.add(juce::FlexItem(clearSwitch)
                                  .withFlex(0)
                                  .withHeight(Constants::Text::labelHeight)
                                  .withAlignSelf(juce::FlexItem::AlignSelf::stretch)
                                  .withMargin(knobMarginSmall));
                box.items.add(juce::FlexItem(clickVolumeDial).withFlex(1).withMargin(knobMarginSmall));
                box.items.add(juce::FlexItem(loopVolumeDial).withFlex(1).withMargin(knobMarginSmall));
                box.items.add(juce::FlexItem(overdubSwitch)
                                  .withFlex(0)
                                  .withHeight(Constants::Text::labelHeight)
                                  .withAlignSelf(juce::FlexItem::AlignSelf::stretch)
                                  .withMargin(knobMarginSmall));
                box.items.add(juce::FlexItem(undoSwitch)
                                  .withFlex(0)
                                  .withHeight(Constants::Text::labelHeight)
                                  .withAlignSelf(juce::FlexItem::AlignSelf::stretch)
                                  .withMargin(knobMarginSmall));
                box.items.add(juce::FlexItem(mixDownSwitch)
                                  .withFlex(0)
                                  .withHeight(Constants::Text::labelHeight)
                                  .withAlignSelf(juce::FlexItem::AlignSelf::stretch)
                                  .withMargin(knobMarginSmall));
                box.performLayout(areas[1].toFloat());
            }
        }
        else
        {
            // auto generated
            // const juce::FlexItem::Margin knobMargin = juce::FlexItem::Margin(Constants::Margins::small);
            const juce::FlexItem::Margin knobMarginSmall = juce::FlexItem::Margin(Constants::Margins::medium);
            std::vector<juce::Rectangle<int>> areas(4);
            const auto colWidth = area.getWidth() / 4;
            areas[0] = area.removeFromLeft(colWidth * 1).reduced(Constants::Margins::small);
            areas[1] = area.removeFromLeft(colWidth * 1).reduced(Constants::Margins::small);
            areas[2] = area.removeFromLeft(colWidth * 1).reduced(Constants::Margins::small);
            areas[3] = area.reduced(Constants::Margins::small);

            {
                juce::FlexBox box;
                box.flexWrap = juce::FlexBox::Wrap::noWrap;
                box.flexDirection = juce::FlexBox::Direction::column;
                box.justifyContent = juce::FlexBox::JustifyContent::spaceAround;
                box.items.add(juce::FlexItem(hostSyncSwitch)
                                  .withFlex(0)
                                  .withHeight(Constants::Text::labelHeight)
                                  .withAlignSelf(juce::FlexItem::AlignSelf::stretch)
                                  .withMargin(knobMarginSmall));
                box.items.add(juce::FlexItem(timeSignatureDrop)
                                  .withFlex(0)
                                  .withHeight(Constants::Text::labelHeight)
                                  .withAlignSelf(juce::FlexItem::AlignSelf::stretch)
                                  .withMargin(knobMarginSmall));
                box.items.add(juce::FlexItem(countInBarsDrop)
                                  .withFlex(0)
                                  .withHeight(Constants::Text::labelHeight)
                                  .withAlignSelf(juce::FlexItem::AlignSelf::stretch)
                                  .withMargin(knobMarginSmall));
                box.items.add(juce::FlexItem(bpmDial).withFlex(1).withMargin(knobMarginSmall));
                box.items.add(juce::FlexItem(sliceDivisionDrop)
                                  .withFlex(0)
                                  .withHeight(Constants::Text::labelHeight)
                                  .withAlignSelf(juce::FlexItem::AlignSelf::stretch)
                                  .withMargin(knobMarginSmall));
                box.items.add(juce::FlexItem(recordBarsDial).withFlex(1).withMargin(knobMarginSmall));
                box.items.add(juce::FlexItem(autoStopSwitch)
                                  .withFlex(0)
                                  .withHeight(Constants::Text::labelHeight)
                                  .withAlignSelf(juce::FlexItem::AlignSelf::stretch)
                                  .withMargin(knobMarginSmall));
                box.performLayout(areas[0].toFloat());
            }
            {
                juce::FlexBox box;
                box.flexWrap = juce::FlexBox::Wrap::noWrap;
                box.flexDirection = juce::FlexBox::Direction::column;
                box.justifyContent = juce::FlexBox::JustifyContent::spaceAround;
                box.items.add(juce::FlexItem(recordSwitch)
                                  .withFlex(0)
                                  .withHeight(Constants::Text::labelHeight)
                                  .withAlignSelf(juce::FlexItem::AlignSelf::stretch)
                                  .withMargin(knobMarginSmall));
                box.items.add(juce::FlexItem(threshRecSwitch)
                                  .withFlex(0)
                                  .withHeight(Constants::Text::labelHeight)
                                  .withAlignSelf(juce::FlexItem::AlignSelf::stretch)
                                  .withMargin(knobMarginSmall));
                box.items.add(juce::FlexItem(recThresholdDial).withFlex(1).withMargin(knobMarginSmall));
                box.items.add(juce::FlexItem(clickVolumeDial).withFlex(1).withMargin(knobMarginSmall));
                box.items.add(juce::FlexItem(clickRecordVolumeDial).withFlex(1).withMargin(knobMarginSmall));
                box.performLayout(areas[1].toFloat());
            }
            {
                juce::FlexBox box;
                box.flexWrap = juce::FlexBox::Wrap::noWrap;
                box.flexDirection = juce::FlexBox::Direction::column;
                box.justifyContent = juce::FlexBox::JustifyContent::spaceAround;
                box.items.add(juce::FlexItem(playSwitch)
                                  .withFlex(0)
                                  .withHeight(Constants::Text::labelHeight)
                                  .withAlignSelf(juce::FlexItem::AlignSelf::stretch)
                                  .withMargin(knobMarginSmall));
                box.items.add(juce::FlexItem(clearSwitch)
                                  .withFlex(0)
                                  .withHeight(Constants::Text::labelHeight)
                                  .withAlignSelf(juce::FlexItem::AlignSelf::stretch)
                                  .withMargin(knobMarginSmall));
                box.items.add(juce::FlexItem(loopVolumeDial).withFlex(1).withMargin(knobMarginSmall));
                box.items.add(juce::FlexItem(freezeSwitch)
                                  .withFlex(0)
                                  .withHeight(Constants::Text::labelHeight)
                                  .withAlignSelf(juce::FlexItem::AlignSelf::stretch)
                                  .withMargin(knobMarginSmall));
                box.performLayout(areas[2].toFloat());
            }
            {
                juce::FlexBox box;
                box.flexWrap = juce::FlexBox::Wrap::noWrap;
                box.flexDirection = juce::FlexBox::Direction::column;
                box.justifyContent = juce::FlexBox::JustifyContent::spaceAround;
                box.items.add(juce::FlexItem(divoLabel)
                                  .withFlex(0)
                                  .withHeight(Constants::Text::labelHeight)
                                  .withAlignSelf(juce::FlexItem::AlignSelf::stretch)
                                  .withMargin(knobMarginSmall));
                box.items.add(juce::FlexItem(overdubSwitch)
                                  .withFlex(0)
                                  .withHeight(Constants::Text::labelHeight)
                                  .withAlignSelf(juce::FlexItem::AlignSelf::stretch)
                                  .withMargin(knobMarginSmall));
                box.items.add(juce::FlexItem(undoSwitch)
                                  .withFlex(0)
                                  .withHeight(Constants::Text::labelHeight)
                                  .withAlignSelf(juce::FlexItem::AlignSelf::stretch)
                                  .withMargin(knobMarginSmall));
                box.items.add(juce::FlexItem(mixDownSwitch)
                                  .withFlex(0)
                                  .withHeight(Constants::Text::labelHeight)
                                  .withAlignSelf(juce::FlexItem::AlignSelf::stretch)
                                  .withMargin(knobMarginSmall));
                box.items.add(juce::FlexItem(divsLabel)
                                  .withFlex(0)
                                  .withHeight(Constants::Text::labelHeight)
                                  .withAlignSelf(juce::FlexItem::AlignSelf::stretch)
                                  .withMargin(knobMarginSmall));
                box.items.add(juce::FlexItem(seqPlaySwitch)
                                  .withFlex(0)
                                  .withHeight(Constants::Text::labelHeight)
                                  .withAlignSelf(juce::FlexItem::AlignSelf::stretch)
                                  .withMargin(knobMarginSmall));
                box.items.add(juce::FlexItem(clearSeqSwitch)
                                  .withFlex(0)
                                  .withHeight(Constants::Text::labelHeight)
                                  .withAlignSelf(juce::FlexItem::AlignSelf::stretch)
                                  .withMargin(knobMarginSmall));
                box.performLayout(areas[3].toFloat());
            }
        }
    }
#pragma GCC diagnostic pop

    void timerCallback() override
    {
        if (processorRef.hasRunner())
        {
            beatGauge.update(processorRef.getWaveDataToShow());
            sliceGauge.update(processorRef.getWaveDataToShow());
            beatGauge.setSampleRate(static_cast<float>(processorRef.getSampleRate()));
            beatGauge.setSamplesPerBar(processorRef.getSamplesPerBar());
            beatGauge.setBarBeats(processorRef.getBarBeats());
            beatGauge.setBarPhase(processorRef.getBarPhase());
            beatGauge.setSubdivisionPositions(processorRef.getSubdivisionPositions());
            beatGauge.setLoopWaveform(processorRef.getLoopWaveform());
            beatGauge.setSliceBoundaries(processorRef.getSliceBoundaries());
            beatGauge.setPlayheadNormalized(processorRef.getPlayheadNormalized());
            beatGauge.setOuterRingBars(processorRef.getOuterRingBars());
            beatGauge.setBarFrameLengths(processorRef.getBarFrameLengths());
            beatGauge.setStateLabel(processorRef.getLooperStateLabel());
            beatGauge.setBarBeatLabel(processorRef.getBarBeatLabel());
            beatGauge.setSpectrogram(processorRef.getSpectrogramData());
            beatGauge.setRecordHeadFrames(processorRef.getSpectrogramHeadFrames());
            beatGauge.setSpectrogramWrapped(processorRef.isSpectrogramWrapped());
            beatGauge.setSpectrogramFedFrames(processorRef.getSpectrogramFedFrames());
            sliceGauge.setSampleRate(static_cast<float>(processorRef.getSampleRate()));
            sliceGauge.setSliceThumbnails(processorRef.getSequencerSliceThumbnails());
            sliceGauge.setSliceBoundaries(processorRef.getSequencerSliceBoundaries());
            sliceGauge.setPlayheadNormalized(processorRef.getSequencerPlayheadNormalized());
            sliceGauge.setStateLabel(processorRef.getSequencerLabel());
            recordSwitch.tickFlash();
            playSwitch.tickFlash();
            overdubSwitch.tickFlash();
            undoSwitch.tickFlash();
            mixDownSwitch.tickFlash();
            clearSwitch.tickFlash();
            freezeSwitch.tickFlash();
            seqPlaySwitch.tickFlash();
            clearSeqSwitch.tickFlash();
            playSwitch.setButtonText(processorRef.isPlaying() ? juce::String::fromUTF8("Stop")
                                                              : juce::String::fromUTF8("Play"));
            overdubSwitch.setButtonText(processorRef.isOverdubbing() ? juce::String::fromUTF8("Overdubbing")
                                                                     : juce::String::fromUTF8("Overdub"));
            undoSwitch.setButtonText(processorRef.hasOverdub() ? juce::String::fromUTF8("Undo Overdub")
                                                               : juce::String::fromUTF8("Undo"));
            seqPlaySwitch.setButtonText(processorRef.isSequencerPlaying() ? juce::String::fromUTF8("Seq Stop")
                                                                          : juce::String::fromUTF8("Seq Play"));

            recordSwitch.setButtonText(
                processorRef.isRecording()
                    ? juce::String::fromUTF8("Recording")
                    : (processorRef.isArmed() ? juce::String::fromUTF8("Armed") : juce::String::fromUTF8("Record")));
            freezeSwitch.setButtonText(
                processorRef.isFreezePending()
                    ? juce::String::fromUTF8("Freezing...")
                    : (juce::String::fromUTF8("Freeze (") + juce::String(processorRef.getFrozenTrackCount()) +
                       juce::String::fromUTF8(" trk/") + juce::String(processorRef.getFrozenSliceCount()) +
                       juce::String::fromUTF8(" sl)")));
            if (const auto saved = processorRef.consumeLastSavedLoopName(); saved.isNotEmpty())
            {
                m_statusBar.showMessage(juce::String::fromUTF8("Saved '") + saved + "'");
            }
            handleLoopLoadOutcome();
            beatGauge.setRemainingRecordLabel(processorRef.getRemainingRecordLabel());
            processorRef.consumeLastLearnedCc();
        }
    }

    void initWidgets()
    {
        addAndMakeVisible(divoLabel);
        divoLabel.setText(juce::String::fromUTF8("—— Overdub ——"), juce::dontSendNotification);
        addAndMakeVisible(divsLabel);
        divsLabel.setText(juce::String::fromUTF8("—— Sequencer ——"), juce::dontSendNotification);
        addAndMakeVisible(recordSwitch);
        recordSwitchAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
            valueTreeState, "record", recordSwitch);

        addAndMakeVisible(playSwitch);
        playSwitchAttachment =
            std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(valueTreeState, "play", playSwitch);

        addAndMakeVisible(overdubSwitch);
        overdubSwitchAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
            valueTreeState, "overdub", overdubSwitch);

        addAndMakeVisible(undoSwitch);
        undoSwitchAttachment =
            std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(valueTreeState, "undo", undoSwitch);

        addAndMakeVisible(mixDownSwitch);
        mixDownSwitchAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
            valueTreeState, "mixDown", mixDownSwitch);

        addAndMakeVisible(clearSwitch);
        clearSwitchAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
            valueTreeState, "clear", clearSwitch);

        addAndMakeVisible(threshRecSwitch);
        threshRecSwitchAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
            valueTreeState, "threshRec", threshRecSwitch);

        addAndMakeVisible(hostSyncSwitch);
        hostSyncSwitchAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
            valueTreeState, "hostSync", hostSyncSwitch);

        addAndMakeVisible(freeRecordSwitch);
        freeRecordSwitchAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
            valueTreeState, "freeRecord", freeRecordSwitch);

        addAndMakeVisible(countInBarsDrop);
        countInBarsDrop.addItemList(valueTreeState.getParameter("countInBars")->getAllValueStrings(), 1);
        countInBarsDropAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
            valueTreeState, "countInBars", countInBarsDrop);
        addAndMakeVisible(timeSignatureDrop);
        timeSignatureDrop.addItemList(valueTreeState.getParameter("timeSignature")->getAllValueStrings(), 1);
        timeSignatureDropAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
            valueTreeState, "timeSignature", timeSignatureDrop);
        addAndMakeVisible(autoStopSwitch);
        autoStopSwitchAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
            valueTreeState, "autoStop", autoStopSwitch);

        addAndMakeVisible(recordBarsDial);
        recordBarsDial.reset(valueTreeState, "recordBars");
        recordBarsDial.setLabelText(juce::String::fromUTF8("Record Bars"));
        addAndMakeVisible(sliceDivisionDrop);
        sliceDivisionDrop.addItemList(valueTreeState.getParameter("sliceDivision")->getAllValueStrings(), 1);
        sliceDivisionDropAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
            valueTreeState, "sliceDivision", sliceDivisionDrop);
        addAndMakeVisible(bpmDial);
        bpmDial.reset(valueTreeState, "bpm");
        bpmDial.setLabelText(juce::String::fromUTF8("BPM"));
        bpmDial.setCcMappable(true, {[this] { processorRef.beginCcLearn(CcTarget::bpm); },
                                     [this] { return processorRef.getCcRange(CcTarget::bpm); },
                                     [this](float lo, float hi) { processorRef.setCcRange(CcTarget::bpm, lo, hi); },
                                     [this] { processorRef.clearCcAssignment(CcTarget::bpm); },
                                     [this] { return processorRef.getCcController(CcTarget::bpm); }});
        addAndMakeVisible(clickVolumeDial);
        clickVolumeDial.reset(valueTreeState, "clickVolume");
        clickVolumeDial.setLabelText(juce::String::fromUTF8("Click Volume"));
        clickVolumeDial.setCcMappable(true, {[this] { processorRef.beginCcLearn(CcTarget::clickVolume); },
                                             [this] { return processorRef.getCcRange(CcTarget::clickVolume); },
                                             [this](float lo, float hi)
                                             { processorRef.setCcRange(CcTarget::clickVolume, lo, hi); },
                                             [this] { processorRef.clearCcAssignment(CcTarget::clickVolume); },
                                             [this] { return processorRef.getCcController(CcTarget::clickVolume); }});
        addAndMakeVisible(clickRecordVolumeDial);
        clickRecordVolumeDial.reset(valueTreeState, "clickRecordVolume");
        clickRecordVolumeDial.setLabelText(juce::String::fromUTF8("Click->Track"));
        clickRecordVolumeDial.setCcMappable(
            true, {[this] { processorRef.beginCcLearn(CcTarget::clickRecordVolume); },
                   [this] { return processorRef.getCcRange(CcTarget::clickRecordVolume); },
                   [this](float lo, float hi) { processorRef.setCcRange(CcTarget::clickRecordVolume, lo, hi); },
                   [this] { processorRef.clearCcAssignment(CcTarget::clickRecordVolume); },
                   [this] { return processorRef.getCcController(CcTarget::clickRecordVolume); }});
        addAndMakeVisible(loopVolumeDial);
        loopVolumeDial.reset(valueTreeState, "loopVolume");
        loopVolumeDial.setLabelText(juce::String::fromUTF8("Loop Volume"));
        loopVolumeDial.setCcMappable(true, {[this] { processorRef.beginCcLearn(CcTarget::loopVolume); },
                                            [this] { return processorRef.getCcRange(CcTarget::loopVolume); },
                                            [this](float lo, float hi)
                                            { processorRef.setCcRange(CcTarget::loopVolume, lo, hi); },
                                            [this] { processorRef.clearCcAssignment(CcTarget::loopVolume); },
                                            [this] { return processorRef.getCcController(CcTarget::loopVolume); }});
        addAndMakeVisible(recThresholdDial);
        recThresholdDial.reset(valueTreeState, "recThreshold");
        recThresholdDial.setLabelText(juce::String::fromUTF8("Rec Threshold"));
        recThresholdDial.setCcMappable(true, {[this] { processorRef.beginCcLearn(CcTarget::recThreshold); },
                                              [this] { return processorRef.getCcRange(CcTarget::recThreshold); },
                                              [this](float lo, float hi)
                                              { processorRef.setCcRange(CcTarget::recThreshold, lo, hi); },
                                              [this] { processorRef.clearCcAssignment(CcTarget::recThreshold); },
                                              [this] { return processorRef.getCcController(CcTarget::recThreshold); }});
        addAndMakeVisible(freezeSwitch);
        freezeSwitchAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
            valueTreeState, "freeze", freezeSwitch);

        addAndMakeVisible(seqPlaySwitch);
        seqPlaySwitchAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
            valueTreeState, "seqPlay", seqPlaySwitch);

        addAndMakeVisible(clearSeqSwitch);
        clearSeqSwitchAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
            valueTreeState, "clearSeq", clearSeqSwitch);

        addAndMakeVisible(beatGauge);
        beatGauge.setLabelText(juce::String::fromUTF8("Bar"));
        addAndMakeVisible(sliceGauge);
        sliceGauge.setLabelText(juce::String::fromUTF8("Sequencer"));

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
            divoLabel.setVisible(false);
            divsLabel.setVisible(false);
            recordSwitch.setVisible(true);
            playSwitch.setVisible(true);
            overdubSwitch.setVisible(true);
            undoSwitch.setVisible(true);
            mixDownSwitch.setVisible(true);
            clearSwitch.setVisible(true);
            threshRecSwitch.setVisible(false);
            hostSyncSwitch.setVisible(false);
            freeRecordSwitch.setVisible(false);
            countInBarsDrop.setVisible(false);
            timeSignatureDrop.setVisible(false);
            autoStopSwitch.setVisible(false);
            recordBarsDial.setVisible(false);
            sliceDivisionDrop.setVisible(false);
            bpmDial.setVisible(false);
            clickVolumeDial.setVisible(true);
            clickRecordVolumeDial.setVisible(false);
            loopVolumeDial.setVisible(true);
            recThresholdDial.setVisible(false);
            freezeSwitch.setVisible(false);
            seqPlaySwitch.setVisible(false);
            clearSeqSwitch.setVisible(false);
            beatGauge.setVisible(true);
            sliceGauge.setVisible(false);
        }
        else
        {
            divoLabel.setVisible(true);
            divsLabel.setVisible(true);
            recordSwitch.setVisible(true);
            playSwitch.setVisible(true);
            overdubSwitch.setVisible(true);
            undoSwitch.setVisible(true);
            mixDownSwitch.setVisible(true);
            clearSwitch.setVisible(true);
            threshRecSwitch.setVisible(true);
            hostSyncSwitch.setVisible(true);
            freeRecordSwitch.setVisible(false);
            countInBarsDrop.setVisible(true);
            timeSignatureDrop.setVisible(true);
            autoStopSwitch.setVisible(true);
            recordBarsDial.setVisible(true);
            sliceDivisionDrop.setVisible(true);
            bpmDial.setVisible(true);
            clickVolumeDial.setVisible(true);
            clickRecordVolumeDial.setVisible(true);
            loopVolumeDial.setVisible(true);
            recThresholdDial.setVisible(true);
            freezeSwitch.setVisible(true);
            seqPlaySwitch.setVisible(true);
            clearSeqSwitch.setVisible(true);
            beatGauge.setVisible(false);
            sliceGauge.setVisible(false);
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
        names.add("Loops");
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
        if (menuName == "Loops")
        {
            return buildLoopsMenu();
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

    void menuItemSelected(int menuItemID, int /*topLevelMenuIndex*/) override
    {
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
        handleLoopMenuSelection(menuItemID);
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
        beatGauge.updateColors();
        sliceGauge.updateColors();

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

    // A "/" in a patch name (e.g. "chorus/classic tri chorus") groups it under a folder
    // submenu; root-level patches stay directly in the returned menu.
    juce::PopupMenu buildGroupedPatchMenu(int idBase, const juce::String& tickedName = {})
    {
        juce::PopupMenu rootMenu;
        std::map<juce::String, juce::PopupMenu> folderMenus;
        for (size_t i = 0; i < m_patchMenuNames.size(); ++i)
        {
            const auto& fullName = m_patchMenuNames[i];
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
        m_patchNameDialog = std::make_unique<juce::AlertWindow>(
            "Save Patch", "Enter a name for this patch:", juce::MessageBoxIconType::NoIcon);
        m_patchNameDialog->addTextEditor("name", processorRef.getCurrentPatchName());
        m_patchNameDialog->addButton("Save", 1, juce::KeyPress(juce::KeyPress::returnKey));
        m_patchNameDialog->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));
        m_patchNameDialog->enterModalState(true,
                                           juce::ModalCallbackFunction::create(
                                               [this](int result)
                                               {
                                                   const auto name =
                                                       m_patchNameDialog->getTextEditorContents("name").trim();
                                                   m_patchNameDialog.reset();
                                                   if (result != 1 || name.isEmpty())
                                                   {
                                                       return;
                                                   }
                                                   if (processorRef.saveCurrentPatchAs(name))
                                                   {
                                                       m_statusBar.showMessage("Saved '" + name + "'");
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
        m_patchNameDialog = std::make_unique<juce::AlertWindow>(
            "Rename Patch", "Enter a new name for \"" + oldName + "\":", juce::MessageBoxIconType::NoIcon);
        m_patchNameDialog->addTextEditor("name", oldName);
        m_patchNameDialog->addButton("Rename", 1, juce::KeyPress(juce::KeyPress::returnKey));
        m_patchNameDialog->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));
        m_patchNameDialog->enterModalState(true,
                                           juce::ModalCallbackFunction::create(
                                               [this, oldName](int result)
                                               {
                                                   const auto newName =
                                                       m_patchNameDialog->getTextEditorContents("name").trim();
                                                   m_patchNameDialog.reset();
                                                   if (result != 1 || newName.isEmpty() || newName == oldName)
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

    juce::PopupMenu buildLoopsMenu()
    {
        m_loopMenuNames = processorRef.listLoopNames();
        const auto currentName = processorRef.getCurrentLoopName();

        juce::PopupMenu loadMenu;
        for (size_t i = 0; i < m_loopMenuNames.size(); ++i)
        {
            loadMenu.addItem(kLoopLoadIdBase + static_cast<int>(i), m_loopMenuNames[i], true,
                             m_loopMenuNames[i] == currentName);
        }

        juce::PopupMenu deleteMenu;
        for (size_t i = 0; i < m_loopMenuNames.size(); ++i)
        {
            deleteMenu.addItem(kLoopDeleteIdBase + static_cast<int>(i), m_loopMenuNames[i]);
        }

        juce::PopupMenu renameMenu;
        for (size_t i = 0; i < m_loopMenuNames.size(); ++i)
        {
            renameMenu.addItem(kLoopRenameIdBase + static_cast<int>(i), m_loopMenuNames[i]);
        }

        juce::PopupMenu loops;
        loops.addSubMenu("Load", loadMenu, !m_loopMenuNames.empty());
        loops.addItem(kLoopSaveAsId, "Save As...");
        loops.addSubMenu("Delete", deleteMenu, !m_loopMenuNames.empty());
        loops.addSubMenu("Rename", renameMenu, !m_loopMenuNames.empty());
        return loops;
    }

    void handleLoopMenuSelection(int menuItemID)
    {
        if (menuItemID == kLoopSaveAsId)
        {
            promptSaveLoopAs();
        }
        else if (menuItemID >= kLoopLoadIdBase &&
                 menuItemID < kLoopLoadIdBase + static_cast<int>(m_loopMenuNames.size()))
        {
            const auto& name = m_loopMenuNames[static_cast<size_t>(menuItemID - kLoopLoadIdBase)];
            processorRef.requestLoadLoop(name);
            m_statusBar.showMessage("Loading '" + name + "'...");
        }
        else if (menuItemID >= kLoopDeleteIdBase &&
                 menuItemID < kLoopDeleteIdBase + static_cast<int>(m_loopMenuNames.size()))
        {
            confirmAndDeleteLoop(m_loopMenuNames[static_cast<size_t>(menuItemID - kLoopDeleteIdBase)]);
        }
        else if (menuItemID >= kLoopRenameIdBase &&
                 menuItemID < kLoopRenameIdBase + static_cast<int>(m_loopMenuNames.size()))
        {
            promptRenameLoop(m_loopMenuNames[static_cast<size_t>(menuItemID - kLoopRenameIdBase)]);
        }
    }

    void promptSaveLoopAs()
    {
        m_loopNameDialog = std::make_unique<juce::AlertWindow>(
            "Save Loop", "Enter a name for this loop:", juce::MessageBoxIconType::NoIcon);
        m_loopNameDialog->addTextEditor("name", "");
        m_loopNameDialog->addButton("Save", 1, juce::KeyPress(juce::KeyPress::returnKey));
        m_loopNameDialog->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));
        m_loopNameDialog->enterModalState(true,
                                          juce::ModalCallbackFunction::create(
                                              [this](int result)
                                              {
                                                  const auto name =
                                                      m_loopNameDialog->getTextEditorContents("name").trim();
                                                  m_loopNameDialog.reset();
                                                  if (result != 1 || name.isEmpty())
                                                  {
                                                      return;
                                                  }
                                                  processorRef.saveLoopAs(name);
                                                  m_statusBar.showMessage("Saving '" + name + "'...");
                                              }),
                                          false);
        focusNameEditor(*m_loopNameDialog);
    }

    void promptRenameLoop(const juce::String& oldName)
    {
        m_loopNameDialog = std::make_unique<juce::AlertWindow>(
            "Rename Loop", "Enter a new name for \"" + oldName + "\":", juce::MessageBoxIconType::NoIcon);
        m_loopNameDialog->addTextEditor("name", oldName);
        m_loopNameDialog->addButton("Rename", 1, juce::KeyPress(juce::KeyPress::returnKey));
        m_loopNameDialog->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));
        m_loopNameDialog->enterModalState(true,
                                          juce::ModalCallbackFunction::create(
                                              [this, oldName](int result)
                                              {
                                                  const auto newName =
                                                      m_loopNameDialog->getTextEditorContents("name").trim();
                                                  m_loopNameDialog.reset();
                                                  if (result != 1 || newName.isEmpty() || newName == oldName)
                                                  {
                                                      return;
                                                  }
                                                  if (processorRef.renameLoop(oldName, newName))
                                                  {
                                                      m_statusBar.showMessage("Renamed to '" + newName + "'");
                                                  }
                                                  else
                                                  {
                                                      m_statusBar.showMessage("Rename failed");
                                                  }
                                              }),
                                          false);
        focusNameEditor(*m_loopNameDialog);
    }

    void confirmAndDeleteLoop(const juce::String& name)
    {
        juce::NativeMessageBox::showAsync(juce::MessageBoxOptions()
                                              .withIconType(juce::MessageBoxIconType::WarningIcon)
                                              .withTitle("Delete Loop")
                                              .withMessage("Delete loop \"" + name + "\"?")
                                              .withButton("Yes")
                                              .withButton("No"),
                                          [this, name](int result)
                                          {
                                              if (result != 0)
                                              {
                                                  return;
                                              }
                                              if (processorRef.deleteLoopNamed(name))
                                              {
                                                  m_statusBar.showMessage("Deleted '" + name + "'");
                                              }
                                              else
                                              {
                                                  m_statusBar.showMessage("Delete failed");
                                              }
                                          });
    }

    // Polled every timer tick (see extra_timer_callbacks); surfaces a BPM
    // conflict prompt or a status message once a background load finishes.
    void handleLoopLoadOutcome()
    {
        const auto outcome = processorRef.consumeLoopLoadOutcome();
        if (!outcome.attempted)
        {
            return;
        }
        if (!outcome.success)
        {
            m_statusBar.showMessage("Load failed");
        }
        else if (!outcome.hasConflict)
        {
            m_statusBar.showMessage("Loaded");
        }
        else
        {
            const auto wavBpm = outcome.wavBpm;
            const auto jsonBpm = outcome.jsonBpm;
            juce::NativeMessageBox::showAsync(
                juce::MessageBoxOptions()
                    .withIconType(juce::MessageBoxIconType::QuestionIcon)
                    .withTitle("Tempo Mismatch")
                    .withMessage("The saved tempo doesn't match the file's embedded tempo. Which one should be used?")
                    .withButton(juce::String::fromUTF8("File (") + juce::String(wavBpm, 1) + " BPM)")
                    .withButton(juce::String::fromUTF8("Saved (") + juce::String(jsonBpm, 1) + " BPM)"),
                [this, wavBpm, jsonBpm](int result)
                {
                    processorRef.resolveLoopLoadBpm(result == 0 ? wavBpm : jsonBpm);
                    m_statusBar.showMessage("Loaded");
                });
        }
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
    static constexpr int kPatchSaveId = 1000;
    static constexpr int kPatchSaveAsId = 1001;
    static constexpr int kPatchLoadIdBase = 2000;
    static constexpr int kPatchDeleteIdBase = 3000;
    static constexpr int kPatchRenameIdBase = 4000;
    std::unique_ptr<juce::AlertWindow> m_patchNameDialog;
    std::vector<juce::String> m_patchMenuNames;

    static constexpr int kLoopSaveAsId = 5000;
    static constexpr int kLoopLoadIdBase = 6000;
    static constexpr int kLoopDeleteIdBase = 7000;
    static constexpr int kLoopRenameIdBase = 8000;
    std::unique_ptr<juce::AlertWindow> m_loopNameDialog;
    std::vector<juce::String> m_loopMenuNames;

    juce::Label divoLabel{};
    juce::Label divsLabel{};
    MomentaryToggleButton recordSwitch{juce::String::fromUTF8("Record")};
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> recordSwitchAttachment;
    MomentaryToggleButton playSwitch{juce::String::fromUTF8("Play")};
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> playSwitchAttachment;
    MomentaryToggleButton overdubSwitch{juce::String::fromUTF8("Overdub")};
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> overdubSwitchAttachment;
    MomentaryToggleButton undoSwitch{juce::String::fromUTF8("Undo")};
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> undoSwitchAttachment;
    MomentaryToggleButton mixDownSwitch{juce::String::fromUTF8("Mix Down")};
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> mixDownSwitchAttachment;
    MomentaryToggleButton clearSwitch{juce::String::fromUTF8("Clear")};
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> clearSwitchAttachment;
    juce::ToggleButton threshRecSwitch{juce::String::fromUTF8("Thresh Rec")};
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> threshRecSwitchAttachment;
    juce::ToggleButton hostSyncSwitch{juce::String::fromUTF8("Host Sync")};
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> hostSyncSwitchAttachment;
    juce::ToggleButton freeRecordSwitch{juce::String::fromUTF8("Free Record")};
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> freeRecordSwitchAttachment;
    juce::ComboBox countInBarsDrop{};
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> countInBarsDropAttachment;
    juce::ComboBox timeSignatureDrop{};
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> timeSignatureDropAttachment;
    juce::ToggleButton autoStopSwitch{juce::String::fromUTF8("Auto Stop")};
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> autoStopSwitchAttachment;
    CustomRotaryDial recordBarsDial{this};
    juce::ComboBox sliceDivisionDrop{};
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> sliceDivisionDropAttachment;
    CustomRotaryDial bpmDial{this};
    CustomRotaryDial clickVolumeDial{this};
    CustomRotaryDial clickRecordVolumeDial{this};
    CustomRotaryDial loopVolumeDial{this};
    CustomRotaryDial recThresholdDial{this};
    MomentaryToggleButton freezeSwitch{juce::String::fromUTF8("Freeze")};
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> freezeSwitchAttachment;
    MomentaryToggleButton seqPlaySwitch{juce::String::fromUTF8("Seq Play")};
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> seqPlaySwitchAttachment;
    MomentaryToggleButton clearSeqSwitch{juce::String::fromUTF8("Clear Seq")};
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> clearSeqSwitchAttachment;
    CircularLoopDisplay beatGauge{};
    SliceWaveDisplay sliceGauge{};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioPluginAudioProcessorEditor)
};
