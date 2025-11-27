#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

class GuiLookAndFeel : public juce::LookAndFeel_V4
{
  private:
    const juce::String mainFont{"Futura"};
    const float fontHeight{Constants::Text::fontHeight};
    juce::Colour backgroundDarkGrey, backgroundDarkGreyDisabled;
    juce::Colour backgroundMidGrey;
    juce::Colour statusOutline, statusOutlineDisabled;
    juce::Colour gradientDarkGrey, gradientDarkGreyDisabled;
    const juce::Font mainFontDefinition;

  public:
    GuiLookAndFeel()
        : mainFontDefinition(juce::FontOptions(mainFont, fontHeight, juce::Font::plain))
    {
        backgroundDarkGrey = juce::Colour(Constants::Colors::bg_DarkGrey);
        backgroundDarkGreyDisabled = juce::Colour(Constants::Colors::bg_DarkGrey).withAlpha(0.35f);
        backgroundMidGrey = juce::Colour(Constants::Colors::bg_MidGrey);
        statusOutline = juce::Colour(Constants::Colors::statusOutline);
        statusOutlineDisabled = juce::Colour(Constants::Colors::statusOutline).withAlpha(0.35f);
        gradientDarkGrey = juce::Colour(Constants::Colors::gd_DarkGreyStart);
        gradientDarkGreyDisabled = juce::Colour(Constants::Colors::gd_DarkGreyStart).withAlpha(0.35f);
    }

    void drawLabel(juce::Graphics& g, juce::Label& label) override
    {
        auto area = label.getLocalBounds();

        g.setColour(label.findColour(juce::Label::textColourId));
        g.setFont(juce::FontOptions(mainFont, fontHeight, juce::Font::plain));
        g.setFont(fontHeight);

        g.drawFittedText(label.getText(), area, juce::Justification::centred, 1, label.getMinimumHorizontalScale());
    }

    void drawToggleButton(juce::Graphics& g, juce::ToggleButton& button, bool /*shouldDrawButtonAsHighlighted*/,
                          bool /*shouldDrawButtonAsDown*/) override
    {
        auto bounds = button.getLocalBounds();
        auto toggleBounds = bounds.removeFromLeft(static_cast<int>(bounds.getHeight() * 1.5f));
        auto textBounds = bounds.reduced(2);

        g.setColour(backgroundDarkGrey);
        g.fillRoundedRectangle(toggleBounds.toFloat().reduced(2.0f), toggleBounds.getHeight() / 2.0f);

        auto diameter = toggleBounds.getHeight() - 13.0f;
        auto circleX = button.getToggleState() ? toggleBounds.getRight() - diameter - 6.5f : toggleBounds.getX() + 6.5f;
        auto circleY = toggleBounds.getY() + 6.5f;

        g.setColour(button.getToggleState() ? statusOutline : gradientDarkGrey);
        g.fillEllipse(circleX, circleY, diameter, diameter);

        if (!button.getButtonText().isEmpty())
        {
            g.setColour(button.findColour(juce::ToggleButton::textColourId));
            g.setFont(juce::FontOptions(mainFont, fontHeight, juce::Font::plain));
            g.setFont(fontHeight);
            g.drawFittedText(button.getButtonText(), textBounds, juce::Justification::centredLeft, 1);
        }
    }

