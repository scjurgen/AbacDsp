#pragma once

#include <juce_data_structures/juce_data_structures.h>
#include <optional>

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

    // nullopt means "never saved" - the caller centres the dialog on its default size
    // instead of restoring a position, rather than us inventing one here.
    [[nodiscard]] static std::optional<juce::Rectangle<int>> loadScriptEditorBounds()
    {
        const auto props = makePropsFile();
        if (!props->containsKey("scriptEditorWidth"))
        {
            return std::nullopt;
        }
        return juce::Rectangle<int>{
            props->getIntValue("scriptEditorX", 100),
            props->getIntValue("scriptEditorY", 100),
            props->getIntValue("scriptEditorWidth", 800),
            props->getIntValue("scriptEditorHeight", 600),
        };
    }

    static void saveScriptEditorBounds(juce::Rectangle<int> bounds)
    {
        const auto props = makePropsFile();
        props->setValue("scriptEditorX", bounds.getX());
        props->setValue("scriptEditorY", bounds.getY());
        props->setValue("scriptEditorWidth", bounds.getWidth());
        props->setValue("scriptEditorHeight", bounds.getHeight());
        props->saveIfNeeded();
    }

    [[nodiscard]] static juce::String loadLlmAssistFolder()
    {
        const auto props = makePropsFile();
        return props->getValue("llmAssistFolder");
    }

    static void saveLlmAssistFolder(const juce::String& folder)
    {
        const auto props = makePropsFile();
        props->setValue("llmAssistFolder", folder);
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
