#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

#include "Constants.h"

class ImageDisplay : public juce::Component
{
  public:
    ImageDisplay()
    {
        backgroundDarkGrey = juce::Colour(Constants::Colors::bg_DarkGrey);
    }

    void paint(juce::Graphics& g) override
    {
        g.setColour(backgroundDarkGrey);
        g.fillRoundedRectangle(getLocalBounds().toFloat(), 3);
    }

    void resized() override {}

    void update(const float& val) {}


  private:
    juce::Colour backgroundDarkGrey;
};
