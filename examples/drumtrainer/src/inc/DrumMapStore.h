#pragma once

#include <juce_core/juce_core.h>
#include <juce_data_structures/juce_data_structures.h>
#include <memory>
#include <optional>

// Remembers the last successfully loaded drum map file in the app's settings file, under its
// own key next to the settings AppSettings keeps there.
class DrumMapStore
{
  public:
    [[nodiscard]] static std::optional<juce::File> savedFile()
    {
        const auto path = makePropsFile()->getValue(kPathKey);
        if (path.isEmpty())
        {
            return std::nullopt;
        }
        return juce::File(path);
    }

    static void saveFile(const juce::File& file)
    {
        const auto props = makePropsFile();
        props->setValue(kPathKey, file.getFullPathName());
        props->saveIfNeeded();
    }

    static void clear()
    {
        const auto props = makePropsFile();
        props->removeValue(kPathKey);
        props->saveIfNeeded();
    }

    // The current map's folder, else the repo's drum-maps/ folder in a dev build, else Documents.
    [[nodiscard]] static juce::File browseStartDirectory(const juce::File& current)
    {
        if (current.existsAsFile())
        {
            return current.getParentDirectory();
        }
#ifdef DRUMTRAINER_DRUM_MAPS_DIR
        if (const juce::File shipped(DRUMTRAINER_DRUM_MAPS_DIR); shipped.isDirectory())
        {
            return shipped;
        }
#endif
        return juce::File::getSpecialLocation(juce::File::userDocumentsDirectory);
    }

  private:
    static constexpr const char* kPathKey = "drumMapPath";

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
