#pragma once

#include <juce_data_structures/juce_data_structures.h>

#include "GuiConstants.h"

class AppSettings
{
  public:
    [[nodiscard]] static GuiConstants::GradientPreset loadTheme()
    {
        const auto props = makePropsFile();
        const auto val = props->getIntValue("theme", static_cast<int>(Themes::makeTheme(0, true)));
        return static_cast<GuiConstants::GradientPreset>(val);
    }

    static void saveTheme(GuiConstants::GradientPreset preset)
    {
        const auto props = makePropsFile();
        props->setValue("theme", static_cast<int>(preset));
        props->saveIfNeeded();
    }

    [[nodiscard]] static juce::Rectangle<int> loadWindowBounds(int defaultWidth, int defaultHeight)
    {
        const auto props = makePropsFile();
        return {
            props->getIntValue("windowX", 100),
            props->getIntValue("windowY", 100),
            props->getIntValue("windowWidth", defaultWidth),
            props->getIntValue("windowHeight", defaultHeight),
        };
    }

    static void saveWindowBounds(juce::Rectangle<int> bounds)
    {
        const auto props = makePropsFile();
        props->setValue("windowX", bounds.getX());
        props->setValue("windowY", bounds.getY());
        props->setValue("windowWidth", bounds.getWidth());
        props->setValue("windowHeight", bounds.getHeight());
        props->saveIfNeeded();
    }

  private:
    [[nodiscard]] static std::unique_ptr<juce::PropertiesFile> makePropsFile()
    {
        juce::PropertiesFile::Options opts;
        opts.applicationName = JucePlugin_Name;
        opts.filenameSuffix = ".xml";
        opts.folderName = "AbacDsp";
        opts.osxLibrarySubFolder = "Application Support";
        return std::make_unique<juce::PropertiesFile>(opts);
    }
};
