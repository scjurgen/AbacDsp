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
    std::vector<juce::Rectangle<int>> areas(5);
    const auto colWidth = area.getWidth() / 9;
    areas[0] =
        area.removeFromLeft(colWidth * 1).reduced(Constants::Margins::small);
    areas[1] =
        area.removeFromLeft(colWidth * 2).reduced(Constants::Margins::small);
    areas[2] =
        area.removeFromLeft(colWidth * 2).reduced(Constants::Margins::small);
    areas[3] =
        area.removeFromLeft(colWidth * 2).reduced(Constants::Margins::small);
    areas[4] = area.reduced(Constants::Margins::small);

    {
      juce::FlexBox box;
      box.flexWrap = juce::FlexBox::Wrap::noWrap;
      box.flexDirection = juce::FlexBox::Direction::column;
      box.justifyContent = juce::FlexBox::JustifyContent::spaceAround;
      box.items.add(
          juce::FlexItem(levelGauge).withFlex(1).withMargin(knobMarginSmall));
      box.items.add(
          juce::FlexItem(cpuGauge).withFlex(1).withMargin(knobMarginSmall));
      box.performLayout(areas[0].toFloat());
    }
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
      box.performLayout(areas[1].toFloat());
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
      box.performLayout(areas[2].toFloat());
    }
    {
      juce::FlexBox box;
      box.flexWrap = juce::FlexBox::Wrap::noWrap;
      box.flexDirection = juce::FlexBox::Direction::column;
      box.justifyContent = juce::FlexBox::JustifyContent::spaceAround;
      box.items.add(
          juce::FlexItem(bulgeDial).withFlex(1).withMargin(knobMarginSmall));
      box.items.add(juce::FlexItem(bottomSizeDial)
                        .withFlex(1)
                        .withMargin(knobMarginSmall));
      box.items.add(
          juce::FlexItem(topSizeDial).withFlex(1).withMargin(knobMarginSmall));
      box.performLayout(areas[3].toFloat());
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
      box.performLayout(areas[4].toFloat());
    }
  }
