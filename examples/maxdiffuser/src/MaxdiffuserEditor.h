#pragma once
/*
 * AUTO GENERATED,
 * NOT A GOOD IDEA TO CHANGE STUFF HERE
 * Keep the file readonly
 */

#include "MaxdiffuserProcessor.h"
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
    std::vector<juce::Rectangle<int>> areas(4);
    const auto colWidth = area.getWidth() / 8;
    areas[0] =
        area.removeFromLeft(colWidth * 1).reduced(Constants::Margins::small);
    areas[1] =
        area.removeFromLeft(colWidth * 1).reduced(Constants::Margins::small);
    areas[2] =
        area.removeFromLeft(colWidth * 1).reduced(Constants::Margins::small);
    areas[3] = area.reduced(Constants::Margins::small);

    {
      juce::FlexBox box;
      box.flexWrap = juce::FlexBox::Wrap::noWrap;
      box.flexDirection = juce::FlexBox::Direction::column;
      box.justifyContent = juce::FlexBox::JustifyContent::spaceAround;
      box.items.add(
          juce::FlexItem(dryDial).withFlex(1).withMargin(knobMarginSmall));
      box.items.add(
          juce::FlexItem(wetDial).withFlex(1).withMargin(knobMarginSmall));
      box.items.add(
          juce::FlexItem(preDelayDial).withFlex(1).withMargin(knobMarginSmall));
      box.items.add(juce::FlexItem(levelGauge)
                        .withHeight(100)
                        .withMargin(knobMarginSmall));
      box.items.add(
          juce::FlexItem(cpuGauge).withHeight(100).withMargin(knobMarginSmall));
      box.performLayout(areas[0].toFloat());
    }
    {
      juce::FlexBox box;
      box.flexWrap = juce::FlexBox::Wrap::noWrap;
      box.flexDirection = juce::FlexBox::Direction::column;
      box.justifyContent = juce::FlexBox::JustifyContent::spaceAround;
      box.items.add(
          juce::FlexItem(elementsDial).withFlex(1).withMargin(knobMarginSmall));
      box.items.add(
          juce::FlexItem(feedbackDial).withFlex(1).withMargin(knobMarginSmall));
      box.items.add(
          juce::FlexItem(bulgeDial).withFlex(1).withMargin(knobMarginSmall));
      box.items.add(juce::FlexItem(bottomSizeDial)
                        .withFlex(1)
                        .withMargin(knobMarginSmall));
      box.items.add(
          juce::FlexItem(topSizeDial).withFlex(1).withMargin(knobMarginSmall));
      box.performLayout(areas[1].toFloat());
    }
    {
      juce::FlexBox box;
      box.flexWrap = juce::FlexBox::Wrap::noWrap;
      box.flexDirection = juce::FlexBox::Direction::column;
      box.justifyContent = juce::FlexBox::JustifyContent::spaceAround;
      box.items.add(juce::FlexItem(modulationDepthDial)
                        .withFlex(1)
                        .withMargin(knobMarginSmall));
      box.items.add(juce::FlexItem(modulationSpeedDial)
                        .withFlex(1)
                        .withMargin(knobMarginSmall));
      box.items.add(
          juce::FlexItem(lowPassDial).withFlex(1).withMargin(knobMarginSmall));
      box.items.add(juce::FlexItem(allPassFirstDial)
                        .withFlex(1)
                        .withMargin(knobMarginSmall));
      box.items.add(juce::FlexItem(allPassLastDial)
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
    }
  }

  void initWidgets() {
    addAndMakeVisible(dryDial);
    dryDial.reset(valueTreeState, "dry");
    dryDial.setLabelText(juce::String::fromUTF8("Dry"));
    addAndMakeVisible(wetDial);
    wetDial.reset(valueTreeState, "wet");
    wetDial.setLabelText(juce::String::fromUTF8("Wet"));
    addAndMakeVisible(preDelayDial);
    preDelayDial.reset(valueTreeState, "preDelay");
    preDelayDial.setLabelText(juce::String::fromUTF8("Pre Delay"));
    addAndMakeVisible(elementsDial);
    elementsDial.reset(valueTreeState, "elements");
    elementsDial.setLabelText(juce::String::fromUTF8("Elements"));
    addAndMakeVisible(feedbackDial);
    feedbackDial.reset(valueTreeState, "feedback");
    feedbackDial.setLabelText(juce::String::fromUTF8("Diffusion"));
    addAndMakeVisible(bulgeDial);
    bulgeDial.reset(valueTreeState, "bulge");
    bulgeDial.setLabelText(juce::String::fromUTF8("Bulge"));
    addAndMakeVisible(bottomSizeDial);
    bottomSizeDial.reset(valueTreeState, "bottomSize");
    bottomSizeDial.setLabelText(juce::String::fromUTF8("Bottom Size"));
    addAndMakeVisible(topSizeDial);
    topSizeDial.reset(valueTreeState, "topSize");
    topSizeDial.setLabelText(juce::String::fromUTF8("Top Size"));
    addAndMakeVisible(modulationDepthDial);
    modulationDepthDial.reset(valueTreeState, "modulationDepth");
    modulationDepthDial.setLabelText(juce::String::fromUTF8("Mod Depth"));
    addAndMakeVisible(modulationSpeedDial);
    modulationSpeedDial.reset(valueTreeState, "modulationSpeed");
    modulationSpeedDial.setLabelText(juce::String::fromUTF8("Mod Speed"));
    addAndMakeVisible(lowPassDial);
    lowPassDial.reset(valueTreeState, "lowPass");
    lowPassDial.setLabelText(juce::String::fromUTF8("Low Pass"));
    addAndMakeVisible(allPassFirstDial);
    allPassFirstDial.reset(valueTreeState, "allPassFirst");
    allPassFirstDial.setLabelText(juce::String::fromUTF8("All Pass First"));
    addAndMakeVisible(allPassLastDial);
    allPassLastDial.reset(valueTreeState, "allPassLast");
    allPassLastDial.setLabelText(juce::String::fromUTF8("All Pass Last"));
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

  CustomRotaryDial dryDial{this};
  CustomRotaryDial wetDial{this};
  CustomRotaryDial preDelayDial{this};
  CustomRotaryDial elementsDial{this};
  CustomRotaryDial feedbackDial{this};
  CustomRotaryDial bulgeDial{this};
  CustomRotaryDial bottomSizeDial{this};
  CustomRotaryDial topSizeDial{this};
  CustomRotaryDial modulationDepthDial{this};
  CustomRotaryDial modulationSpeedDial{this};
  CustomRotaryDial lowPassDial{this};
  CustomRotaryDial allPassFirstDial{this};
  CustomRotaryDial allPassLastDial{this};
  CpuGauge cpuGauge{};
  Gauge levelGauge{};
  SpectrogramDisplay spectrogramGauge{AppSettings::loadTheme()};

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioPluginAudioProcessorEditor)
};
