#pragma once
/*
 * AUTO GENERATED,
 * NOT A GOOD IDEA TO CHANGE STUFF HERE
 * Keep the file readonly
 */

#include "MinireverbProcessor.h"
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

    std::vector<juce::Rectangle<int>> areas(6);
    const auto colWidth = area.getWidth() / 13;
    const auto rowHeight = area.getHeight() / 6;
    areas[0] =
        area.removeFromLeft(colWidth * 1).reduced(Constants::Margins::small);
    auto keepArea = area;
    areas[1] =
        area.removeFromTop(rowHeight * 2).reduced(Constants::Margins::small);
    areas[2] =
        area.removeFromTop(rowHeight * 1).reduced(Constants::Margins::small);
    areas[3] =
        area.removeFromTop(rowHeight * 1).reduced(Constants::Margins::small);
    areas[4] =
        area.removeFromTop(rowHeight * 1).reduced(Constants::Margins::small);
    areas[5] = area.reduced(Constants::Margins::small);

    {
      juce::FlexBox box;
      box.flexWrap = juce::FlexBox::Wrap::noWrap;
      box.flexDirection = juce::FlexBox::Direction::column;
      box.justifyContent = juce::FlexBox::JustifyContent::spaceAround;
      box.items.add(juce::FlexItem(levelGauge)
                        .withHeight(500)
                        .withMargin(knobMarginSmall));
      box.items.add(
          juce::FlexItem(cpuGauge).withHeight(200).withMargin(knobMarginSmall));
      box.performLayout(areas[0].toFloat());
    }
    {
      juce::FlexBox box;
      box.flexWrap = juce::FlexBox::Wrap::noWrap;
      box.flexDirection = juce::FlexBox::Direction::row;
      box.justifyContent = juce::FlexBox::JustifyContent::spaceAround;
      box.items.add(
          juce::FlexItem(decayDial).withFlex(1).withMargin(knobMarginSmall));
      box.items.add(
          juce::FlexItem(dryDial).withFlex(1).withMargin(knobMarginSmall));
      box.items.add(
          juce::FlexItem(wetDial).withFlex(1).withMargin(knobMarginSmall));
      box.items.add(juce::FlexItem(stereoWidthDial)
                        .withFlex(1)
                        .withMargin(knobMarginSmall));
      box.items.add(juce::FlexItem(spectrogramGauge)
                        .withWidth(600)
                        .withMargin(knobMarginSmall));
      box.performLayout(areas[1].toFloat());
    }
    {
      juce::FlexBox box;
      box.flexWrap = juce::FlexBox::Wrap::noWrap;
      box.flexDirection = juce::FlexBox::Direction::row;
      box.justifyContent = juce::FlexBox::JustifyContent::spaceAround;
      box.items.add(
          juce::FlexItem(baseSizeDial).withFlex(1).withMargin(knobMarginSmall));
      box.items.add(juce::FlexItem(sizeFactorDial)
                        .withFlex(1)
                        .withMargin(knobMarginSmall));
      box.items.add(
          juce::FlexItem(bulgeDial).withFlex(1).withMargin(knobMarginSmall));
      box.items.add(juce::FlexItem(uniqueDelaySwitch)
                        .withWidth(Constants::Text::labelWidth)
                        .withHeight(Constants::Text::labelHeight)
                        .withAlignSelf(juce::FlexItem::AlignSelf::center)
                        .withMargin(knobMarginSmall));
      box.performLayout(areas[2].toFloat());
    }
    {
      juce::FlexBox box;
      box.flexWrap = juce::FlexBox::Wrap::noWrap;
      box.flexDirection = juce::FlexBox::Direction::row;
      box.justifyContent = juce::FlexBox::JustifyContent::spaceAround;
      box.items.add(juce::FlexItem(allPassUpDial)
                        .withFlex(1)
                        .withMargin(knobMarginSmall));
      box.items.add(juce::FlexItem(allPassDownDial)
                        .withFlex(1)
                        .withMargin(knobMarginSmall));
      box.items.add(
          juce::FlexItem(lowPassDial).withFlex(1).withMargin(knobMarginSmall));
      box.items.add(juce::FlexItem(lowPassCountDrop)
                        .withFlex(1)
                        .withHeight(Constants::Text::labelHeight)
                        .withAlignSelf(juce::FlexItem::AlignSelf::center)
                        .withMargin(knobMarginSmall));
      box.items.add(
          juce::FlexItem(highPassDial).withFlex(1).withMargin(knobMarginSmall));
      box.items.add(juce::FlexItem(highPassCountDrop)
                        .withFlex(1)
                        .withHeight(Constants::Text::labelHeight)
                        .withAlignSelf(juce::FlexItem::AlignSelf::center)
                        .withMargin(knobMarginSmall));
      box.performLayout(areas[3].toFloat());
    }
    {
      juce::FlexBox box;
      box.flexWrap = juce::FlexBox::Wrap::noWrap;
      box.flexDirection = juce::FlexBox::Direction::row;
      box.justifyContent = juce::FlexBox::JustifyContent::spaceAround;
      box.items.add(juce::FlexItem(orderDrop)
                        .withFlex(1)
                        .withHeight(Constants::Text::labelHeight)
                        .withAlignSelf(juce::FlexItem::AlignSelf::center)
                        .withMargin(knobMarginSmall));
      box.items.add(juce::FlexItem(modulationDepthDial)
                        .withFlex(1)
                        .withMargin(knobMarginSmall));
      box.items.add(juce::FlexItem(modulationSpeedDial)
                        .withFlex(1)
                        .withMargin(knobMarginSmall));
      box.performLayout(areas[4].toFloat());
    }
    {
      juce::FlexBox box;
      box.flexWrap = juce::FlexBox::Wrap::noWrap;
      box.flexDirection = juce::FlexBox::Direction::row;
      box.justifyContent = juce::FlexBox::JustifyContent::spaceAround;
      box.items.add(juce::FlexItem(reversePitchSwitch)
                        .withWidth(Constants::Text::labelWidth)
                        .withHeight(Constants::Text::labelHeight)
                        .withAlignSelf(juce::FlexItem::AlignSelf::center)
                        .withMargin(knobMarginSmall));
      box.items.add(juce::FlexItem(pitchStrengthDial)
                        .withFlex(1)
                        .withMargin(knobMarginSmall));
      box.items.add(juce::FlexItem(pitch1InplaceDial)
                        .withFlex(1)
                        .withMargin(knobMarginSmall));
      box.items.add(juce::FlexItem(pitch2InplaceDial)
                        .withFlex(1)
                        .withMargin(knobMarginSmall));
      box.performLayout(areas[5].toFloat());
    }
  }