    void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height, float sliderPosProportional,
                          float rotaryStartAngle, float rotaryEndAngle, juce::Slider& slider) override
    {
        const auto radius = static_cast<float>(juce::jmin(width / 2, height / 2)) * 0.9f;
        const auto cx = static_cast<float>(x) + static_cast<float>(width) * 0.5f;
        const auto cy = static_cast<float>(y) + static_cast<float>(height) * 0.5f;
        const auto rx = cx - radius;
        const auto ry = cy - radius;
        const auto rw = radius * 2.0f;

        const auto zeroPos =
            slider.getMinimum() < 0 && slider.getMaximum() > 0
                ? static_cast<float>(-slider.getMinimum() / (slider.getMaximum() - slider.getMinimum()))
                : 0;
        const auto zeroAngle = rotaryStartAngle + zeroPos * (rotaryEndAngle - rotaryStartAngle);
        const auto angle = rotaryStartAngle + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);

        constexpr auto bedThickness = 2.0f;
        constexpr auto bedOutline = 1.4f;
        constexpr auto statusOutlineThickness = 2.5f;
        constexpr auto extraMargin = 2.0f;
        constexpr auto strokeSliderBase = 1.5f;
        constexpr auto strokeSliderRange = 4.5f;
        // constexpr auto handThickness = 3.0f;

        auto rect = juce::Rectangle<float>(rx, ry, rw, rw);

        // view background to understand borders
        //        g.setColour(juce::Colour(0x80ff0000));
        //        g.fillRect(rect);

        slider.isEnabled() ? g.setColour(backgroundDarkGrey) : g.setColour(backgroundDarkGreyDisabled);
        juce::Path bgPath;
        bgPath.addCentredArc(cx, cy, radius - extraMargin, radius - extraMargin, 0.0f, rotaryStartAngle, rotaryEndAngle,
                             true);
        g.strokePath(bgPath, juce::PathStrokeType(strokeSliderBase));

        slider.isEnabled() ? g.setColour(statusOutline) : g.setColour(statusOutlineDisabled);
        juce::Path statusRingPath;
        statusRingPath.addCentredArc(cx, cy, radius - extraMargin, radius - extraMargin, 0.0f, zeroAngle, angle, true);
        g.strokePath(statusRingPath, juce::PathStrokeType(strokeSliderRange));

        slider.isEnabled() ? g.setGradientFill(juce::ColourGradient(backgroundMidGrey, rx + rw / 2, ry,
                                                                    gradientDarkGrey, rx + rw / 2, ry + rw, false))
                           : g.setColour(gradientDarkGreyDisabled);
        g.fillEllipse(rect.reduced(extraMargin + statusOutlineThickness + bedOutline + bedThickness));

        juce::Path dialPointerPath;
        dialPointerPath.addEllipse(-strokeSliderRange, -radius + 12.0f, strokeSliderRange * 2, strokeSliderRange * 2);
        dialPointerPath.applyTransform(juce::AffineTransform::rotation(angle).translated(cx, cy));
        g.setColour(statusOutline);
        g.fillPath(dialPointerPath);
    }


    void drawButtonBackground(juce::Graphics& g, juce::Button& button, const juce::Colour& /*backgroundColour*/,
                              bool /*shouldDrawButtonAsHighlighted*/, bool /*shouldDrawButtonAsDown*/) override
    {
        const auto bounds = button.getLocalBounds().toFloat();

        g.setColour(button.getToggleState() ? statusOutline.withMultipliedAlpha(0.8f) : backgroundMidGrey);
        g.fillRoundedRectangle(bounds, 2);
    }

    void drawButtonText(juce::Graphics& g, juce::TextButton& button, bool /*shouldDrawButtonAsHighlighted*/,
                        bool /*shouldDrawButtonAsDown*/) override
    {
        g.setFont(mainFontDefinition);
        g.setColour(button
                        .findColour(button.getToggleState() ? juce::TextButton::textColourOnId
                                                            : juce::TextButton::textColourOffId)
                        .withMultipliedAlpha(button.isEnabled() ? 1.0f : 0.5f));

        const auto yIndent = juce::jmin(4, button.proportionOfHeight(0.3f));
        const auto cornerSize = juce::jmin(button.getHeight(), button.getWidth()) / 2;

        const auto fontHeightInt = juce::roundToInt(mainFontDefinition.getHeight());
        const auto leftIndent = juce::jmin(fontHeightInt, 2 + cornerSize / (button.isConnectedOnLeft() ? 4 : 2));
        const auto rightIndent = juce::jmin(fontHeightInt, 2 + cornerSize / (button.isConnectedOnRight() ? 4 : 2));
        const auto textWidth = button.getWidth() - leftIndent - rightIndent;

        if (textWidth > 0)
        {
            g.drawFittedText(button.getButtonText(), leftIndent, yIndent, textWidth, button.getHeight() - yIndent * 2,
                             juce::Justification::centred, 2);
        }
    }

    void drawBubble(juce::Graphics& g, juce::BubbleComponent& /*b*/, const juce::Point<float>& /*tip*/,
                    const juce::Rectangle<float>& body) override
    {
        g.setColour(backgroundMidGrey);
        g.fillRoundedRectangle(body, 2);
    }

    void drawComboBox(juce::Graphics& g, int width, int height, bool /*isButtonDown*/, int /*buttonX*/, int /*buttonY*/,
                      int /*buttonW*/, int /*buttonH*/, juce::ComboBox& box) override
    {
        auto cornerSize = box.findParentComponentOfClass<juce::ChoicePropertyComponent>() != nullptr ? 0.0f : 1.0f;
        juce::Rectangle<int> boxBounds(0, 0, width, height);

        g.setColour(backgroundDarkGrey);
        g.fillRoundedRectangle(boxBounds.toFloat(), cornerSize);

        juce::Rectangle<int> arrowZone(width - 30, 0, 20, height);
        juce::Path path;
        path.startNewSubPath(arrowZone.getX() + 3.0f, arrowZone.getCentreY() - 2.0f);
        path.lineTo(static_cast<float>(arrowZone.getCentreX()), arrowZone.getCentreY() + 3.0f);
        path.lineTo(arrowZone.getRight() - 3.0f, arrowZone.getCentreY() - 2.0f);

        g.setColour(box.findColour(juce::ComboBox::arrowColourId).withAlpha((box.isEnabled() ? 0.9f : 0.2f)));
        g.strokePath(path, juce::PathStrokeType(2.0f));
    }

    void positionComboBoxText(juce::ComboBox& box, juce::Label& label) override
    {
        label.setBounds(1, 1, box.getWidth()-30, box.getHeight() - 2);
        label.setFont(getComboBoxFont(box));
    }

    void drawPopupMenuBackground(juce::Graphics& g, int width, int height) override
    {
        g.fillAll(backgroundMidGrey);
        juce::ignoreUnused(width, height);

#if !JUCE_MAC
        g.setColour(backgroundDarkGrey.withAlpha(0.6f));
        g.drawRect(0, 0, width, height);
#endif
    }

    juce::Font getSliderPopupFont(juce::Slider&) override
    {
        return mainFontDefinition;
    }

    juce::Font getLabelFont(juce::Label& label) override
    {
        if (auto* slider = dynamic_cast<juce::Slider*>(label.getParentComponent()))
        {
            label.setBorderSize(juce::BorderSize<int>(0));
            label.setColour(juce::Label::outlineColourId, juce::Colours::transparentBlack);
            label.setColour(juce::Label::outlineWhenEditingColourId, juce::Colours::transparentBlack);
        }
        return mainFontDefinition;
    }

    juce::Font getTextButtonFont(juce::TextButton& /*button*/, int /*height*/) override
    {
        return mainFontDefinition;
    }

    juce::Font getAlertWindowMessageFont() override
    {
        return mainFontDefinition;
    }

    juce::Font getPopupMenuFont() override
    {
        return mainFontDefinition;
    }

    juce::Font getComboBoxFont(juce::ComboBox&) override
    {
        return mainFontDefinition;
    }
};
