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
        backgroundApp(juce::Colour(GuiConstants::instance().colors.bg_App)),
        m_menuBar(this) {
    setLookAndFeel(&m_laf);
    addAndMakeVisible(m_menuBar);
    initWidgets();
    setResizable(true, true);
    setResizeLimits(GuiConstants::instance().init.WindowWidth,
                    GuiConstants::instance().init.WindowHeight, 4000, 3000);
    const auto saved = AppSettings::loadWindowBounds(
        GuiConstants::instance().init.WindowWidth,
        GuiConstants::instance().init.WindowHeight);
    setSize(saved.getWidth(), saved.getHeight());
    startTimerHz(GuiConstants::instance().init.TimerHertz);
  }

  ~AudioPluginAudioProcessorEditor() override {
    if (m_topLevel != nullptr) {
      m_topLevel->removeComponentListener(this);
    }
    stopTimer();
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
    std::vector<juce::Rectangle<int>> areas(3);
    const auto rowHeight = area.getHeight() / 6;
    areas[0] =
        area.removeFromTop(rowHeight * 1).reduced(Constants::Margins::small);
    areas[1] =
        area.removeFromTop(rowHeight * 1).reduced(Constants::Margins::small);
    areas[2] = area.reduced(Constants::Margins::small);

    {
      juce::FlexBox box;
      box.flexWrap = juce::FlexBox::Wrap::noWrap;
      box.flexDirection = juce::FlexBox::Direction::row;
      box.justifyContent = juce::FlexBox::JustifyContent::spaceAround;
      box.items.add(juce::FlexItem(subsetDrop)
                        .withFlex(0)
                        .withWidth(120)
                        .withHeight(Constants::Text::labelHeight)
                        .withAlignSelf(juce::FlexItem::AlignSelf::center)
                        .withMargin(knobMarginSmall));
      box.items.add(juce::FlexItem(presetDrop)
                        .withFlex(0)
                        .withWidth(200)
                        .withHeight(Constants::Text::labelHeight)
                        .withAlignSelf(juce::FlexItem::AlignSelf::center)
                        .withMargin(knobMarginSmall));
      box.items.add(juce::FlexItem(dropBarsDrop)
                        .withFlex(1)
                        .withHeight(Constants::Text::labelHeight)
                        .withAlignSelf(juce::FlexItem::AlignSelf::center)
                        .withMargin(knobMarginSmall));
      if (swingRatioDial.isVisible()) {
        box.items.add(juce::FlexItem(swingRatioDial)
                          .withFlex(1)
                          .withMargin(knobMarginSmall));
      }
      box.items.add(juce::FlexItem(onOffSwitch)
                        .withWidth(Constants::Text::labelWidth)
                        .withHeight(Constants::Text::labelHeight)
                        .withAlignSelf(juce::FlexItem::AlignSelf::center)
                        .withMargin(knobMarginSmall));
      box.items.add(
          juce::FlexItem(bpmDial).withFlex(1).withMargin(knobMarginSmall));
      box.performLayout(areas[0].toFloat());
    }
    {
      juce::FlexBox box;
      box.flexWrap = juce::FlexBox::Wrap::noWrap;
      box.flexDirection = juce::FlexBox::Direction::row;
      box.justifyContent = juce::FlexBox::JustifyContent::spaceAround;
      box.items.add(juce::FlexItem(subVolumeDial)
                        .withFlex(1)
                        .withMargin(knobMarginSmall));
      box.items.add(juce::FlexItem(metroVolumeDial)
                        .withFlex(1)
                        .withMargin(knobMarginSmall));
      box.items.add(juce::FlexItem(inputVolumeDial)
                        .withFlex(1)
                        .withMargin(knobMarginSmall));
      box.performLayout(areas[1].toFloat());
    }
    {
      juce::FlexBox box;
      box.flexWrap = juce::FlexBox::Wrap::noWrap;
      box.flexDirection = juce::FlexBox::Direction::row;
      box.justifyContent = juce::FlexBox::JustifyContent::spaceAround;
      box.items.add(
          juce::FlexItem(signalGauge).withFlex(1).withMargin(knobMarginSmall));
      box.performLayout(areas[2].toFloat());
    }
  }
