#pragma once

#include <juce_core/juce_core.h>
#include <juce_data_structures/juce_data_structures.h>
#include <memory>
#include <string>

// Keeps the lesson progress, a JSON text, in the app's settings file under its own key.
class ProgressStore
{
  public:
    [[nodiscard]] static std::string load()
    {
        return makePropsFile()->getValue(kKey).toStdString();
    }

    static void save(const std::string& json)
    {
        const auto props = makePropsFile();
        props->setValue(kKey, juce::String(json));
        props->saveIfNeeded();
    }

  private:
    static constexpr const char* kKey = "lessonProgress";

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
