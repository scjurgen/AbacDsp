#pragma once
/*
 * AUTO GENERATED,
 * NOT A GOOD IDEA TO CHANGE STUFF HERE
 * Keep the file readonly
 */

#include "PlaingainProcessor.h"
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
    std::vector<juce::Rectangle<int>> areas(3);
    const auto colWidth = area.getWidth() / 7;
    areas[0] =
        area.removeFromLeft(colWidth * 1).reduced(Constants::Margins::small);
    areas[1] =
        area.removeFromLeft(colWidth * 1).reduced(Constants::Margins::small);
    areas[2] = area.reduced(Constants::Margins::small);

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
      box.items.add(
          juce::FlexItem(latencyDial).withFlex(1).withMargin(knobMarginSmall));
      box.items.add(juce::FlexItem(levelGauge)
                        .withHeight(400)
                        .withMargin(knobMarginSmall));
      box.performLayout(areas[0].toFloat());
    }
    {
      juce::FlexBox box;
      box.flexWrap = juce::FlexBox::Wrap::noWrap;
      box.flexDirection = juce::FlexBox::Direction::column;
      box.justifyContent = juce::FlexBox::JustifyContent::spaceAround;
      box.items.add(
          juce::FlexItem(gainDial).withFlex(1).withMargin(knobMarginSmall));
      box.items.add(juce::FlexItem(lowShelvingDial)
                        .withFlex(1)
                        .withMargin(knobMarginSmall));
      box.items.add(juce::FlexItem(highShelvingDial)
                        .withFlex(1)
                        .withMargin(knobMarginSmall));
      box.performLayout(areas[1].toFloat());
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
      box.performLayout(areas[2].toFloat());
    }
  }
#pragma GCC diagnostic pop

  void timerCallback() override {
    if (processorRef.hasRunner()) {
      levelGauge.update(processorRef.getInputDbLoad(),
                        processorRef.getOutputDbLoad());
      spectrogramGauge.update(processorRef.getSpectrogram());
      signalGauge.update(processorRef.getWaveDataToShow());
    }
  }

  void initWidgets() {
    addAndMakeVisible(subsetDrop);
    subsetDrop.addItemList(
        valueTreeState.getParameter("subset")->getAllValueStrings(), 1);
    subsetDropAttachment = std::make_unique<
        juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        valueTreeState, "subset", subsetDrop);
    addAndMakeVisible(gainDial);
    gainDial.reset(valueTreeState, "gain");
    gainDial.setLabelText(juce::String::fromUTF8("Gain"));
    addAndMakeVisible(lowShelvingDial);
    lowShelvingDial.reset(valueTreeState, "lowShelving");
    lowShelvingDial.setLabelText(juce::String::fromUTF8("Low"));
    addAndMakeVisible(highShelvingDial);
    highShelvingDial.reset(valueTreeState, "highShelving");
    highShelvingDial.setLabelText(juce::String::fromUTF8("High"));
    addAndMakeVisible(latencyDial);
    latencyDial.reset(valueTreeState, "latency");
    latencyDial.setLabelText(juce::String::fromUTF8("Latency"));
    addAndMakeVisible(levelGauge);
    levelGauge.setLabelText(juce::String::fromUTF8("Level"));
    addAndMakeVisible(spectrogramGauge);
    spectrogramGauge.setLabelText(juce::String::fromUTF8("Spectrogram"));
    addAndMakeVisible(signalGauge);
    signalGauge.setLabelText(juce::String::fromUTF8("Signal"));
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
    levelGauge.updateColors();
    spectrogramGauge.setGradientPreset(preset);
    signalGauge.updateColors();

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

  juce::ComboBox subsetDrop{};
  std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>
      subsetDropAttachment;
  CustomRotaryDial gainDial{this};
  CustomRotaryDial lowShelvingDial{this};
  CustomRotaryDial highShelvingDial{this};
  CustomRotaryDial latencyDial{this};
  Gauge levelGauge{};
  SpectrogramDisplay spectrogramGauge{AppSettings::loadTheme()};
  WaveformGauge signalGauge{};

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioPluginAudioProcessorEditor)
};
