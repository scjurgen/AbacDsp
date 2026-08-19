#pragma once

#include <algorithm>
#include <functional>
#include <juce_audio_processors/juce_audio_processors.h>
#include <memory>
#include <utility>

struct CcMenuCallbacks
{
    std::function<void()> onLearn;
    std::function<std::pair<float, float>()> getRange;
    std::function<void(float, float)> setRange;
    std::function<void()> onClear;
    std::function<int()> getController;
};

// Label-left, editor-right row: juce::AlertWindow::addTextEditor draws its
// onScreenLabel above the editor (clipped to the editor's own width), which
// truncates longer labels. This lays the two out side by side instead.
class CcRangeRow : public juce::Component
{
  public:
    CcRangeRow(const juce::String& labelText, const float initialValue)
    {
        m_label.setText(labelText, juce::dontSendNotification);
        m_label.setJustificationType(juce::Justification::centredLeft);
        addAndMakeVisible(m_label);

        m_editor.setText(juce::String(initialValue), juce::dontSendNotification);
        m_editor.setInputRestrictions(0, "-0123456789.");
        m_editor.setSelectAllWhenFocused(true);
        addAndMakeVisible(m_editor);
    }

    void resized() override
    {
        auto bounds = getLocalBounds();
        m_label.setBounds(bounds.removeFromLeft(120));
        m_editor.setBounds(bounds);
    }

    [[nodiscard]] float getValue() const
    {
        return m_editor.getText().getFloatValue();
    }

  private:
    juce::Label m_label;
    juce::TextEditor m_editor;
};

class ModRotaryDial : public juce::Slider
{
  public:
    explicit ModRotaryDial(juce::Label* l)
        : m_label(l)
        , m_isModifiable(false)
    {
    }

    ~ModRotaryDial() override
    {
        m_label = nullptr;
    }

    void mouseDown(const juce::MouseEvent& e) override
    {
        const juce::ModifierKeys modifiers = juce::ModifierKeys::getCurrentModifiersRealtime();
        if (m_ccMappable && modifiers.isPopupMenu())
        {
            juce::PopupMenu menu;
            const auto controller = m_ccCallbacks.getController ? m_ccCallbacks.getController() : -1;
            menu.addSectionHeader(controller >= 0 ? ("Assigned CC: " + juce::String(controller))
                                                  : juce::String("No CC assigned"));
            menu.addSeparator();
            menu.addItem("MIDI Learn CC...",
                         [this]
                         {
                             if (m_ccCallbacks.onLearn)
                             {
                                 m_ccCallbacks.onLearn();
                             }
                         });
            menu.addItem("Set CC Range...", [this] { showSetRangeDialog(); });
            menu.addItem("Clear CC Assignment",
                         [this]
                         {
                             if (m_ccCallbacks.onClear)
                             {
                                 m_ccCallbacks.onClear();
                             }
                         });
            menu.showMenuAsync(juce::PopupMenu::Options());
            return;
        }
        if (m_isModifiable && modifiers.isPopupMenu())
        {
            if (isEnabled() && m_label->isEnabled())
            {
                setEnabled(false);
                m_label->setEnabled(false);
            }
            else
            {
                setEnabled(true);
                m_label->setEnabled(true);
            }
        }
        else
        {
            Slider::mouseDown(e);
        }
    }

    void setHasModifiers(const bool mod)
    {
        m_isModifiable = mod;
    }

    bool hasModifier() const
    {
        return m_isModifiable;
    }

    void setCcMappable(const bool mappable, CcMenuCallbacks callbacks)
    {
        m_ccMappable = mappable;
        m_ccCallbacks = std::move(callbacks);
    }

