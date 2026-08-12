#pragma once

#include <cassert>
#include <cmath>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <memory>
#include <string>
#include <vector>

#include "GuiConstants.h"
#include "LuaParamRangeMath.h"

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
// in a row, none for parameters the caller doesn't currently claim. Each widget owns a
// juce::ParameterAttachment bound to its raw [0,1] pool parameter, mapped to/from the
// descriptor's declared display range via LuaParamRangeMath (the same math the engine
// uses audio-thread-side) - push-driven by the parameter's own change notifications, not
// polled. refresh() is driven by the Editor's own timer only to detect when the claimed
// set itself changes and needs rebuilding; per-widget value sync needs no timer at all.
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
            , m_attachment(*requireParameter(valueTreeState, descriptor.parameterId),
                           [this](const float normalized) { applyNormalizedValue(normalized); })
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
            m_attachment.sendInitialUpdate();
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

      private:
        // The fixed pool always has all kMaxLuaParams parameters created up front, so a
        // claimed descriptor's parameterId is always resolvable.
        [[nodiscard]] static juce::RangedAudioParameter* requireParameter(
            juce::AudioProcessorValueTreeState& valueTreeState, const std::string& parameterId)
        {
            auto* param = valueTreeState.getParameter(parameterId);
            assert(param != nullptr);
            return param;
        }

        void buildKnob()
        {
            m_slider = std::make_unique<juce::Slider>(juce::Slider::SliderStyle::RotaryHorizontalVerticalDrag,
                                                      juce::Slider::TextEntryBoxPosition::TextBoxBelow);
            m_slider->setRange(static_cast<double>(m_descriptor.rangeMin), static_cast<double>(m_descriptor.rangeMax),
                               static_cast<double>(m_descriptor.rangeStep));
            m_slider->setSkewFactor(static_cast<double>(m_descriptor.rangeSkew));
            m_slider->setDescription(accessibilityDescription());
            m_slider->setTooltip(accessibilityDescription());
            m_slider->onDragStart = [this] { m_attachment.beginGesture(); };
            m_slider->onDragEnd = [this] { m_attachment.endGesture(); };
            m_slider->onValueChange = [this] { pushDisplayValue(static_cast<float>(m_slider->getValue()), false); };
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
            m_combo->setTooltip(accessibilityDescription());
            m_combo->onChange = [this] { pushDisplayValue(static_cast<float>(m_combo->getSelectedId() - 1), true); };
            addAndMakeVisible(*m_combo);
        }

        void buildSwitch()
        {
            m_toggle = std::make_unique<juce::ToggleButton>();
            m_toggle->setDescription(accessibilityDescription());
            m_toggle->setTooltip(accessibilityDescription());
            m_toggle->onClick = [this] { pushDisplayValue(m_toggle->getToggleState() ? 1.f : 0.f, true); };
            addAndMakeVisible(*m_toggle);
        }

        // Pushes a widget-driven display value to the underlying pool parameter.
        // completeGesture is a one-shot change (combo/toggle click) vs. part of an
        // already-bracketed drag (slider, begin/endGesture wrap the whole drag).
        void pushDisplayValue(const float display, const bool completeGesture)
        {
            if (m_ignoreCallbacks)
            {
                return;
            }
            const float normalized = luaParamDisplayToNormalized(m_descriptor.rangeMin, m_descriptor.rangeMax,
                                                                 m_descriptor.rangeSkew, display);
            if (completeGesture)
            {
                m_attachment.setValueAsCompleteGesture(normalized);
            }
            else
            {
                m_attachment.setValueAsPartOfGesture(normalized);
            }
        }

        // Called on the message thread whenever the pool parameter actually changes,
        // including echoing our own pushDisplayValue() above - m_ignoreCallbacks skips
        // reacting to that echo, matching juce::SliderParameterAttachment's own idiom.
        void applyNormalizedValue(const float normalized)
        {
            const juce::ScopedValueSetter<bool> ignoreScope(m_ignoreCallbacks, true);
            const float display =
                luaParamNormalizedToDisplay(m_descriptor.rangeMin, m_descriptor.rangeMax, m_descriptor.rangeStep,
                                            m_descriptor.rangeSkew, normalized);
            if (m_slider != nullptr)
            {
                m_slider->setValue(display, juce::dontSendNotification);
            }
            else if (m_combo != nullptr)
            {
                m_combo->setSelectedId(static_cast<int>(std::lround(display)) + 1, juce::dontSendNotification);
            }
            else if (m_toggle != nullptr)
            {
                m_toggle->setToggleState(display >= 0.5f, juce::dontSendNotification);
            }
        }

        [[nodiscard]] juce::String accessibilityDescription() const
        {
            return juce::String(m_descriptor.description.empty() ? m_descriptor.name : m_descriptor.description);
        }

        LuaControlDescriptor m_descriptor;
        juce::ParameterAttachment m_attachment;
        bool m_ignoreCallbacks{false};
        juce::Label m_label;
        std::unique_ptr<juce::Slider> m_slider;
        std::unique_ptr<juce::ComboBox> m_combo;
        std::unique_ptr<juce::ToggleButton> m_toggle;
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
