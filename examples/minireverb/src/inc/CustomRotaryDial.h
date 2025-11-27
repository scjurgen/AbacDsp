#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

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

    void setHasModifiers(bool mod)
    {
        m_isModifiable = mod;
    }

    bool hasModifier() const
    {
        return m_isModifiable;
    }

  private:
    juce::Label* m_label;
    bool m_isModifiable;
};


class CustomRotaryDial : public juce::Component
{
    using SliderAttachment = std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>;
    using ButtonAttachment = std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>;

  public:
    CustomRotaryDial(Component* /*parent = nullptr*/)
        : m_slider(&m_label)
    {
        addAndMakeVisible(m_slider);
        m_slider.setSliderStyle(juce::Slider::SliderStyle::RotaryHorizontalVerticalDrag);
        m_slider.setTextBoxStyle(juce::Slider::TextEntryBoxPosition::TextBoxBelow, false, 80,
                                 static_cast<int>(Constants::Text::labelHeight));
        m_slider.setPopupDisplayEnabled(false, false, nullptr, 0);

        addAndMakeVisible(m_label);
        m_label.setJustificationType(juce::Justification::centred);
    }

    void reset(juce::AudioProcessorValueTreeState& state, const juce::String& paramID)
    {
        m_sliderAttachment.reset(new juce::AudioProcessorValueTreeState::SliderAttachment(state, paramID, m_slider));
    }

    void setLabelText(const juce::String& text)
    {
        m_label.setText(text, juce::dontSendNotification);
    }

    void resized() override
    {
        auto bounds = getLocalBounds().reduced(2);
        const auto fontHeight = m_label.getFont().getHeight();

        const auto labelBounds = bounds.removeFromBottom(static_cast<int>(fontHeight));
        m_slider.setBounds(bounds);
        m_label.setBounds(labelBounds);
    }

    void setHasModifier(bool mod)
    {
        m_slider.setHasModifiers(mod);
    }

    bool hasModifier() const
    {
        return m_slider.hasModifier();
    }

  private:
    ModRotaryDial m_slider;
    juce::Label m_label;
    SliderAttachment m_sliderAttachment;
    ButtonAttachment m_buttonAttachment;
};