#pragma GCC diagnostic pop

  void timerCallback() override {
    if (processorRef.hasRunner()) {
      cpuGauge.update(processorRef.getCpuLoad());
      levelGauge.update(processorRef.getInputDbLoad(),
                        processorRef.getOutputDbLoad());
      spectrogramGauge.update(processorRef.getSpectrogram());
    }
  }

  void initWidgets() {
    addAndMakeVisible(orderDrop);
    orderDrop.addItemList(
        valueTreeState.getParameter("order")->getAllValueStrings(), 1);
    orderDropAttachment = std::make_unique<
        juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        valueTreeState, "order", orderDrop);
    addAndMakeVisible(dryDial);
    dryDial.reset(valueTreeState, "dry");
    dryDial.setLabelText(juce::String::fromUTF8("Dry"));
    addAndMakeVisible(wetDial);
    wetDial.reset(valueTreeState, "wet");
    wetDial.setLabelText(juce::String::fromUTF8("Wet"));
    addAndMakeVisible(stereoWidthDial);
    stereoWidthDial.reset(valueTreeState, "stereoWidth");
    stereoWidthDial.setLabelText(juce::String::fromUTF8("Stereo Width"));
    addAndMakeVisible(baseSizeDial);
    baseSizeDial.reset(valueTreeState, "baseSize");
    baseSizeDial.setLabelText(juce::String::fromUTF8("Base size"));
    addAndMakeVisible(sizeFactorDial);
    sizeFactorDial.reset(valueTreeState, "sizeFactor");
    sizeFactorDial.setLabelText(juce::String::fromUTF8("Size Factor"));
    addAndMakeVisible(bulgeDial);
    bulgeDial.reset(valueTreeState, "bulge");
    bulgeDial.setLabelText(juce::String::fromUTF8("Bulge"));
    addAndMakeVisible(uniqueDelaySwitch);
    uniqueDelaySwitchAttachment =
        std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
            valueTreeState, "uniqueDelay", uniqueDelaySwitch);

    addAndMakeVisible(decayDial);
    decayDial.reset(valueTreeState, "decay");
    decayDial.setLabelText(juce::String::fromUTF8("Decay Low"));
    addAndMakeVisible(allPassUpDial);
    allPassUpDial.reset(valueTreeState, "allPassUp");
    allPassUpDial.setLabelText(juce::String::fromUTF8("All pass First"));
    addAndMakeVisible(allPassDownDial);
    allPassDownDial.reset(valueTreeState, "allPassDown");
    allPassDownDial.setLabelText(juce::String::fromUTF8("All pass Last"));
    addAndMakeVisible(lowPassDial);
    lowPassDial.reset(valueTreeState, "lowPass");
    lowPassDial.setLabelText(juce::String::fromUTF8("Low pass"));
    addAndMakeVisible(lowPassCountDrop);
    lowPassCountDrop.addItemList(
        valueTreeState.getParameter("lowPassCount")->getAllValueStrings(), 1);
    lowPassCountDropAttachment = std::make_unique<
        juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        valueTreeState, "lowPassCount", lowPassCountDrop);
    addAndMakeVisible(highPassDial);
    highPassDial.reset(valueTreeState, "highPass");
    highPassDial.setLabelText(juce::String::fromUTF8("High pass"));
    addAndMakeVisible(highPassCountDrop);
    highPassCountDrop.addItemList(
        valueTreeState.getParameter("highPassCount")->getAllValueStrings(), 1);
    highPassCountDropAttachment = std::make_unique<
        juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        valueTreeState, "highPassCount", highPassCountDrop);
    addAndMakeVisible(modulationDepthDial);
    modulationDepthDial.reset(valueTreeState, "modulationDepth");
    modulationDepthDial.setLabelText(juce::String::fromUTF8("Mod depth"));
    addAndMakeVisible(modulationSpeedDial);
    modulationSpeedDial.reset(valueTreeState, "modulationSpeed");
    modulationSpeedDial.setLabelText(juce::String::fromUTF8("Mod speed"));
    addAndMakeVisible(reversePitchSwitch);
    reversePitchSwitchAttachment =
        std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
            valueTreeState, "reversePitch", reversePitchSwitch);

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
    cpuGauge.updateColors();
    levelGauge.updateColors();
    spectrogramGauge.setGradientPreset(preset);

    repaint();
  }

