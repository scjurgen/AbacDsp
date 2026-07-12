#pragma once
/*
 * AUTO GENERATED,
 * NOT A GOOD IDEA TO CHANGE STUFF HERE
 * Keep the file readonly
 */

#include "MetronomeProcessor.h"
#include "UiElements.h"

//==============================================================================
class AudioPluginAudioProcessorEditor : public juce::AudioProcessorEditor,
                                        juce::Timer,
                                        juce::MenuBarModel,
                                        juce::ComponentListener {
public:
  explicit AudioPluginAudioProcessorEditor(
      AudioPluginAudioProcessor &p, juce::AudioProcessorValueTreeState &vts)
      : AudioProcessorEditor(&p), processorRef(p), valueTreeState(vts),
        backgroundApp(juce::Colour(GuiConstants::instance().colors.background)),
        m_menuBar(this) {
    m_laf = std::make_unique<GuiLookAndFeel>();
    setLookAndFeel(m_laf.get());
    juce::LookAndFeel::setDefaultLookAndFeel(m_laf.get());
    addAndMakeVisible(m_menuBar);
    initWidgets();
    setResizable(true, true);
    setResizeLimits(GuiConstants::instance().init.WindowWidth,
                    GuiConstants::instance().init.WindowHeight, 4000, 3000);
    // Saved window bounds (position in particular) only make sense for the
    // Standalone app's own OS window. Applying a remembered on-screen X/Y to
    // a hosted plugin editor's top-level component can push its native peer
    // to coordinates outside any connected display, leaving the host with
    // an empty content area even though the editor itself constructed fine.
    if (processorRef.wrapperType ==
        juce::AudioProcessor::wrapperType_Standalone) {
      const auto saved = AppSettings::loadWindowBounds(
          GuiConstants::instance().init.WindowWidth,
          GuiConstants::instance().init.WindowHeight);
      setSize(saved.getWidth(), saved.getHeight());
    } else {
      setSize(GuiConstants::instance().init.WindowWidth,
              GuiConstants::instance().init.WindowHeight);
    }
    startTimerHz(GuiConstants::instance().init.TimerHertz);
  }

  ~AudioPluginAudioProcessorEditor() override {
    if (m_topLevel != nullptr) {
      m_topLevel->removeComponentListener(this);
    }
    stopTimer();
    juce::LookAndFeel::setDefaultLookAndFeel(nullptr);
    setLookAndFeel(nullptr);
  }

  void paint(juce::Graphics &g) override { g.fillAll(backgroundApp); }

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wfloat-conversion"
  void resized() override {
    auto area = getLocalBounds();
    m_menuBar.setBounds(
        area.removeFromTop(getLookAndFeel().getDefaultMenuBarHeight()));
    area = area.reduced(static_cast<int>(Constants::Margins::big));

    // auto generated
    // const juce::FlexItem::Margin knobMargin =
    // juce::FlexItem::Margin(Constants::Margins::small);
    const juce::FlexItem::Margin knobMarginSmall =
        juce::FlexItem::Margin(Constants::Margins::medium);
    std::vector<juce::Rectangle<int>> areas(2);
    const auto colWidth = area.getWidth() / 6;
    areas[0] =
        area.removeFromLeft(colWidth * 1).reduced(Constants::Margins::small);
    areas[1] = area.reduced(Constants::Margins::small);

    {
      juce::FlexBox box;
      box.flexWrap = juce::FlexBox::Wrap::noWrap;
      box.flexDirection = juce::FlexBox::Direction::column;
      box.justifyContent = juce::FlexBox::JustifyContent::spaceAround;
      box.items.add(juce::FlexItem(subsetDrop)
                        .withFlex(0)
                        .withHeight(Constants::Text::labelHeight)
                        .withAlignSelf(juce::FlexItem::AlignSelf::stretch)
                        .withMargin(knobMarginSmall));
      box.items.add(juce::FlexItem(presetDrop)
                        .withFlex(0)
                        .withHeight(Constants::Text::labelHeight)
                        .withAlignSelf(juce::FlexItem::AlignSelf::stretch)
                        .withMargin(knobMarginSmall));
      box.items.add(juce::FlexItem(dropBarsDrop)
                        .withFlex(0)
                        .withHeight(Constants::Text::labelHeight)
                        .withAlignSelf(juce::FlexItem::AlignSelf::stretch)
                        .withMargin(knobMarginSmall));
      if (swingRatioDial.isVisible()) {
        box.items.add(juce::FlexItem(swingRatioDial)
                          .withFlex(1)
                          .withMargin(knobMarginSmall));
      }
      box.items.add(juce::FlexItem(onOffSwitch)
                        .withFlex(0)
                        .withHeight(Constants::Text::labelHeight)
                        .withAlignSelf(juce::FlexItem::AlignSelf::stretch)
                        .withMargin(knobMarginSmall));
      box.items.add(juce::FlexItem(hostSyncSwitch)
                        .withFlex(0)
                        .withHeight(Constants::Text::labelHeight)
                        .withAlignSelf(juce::FlexItem::AlignSelf::stretch)
                        .withMargin(knobMarginSmall));
      box.items.add(
          juce::FlexItem(bpmDial).withFlex(1).withMargin(knobMarginSmall));
      box.items.add(juce::FlexItem(subVolumeDial)
                        .withFlex(1)
                        .withMargin(knobMarginSmall));
      box.items.add(juce::FlexItem(metroVolumeDial)
                        .withFlex(1)
                        .withMargin(knobMarginSmall));
      box.items.add(juce::FlexItem(inputVolumeDial)
                        .withFlex(1)
                        .withMargin(knobMarginSmall));
      box.performLayout(areas[0].toFloat());
    }
    {
      juce::FlexBox box;
      box.flexWrap = juce::FlexBox::Wrap::noWrap;
      box.flexDirection = juce::FlexBox::Direction::column;
      box.justifyContent = juce::FlexBox::JustifyContent::spaceAround;
      box.items.add(
          juce::FlexItem(signalGauge).withFlex(1).withMargin(knobMarginSmall));
      box.items.add(
          juce::FlexItem(irisGauge).withFlex(2).withMargin(knobMarginSmall));
      box.performLayout(areas[1].toFloat());
    }
  }
#pragma GCC diagnostic pop

  void timerCallback() override {
    if (processorRef.hasRunner()) {
      signalGauge.update(processorRef.getWaveDataToShow());

      {
        const float sr = static_cast<float>(processorRef.getSampleRate());
        const float bpm = processorRef.getCurrentClickBpm();
        const size_t spb = static_cast<size_t>(sr * 60.f / bpm);
        bpmDial.setEnabled(!processorRef.isHostSynced());
        if (processorRef.isHostSynced()) {
          bpmDial.setValue(bpm);
        }
        onOffSwitch.setEnabled(!processorRef.isHostSynced());
        signalGauge.setSampleRate(sr);
        signalGauge.setSamplesPerBeat(spb);
        signalGauge.setBeatIndex(processorRef.getWaveDataBeatIndex());
        signalGauge.setSubdivisionPositions(
            processorRef.getSubdivisionPositions());
        irisGauge.setSampleRate(sr);
        irisGauge.setSamplesPerBeat(spb);
        irisGauge.setBarBeats(processorRef.getBarBeats());
        irisGauge.setBarPhase(processorRef.getBarPhase());
        irisGauge.setSubdivisionPositions(
            processorRef.getSubdivisionPositions());
        irisGauge.update(processorRef.getInputSpectrogram());
      }
      processorRef.consumeLastLearnedCc();
    }
  }

  void initWidgets() {
    addAndMakeVisible(subsetDrop);
    subsetDrop.addItemList(
        valueTreeState.getParameter("subset")->getAllValueStrings(), 1);
    subsetDropAttachment = std::make_unique<
        juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        valueTreeState, "subset", subsetDrop);
    addAndMakeVisible(bpmDial);
    bpmDial.reset(valueTreeState, "bpm");
    bpmDial.setLabelText(juce::String::fromUTF8("BPM"));
    bpmDial.setCcMappable(
        true, {[this] { processorRef.beginCcLearn(CcTarget::bpm); },
               [this] { return processorRef.getCcRange(CcTarget::bpm); },
               [this](float lo, float hi) {
                 processorRef.setCcRange(CcTarget::bpm, lo, hi);
               },
               [this] { processorRef.clearCcAssignment(CcTarget::bpm); },
               [this] { return processorRef.getCcController(CcTarget::bpm); }});
    addAndMakeVisible(dropBarsDrop);
    dropBarsDrop.addItemList(
        valueTreeState.getParameter("dropBars")->getAllValueStrings(), 1);
    dropBarsDropAttachment = std::make_unique<
        juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        valueTreeState, "dropBars", dropBarsDrop);
    addAndMakeVisible(metroVolumeDial);
    metroVolumeDial.reset(valueTreeState, "metroVolume");
    metroVolumeDial.setLabelText(juce::String::fromUTF8("Metro Volume"));
    metroVolumeDial.setCcMappable(
        true,
        {[this] { processorRef.beginCcLearn(CcTarget::metroVolume); },
         [this] { return processorRef.getCcRange(CcTarget::metroVolume); },
         [this](float lo, float hi) {
           processorRef.setCcRange(CcTarget::metroVolume, lo, hi);
         },
         [this] { processorRef.clearCcAssignment(CcTarget::metroVolume); },
         [this] {
           return processorRef.getCcController(CcTarget::metroVolume);
         }});
    addAndMakeVisible(inputVolumeDial);
    inputVolumeDial.reset(valueTreeState, "inputVolume");
    inputVolumeDial.setLabelText(juce::String::fromUTF8("Input Volume"));
    inputVolumeDial.setCcMappable(
        true,
        {[this] { processorRef.beginCcLearn(CcTarget::inputVolume); },
         [this] { return processorRef.getCcRange(CcTarget::inputVolume); },
         [this](float lo, float hi) {
           processorRef.setCcRange(CcTarget::inputVolume, lo, hi);
         },
         [this] { processorRef.clearCcAssignment(CcTarget::inputVolume); },
         [this] {
           return processorRef.getCcController(CcTarget::inputVolume);
         }});
    addAndMakeVisible(subVolumeDial);
    subVolumeDial.reset(valueTreeState, "subVolume");
    subVolumeDial.setLabelText(juce::String::fromUTF8("Sub Volume"));
    subVolumeDial.setCcMappable(
        true,
        {[this] { processorRef.beginCcLearn(CcTarget::subVolume); },
         [this] { return processorRef.getCcRange(CcTarget::subVolume); },
         [this](float lo, float hi) {
           processorRef.setCcRange(CcTarget::subVolume, lo, hi);
         },
         [this] { processorRef.clearCcAssignment(CcTarget::subVolume); },
         [this] { return processorRef.getCcController(CcTarget::subVolume); }});
    addAndMakeVisible(onOffSwitch);
    onOffSwitchAttachment =
        std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
            valueTreeState, "onOff", onOffSwitch);

    addAndMakeVisible(hostSyncSwitch);
    hostSyncSwitchAttachment =
        std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
            valueTreeState, "hostSync", hostSyncSwitch);

    addAndMakeVisible(presetDrop);
    presetDrop.addItemList(
        valueTreeState.getParameter("preset")->getAllValueStrings(), 1);
    presetDropAttachment = std::make_unique<
        juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        valueTreeState, "preset", presetDrop);
    presetDrop.onChange = [this] { updateSwingRatioVisibility(); };
    updateSwingRatioVisibility();
    addChildComponent(swingRatioDial);
    swingRatioDial.reset(valueTreeState, "swingRatio");
    swingRatioDial.setLabelText(juce::String::fromUTF8("Swing"));
    swingRatioDial.setCcMappable(
        true, {[this] { processorRef.beginCcLearn(CcTarget::swingRatio); },
               [this] { return processorRef.getCcRange(CcTarget::swingRatio); },
               [this](float lo, float hi) {
                 processorRef.setCcRange(CcTarget::swingRatio, lo, hi);
               },
               [this] { processorRef.clearCcAssignment(CcTarget::swingRatio); },
               [this] {
                 return processorRef.getCcController(CcTarget::swingRatio);
               }});
    addAndMakeVisible(signalGauge);
    signalGauge.setLabelText(juce::String::fromUTF8("Beat"));
    addAndMakeVisible(irisGauge);
    irisGauge.setLabelText(juce::String::fromUTF8("Spectrum Iris"));
  }

  void parentHierarchyChanged() override {
    auto *top = getTopLevelComponent();
    if (top == this) {
      return;
    }

    if (m_topLevel != top) {
      if (m_topLevel != nullptr) {
        m_topLevel->removeComponentListener(this);
      }
      m_topLevel = top;
      m_topLevel->addComponentListener(this);
    }

    if (processorRef.wrapperType ==
            juce::AudioProcessor::wrapperType_Standalone &&
        !m_boundsRestored && m_topLevel->isOnDesktop()) {
      const auto saved = AppSettings::loadWindowBounds(getWidth(), getHeight());
      m_topLevel->setTopLeftPosition(saved.getX(), saved.getY());
      m_boundsRestored = true;
    }
  }

  void componentMovedOrResized(juce::Component &component, bool /*wasMoved*/,
                               bool /*wasResized*/) override {
    if (m_boundsRestored) {
      AppSettings::saveWindowBounds(component.getScreenBounds());
    }
  }

  juce::StringArray getMenuBarNames() override { return {"Settings"}; }

  juce::PopupMenu getMenuForIndex(int menuIndex,
                                  const juce::String &) override {
    juce::PopupMenu menu;
    if (menuIndex == 0) {
      juce::PopupMenu themeMenu;
      for (size_t i = 0; i < Themes::kThemes.size(); ++i) {
        themeMenu.addItem(static_cast<int>(i) + 1, Themes::kThemes[i].name);
      }
      menu.addSubMenu("Theme", themeMenu);
    }
    return menu;
  }

  void menuItemSelected(int menuItemID, int /*topLevelMenuIndex*/) override {
    if (menuItemID >= 1 &&
        menuItemID <= static_cast<int>(Themes::kThemes.size())) {
      applyTheme(static_cast<GuiConstants::Theme>(menuItemID - 1));
    }
  }

  void applyTheme(GuiConstants::Theme preset) {
    AppSettings::saveTheme(preset);
    GuiConstants::setPreset(preset);
    setLookAndFeel(nullptr);
    m_laf = std::make_unique<GuiLookAndFeel>();
    setLookAndFeel(m_laf.get());
    juce::LookAndFeel::setDefaultLookAndFeel(m_laf.get());
    backgroundApp = juce::Colour(GuiConstants::instance().colors.background);
    signalGauge.updateColors();
    irisGauge.setGradientPreset(preset);

    repaint();
  }

  void updateSwingRatioVisibility() {
    swingRatioDial.setVisible(
        processorRef.presetHasSwing(presetDrop.getSelectedItemIndex()));
    resized();
  }

