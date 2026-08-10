#pragma once

#include <cmath>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <memory>
#include <string>
#include <vector>

#include "GuiConstants.h"

// Fixed, always-copied shared component (CPP_SOURCE_FILES_FIXED) - must stay JUCE+std
// only, no reaching into one example's impl/, or an unrelated blueprint's regeneration
// breaks. See examples/dronesequencer's impl/LuaControlBridge.h for the conversion side.

enum class LuaControlType
{
    Knob,
    Drop,
    Switch
};

// One already-claimed control to render, fully resolved by the caller: which APVTS
// parameter it's bound to, its display metadata, nothing else. LuaControlArea has no
// knowledge of where this data originates.
struct LuaControlDescriptor
{
    std::string parameterId;
    std::string name;
    LuaControlType type{LuaControlType::Knob};
    float rangeMin{0.f};
    float rangeMax{1.f};
    float rangeStep{0.f};
    float rangeSkew{1.f};
    std::string description;
    std::vector<std::string> items;

    // Detects "did the caller's descriptor list change since the last rebuild"; exact
    // float equality is intentional, not a tolerance check.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wfloat-equal"
    bool operator==(const LuaControlDescriptor&) const = default;
#pragma GCC diagnostic pop
};

// Renders a script-driven parameter set: one child widget per descriptor, auto-arranged
// in a row, none for parameters the caller doesn't currently claim. Each widget's raw
// APVTS parameter is assumed normalized 0..1; it's converted to/from the descriptor's
// declared display range via juce::NormalisableRange - the same range-mapping math the
// generator's own dial codegen does at compile time, done here at runtime instead.
// refresh() is driven by the Editor's own timer; it rebuilds child widgets only when the
// descriptor list actually changes, and otherwise just pushes each parameter's current
// value into its widget (skipped while the user is actively interacting with it).
class LuaControlArea : public juce::Component
{
  public:
    LuaControlArea() = default;

    void refresh(const std::vector<LuaControlDescriptor>& controls, juce::AudioProcessorValueTreeState& valueTreeState)
    {
        if (!(controls == m_lastBuiltControls))
        {
            rebuild(controls, valueTreeState);
            m_lastBuiltControls = controls;
        }
        for (auto& widget : m_widgets)
        {
            widget->syncFromParameter();
        }
    }

    void resized() override
    {
        juce::FlexBox box;
        box.flexWrap = juce::FlexBox::Wrap::noWrap;
        box.flexDirection = juce::FlexBox::Direction::row;
        box.justifyContent = juce::FlexBox::JustifyContent::spaceAround;
        for (auto& widget : m_widgets)
        {
            box.items.add(juce::FlexItem(*widget).withFlex(1).withMargin(Constants::Margins::small));
        }
        box.performLayout(getLocalBounds().toFloat());
    }

  private:
    // One descriptor's label plus its knob/dropdown/switch control - exactly one of
    // m_slider/m_combo/m_toggle is non-null, matching m_descriptor.type.
    class ParamWidget : public juce::Component
    {
      public:
        ParamWidget(const LuaControlDescriptor& descriptor, juce::AudioProcessorValueTreeState& valueTreeState)
            : m_descriptor(descriptor)
            , m_valueTreeState(valueTreeState)
            , m_paramId(descriptor.parameterId)
        {
            m_label.setText(juce::String(descriptor.name), juce::dontSendNotification);
            m_label.setJustificationType(juce::Justification::centred);
            addAndMakeVisible(m_label);

            switch (descriptor.type)
            {
                case LuaControlType::Knob:
                    buildKnob();
                    break;
                case LuaControlType::Drop:
                    buildDrop();
                    break;
                case LuaControlType::Switch:
                    buildSwitch();
                    break;
            }
            syncFromParameter();
        }

        void resized() override
        {
            auto bounds = getLocalBounds();
            const auto labelHeight = static_cast<int>(GuiConstants::instance().text.labelHeight);
            m_label.setBounds(bounds.removeFromTop(labelHeight));
            if (m_slider != nullptr)
            {
                m_slider->setBounds(bounds);
            }
            else if (m_combo != nullptr)
            {
                m_combo->setBounds(
                    bounds.removeFromTop(labelHeight).reduced(static_cast<int>(Constants::Margins::small), 0));
            }
            else if (m_toggle != nullptr)
            {
                m_toggle->setBounds(bounds.removeFromTop(labelHeight));
            }
        }

        // Pushes the parameter's current value into the control, unless the user is
        // mid-interaction (would otherwise fight a live drag/host automation echo).
        void syncFromParameter()
        {
            if (m_interacting)
            {
                return;
            }
            const auto* raw = m_valueTreeState.getRawParameterValue(m_paramId);
            if (raw == nullptr)
            {
                return;
            }
            const float display = normalizedToDisplay(raw->load());
            if (m_slider != nullptr)
            {
                m_slider->setValue(display, juce::dontSendNotification);
            }
            else if (m_combo != nullptr)
            {
                syncCombo(display);
            }
            else if (m_toggle != nullptr)
            {
                m_toggle->setToggleState(display >= 0.5f, juce::dontSendNotification);
            }
        }

