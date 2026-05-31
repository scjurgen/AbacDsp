#pragma once
/*
 * AUTO GENERATED,
 * NOT A GOOD IDEA TO CHANGE STUFF HERE
 * Keep the file readonly
 */

#include "/*MODULE_UPPER*/Processor.h"
#include "UiElements.h"


//==============================================================================
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
        , backgroundApp(juce::Colour(GuiConstants::instance().colors.bg_App))
        , m_menuBar(this)
    {
        setLookAndFeel(&m_laf);
        addAndMakeVisible(m_menuBar);
        initWidgets();
        setResizable(true, true);
        setResizeLimits(GuiConstants::instance().init.WindowWidth, GuiConstants::instance().init.WindowHeight, 4000,
                        3000);
        const auto saved = AppSettings::loadWindowBounds(GuiConstants::instance().init.WindowWidth,
                                                         GuiConstants::instance().init.WindowHeight);
        setSize(saved.getWidth(), saved.getHeight());
        startTimerHz(GuiConstants::instance().init.TimerHertz);
    }

    ~AudioPluginAudioProcessorEditor() override
    {
        if (m_topLevel != nullptr)
        {
            m_topLevel->removeComponentListener(this);
        }
        stopTimer();
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
        area = area.reduced(static_cast<int>(Constants::Margins::big));
        /*RESIZED_AREA*/
    }
#pragma GCC diagnostic pop

    void timerCallback() override
    {
        if (processorRef.hasRunner())
        {
            /*TIMER_CALLBACKS*/
        }
    }

    void initWidgets()
    {
        /*INIT_WIDGETS*/
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

        if (!m_boundsRestored && m_topLevel->isOnDesktop())
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
        return {"Settings"};
    }

    juce::PopupMenu getMenuForIndex(int menuIndex, const juce::String&) override
    {
        juce::PopupMenu menu;
        if (menuIndex == 0)
        {
            static constexpr auto kThemeNames =
                std::to_array<const char*>({"Classic", "Viridis", "Inferno", "Grayscale", "Heat", "Ink", "Teal"});
            juce::PopupMenu themeMenu;
            for (int i = 0; i < static_cast<int>(kThemeNames.size()); ++i)
            {
                themeMenu.addItem(i + 1, kThemeNames[static_cast<size_t>(i)]);
            }
            menu.addSubMenu("Theme", themeMenu);
            menu.addSeparator();
            menu.addItem(kAudioSettingsId, "Audio Settings");
        }
        return menu;
    }

    void menuItemSelected(int menuItemID, int /*topLevelMenuIndex*/) override
    {
        static constexpr auto kPresets = std::to_array<GuiConstants::GradientPreset>({
            GuiConstants::GradientPreset::Classic,
            GuiConstants::GradientPreset::Viridis,
            GuiConstants::GradientPreset::Inferno,
            GuiConstants::GradientPreset::Grayscale,
            GuiConstants::GradientPreset::Heat,
            GuiConstants::GradientPreset::Ink,
            GuiConstants::GradientPreset::Teal,
        });
        if (menuItemID >= 1 && menuItemID <= static_cast<int>(kPresets.size()))
        {
            AppSettings::saveTheme(kPresets[static_cast<size_t>(menuItemID - 1)]);
            juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::InfoIcon, "Theme",
                                                   "Theme will take effect after restart.");
        }
        else if (menuItemID == kAudioSettingsId)
        {
        }
    }

    /*EXTRA_PRIVATE_METHODS*/
  private:
    static constexpr int kAudioSettingsId = 100;

    AudioPluginAudioProcessor& processorRef;
    juce::AudioProcessorValueTreeState& valueTreeState;
    GuiLookAndFeel m_laf;
    juce::Colour backgroundApp;
    juce::MenuBarComponent m_menuBar;
    juce::Component* m_topLevel{nullptr};
    bool m_boundsRestored{false};

    /*WIDGETS_DECL*/
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioPluginAudioProcessorEditor)
};
