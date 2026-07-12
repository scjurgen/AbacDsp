#pragma once

#include <functional>
#include <juce_gui_basics/juce_gui_basics.h>
#include <memory>
#include <vector>

// Preset browser: lists saved patches by name and lets the user load, save, save-as and
// delete them. Storage, naming rules and the "unsaved changes" prompt all live in the
// processor (see AudioPluginAudioProcessor::requestLoadPatch); this widget only captures
// user intent (which name, which button) and asks for a name when one is needed.
class PatchBrowser final : public juce::Component
{
  public:
    PatchBrowser()
    {
        addAndMakeVisible(m_combo);
        addAndMakeVisible(m_saveButton);
        addAndMakeVisible(m_saveAsButton);
        addAndMakeVisible(m_deleteButton);

        m_combo.onChange = [this]
        {
            if (onLoad)
            {
                onLoad(m_combo.getText());
            }
        };
        m_saveButton.onClick = [this] { save(); };
        m_saveAsButton.onClick = [this] { promptAndSaveAs(); };
        m_deleteButton.onClick = [this] { confirmAndDelete(); };
    }

    // Kept for interface parity with the generated gauge/dial widgets, which all get a
    // setLabelText() call from the generator; this browser has no separate title of its own.
    void setLabelText(const juce::String& /*label*/) noexcept {}

    void refresh()
    {
        const auto currentName = onGetCurrentName ? onGetCurrentName() : juce::String{};
        m_combo.clear(juce::dontSendNotification);
        int itemId = 1;
        int selectedId = 0;
        for (const auto& name : onListNames ? onListNames() : std::vector<juce::String>{})
        {
            m_combo.addItem(name, itemId);
            if (name == currentName)
            {
                selectedId = itemId;
            }
            ++itemId;
        }
        m_combo.setSelectedId(selectedId, juce::dontSendNotification);
        m_combo.setTextWhenNothingSelected(currentName.isNotEmpty() ? currentName : "(unsaved)");
    }

    void resized() override
    {
        auto area = getLocalBounds();
        m_combo.setBounds(area.removeFromTop(area.getHeight() / 2).reduced(1));
        const auto buttonWidth = area.getWidth() / 3;
        m_saveButton.setBounds(area.removeFromLeft(buttonWidth).reduced(1));
        m_saveAsButton.setBounds(area.removeFromLeft(buttonWidth).reduced(1));
        m_deleteButton.setBounds(area.reduced(1));
    }

    std::function<std::vector<juce::String>()> onListNames;
    std::function<juce::String()> onGetCurrentName;
    std::function<void(const juce::String&)> onLoad;
    std::function<bool(const juce::String&)> onSaveAs;
    std::function<bool(const juce::String&)> onDelete;

  private:
    void save()
    {
        const auto currentName = onGetCurrentName ? onGetCurrentName() : juce::String{};
        if (currentName.isEmpty())
        {
            promptAndSaveAs();
            return;
        }
        if (onSaveAs)
        {
            onSaveAs(currentName);
        }
        refresh();
    }

    void promptAndSaveAs()
    {
        m_nameDialog = std::make_unique<juce::AlertWindow>(
            "Save Patch", "Enter a name for this patch:", juce::MessageBoxIconType::NoIcon);
        m_nameDialog->addTextEditor("name", onGetCurrentName ? onGetCurrentName() : juce::String{});
        m_nameDialog->addButton("Save", 1, juce::KeyPress(juce::KeyPress::returnKey));
        m_nameDialog->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));
        m_nameDialog->enterModalState(true,
                                      juce::ModalCallbackFunction::create(
                                          [this](int result)
                                          {
                                              const auto name = m_nameDialog->getTextEditorContents("name").trim();
                                              m_nameDialog.reset();
                                              if (result != 1 || name.isEmpty())
                                              {
                                                  return;
                                              }
                                              if (onSaveAs)
                                              {
                                                  onSaveAs(name);
                                              }
                                              refresh();
                                          }),
                                      false);
    }

    void confirmAndDelete()
    {
        const auto name = m_combo.getText();
        if (name.isEmpty())
        {
            return;
        }
        juce::NativeMessageBox::showAsync(juce::MessageBoxOptions()
                                              .withIconType(juce::MessageBoxIconType::WarningIcon)
                                              .withTitle("Delete Patch")
                                              .withMessage("Delete patch \"" + name + "\"?")
                                              .withButton("Yes")
                                              .withButton("No"),
                                          [this, name](int result)
                                          {
                                              // showAsync returns the plain index of the
                                              // clicked button (0 = "Yes", 1 = "No").
                                              if (result != 0)
                                              {
                                                  return;
                                              }
                                              if (onDelete)
                                              {
                                                  onDelete(name);
                                              }
                                              refresh();
                                          });
    }

    juce::ComboBox m_combo;
    juce::TextButton m_saveButton{"Save"};
    juce::TextButton m_saveAsButton{"New"};
    juce::TextButton m_deleteButton{"Del"};
    std::unique_ptr<juce::AlertWindow> m_nameDialog;
};
