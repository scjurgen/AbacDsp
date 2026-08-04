#pragma once
/*
 * AUTO GENERATED,
 * NOT A GOOD IDEA TO CHANGE STUFF HERE
 * Keep the file readonly
 */

#include <map>

#include "/*MODULE_UPPER*/Processor.h"
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
        /*START_PERFORMANCEPAGE*/
        auto pageSwitchArea = area.removeFromTop(static_cast<int>(Constants::Text::labelHeight));
        m_pagePerformanceButton.setBounds(pageSwitchArea.removeFromLeft(pageSwitchArea.getWidth() / 2));
        m_pageSettingsButton.setBounds(pageSwitchArea);
        /*END_PERFORMANCEPAGE*/
        area = area.reduced(static_cast<int>(Constants::Margins::big));
        /*START_PERFORMANCEPAGE*/
        if (m_currentPage == Page::Performance)
        {
            /*RESIZED_AREA_PERFORMANCE*/
        }
        else
        /*END_PERFORMANCEPAGE*/
        {
            /*RESIZED_AREA*/
        }
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
        /*START_PERFORMANCEPAGE*/
        addAndMakeVisible(m_pagePerformanceButton);
        addAndMakeVisible(m_pageSettingsButton);
        m_pagePerformanceButton.onClick = [this] { switchPage(Page::Performance); };
        m_pageSettingsButton.onClick = [this] { switchPage(Page::Settings); };
        switchPage(Page::Performance);
        /*END_PERFORMANCEPAGE*/
    }

    /*START_PERFORMANCEPAGE*/
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
            /*PAGE_SHOW_PERFORMANCE*/
        }
        else
        {
            /*PAGE_SHOW_SETTINGS*/
        }
        resized();
    }
    /*END_PERFORMANCEPAGE*/

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
        /*START_PRESETBROWSER*/
        names.add("Patches");
        /*END_PRESETBROWSER*/
        /*START_LOOPBROWSER*/
        names.add("Loops");
        /*END_LOOPBROWSER*/
        return names;
    }

    juce::PopupMenu getMenuForIndex(int /*menuIndex*/, const juce::String& menuName) override
    {
        if (menuName == "Theme")
        {
            return buildThemeMenu();
        }
        /*START_PRESETBROWSER*/
        if (menuName == "Patches")
        {
            return buildPatchesMenu();
        }
        /*END_PRESETBROWSER*/
        /*START_LOOPBROWSER*/
        if (menuName == "Loops")
        {
            return buildLoopsMenu();
        }
        /*END_LOOPBROWSER*/
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
        /*START_PRESETBROWSER*/
        handlePatchMenuSelection(menuItemID);
        /*END_PRESETBROWSER*/
        /*START_LOOPBROWSER*/
        handleLoopMenuSelection(menuItemID);
        /*END_LOOPBROWSER*/
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
        /*APPLY_THEME_CALLBACKS*/
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

    /*START_PRESETBROWSER*/
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
    /*END_PRESETBROWSER*/

    /*START_LOOPBROWSER*/
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
            if (!outcome.patchParamsJson.empty())
            {
                processorRef.applyLoadedLoopPatchParams(juce::String(outcome.patchParamsJson));
            }
            m_statusBar.showMessage("Loaded");
        }
        else
        {
            const auto wavBpm = outcome.wavBpm;
            const auto jsonBpm = outcome.jsonBpm;
            const auto patchParamsJson = juce::String(outcome.patchParamsJson);
            juce::NativeMessageBox::showAsync(
                juce::MessageBoxOptions()
                    .withIconType(juce::MessageBoxIconType::QuestionIcon)
                    .withTitle("Tempo Mismatch")
                    .withMessage("The saved tempo doesn't match the file's embedded tempo. Which one should be used?")
                    .withButton(juce::String::fromUTF8("File (") + juce::String(wavBpm, 1) + " BPM)")
                    .withButton(juce::String::fromUTF8("Saved (") + juce::String(jsonBpm, 1) + " BPM)"),
                [this, wavBpm, jsonBpm, patchParamsJson](int result)
                {
                    processorRef.resolveLoopLoadBpm(result == 0 ? wavBpm : jsonBpm);
                    if (patchParamsJson.isNotEmpty())
                    {
                        processorRef.applyLoadedLoopPatchParams(patchParamsJson);
                    }
                    m_statusBar.showMessage("Loaded");
                });
        }
    }
    /*END_LOOPBROWSER*/

    /*EXTRA_PRIVATE_METHODS*/
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
    /*START_PERFORMANCEPAGE*/
    Page m_currentPage{Page::Performance};
    juce::TextButton m_pagePerformanceButton{"Performance"};
    juce::TextButton m_pageSettingsButton{"Settings"};
    /*END_PERFORMANCEPAGE*/
    static constexpr int kThemeModeLightId = 9000;
    static constexpr int kThemeModeDarkId = 9001;
    static constexpr int kThemeBaseBichromaticId = 9002;
    static constexpr int kThemeBaseTrichromaticId = 9003;
    /*START_PRESETBROWSER*/
    static constexpr int kPatchSaveId = 1000;
    static constexpr int kPatchSaveAsId = 1001;
    static constexpr int kPatchLoadIdBase = 2000;
    static constexpr int kPatchDeleteIdBase = 3000;
    static constexpr int kPatchRenameIdBase = 4000;
    std::unique_ptr<juce::AlertWindow> m_patchNameDialog;
    std::vector<juce::String> m_patchMenuNames;
    /*END_PRESETBROWSER*/

    /*START_LOOPBROWSER*/
    static constexpr int kLoopSaveAsId = 5000;
    static constexpr int kLoopLoadIdBase = 6000;
    static constexpr int kLoopDeleteIdBase = 7000;
    static constexpr int kLoopRenameIdBase = 8000;
    std::unique_ptr<juce::AlertWindow> m_loopNameDialog;
    std::vector<juce::String> m_loopMenuNames;
    /*END_LOOPBROWSER*/

    /*WIDGETS_DECL*/
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioPluginAudioProcessorEditor)
};