#pragma GCC diagnostic pop

  void timerCallback() override {
    if (processorRef.hasRunner()) {
      cpuGauge.update(processorRef.getCpuLoad());
      levelGauge.update(processorRef.getInputDbLoad(),
                        processorRef.getOutputDbLoad());
      processorRef.consumeLastLearnedCc();
    }
  }

  void initWidgets() {
    addAndMakeVisible(dryDial);
    dryDial.reset(valueTreeState, "dry");
    dryDial.setLabelText(juce::String::fromUTF8("Dry"));
    dryDial.setCcMappable(
        true, {[this] { processorRef.beginCcLearn(CcTarget::dry); },
               [this] { return processorRef.getCcRange(CcTarget::dry); },
               [this](float lo, float hi) {
                 processorRef.setCcRange(CcTarget::dry, lo, hi);
               },
               [this] { processorRef.clearCcAssignment(CcTarget::dry); },
               [this] { return processorRef.getCcController(CcTarget::dry); }});
    addAndMakeVisible(wetDial);
    wetDial.reset(valueTreeState, "wet");
    wetDial.setLabelText(juce::String::fromUTF8("Wet"));
    wetDial.setCcMappable(
        true, {[this] { processorRef.beginCcLearn(CcTarget::wet); },
               [this] { return processorRef.getCcRange(CcTarget::wet); },
               [this](float lo, float hi) {
                 processorRef.setCcRange(CcTarget::wet, lo, hi);
               },
               [this] { processorRef.clearCcAssignment(CcTarget::wet); },
               [this] { return processorRef.getCcController(CcTarget::wet); }});
    addAndMakeVisible(preDelayDial);
    preDelayDial.reset(valueTreeState, "preDelay");
    preDelayDial.setLabelText(juce::String::fromUTF8("Pre Delay"));
    preDelayDial.setCcMappable(
        true,
        {[this] { processorRef.beginCcLearn(CcTarget::preDelay); },
         [this] { return processorRef.getCcRange(CcTarget::preDelay); },
         [this](float lo, float hi) {
           processorRef.setCcRange(CcTarget::preDelay, lo, hi);
         },
         [this] { processorRef.clearCcAssignment(CcTarget::preDelay); },
         [this] { return processorRef.getCcController(CcTarget::preDelay); }});
    addAndMakeVisible(elementsDial);
    elementsDial.reset(valueTreeState, "elements");
    elementsDial.setLabelText(juce::String::fromUTF8("Elements"));
    elementsDial.setCcMappable(
        true,
        {[this] { processorRef.beginCcLearn(CcTarget::elements); },
         [this] { return processorRef.getCcRange(CcTarget::elements); },
         [this](float lo, float hi) {
           processorRef.setCcRange(CcTarget::elements, lo, hi);
         },
         [this] { processorRef.clearCcAssignment(CcTarget::elements); },
         [this] { return processorRef.getCcController(CcTarget::elements); }});
    addAndMakeVisible(feedbackDial);
    feedbackDial.reset(valueTreeState, "feedback");
    feedbackDial.setLabelText(juce::String::fromUTF8("Diffusion"));
    feedbackDial.setCcMappable(
        true,
        {[this] { processorRef.beginCcLearn(CcTarget::feedback); },
         [this] { return processorRef.getCcRange(CcTarget::feedback); },
         [this](float lo, float hi) {
           processorRef.setCcRange(CcTarget::feedback, lo, hi);
         },
         [this] { processorRef.clearCcAssignment(CcTarget::feedback); },
         [this] { return processorRef.getCcController(CcTarget::feedback); }});
    addAndMakeVisible(bulgeDial);
    bulgeDial.reset(valueTreeState, "bulge");
    bulgeDial.setLabelText(juce::String::fromUTF8("Bulge"));
    bulgeDial.setCcMappable(
        true,
        {[this] { processorRef.beginCcLearn(CcTarget::bulge); },
         [this] { return processorRef.getCcRange(CcTarget::bulge); },
         [this](float lo, float hi) {
           processorRef.setCcRange(CcTarget::bulge, lo, hi);
         },
         [this] { processorRef.clearCcAssignment(CcTarget::bulge); },
         [this] { return processorRef.getCcController(CcTarget::bulge); }});
    addAndMakeVisible(bottomSizeDial);
    bottomSizeDial.reset(valueTreeState, "bottomSize");
    bottomSizeDial.setLabelText(juce::String::fromUTF8("Bottom Size"));
    bottomSizeDial.setCcMappable(
        true, {[this] { processorRef.beginCcLearn(CcTarget::bottomSize); },
               [this] { return processorRef.getCcRange(CcTarget::bottomSize); },
               [this](float lo, float hi) {
                 processorRef.setCcRange(CcTarget::bottomSize, lo, hi);
               },
               [this] { processorRef.clearCcAssignment(CcTarget::bottomSize); },
               [this] {
                 return processorRef.getCcController(CcTarget::bottomSize);
               }});
    addAndMakeVisible(topSizeDial);
    topSizeDial.reset(valueTreeState, "topSize");
    topSizeDial.setLabelText(juce::String::fromUTF8("Top Size"));
    topSizeDial.setCcMappable(
        true,
        {[this] { processorRef.beginCcLearn(CcTarget::topSize); },
         [this] { return processorRef.getCcRange(CcTarget::topSize); },
         [this](float lo, float hi) {
           processorRef.setCcRange(CcTarget::topSize, lo, hi);
         },
         [this] { processorRef.clearCcAssignment(CcTarget::topSize); },
         [this] { return processorRef.getCcController(CcTarget::topSize); }});
    addAndMakeVisible(modulationDepthDial);
    modulationDepthDial.reset(valueTreeState, "modulationDepth");
    modulationDepthDial.setLabelText(juce::String::fromUTF8("Mod Depth"));
    modulationDepthDial.setCcMappable(
        true,
        {[this] { processorRef.beginCcLearn(CcTarget::modulationDepth); },
         [this] { return processorRef.getCcRange(CcTarget::modulationDepth); },
         [this](float lo, float hi) {
           processorRef.setCcRange(CcTarget::modulationDepth, lo, hi);
         },
         [this] { processorRef.clearCcAssignment(CcTarget::modulationDepth); },
         [this] {
           return processorRef.getCcController(CcTarget::modulationDepth);
         }});
    addAndMakeVisible(modulationSpeedDial);
    modulationSpeedDial.reset(valueTreeState, "modulationSpeed");
    modulationSpeedDial.setLabelText(juce::String::fromUTF8("Mod Speed"));
    modulationSpeedDial.setCcMappable(
        true,
        {[this] { processorRef.beginCcLearn(CcTarget::modulationSpeed); },
         [this] { return processorRef.getCcRange(CcTarget::modulationSpeed); },
         [this](float lo, float hi) {
           processorRef.setCcRange(CcTarget::modulationSpeed, lo, hi);
         },
         [this] { processorRef.clearCcAssignment(CcTarget::modulationSpeed); },
         [this] {
           return processorRef.getCcController(CcTarget::modulationSpeed);
         }});
    addAndMakeVisible(lowPassDial);
    lowPassDial.reset(valueTreeState, "lowPass");
    lowPassDial.setLabelText(juce::String::fromUTF8("Low Pass"));
    lowPassDial.setCcMappable(
        true,
        {[this] { processorRef.beginCcLearn(CcTarget::lowPass); },
         [this] { return processorRef.getCcRange(CcTarget::lowPass); },
         [this](float lo, float hi) {
           processorRef.setCcRange(CcTarget::lowPass, lo, hi);
         },
         [this] { processorRef.clearCcAssignment(CcTarget::lowPass); },
         [this] { return processorRef.getCcController(CcTarget::lowPass); }});
    addAndMakeVisible(cpuGauge);
    cpuGauge.setLabelText(juce::String::fromUTF8("CPU"));
    addAndMakeVisible(levelGauge);
    levelGauge.setLabelText(juce::String::fromUTF8("Level"));
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
    cpuGauge.updateColors();
    levelGauge.updateColors();

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
  CpuGauge cpuGauge{};
  Gauge levelGauge{};

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioPluginAudioProcessorEditor)
};