private:
  AudioPluginAudioProcessor &processorRef;
  juce::AudioProcessorValueTreeState &valueTreeState;
  std::unique_ptr<GuiLookAndFeel> m_laf;
  juce::Colour backgroundApp;
  juce::MenuBarComponent m_menuBar;
  juce::Component *m_topLevel{nullptr};
  bool m_boundsRestored{false};

  juce::ComboBox orderDrop{};
  std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>
      orderDropAttachment;
  CustomRotaryDial dryDial{this};
  CustomRotaryDial wetDial{this};
  CustomRotaryDial stereoWidthDial{this};
  CustomRotaryDial baseSizeDial{this};
  CustomRotaryDial sizeFactorDial{this};
  CustomRotaryDial bulgeDial{this};
  juce::ToggleButton uniqueDelaySwitch{juce::String::fromUTF8("Unique delay")};
  std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>
      uniqueDelaySwitchAttachment;
  CustomRotaryDial decayDial{this};
  CustomRotaryDial allPassUpDial{this};
  CustomRotaryDial allPassDownDial{this};
  CustomRotaryDial lowPassDial{this};
  juce::ComboBox lowPassCountDrop{};
  std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>
      lowPassCountDropAttachment;
  CustomRotaryDial highPassDial{this};
  juce::ComboBox highPassCountDrop{};
  std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>
      highPassCountDropAttachment;
  CustomRotaryDial modulationDepthDial{this};
  CustomRotaryDial modulationSpeedDial{this};
  juce::ToggleButton reversePitchSwitch{
      juce::String::fromUTF8("Reverse pitch")};
  std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>
      reversePitchSwitchAttachment;
  CustomRotaryDial pitchStrengthDial{this};
  CustomRotaryDial pitch1InplaceDial{this};
  CustomRotaryDial pitch2InplaceDial{this};
  CpuGauge cpuGauge{};
  Gauge levelGauge{};
  SpectrogramDisplay spectrogramGauge{AppSettings::loadTheme()};

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioPluginAudioProcessorEditor)
};
