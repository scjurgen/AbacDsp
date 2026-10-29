#pragma once
/*
 * AUTO GENERATED,
 * NOT A GOOD IDEA TO CHANGE STUFF HERE
 * Keep the file readonly
 */

#include <map>

#include "SonalysisProcessor.h"
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
        const int pageButtonWidth = pageSwitchArea.getWidth() / static_cast<int>(m_pageButtons.size());
        for (size_t i = 0; i < m_pageButtons.size(); ++i)
        {
            const bool isLast = i + 1 == m_pageButtons.size();
            m_pageButtons[i].setBounds(isLast ? pageSwitchArea : pageSwitchArea.removeFromLeft(pageButtonWidth));
        }
        area = area.reduced(static_cast<int>(Constants::Margins::big));
        {
            switch (m_currentPage)
            {
                case 0:
                {
                    // auto generated
                    // const juce::FlexItem::Margin knobMargin = juce::FlexItem::Margin(Constants::Margins::small);
                    const juce::FlexItem::Margin knobMarginSmall = juce::FlexItem::Margin(Constants::Margins::medium);
                    std::vector<juce::Rectangle<int>> areas(2);
                    const auto colWidth = area.getWidth() / 4;
                    areas[0] = area.removeFromLeft(colWidth * 1).reduced(Constants::Margins::small);
                    areas[1] = area.reduced(Constants::Margins::small);

                    {
                        juce::FlexBox box;
                        box.flexWrap = juce::FlexBox::Wrap::noWrap;
                        box.flexDirection = juce::FlexBox::Direction::column;
                        box.justifyContent = juce::FlexBox::JustifyContent::spaceAround;
                        box.items.add(juce::FlexItem(peakFallDrop)
                                          .withFlex(0)
                                          .withHeight(Constants::Text::labelHeight)
                                          .withAlignSelf(juce::FlexItem::AlignSelf::stretch)
                                          .withMargin(knobMarginSmall));
                        box.items.add(juce::FlexItem(rmsWindowDrop)
                                          .withFlex(0)
                                          .withHeight(Constants::Text::labelHeight)
                                          .withAlignSelf(juce::FlexItem::AlignSelf::stretch)
                                          .withMargin(knobMarginSmall));
                        box.items.add(juce::FlexItem(peakHoldDrop)
                                          .withFlex(0)
                                          .withHeight(Constants::Text::labelHeight)
                                          .withAlignSelf(juce::FlexItem::AlignSelf::stretch)
                                          .withMargin(knobMarginSmall));
                        box.items.add(juce::FlexItem(holdResetSwitch)
                                          .withFlex(0)
                                          .withHeight(Constants::Text::labelHeight)
                                          .withAlignSelf(juce::FlexItem::AlignSelf::stretch)
                                          .withMargin(knobMarginSmall));
                        box.items.add(juce::FlexItem(thruSwitch)
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
                        box.items.add(juce::FlexItem(levelMeterGauge).withFlex(1).withMargin(knobMarginSmall));
                        box.performLayout(areas[1].toFloat());
                    }
                    break;
                }
                case 1:
                {
                    // auto generated
                    // const juce::FlexItem::Margin knobMargin = juce::FlexItem::Margin(Constants::Margins::small);
                    const juce::FlexItem::Margin knobMarginSmall = juce::FlexItem::Margin(Constants::Margins::medium);
                    std::vector<juce::Rectangle<int>> areas(2);
                    const auto colWidth = area.getWidth() / 6;
                    areas[0] = area.removeFromLeft(colWidth * 1).reduced(Constants::Margins::small);
                    areas[1] = area.reduced(Constants::Margins::small);

                    {
                        juce::FlexBox box;
                        box.flexWrap = juce::FlexBox::Wrap::noWrap;
                        box.flexDirection = juce::FlexBox::Direction::column;
                        box.justifyContent = juce::FlexBox::JustifyContent::spaceAround;
                        box.items.add(juce::FlexItem(fftSizeDrop)
                                          .withFlex(0)
                                          .withHeight(Constants::Text::labelHeight)
                                          .withAlignSelf(juce::FlexItem::AlignSelf::stretch)
                                          .withMargin(knobMarginSmall));
                        box.items.add(juce::FlexItem(fftFallDrop)
                                          .withFlex(0)
                                          .withHeight(Constants::Text::labelHeight)
                                          .withAlignSelf(juce::FlexItem::AlignSelf::stretch)
                                          .withMargin(knobMarginSmall));
                        box.items.add(juce::FlexItem(spectrumPeakHoldSwitch)
                                          .withFlex(0)
                                          .withHeight(Constants::Text::labelHeight)
                                          .withAlignSelf(juce::FlexItem::AlignSelf::stretch)
                                          .withMargin(knobMarginSmall));
                        box.items.add(juce::FlexItem(thruSwitch)
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
                        box.items.add(juce::FlexItem(spectrumGauge).withFlex(1).withMargin(knobMarginSmall));
                        box.performLayout(areas[1].toFloat());
                    }
                    break;
                }
                case 2:
                {
                    // auto generated
                    // const juce::FlexItem::Margin knobMargin = juce::FlexItem::Margin(Constants::Margins::small);
                    const juce::FlexItem::Margin knobMarginSmall = juce::FlexItem::Margin(Constants::Margins::medium);
                    std::vector<juce::Rectangle<int>> areas(2);
                    const auto colWidth = area.getWidth() / 4;
                    areas[0] = area.removeFromLeft(colWidth * 1).reduced(Constants::Margins::small);
                    areas[1] = area.reduced(Constants::Margins::small);

                    {
                        juce::FlexBox box;
                        box.flexWrap = juce::FlexBox::Wrap::noWrap;
                        box.flexDirection = juce::FlexBox::Direction::column;
                        box.justifyContent = juce::FlexBox::JustifyContent::spaceAround;
                        box.items.add(juce::FlexItem(referencePitchDial).withFlex(1).withMargin(knobMarginSmall));
                        box.items.add(juce::FlexItem(pitchSourceDrop)
                                          .withFlex(0)
                                          .withHeight(Constants::Text::labelHeight)
                                          .withAlignSelf(juce::FlexItem::AlignSelf::stretch)
                                          .withMargin(knobMarginSmall));
                        box.items.add(juce::FlexItem(thruSwitch)
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
                        box.items.add(juce::FlexItem(tunerGauge).withFlex(1).withMargin(knobMarginSmall));
                        box.performLayout(areas[1].toFloat());
                    }
                    break;
                }
                case 3:
                {
                    // auto generated
                    // const juce::FlexItem::Margin knobMargin = juce::FlexItem::Margin(Constants::Margins::small);
                    const juce::FlexItem::Margin knobMarginSmall = juce::FlexItem::Margin(Constants::Margins::medium);
                    std::vector<juce::Rectangle<int>> areas(2);
                    const auto colWidth = area.getWidth() / 6;
                    areas[0] = area.removeFromLeft(colWidth * 1).reduced(Constants::Margins::small);
                    areas[1] = area.reduced(Constants::Margins::small);

                    {
                        juce::FlexBox box;
                        box.flexWrap = juce::FlexBox::Wrap::noWrap;
                        box.flexDirection = juce::FlexBox::Direction::column;
                        box.justifyContent = juce::FlexBox::JustifyContent::spaceAround;
                        box.items.add(juce::FlexItem(thruSwitch)
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
                        box.items.add(juce::FlexItem(xyScopeGauge).withFlex(1).withMargin(knobMarginSmall));
                        box.performLayout(areas[1].toFloat());
                    }
                    break;
                }
                case 4:
                {
                    // auto generated
                    // const juce::FlexItem::Margin knobMargin = juce::FlexItem::Margin(Constants::Margins::small);
                    const juce::FlexItem::Margin knobMarginSmall = juce::FlexItem::Margin(Constants::Margins::medium);
                    std::vector<juce::Rectangle<int>> areas(2);
                    const auto colWidth = area.getWidth() / 4;
                    areas[0] = area.removeFromLeft(colWidth * 1).reduced(Constants::Margins::small);
                    areas[1] = area.reduced(Constants::Margins::small);

                    {
                        juce::FlexBox box;
                        box.flexWrap = juce::FlexBox::Wrap::noWrap;
                        box.flexDirection = juce::FlexBox::Direction::column;
                        box.justifyContent = juce::FlexBox::JustifyContent::spaceAround;
                        box.items.add(juce::FlexItem(beatResetSwitch)
                                          .withFlex(0)
                                          .withHeight(Constants::Text::labelHeight)
                                          .withAlignSelf(juce::FlexItem::AlignSelf::stretch)
                                          .withMargin(knobMarginSmall));
                        box.items.add(juce::FlexItem(thruSwitch)
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
                        box.items.add(juce::FlexItem(beatGauge).withFlex(1).withMargin(knobMarginSmall));
                        box.performLayout(areas[1].toFloat());
                    }
                    break;
                }
                default:
                    break;
            }
        }
    }
#pragma GCC diagnostic pop

    void timerCallback() override
    {
        if (processorRef.hasRunner())
        {
            levelMeterGauge.setLevels(processorRef.getLevelSnapshot());
            spectrumGauge.setSpectrum(processorRef.getSpectrumSnapshot());
            tunerGauge.setPitch(processorRef.getPitchSnapshot());
            xyScopeGauge.setPoints(processorRef.getXySnapshot());
            beatGauge.setBeat(processorRef.getBeatSnapshot());
            holdResetSwitch.tickFlash();
            beatResetSwitch.tickFlash();

            processorRef.setPitchTrackingEnabled(juce::String(kPageNames[m_currentPage]) == "Tuner");
        }
    }

    void initWidgets()
    {
        addAndMakeVisible(peakFallDrop);
        peakFallDrop.addItemList(valueTreeState.getParameter("peakFall")->getAllValueStrings(), 1);
        peakFallDropAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
            valueTreeState, "peakFall", peakFallDrop);
        peakFallDrop.setTooltip(juce::String::fromUTF8("Peak Fall (6 dB/s, 11.8 dB/s (PPM), 24 dB/s, 48 dB/s)"));
        addAndMakeVisible(rmsWindowDrop);
        rmsWindowDrop.addItemList(valueTreeState.getParameter("rmsWindow")->getAllValueStrings(), 1);
        rmsWindowDropAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
            valueTreeState, "rmsWindow", rmsWindowDrop);
        rmsWindowDrop.setTooltip(juce::String::fromUTF8("RMS Window (50 ms, 300 ms (VU), 1 s)"));
        addAndMakeVisible(peakHoldDrop);
        peakHoldDrop.addItemList(valueTreeState.getParameter("peakHold")->getAllValueStrings(), 1);
        peakHoldDropAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
            valueTreeState, "peakHold", peakHoldDrop);
        peakHoldDrop.setTooltip(juce::String::fromUTF8("Peak Hold (Off, 1 s, 3 s, Infinite)"));
        addAndMakeVisible(holdResetSwitch);
        holdResetSwitchAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
            valueTreeState, "holdReset", holdResetSwitch);
        holdResetSwitch.setTooltip(juce::String::fromUTF8("Reset Hold"));

        addAndMakeVisible(fftSizeDrop);
        fftSizeDrop.addItemList(valueTreeState.getParameter("fftSize")->getAllValueStrings(), 1);
        fftSizeDropAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
            valueTreeState, "fftSize", fftSizeDrop);
        fftSizeDrop.setTooltip(juce::String::fromUTF8("FFT Size (2048, 4096, 8192, 16384)"));
        addAndMakeVisible(fftFallDrop);
        fftFallDrop.addItemList(valueTreeState.getParameter("fftFall")->getAllValueStrings(), 1);
        fftFallDropAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
            valueTreeState, "fftFall", fftFallDrop);
        fftFallDrop.setTooltip(juce::String::fromUTF8("FFT Fall (Instant, 12 dB/s, 24 dB/s, 48 dB/s)"));
        addAndMakeVisible(spectrumPeakHoldSwitch);
        spectrumPeakHoldSwitchAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
            valueTreeState, "spectrumPeakHold", spectrumPeakHoldSwitch);
        spectrumPeakHoldSwitch.setTooltip(juce::String::fromUTF8("Peak Curve"));

        addAndMakeVisible(referencePitchDial);
        referencePitchDial.reset(valueTreeState, "referencePitch");
        referencePitchDial.setLabelText(juce::String::fromUTF8("A4"));
        referencePitchDial.setTooltip(juce::String::fromUTF8("A4 (415 to 466 Hz)"));
        addAndMakeVisible(pitchSourceDrop);
        pitchSourceDrop.addItemList(valueTreeState.getParameter("pitchSource")->getAllValueStrings(), 1);
        pitchSourceDropAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
            valueTreeState, "pitchSource", pitchSourceDrop);
        pitchSourceDrop.setTooltip(juce::String::fromUTF8("Pitch Source (L+R, L, R)"));
        addAndMakeVisible(beatResetSwitch);
        beatResetSwitchAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
            valueTreeState, "beatReset", beatResetSwitch);
        beatResetSwitch.setTooltip(juce::String::fromUTF8("Reset Beat"));

        addAndMakeVisible(thruSwitch);
        thruSwitchAttachment =
            std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(valueTreeState, "thru", thruSwitch);
        thruSwitch.setTooltip(juce::String::fromUTF8("Thru"));

        addAndMakeVisible(levelMeterGauge);
        levelMeterGauge.setLabelText(juce::String::fromUTF8("Levels"));
        addAndMakeVisible(spectrumGauge);
        spectrumGauge.setLabelText(juce::String::fromUTF8("Spectrum"));
        addAndMakeVisible(tunerGauge);
        tunerGauge.setLabelText(juce::String::fromUTF8("Tuner"));
        addAndMakeVisible(xyScopeGauge);
        xyScopeGauge.setLabelText(juce::String::fromUTF8("XY"));
        addAndMakeVisible(beatGauge);
        beatGauge.setLabelText(juce::String::fromUTF8("Beat"));

        for (size_t i = 0; i < m_pageButtons.size(); ++i)
        {
            m_pageButtons[i].setButtonText(kPageNames[i]);
            m_pageButtons[i].onClick = [this, i] { switchPage(i); };
            addAndMakeVisible(m_pageButtons[i]);
        }
        switchPage(0);
    }

    void switchPage(const size_t page)
    {
        m_currentPage = page;
        for (size_t i = 0; i < m_pageButtons.size(); ++i)
        {
            m_pageButtons[i].setToggleState(i == page, juce::dontSendNotification);
        }
        peakFallDrop.setVisible(page == 0);
        rmsWindowDrop.setVisible(page == 0);
        peakHoldDrop.setVisible(page == 0);
        holdResetSwitch.setVisible(page == 0);
        fftSizeDrop.setVisible(page == 1);
        fftFallDrop.setVisible(page == 1);
        spectrumPeakHoldSwitch.setVisible(page == 1);
        referencePitchDial.setVisible(page == 2);
        pitchSourceDrop.setVisible(page == 2);
        beatResetSwitch.setVisible(page == 4);
        thruSwitch.setVisible(true);
        levelMeterGauge.setVisible(page == 0);
        spectrumGauge.setVisible(page == 1);
        tunerGauge.setVisible(page == 2);
        xyScopeGauge.setVisible(page == 3);
        beatGauge.setVisible(page == 4);

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

        names.add("About");
        return names;
    }

    juce::PopupMenu getMenuForIndex(int /*menuIndex*/, const juce::String& menuName) override
    {
        if (menuName == "Theme")
        {
            return buildThemeMenu();
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
            "Measurement tool for musicians: levels, spectrum, tuner, XY scope and beat finder.\n\nPart of the AbacDsp "
            "project - core DSP library is MIT licensed.\n\nBuilt with JUCE, licensed under AGPLv3 (or a commercial "
            "JUCE licence).\n\nFull third-party license details: THIRD-PARTY-LICENSES.md in the AbacDsp repository.";

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

    // The unique, sorted set of folder prefixes already used by an existing name list
    // (each entry "folder/leaf" or a root-level "leaf"), for a Save/Rename dialog's
    // folder dropdown.
    [[nodiscard]] static juce::StringArray collectFolderNames(const std::vector<juce::String>& names)
    {
        juce::StringArray folders;
        for (const auto& fullName : names)
        {
            const int slashIndex = fullName.lastIndexOfChar('/');
            if (slashIndex >= 0)
            {
                folders.addIfNotAlreadyThere(fullName.substring(0, slashIndex));
            }
        }
        folders.sort(false);
        return folders;
    }

    // Adds an editable folder dropdown to a Save/Rename dialog: existing folders to
    // pick from, or type a new one in the same box. currentValue empty means root.
    static void addFolderComboBox(juce::AlertWindow& dialog, const std::vector<juce::String>& existingNames,
                                  const juce::String& currentValue)
    {
        juce::StringArray items{"(none)"};
        items.addArray(collectFolderNames(existingNames));
        dialog.addComboBox("folder", items, "Folder:");
        if (auto* combo = dialog.getComboBoxComponent("folder"))
        {
            combo->setEditableText(true);
            combo->setText(currentValue.isEmpty() ? "(none)" : currentValue, juce::dontSendNotification);
        }
    }

    // Reads back addFolderComboBox()'s current value (picked or freely typed),
    // mapping the "(none)" placeholder back to root/empty.
    [[nodiscard]] static juce::String readFolderComboBox(const juce::AlertWindow& dialog)
    {
        if (auto* combo = dialog.getComboBoxComponent("folder"))
        {
            const auto text = combo->getText().trim();
            return text == "(none)" ? juce::String() : text;
        }
        return {};
    }


  private:
    AudioPluginAudioProcessor& processorRef;
    juce::AudioProcessorValueTreeState& valueTreeState;
    std::unique_ptr<GuiLookAndFeel> m_laf;
    juce::Colour backgroundApp;
    juce::MenuBarComponent m_menuBar;
    StatusBar m_statusBar;
    juce::TooltipWindow m_tooltipWindow{this};
    juce::Component* m_topLevel{nullptr};
    bool m_boundsRestored{false};
    GuiConstants::Theme m_currentTheme{AppSettings::loadTheme()};
    static constexpr auto kPageNames = std::to_array<const char*>({"Levels", "Spectrum", "Tuner", "XY", "Beat"});
    std::array<juce::TextButton, kPageNames.size()> m_pageButtons;
    size_t m_currentPage{0};
    static constexpr int kThemeModeLightId = 9000;
    static constexpr int kThemeModeDarkId = 9001;
    static constexpr int kThemeBaseBichromaticId = 9002;
    static constexpr int kThemeBaseTrichromaticId = 9003;
    static constexpr int kAboutShowInfoId = 14000;


    juce::ComboBox peakFallDrop{};
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> peakFallDropAttachment;
    juce::ComboBox rmsWindowDrop{};
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> rmsWindowDropAttachment;
    juce::ComboBox peakHoldDrop{};
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> peakHoldDropAttachment;
    MomentaryToggleButton holdResetSwitch{juce::String::fromUTF8("Reset Hold")};
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> holdResetSwitchAttachment;
    juce::ComboBox fftSizeDrop{};
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> fftSizeDropAttachment;
    juce::ComboBox fftFallDrop{};
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> fftFallDropAttachment;
    juce::ToggleButton spectrumPeakHoldSwitch{juce::String::fromUTF8("Peak Curve")};
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> spectrumPeakHoldSwitchAttachment;
    CustomRotaryDial referencePitchDial{this};
    juce::ComboBox pitchSourceDrop{};
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> pitchSourceDropAttachment;
    MomentaryToggleButton beatResetSwitch{juce::String::fromUTF8("Reset Beat")};
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> beatResetSwitchAttachment;
    juce::ToggleButton thruSwitch{juce::String::fromUTF8("Thru")};
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> thruSwitchAttachment;
    LevelMeterDisplay levelMeterGauge{};
    SpectrumDisplay spectrumGauge{};
    TunerDisplay tunerGauge{};
    XyScopeDisplay xyScopeGauge{};
    BeatDisplay beatGauge{};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioPluginAudioProcessorEditor)
};