  private:
    void showSetRangeDialog()
    {
        if (!m_ccCallbacks.getRange || !m_ccCallbacks.setRange)
        {
            return;
        }
        const auto [lo, hi] = m_ccCallbacks.getRange();
        // Both title and message left empty: AlertWindow paints them as a large,
        // prominently-centred block and its "balanced line length" wrapping mangles
        // even short titles unpredictably. The row labels below already say enough.
        auto alertWindow =
            std::make_unique<juce::AlertWindow>(juce::String{}, juce::String{}, juce::MessageBoxIconType::NoIcon);

        auto lowRow = std::make_unique<CcRangeRow>("Value at CC 0:", lo);
        auto highRow = std::make_unique<CcRangeRow>("Value at CC 127:", hi);
        lowRow->setSize(280, 24);
        highRow->setSize(280, 24);
        alertWindow->addCustomComponent(lowRow.get());
        alertWindow->addCustomComponent(highRow.get());
        alertWindow->addButton("OK", 1, juce::KeyPress(juce::KeyPress::returnKey));
        alertWindow->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));

        auto* rawAlert = alertWindow.get();
        rawAlert->enterModalState(true,
                                  juce::ModalCallbackFunction::create(
                                      [alert = std::move(alertWindow), low = std::move(lowRow),
                                       high = std::move(highRow), callbacks = m_ccCallbacks](const int result) mutable
                                      {
                                          if (result == 1 && callbacks.setRange)
                                          {
                                              callbacks.setRange(low->getValue(), high->getValue());
                                          }
                                          alert.reset();
                                          low.reset();
                                          high.reset();
                                      }));
    }

    juce::Label* m_label;
    bool m_isModifiable;
    bool m_ccMappable{false};
    CcMenuCallbacks m_ccCallbacks;
};


class CustomRotaryDial : public juce::Component
{
    using SliderAttachment = std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>;
    using ButtonAttachment = std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>;

  public:
    explicit CustomRotaryDial(Component* /*parent = nullptr*/)
        : m_slider(&m_label)
    {
        addAndMakeVisible(m_slider);
        m_slider.setSliderStyle(juce::Slider::SliderStyle::RotaryHorizontalVerticalDrag);
        m_slider.setTextBoxStyle(juce::Slider::TextEntryBoxPosition::TextBoxBelow, false, 80,
                                 static_cast<int>(GuiConstants::instance().text.labelHeight));
        m_slider.setPopupDisplayEnabled(false, false, nullptr, 0);

        addAndMakeVisible(m_label);
        m_label.setJustificationType(juce::Justification::centred);
    }

    void reset(juce::AudioProcessorValueTreeState& state, const juce::String& paramID)
    {
        m_sliderAttachment =
            std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(state, paramID, m_slider);
    }

    void setLabelText(const juce::String& text)
    {
        m_label.setText(text, juce::dontSendNotification);
    }

    // Moves the displayed value without touching the bound parameter (mirrors
    // how SliderAttachment itself reflects external parameter changes).
    void setValue(const double value, const juce::NotificationType notification = juce::dontSendNotification)
    {
        m_slider.setValue(value, notification);
    }

    void resized() override
    {
        const auto bounds = getLocalBounds().reduced(2);
        const auto fontHeight = static_cast<int>(m_label.getFont().getHeight());

        const auto knobSize =
            std::clamp(std::min(bounds.getWidth(), bounds.getHeight() - fontHeight),
                       static_cast<int>(Constants::Dial::minSize), static_cast<int>(Constants::Dial::maxSize));
        const auto xOffset = (bounds.getWidth() - knobSize) / 2;
        const auto yOffset = (bounds.getHeight() - knobSize - fontHeight) / 2;

        m_label.setBounds(bounds.getX(), bounds.getY() + yOffset, bounds.getWidth(), fontHeight);
        m_slider.setBounds(bounds.getX() + xOffset, bounds.getY() + yOffset + fontHeight, knobSize, knobSize);
    }


    void setHasModifier(const bool mod)
    {
        m_slider.setHasModifiers(mod);
    }

    bool hasModifier() const
    {
        return m_slider.hasModifier();
    }

    void setCcMappable(const bool mappable, CcMenuCallbacks callbacks)
    {
        m_slider.setCcMappable(mappable, std::move(callbacks));
    }

    // JUCE's TooltipWindow only checks the exact component under the mouse
    // (no parent-chain walk), so both hoverable children need the text set.
    void setTooltip(const juce::String& text)
    {
        m_slider.setTooltip(text);
        m_label.setTooltip(text);
    }

  private:
    ModRotaryDial m_slider;
    juce::Label m_label;
    SliderAttachment m_sliderAttachment;
    ButtonAttachment m_buttonAttachment;
};