      private:
        void buildKnob()
        {
            m_slider = std::make_unique<juce::Slider>(juce::Slider::SliderStyle::RotaryHorizontalVerticalDrag,
                                                      juce::Slider::TextEntryBoxPosition::TextBoxBelow);
            m_slider->setRange(static_cast<double>(m_descriptor.rangeMin), static_cast<double>(m_descriptor.rangeMax),
                               static_cast<double>(m_descriptor.rangeStep));
            m_slider->setSkewFactor(static_cast<double>(m_descriptor.rangeSkew));
            m_slider->setDescription(accessibilityDescription());
            m_slider->onDragStart = [this] { beginInteraction(); };
            m_slider->onDragEnd = [this] { endInteraction(); };
            m_slider->onValueChange = [this] { pushValue(static_cast<float>(m_slider->getValue())); };
            addAndMakeVisible(*m_slider);
        }

        void buildDrop()
        {
            m_combo = std::make_unique<juce::ComboBox>();
            for (size_t i = 0; i < m_descriptor.items.size(); ++i)
            {
                m_combo->addItem(juce::String(m_descriptor.items[i]), static_cast<int>(i) + 1);
            }
            m_combo->setDescription(accessibilityDescription());
            m_combo->onChange = [this]
            {
                beginInteraction();
                pushValue(static_cast<float>(m_combo->getSelectedId() - 1));
                endInteraction();
            };
            addAndMakeVisible(*m_combo);
        }

        void buildSwitch()
        {
            m_toggle = std::make_unique<juce::ToggleButton>();
            m_toggle->setDescription(accessibilityDescription());
            m_toggle->onClick = [this]
            {
                beginInteraction();
                pushValue(m_toggle->getToggleState() ? 1.f : 0.f);
                endInteraction();
            };
            addAndMakeVisible(*m_toggle);
        }

        // juce::ComboBox commits selections asynchronously (AsyncUpdater) - isPopupActive()
        // already goes false before that fires, so a poll landing in that gap can silently
        // overwrite a pending click. This grace period covers it without hooking the dispatch.
        static constexpr int kComboSyncGraceTicks{5};

        void syncCombo(const float display)
        {
            if (m_combo->isPopupActive())
            {
                m_comboJustClosed = true;
                return;
            }
            if (m_comboJustClosed)
            {
                m_comboJustClosed = false;
                m_comboSyncGraceTicksLeft = kComboSyncGraceTicks;
            }
            if (m_comboSyncGraceTicksLeft > 0)
            {
                --m_comboSyncGraceTicksLeft;
                return;
            }
            m_combo->setSelectedId(static_cast<int>(std::lround(display)) + 1, juce::dontSendNotification);
        }

        [[nodiscard]] juce::String accessibilityDescription() const
        {
            return juce::String(m_descriptor.description.empty() ? m_descriptor.name : m_descriptor.description);
        }

        [[nodiscard]] juce::NormalisableRange<float> displayRange() const
        {
            return {m_descriptor.rangeMin, m_descriptor.rangeMax,
                    m_descriptor.rangeStep > 0.f ? m_descriptor.rangeStep : 0.f, m_descriptor.rangeSkew};
        }

        [[nodiscard]] float normalizedToDisplay(const float normalized) const
        {
            return displayRange().convertFrom0to1(normalized);
        }

        void pushValue(const float display) const
        {
            if (auto* param = m_valueTreeState.getParameter(m_paramId))
            {
                param->setValueNotifyingHost(displayRange().convertTo0to1(display));
            }
        }

        void beginInteraction()
        {
            m_interacting = true;
            if (auto* param = m_valueTreeState.getParameter(m_paramId))
            {
                param->beginChangeGesture();
            }
        }

        void endInteraction()
        {
            if (auto* param = m_valueTreeState.getParameter(m_paramId))
            {
                param->endChangeGesture();
            }
            m_interacting = false;
        }

        LuaControlDescriptor m_descriptor;
        juce::AudioProcessorValueTreeState& m_valueTreeState;
        juce::String m_paramId;
        juce::Label m_label;
        std::unique_ptr<juce::Slider> m_slider;
        std::unique_ptr<juce::ComboBox> m_combo;
        std::unique_ptr<juce::ToggleButton> m_toggle;
        bool m_interacting{false};
        bool m_comboJustClosed{false};
        int m_comboSyncGraceTicksLeft{0};
    };

    void rebuild(const std::vector<LuaControlDescriptor>& controls, juce::AudioProcessorValueTreeState& valueTreeState)
    {
        m_widgets.clear();
        for (const auto& descriptor : controls)
        {
            auto widget = std::make_unique<ParamWidget>(descriptor, valueTreeState);
            addAndMakeVisible(*widget);
            m_widgets.push_back(std::move(widget));
        }
        resized();
    }

    std::vector<std::unique_ptr<ParamWidget>> m_widgets{};
    std::vector<LuaControlDescriptor> m_lastBuiltControls{};
};
