#pragma once
/*
 * AUTO GENERATED,
 * NOT A GOOD IDEA TO CHANGE STUFF HERE
 * Keep the file readonly
 */

#include "GuisandboxProcessor.h"
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
    std::vector<juce::Rectangle<int>> areas(4);
    const auto colWidth = area.getWidth() / 13;
    areas[0] =
        area.removeFromLeft(colWidth * 1).reduced(Constants::Margins::small);
    areas[1] =
        area.removeFromLeft(colWidth * 2).reduced(Constants::Margins::small);
    areas[2] =
        area.removeFromLeft(colWidth * 2).reduced(Constants::Margins::small);
    areas[3] = area.reduced(Constants::Margins::small);

    {
      juce::FlexBox box;
      box.flexWrap = juce::FlexBox::Wrap::noWrap;
      box.flexDirection = juce::FlexBox::Direction::column;
      box.justifyContent = juce::FlexBox::JustifyContent::spaceAround;
      box.items.add(juce::FlexItem(levelGauge)
                        .withHeight(2400)
                        .withMargin(knobMarginSmall));
      box.items.add(juce::FlexItem(cpuGauge).withHeight(2400).withMargin(
          knobMarginSmall));
      box.performLayout(areas[0].toFloat());
    }
    {
      juce::FlexBox box;
      box.flexWrap = juce::FlexBox::Wrap::noWrap;
      box.flexDirection = juce::FlexBox::Direction::column;
      box.justifyContent = juce::FlexBox::JustifyContent::spaceAround;
      box.items.add(juce::FlexItem(patchDrop)
                        .withFlex(1)
                        .withHeight(Constants::Text::labelHeight)
                        .withAlignSelf(juce::FlexItem::AlignSelf::center)
                        .withMargin(knobMarginSmall));
      box.items.add(juce::FlexItem(subPatchDrop)
                        .withFlex(1)
                        .withHeight(Constants::Text::labelHeight)
                        .withAlignSelf(juce::FlexItem::AlignSelf::center)
                        .withMargin(knobMarginSmall));
      box.items.add(juce::FlexItem(onOffSwitch)
                        .withWidth(Constants::Text::labelWidth)
                        .withHeight(Constants::Text::labelHeight)
                        .withAlignSelf(juce::FlexItem::AlignSelf::center)
                        .withMargin(knobMarginSmall));
      box.items.add(
          juce::FlexItem(mixDial).withFlex(1).withMargin(knobMarginSmall));
      box.items.add(juce::FlexItem(div2Label)
                        .withWidth(Constants::Text::labelWidth)
                        .withHeight(Constants::Text::labelHeight)
                        .withAlignSelf(juce::FlexItem::AlignSelf::center)
                        .withMargin(knobMarginSmall));
      box.items.add(juce::FlexItem(modulationDepthDial)
                        .withFlex(1)
                        .withMargin(knobMarginSmall));
      box.items.add(
          juce::FlexItem(kneeDial).withFlex(1).withMargin(knobMarginSmall));
      box.performLayout(areas[1].toFloat());
    }
    {
      juce::FlexBox box;
      box.flexWrap = juce::FlexBox::Wrap::noWrap;
      box.flexDirection = juce::FlexBox::Direction::column;
      box.justifyContent = juce::FlexBox::JustifyContent::spaceAround;
      box.items.add(
          juce::FlexItem(inputDial).withFlex(1).withMargin(knobMarginSmall));
      box.items.add(juce::FlexItem(div1Label)
                        .withWidth(Constants::Text::labelWidth)
                        .withHeight(Constants::Text::labelHeight)
                        .withAlignSelf(juce::FlexItem::AlignSelf::center)
                        .withMargin(knobMarginSmall));
      box.items.add(
          juce::FlexItem(densityDial).withFlex(1).withMargin(knobMarginSmall));
      box.items.add(juce::FlexItem(thresholdDial)
                        .withFlex(1)
                        .withMargin(knobMarginSmall));
      box.performLayout(areas[2].toFloat());
    }
    {
      juce::FlexBox box;
      box.flexWrap = juce::FlexBox::Wrap::noWrap;
      box.flexDirection = juce::FlexBox::Direction::column;
      box.justifyContent = juce::FlexBox::JustifyContent::spaceAround;
      box.items.add(juce::FlexItem(spectrogramGauge)
                        .withFlex(1)
                        .withMargin(knobMarginSmall));
      box.items.add(
          juce::FlexItem(signalGauge).withFlex(1).withMargin(knobMarginSmall));
      box.performLayout(areas[3].toFloat());
    }
  }
#pragma GCC diagnostic pop

  void timerCallback() override {
    if (processorRef.hasRunner()) {
      cpuGauge.update(processorRef.getCpuLoad());
      levelGauge.update(processorRef.getInputDbLoad(),
                        processorRef.getOutputDbLoad());
      spectrogramGauge.update(processorRef.getSpectrogram());
      signalGauge.update(processorRef.getWaveDataToShow());
    }
  }

  void initWidgets() {
    addAndMakeVisible(patchDrop);
    patchDrop.addItemList(
        valueTreeState.getParameter("patch")->getAllValueStrings(), 1);
    patchDropAttachment = std::make_unique<
        juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        valueTreeState, "patch", patchDrop);
    addAndMakeVisible(subPatchDrop);
    subPatchDrop.addItemList(
        valueTreeState.getParameter("subPatch")->getAllValueStrings(), 1);
    subPatchDropAttachment = std::make_unique<
        juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        valueTreeState, "subPatch", subPatchDrop);
    addAndMakeVisible(onOffSwitch);
    onOffSwitchAttachment =
        std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
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
    div1Label.setText(juce::String::fromUTF8("-- drop this too ----"),
                      juce::dontSendNotification);
    addAndMakeVisible(div2Label);
    div2Label.setText(juce::String::fromUTF8("DropIt!"),
                      juce::dontSendNotification);
    addAndMakeVisible(cpuGauge);
    cpuGauge.setLabelText(juce::String::fromUTF8("CPU"));
    addAndMakeVisible(levelGauge);
    levelGauge.setLabelText(juce::String::fromUTF8("Level"));
    addAndMakeVisible(spectrogramGauge);
    spectrogramGauge.setLabelText(juce::String::fromUTF8("Spectrogram"));
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

private:
  static constexpr int kAudioSettingsId = 100;

  AudioPluginAudioProcessor &processorRef;
  juce::AudioProcessorValueTreeState &valueTreeState;
  GuiLookAndFeel m_laf;
  juce::Colour backgroundApp;
  juce::MenuBarComponent m_menuBar;
  juce::Component *m_topLevel{nullptr};
  bool m_boundsRestored{false};

  juce::ComboBox patchDrop{};
  std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>
      patchDropAttachment;
  juce::ComboBox subPatchDrop{};
  std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>
      subPatchDropAttachment;
  juce::ToggleButton onOffSwitch{juce::String::fromUTF8("Power")};
  std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>
      onOffSwitchAttachment;
  CustomRotaryDial inputDial{this};
  CustomRotaryDial modulationDepthDial{this};
  CustomRotaryDial mixDial{this};
  CustomRotaryDial densityDial{this};
  CustomRotaryDial thresholdDial{this};
  CustomRotaryDial kneeDial{this};
  juce::Label div1Label{};
  juce::Label div2Label{};
  CpuGauge cpuGauge{};
  Gauge levelGauge{};
  SpectrogramDisplay spectrogramGauge{AppSettings::loadTheme()};
  WaveformGauge signalGauge{};

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioPluginAudioProcessorEditor)
};