private:
  AudioPluginAudioProcessor &processorRef;
  juce::AudioProcessorValueTreeState &valueTreeState;
  std::unique_ptr<GuiLookAndFeel> m_laf;
  juce::Colour backgroundApp;
  juce::MenuBarComponent m_menuBar;
  juce::Component *m_topLevel{nullptr};
  bool m_boundsRestored{false};

  juce::ComboBox subsetDrop{};
  std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>
      subsetDropAttachment;
  CustomRotaryDial bpmDial{this};
  juce::ComboBox dropBarsDrop{};
  std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>
      dropBarsDropAttachment;
  CustomRotaryDial metroVolumeDial{this};
  CustomRotaryDial inputVolumeDial{this};
  CustomRotaryDial subVolumeDial{this};
  juce::ToggleButton onOffSwitch{juce::String::fromUTF8("Start")};
  std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>
      onOffSwitchAttachment;
  juce::ToggleButton hostSyncSwitch{juce::String::fromUTF8("Host Sync")};
  std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>
      hostSyncSwitchAttachment;
  juce::ComboBox presetDrop{};
  std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>
      presetDropAttachment;
  CustomRotaryDial swingRatioDial{this};
  CircularBeatDisplay signalGauge{};
  CircularSpectrogramDisplay irisGauge{};

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioPluginAudioProcessorEditor)
};
