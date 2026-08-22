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
        /*START_SCRIPTBROWSER*/
        initLlmAssist();
        /*END_SCRIPTBROWSER*/
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
        /*START_SCRIPTBROWSER*/
        names.add("Scripts");
        /*END_SCRIPTBROWSER*/
        names.add("About");
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
        /*START_SCRIPTBROWSER*/
        if (menuName == "Scripts")
        {
            return buildScriptsMenu();
        }
        /*END_SCRIPTBROWSER*/
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
        const juce::String body = juce::String(JucePlugin_Manufacturer) + "\n\n"
                                                                          "/*ABOUT_TEXT*/";

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
        /*START_PRESETBROWSER*/
        handlePatchMenuSelection(menuItemID);
        /*END_PRESETBROWSER*/
        /*START_LOOPBROWSER*/
        handleLoopMenuSelection(menuItemID);
        /*END_LOOPBROWSER*/
        /*START_SCRIPTBROWSER*/
        handleScriptMenuSelection(menuItemID);
        /*END_SCRIPTBROWSER*/
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

    /*START_PRESETBROWSER*/
    // A "/" in a name (e.g. "chorus/classic tri chorus") groups it under a folder
    // submenu; root-level entries stay directly in the returned menu. Shared by the
    // patches and (when present) loops menus.
    juce::PopupMenu buildGroupedMenu(const std::vector<juce::String>& names, int idBase,
                                     const juce::String& tickedName = {})
    {
        juce::PopupMenu rootMenu;
        std::map<juce::String, juce::PopupMenu> folderMenus;
        for (size_t i = 0; i < names.size(); ++i)
        {
            const auto& fullName = names[i];
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

    juce::PopupMenu buildGroupedPatchMenu(int idBase, const juce::String& tickedName = {})
    {
        return buildGroupedMenu(m_patchMenuNames, idBase, tickedName);
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
        const auto [folder, name] = splitFolderAndName(processorRef.getCurrentPatchName());
        m_patchNameDialog =
            std::make_unique<juce::AlertWindow>("Save Patch", juce::String(), juce::MessageBoxIconType::NoIcon);
        addFolderComboBox(*m_patchNameDialog, m_patchMenuNames, folder);
        m_patchNameDialog->addTextEditor("name", name, "Name:");
        m_patchNameDialog->addButton("Save", 1, juce::KeyPress(juce::KeyPress::returnKey));
        m_patchNameDialog->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));
        m_patchNameDialog->enterModalState(true,
                                           juce::ModalCallbackFunction::create(
                                               [this](int result)
                                               {
                                                   const auto folderText = readFolderComboBox(*m_patchNameDialog);
                                                   const auto nameText =
                                                       m_patchNameDialog->getTextEditorContents("name").trim();
                                                   m_patchNameDialog.reset();
                                                   if (result != 1 || nameText.isEmpty())
                                                   {
                                                       return;
                                                   }
                                                   const auto fullName = combineFolderAndName(folderText, nameText);
                                                   if (processorRef.saveCurrentPatchAs(fullName))
                                                   {
                                                       m_statusBar.showMessage("Saved '" + fullName + "'");
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
        const auto [folder, name] = splitFolderAndName(oldName);
        m_patchNameDialog = std::make_unique<juce::AlertWindow>("Rename Patch \"" + oldName + "\"", juce::String(),
                                                                juce::MessageBoxIconType::NoIcon);
        addFolderComboBox(*m_patchNameDialog, m_patchMenuNames, folder);
        m_patchNameDialog->addTextEditor("name", name, "Name:");
        m_patchNameDialog->addButton("Rename", 1, juce::KeyPress(juce::KeyPress::returnKey));
        m_patchNameDialog->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));
        m_patchNameDialog->enterModalState(true,
                                           juce::ModalCallbackFunction::create(
                                               [this, oldName](int result)
                                               {
                                                   const auto folderText = readFolderComboBox(*m_patchNameDialog);
                                                   const auto nameText =
                                                       m_patchNameDialog->getTextEditorContents("name").trim();
                                                   m_patchNameDialog.reset();
                                                   if (result != 1 || nameText.isEmpty())
                                                   {
                                                       return;
                                                   }
                                                   const auto newName = combineFolderAndName(folderText, nameText);
                                                   if (newName == oldName)
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
    // Reuses buildGroupedMenu() from the PRESETBROWSER section above; a blueprint
    // with loops but no patches would need that helper pulled out of its guard.
    juce::PopupMenu buildLoopsMenu()
    {
        m_loopMenuNames = processorRef.listLoopNames();
        const auto currentName = processorRef.getCurrentLoopName();

        auto loadMenu = buildGroupedMenu(m_loopMenuNames, kLoopLoadIdBase, currentName);
        auto deleteMenu = buildGroupedMenu(m_loopMenuNames, kLoopDeleteIdBase);
        auto renameMenu = buildGroupedMenu(m_loopMenuNames, kLoopRenameIdBase);

        juce::PopupMenu loops;
        loops.addSubMenu("Load", loadMenu, !m_loopMenuNames.empty());
        loops.addItem(kLoopSaveId, "Save", !currentName.isEmpty());
        loops.addItem(kLoopSaveAsId, "Save As...");
        loops.addSubMenu("Delete", deleteMenu, !m_loopMenuNames.empty());
        loops.addSubMenu("Rename", renameMenu, !m_loopMenuNames.empty());
        return loops;
    }

    void handleLoopMenuSelection(int menuItemID)
    {
        if (menuItemID == kLoopSaveId)
        {
            const auto currentName = processorRef.getCurrentLoopName();
            if (!currentName.isEmpty())
            {
                processorRef.saveLoopAs(currentName);
                m_statusBar.showMessage("Saving '" + currentName + "'...");
            }
        }
        else if (menuItemID == kLoopSaveAsId)
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
        m_loopNameDialog =
            std::make_unique<juce::AlertWindow>("Save Loop", juce::String(), juce::MessageBoxIconType::NoIcon);
        addFolderComboBox(*m_loopNameDialog, m_loopMenuNames, "");
        m_loopNameDialog->addTextEditor("name", "", "Name:");
        m_loopNameDialog->addButton("Save", 1, juce::KeyPress(juce::KeyPress::returnKey));
        m_loopNameDialog->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));
        m_loopNameDialog->enterModalState(true,
                                          juce::ModalCallbackFunction::create(
                                              [this](int result)
                                              {
                                                  const auto folderText = readFolderComboBox(*m_loopNameDialog);
                                                  const auto nameText =
                                                      m_loopNameDialog->getTextEditorContents("name").trim();
                                                  m_loopNameDialog.reset();
                                                  if (result != 1 || nameText.isEmpty())
                                                  {
                                                      return;
                                                  }
                                                  const auto fullName = combineFolderAndName(folderText, nameText);
                                                  processorRef.saveLoopAs(fullName);
                                                  m_statusBar.showMessage("Saving '" + fullName + "'...");
                                              }),
                                          false);
        focusNameEditor(*m_loopNameDialog);
    }

    void promptRenameLoop(const juce::String& oldName)
    {
        const auto [folder, name] = splitFolderAndName(oldName);
        m_loopNameDialog = std::make_unique<juce::AlertWindow>("Rename Loop \"" + oldName + "\"", juce::String(),
                                                               juce::MessageBoxIconType::NoIcon);
        addFolderComboBox(*m_loopNameDialog, m_loopMenuNames, folder);
        m_loopNameDialog->addTextEditor("name", name, "Name:");
        m_loopNameDialog->addButton("Rename", 1, juce::KeyPress(juce::KeyPress::returnKey));
        m_loopNameDialog->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));
        m_loopNameDialog->enterModalState(true,
                                          juce::ModalCallbackFunction::create(
                                              [this, oldName](int result)
                                              {
                                                  const auto folderText = readFolderComboBox(*m_loopNameDialog);
                                                  const auto nameText =
                                                      m_loopNameDialog->getTextEditorContents("name").trim();
                                                  m_loopNameDialog.reset();
                                                  if (result != 1 || nameText.isEmpty())
                                                  {
                                                      return;
                                                  }
                                                  const auto newName = combineFolderAndName(folderText, nameText);
                                                  if (newName == oldName)
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

    /*START_SCRIPTBROWSER*/
    // Reuses buildGroupedMenu() from the PRESETBROWSER section above, same as
    // LOOPBROWSER does - a blueprint with a script port but no patches would need that
    // helper pulled out of its guard.
    void openScriptEditor()
    {
        // Non-modal (see ScriptEditorDialogWindow), so this can already be open - just
        // bring it forward rather than spawning a second editor.
        if (m_scriptEditorWindow != nullptr)
        {
            m_scriptEditorWindow->toFront(true);
            return;
        }

        juce::StringArray libraryScriptNames;
        for (const auto& n : processorRef.getLibraryScriptNames())
        {
            libraryScriptNames.add(n);
        }

        auto* editorComponent = new ScriptEditorWindow();
        editorComponent->setScriptText(processorRef.getScriptText());
        // While LLM-Assist is active, a manual edit could race with (and silently lose
        // to) a script the watcher applies from the folder - view-only instead of blocked.
        editorComponent->setReadOnly(m_llmAssistActive);
        editorComponent->setLibraryScripts(libraryScriptNames, [this](const juce::String& name)
                                           { return processorRef.getLibraryScriptText(name); });
        editorComponent->onApply = [this](const juce::String& text) -> juce::String
        {
            if (processorRef.applyScriptText(text))
            {
                m_statusBar.showMessage("Script applied");
                return {};
            }
            return juce::String(processorRef.scriptErrorMessage());
        };
        editorComponent->onReset = [this] { return juce::String(processorRef.getScriptSkeleton()); };

        auto* dialogWindow =
            new ScriptEditorDialogWindow("Edit Script", juce::Colour(GuiConstants::instance().colors.background));
        dialogWindow->setContentOwned(editorComponent, true);
        dialogWindow->setUsingNativeTitleBar(true);
        dialogWindow->setResizable(true, false);
        const auto savedBounds = AppSettings::loadScriptEditorBounds();
        if (savedBounds)
        {
            dialogWindow->setBounds(*savedBounds);
        }
        else
        {
            dialogWindow->centreAroundComponent(nullptr, dialogWindow->getWidth(), dialogWindow->getHeight());
        }
        dialogWindow->setVisible(true);
        // setVisible() can itself shift the window once the OS actually places it on
        // screen - reapply so it lands exactly where it was left, not off by that shift.
        if (savedBounds)
        {
            dialogWindow->setBounds(*savedBounds);
        }
        dialogWindow->armBoundsPersistence();
        m_scriptEditorWindow = dialogWindow;
        m_scriptEditorContent = editorComponent;
    }

    // Apply-time only catches errors the script hits while its top-level chunk runs
    // (i.e. at load); a script that compiles fine but errors when NextNotes()/OnTiming()
    // are actually called later (on the audio thread, once real data flows through it)
    // has nowhere else to surface that - poll for it instead. Called every timer tick
    // (see extra_timer_callbacks); tracks the last-shown message so a persistent error
    // doesn't keep resetting the status bar's fade timer forever.
    void pollScriptError()
    {
        if (!processorRef.hasScriptError())
        {
            m_lastScriptErrorShown.clear();
            return;
        }
        const auto message = juce::String(processorRef.scriptErrorMessage());
        if (message == m_lastScriptErrorShown)
        {
            return;
        }
        m_lastScriptErrorShown = message;
        m_statusBar.showMessage("Script error: " + message);
    }

    juce::PopupMenu buildScriptsMenu()
    {
        m_scriptMenuNames = processorRef.listScriptNames();
        const auto currentName = processorRef.getCurrentScriptName();

        auto loadMenu = buildGroupedMenu(m_scriptMenuNames, kScriptLoadIdBase, currentName);
        auto deleteMenu = buildGroupedMenu(m_scriptMenuNames, kScriptDeleteIdBase);
        auto renameMenu = buildGroupedMenu(m_scriptMenuNames, kScriptRenameIdBase);

        juce::PopupMenu scripts;
        scripts.addItem(kScriptEditId, "Edit...");
        scripts.addSubMenu("Load", loadMenu, !m_scriptMenuNames.empty());
        scripts.addItem(kScriptSaveAsId, "Save As...");
        scripts.addSubMenu("Delete", deleteMenu, !m_scriptMenuNames.empty());
        scripts.addSubMenu("Rename", renameMenu, !m_scriptMenuNames.empty());
        scripts.addSeparator();
        scripts.addSubMenu("LLM-Assist", buildLlmAssistMenu());
        return scripts;
    }

    juce::PopupMenu buildLlmAssistMenu()
    {
        juce::PopupMenu menu;
        menu.addItem(kLlmAssistToggleId, m_llmAssistActive ? "Disable" : "Enable", true, m_llmAssistActive);
        menu.addItem(kLlmAssistChooseFolderId, "Choose Folder...");
        return menu;
    }

    void handleScriptMenuSelection(int menuItemID)
    {
        if (menuItemID == kLlmAssistToggleId)
        {
            toggleLlmAssist();
        }
        else if (menuItemID == kLlmAssistChooseFolderId)
        {
            chooseLlmAssistFolder(false);
        }
        else if (menuItemID == kScriptEditId)
        {
            openScriptEditor();
        }
        else if (menuItemID == kScriptSaveAsId)
        {
            promptSaveScriptAs();
        }
        else if (menuItemID >= kScriptLoadIdBase &&
                 menuItemID < kScriptLoadIdBase + static_cast<int>(m_scriptMenuNames.size()))
        {
            const auto& name = m_scriptMenuNames[static_cast<size_t>(menuItemID - kScriptLoadIdBase)];
            if (processorRef.requestLoadScript(name))
            {
                m_statusBar.showMessage("Loaded '" + name + "'");
            }
            else
            {
                m_statusBar.showMessage("Load failed");
            }
        }
        else if (menuItemID >= kScriptDeleteIdBase &&
                 menuItemID < kScriptDeleteIdBase + static_cast<int>(m_scriptMenuNames.size()))
        {
            confirmAndDeleteScript(m_scriptMenuNames[static_cast<size_t>(menuItemID - kScriptDeleteIdBase)]);
        }
        else if (menuItemID >= kScriptRenameIdBase &&
                 menuItemID < kScriptRenameIdBase + static_cast<int>(m_scriptMenuNames.size()))
        {
            promptRenameScript(m_scriptMenuNames[static_cast<size_t>(menuItemID - kScriptRenameIdBase)]);
        }
    }

    void promptSaveScriptAs()
    {
        m_scriptNameDialog =
            std::make_unique<juce::AlertWindow>("Save Script", juce::String(), juce::MessageBoxIconType::NoIcon);
        addFolderComboBox(*m_scriptNameDialog, m_scriptMenuNames, "");
        m_scriptNameDialog->addTextEditor("name", "", "Name:");
        m_scriptNameDialog->addButton("Save", 1, juce::KeyPress(juce::KeyPress::returnKey));
        m_scriptNameDialog->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));
        m_scriptNameDialog->enterModalState(true,
                                            juce::ModalCallbackFunction::create(
                                                [this](int result)
                                                {
                                                    const auto folderText = readFolderComboBox(*m_scriptNameDialog);
                                                    const auto nameText =
                                                        m_scriptNameDialog->getTextEditorContents("name").trim();
                                                    m_scriptNameDialog.reset();
                                                    if (result != 1 || nameText.isEmpty())
                                                    {
                                                        return;
                                                    }
                                                    const auto fullName = combineFolderAndName(folderText, nameText);
                                                    if (processorRef.saveCurrentScriptAs(fullName))
                                                    {
                                                        m_statusBar.showMessage("Saved '" + fullName + "'");
                                                    }
                                                    else
                                                    {
                                                        m_statusBar.showMessage("Save failed");
                                                    }
                                                }),
                                            false);
        focusNameEditor(*m_scriptNameDialog);
    }

    void promptRenameScript(const juce::String& oldName)
    {
        const auto [folder, name] = splitFolderAndName(oldName);
        m_scriptNameDialog = std::make_unique<juce::AlertWindow>("Rename Script \"" + oldName + "\"", juce::String(),
                                                                 juce::MessageBoxIconType::NoIcon);
        addFolderComboBox(*m_scriptNameDialog, m_scriptMenuNames, folder);
        m_scriptNameDialog->addTextEditor("name", name, "Name:");
        m_scriptNameDialog->addButton("Rename", 1, juce::KeyPress(juce::KeyPress::returnKey));
        m_scriptNameDialog->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));
        m_scriptNameDialog->enterModalState(true,
                                            juce::ModalCallbackFunction::create(
                                                [this, oldName](int result)
                                                {
                                                    const auto folderText = readFolderComboBox(*m_scriptNameDialog);
                                                    const auto nameText =
                                                        m_scriptNameDialog->getTextEditorContents("name").trim();
                                                    m_scriptNameDialog.reset();
                                                    if (result != 1 || nameText.isEmpty())
                                                    {
                                                        return;
                                                    }
                                                    const auto newName = combineFolderAndName(folderText, nameText);
                                                    if (newName == oldName)
                                                    {
                                                        return;
                                                    }
                                                    if (processorRef.renameScript(oldName, newName))
                                                    {
                                                        m_statusBar.showMessage("Renamed to '" + newName + "'");
                                                    }
                                                    else
                                                    {
                                                        m_statusBar.showMessage("Rename failed");
                                                    }
                                                }),
                                            false);
        focusNameEditor(*m_scriptNameDialog);
    }

    void confirmAndDeleteScript(const juce::String& name)
    {
        juce::NativeMessageBox::showAsync(juce::MessageBoxOptions()
                                              .withIconType(juce::MessageBoxIconType::WarningIcon)
                                              .withTitle("Delete Script")
                                              .withMessage("Delete script \"" + name + "\"?")
                                              .withButton("Yes")
                                              .withButton("No"),
                                          [this, name](int result)
                                          {
                                              if (result != 0)
                                              {
                                                  return;
                                              }
                                              if (processorRef.deleteScriptNamed(name))
                                              {
                                                  m_statusBar.showMessage("Deleted '" + name + "'");
                                              }
                                              else
                                              {
                                                  m_statusBar.showMessage("Delete failed");
                                              }
                                          });
    }

    void initLlmAssist()
    {
        m_llmAssistWatcher.applyScriptText = [this](const juce::String& text)
        { return processorRef.applyScriptText(text); };
        m_llmAssistWatcher.scriptErrorMessage = [this] { return juce::String(processorRef.scriptErrorMessage()); };
        m_llmAssistWatcher.currentPatchName = [this] { return processorRef.getCurrentPatchName(); };
        m_llmAssistWatcher.currentScriptText = [this] { return processorRef.getScriptText(); };
        m_llmAssistWatcher.saveUserLibraryScript = [this](const juce::String& name, const juce::String& content)
        { return processorRef.saveUserLibraryScript(name, content); };
    }

    void toggleLlmAssist()
    {
        if (m_llmAssistActive)
        {
            m_llmAssistActive = false;
            m_statusBar.showMessage("LLM-Assist disabled");
            updateScriptEditorReadOnlyState();
            return;
        }
        if (m_llmAssistFolder.isEmpty())
        {
            chooseLlmAssistFolder(true);
            return;
        }
        m_llmAssistActive = true;
        m_statusBar.showMessage("LLM-Assist watching " + m_llmAssistFolder);
        updateScriptEditorReadOnlyState();
    }

    // Pushed into an already-open editor whenever LLM-Assist toggles, so its read-only
    // state always reflects whether a watched-folder pull could currently race an edit.
    void updateScriptEditorReadOnlyState()
    {
        if (m_scriptEditorContent != nullptr)
        {
            m_scriptEditorContent->setReadOnly(m_llmAssistActive);
        }
    }

    void chooseLlmAssistFolder(bool activateOnPick)
    {
        m_llmAssistFolderChooser =
            std::make_unique<juce::FileChooser>("Choose LLM-Assist Folder", juce::File(m_llmAssistFolder));
        m_llmAssistFolderChooser->launchAsync(juce::FileBrowserComponent::openMode |
                                                  juce::FileBrowserComponent::canSelectDirectories,
                                              [this, activateOnPick](const juce::FileChooser& fc)
                                              {
                                                  const auto result = fc.getResult();
                                                  if (!result.isDirectory())
                                                  {
                                                      return;
                                                  }
                                                  m_llmAssistFolder = result.getFullPathName();
                                                  AppSettings::saveLlmAssistFolder(m_llmAssistFolder);
                                                  m_llmAssistActive = m_llmAssistActive || activateOnPick;
                                                  m_statusBar.showMessage("LLM-Assist folder: " + m_llmAssistFolder);
                                                  updateScriptEditorReadOnlyState();
                                              });
    }

    // Runs at a fraction of the timer rate (see kLlmAssistPollEveryNTicks) - a folder
    // scan every tick is unnecessary for a workflow driven by an LLM/human editing text.
    void pollLlmAssistWatcher()
    {
        if (!m_llmAssistActive)
        {
            return;
        }
        if (++m_llmAssistPollCounter % kLlmAssistPollEveryNTicks != 0)
        {
            return;
        }
        const auto result = m_llmAssistWatcher.poll(juce::File(m_llmAssistFolder));
        if (!result)
        {
            return;
        }
        if (result->kind == LlmAssistResultKind::Library)
        {
            m_statusBar.showMessage(result->compiled
                                        ? "LLM-Assist library '" + result->scriptName + "' saved, current script OK"
                                        : "LLM-Assist library '" + result->scriptName +
                                              "' saved, current script error: " + result->error,
                                    true);
        }
        else if (result->compiled)
        {
            m_statusBar.showMessage("LLM-Assist applied '" + result->scriptName + "'", true);
            if (m_scriptEditorContent != nullptr)
            {
                m_scriptEditorContent->setScriptText(processorRef.getScriptText());
            }
        }
        else
        {
            m_statusBar.showMessage("LLM-Assist error in '" + result->scriptName + "': " + result->error, true);
        }
    }
    /*END_SCRIPTBROWSER*/

    /*EXTRA_PRIVATE_METHODS*/
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
    /*START_PERFORMANCEPAGE*/
    Page m_currentPage{Page::Performance};
    juce::TextButton m_pagePerformanceButton{"Performance"};
    juce::TextButton m_pageSettingsButton{"Settings"};
    /*END_PERFORMANCEPAGE*/
    static constexpr int kThemeModeLightId = 9000;
    static constexpr int kThemeModeDarkId = 9001;
    static constexpr int kThemeBaseBichromaticId = 9002;
    static constexpr int kThemeBaseTrichromaticId = 9003;
    static constexpr int kAboutShowInfoId = 14000;
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
    static constexpr int kLoopSaveId = 4999;
    static constexpr int kLoopSaveAsId = 5000;
    static constexpr int kLoopLoadIdBase = 6000;
    static constexpr int kLoopDeleteIdBase = 7000;
    static constexpr int kLoopRenameIdBase = 8000;
    std::unique_ptr<juce::AlertWindow> m_loopNameDialog;
    std::vector<juce::String> m_loopMenuNames;
    /*END_LOOPBROWSER*/

    /*START_SCRIPTBROWSER*/
    static constexpr int kScriptEditId = 9004;
    static constexpr int kScriptSaveAsId = 10000;
    static constexpr int kScriptLoadIdBase = 11000;
    static constexpr int kScriptDeleteIdBase = 12000;
    static constexpr int kScriptRenameIdBase = 13000;
    std::unique_ptr<juce::AlertWindow> m_scriptNameDialog;
    std::vector<juce::String> m_scriptMenuNames;
    juce::String m_lastScriptErrorShown;
    // Non-modal; deletes itself on close (see ScriptEditorDialogWindow), hence SafePointer
    // rather than an owning pointer here.
    juce::Component::SafePointer<ScriptEditorDialogWindow> m_scriptEditorWindow;
    // Points at the window's content component, so pollLlmAssistWatcher()/toggleLlmAssist()
    // can push a refresh/read-only update without reaching into ScriptEditorDialogWindow.
    juce::Component::SafePointer<ScriptEditorWindow> m_scriptEditorContent;
    static constexpr int kLlmAssistToggleId = 15000;
    static constexpr int kLlmAssistChooseFolderId = 15001;
    static constexpr int kLlmAssistPollEveryNTicks = 15;
    bool m_llmAssistActive{false};
    int m_llmAssistPollCounter{0};
    juce::String m_llmAssistFolder{AppSettings::loadLlmAssistFolder()};
    LlmAssistWatcher m_llmAssistWatcher;
    std::unique_ptr<juce::FileChooser> m_llmAssistFolderChooser;
    /*END_SCRIPTBROWSER*/

    /*WIDGETS_DECL*/
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioPluginAudioProcessorEditor)
};