#pragma GCC diagnostic pop

  void timerCallback() override {
    if (processorRef.hasRunner()) {
      signalGauge.update(processorRef.getWaveDataToShow());

      {
        const float sr = static_cast<float>(processorRef.getSampleRate());
        const float bpm = static_cast<float>(
            valueTreeState.getParameterAsValue("bpm").getValue());
        const size_t spb = static_cast<size_t>(sr * 60.f / bpm);
        signalGauge.setSampleRate(sr);
        signalGauge.setSamplesPerBeat(spb);
        signalGauge.setBeatIndex(processorRef.getWaveDataBeatIndex());
        signalGauge.setSubdivisionPositions(
            processorRef.getSubdivisionPositions());
      }
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
    addAndMakeVisible(dropBarsDrop);
    dropBarsDrop.addItemList(
        valueTreeState.getParameter("dropBars")->getAllValueStrings(), 1);
    dropBarsDropAttachment = std::make_unique<
        juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        valueTreeState, "dropBars", dropBarsDrop);
    addAndMakeVisible(metroVolumeDial);
    metroVolumeDial.reset(valueTreeState, "metroVolume");
    metroVolumeDial.setLabelText(juce::String::fromUTF8("Metro Volume"));
    addAndMakeVisible(inputVolumeDial);
    inputVolumeDial.reset(valueTreeState, "inputVolume");
    inputVolumeDial.setLabelText(juce::String::fromUTF8("Input Volume"));
    addAndMakeVisible(subVolumeDial);
    subVolumeDial.reset(valueTreeState, "subVolume");
    subVolumeDial.setLabelText(juce::String::fromUTF8("Sub Volume"));
    addAndMakeVisible(onOffSwitch);
    onOffSwitchAttachment =
        std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
            valueTreeState, "onOff", onOffSwitch);

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
    addAndMakeVisible(signalGauge);
    signalGauge.setLabelText(juce::String::fromUTF8("Beat"));
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

    if (!m_boundsRestored && m_topLevel->isOnDesktop()) {
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
      static constexpr auto kThemeNames =
          std::to_array<const char *>({"Classic", "Viridis", "Inferno",
                                       "Grayscale", "Heat", "Ink", "Teal"});
      juce::PopupMenu themeMenu;
      for (int i = 0; i < static_cast<int>(kThemeNames.size()); ++i) {
        themeMenu.addItem(i + 1, kThemeNames[static_cast<size_t>(i)]);
      }
      menu.addSubMenu("Theme", themeMenu);
      menu.addSeparator();
      menu.addItem(kAudioSettingsId, "Audio Settings");
    }
    return menu;
  }

  void menuItemSelected(int menuItemID, int /*topLevelMenuIndex*/) override {
    static constexpr auto kPresets =
        std::to_array<GuiConstants::GradientPreset>({
            GuiConstants::GradientPreset::Classic,
            GuiConstants::GradientPreset::Viridis,
            GuiConstants::GradientPreset::Inferno,
            GuiConstants::GradientPreset::Grayscale,
            GuiConstants::GradientPreset::Heat,
            GuiConstants::GradientPreset::Ink,
            GuiConstants::GradientPreset::Teal,
        });
    if (menuItemID >= 1 && menuItemID <= static_cast<int>(kPresets.size())) {
      AppSettings::saveTheme(kPresets[static_cast<size_t>(menuItemID - 1)]);
      juce::AlertWindow::showMessageBoxAsync(
          juce::MessageBoxIconType::InfoIcon, "Theme",
          "Theme will take effect after restart.");
    } else if (menuItemID == kAudioSettingsId) {
    }
  }

  void updateSwingRatioVisibility() {
    swingRatioDial.setVisible(
        processorRef.presetHasSwing(presetDrop.getSelectedItemIndex()));
    resized();
  }

private:
  static constexpr int kAudioSettingsId = 100;

  AudioPluginAudioProcessor &processorRef;
  juce::AudioProcessorValueTreeState &valueTreeState;
  GuiLookAndFeel m_laf;
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
  juce::ComboBox presetDrop{};
  std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>
      presetDropAttachment;
  CustomRotaryDial swingRatioDial{this};
  MetronomeWaveDisplay signalGauge{};

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioPluginAudioProcessorEditor)
};
