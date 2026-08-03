#pragma once

#include <algorithm>
#include <fstream>
#include <functional>
#include <iostream>
#include <juce_core/juce_core.h>
#include <string>
#include <vector>

#include "PatchParameters.h"

class FileIo
{
  public:
    using PromptCallback = std::function<bool(const std::string&)>;

    FileIo()
        : m_currentPatch{0}
    {
    }

    void initialize(const std::vector<int>& patchIndex)
    {
        m_currentPatch = patchIndex;
        loadPatch(patchIndex);
        m_currentParams.clearModified();
        m_isInitialized = true;
    }

    void enable()
    {
        m_enabled = true;
    }

    [[nodiscard]] bool areParametersModified() const
    {
        if (!m_enabled)
        {
            return false;
        }
        return m_currentParams.isModified();
    }

    void loadPatchDirect(const std::vector<int>& patchIndex)
    {
        if (patchIndex == m_currentPatch)
        {
            return;
        }
        m_currentPatch = patchIndex;
        loadPatch(patchIndex);
        m_currentParams.clearModified();
    }

    void updateParameter(const PatchParameters::Id id, const float value)
    {
        if (!m_isInitialized)
        {
            return;
        }
        m_currentParams.updateById(id, value);
    }

    bool handlePatchChange(const std::vector<int>& newPatchIndex, const PromptCallback& promptCallback)
    {
        if (newPatchIndex == m_currentPatch)
        {
            return false;
        }
        if (m_currentParams.isModified())
        {
            const std::string message = "Parameters have changed, do you want to save before loading new patch?";
            if (promptCallback && promptCallback(message))
            {
                savePatch(m_currentPatch);
            }
        }
        m_currentPatch = newPatchIndex;
        loadPatch(newPatchIndex);
        m_currentParams.clearModified();
        return true;
    }

    [[nodiscard]] const PatchParameters& getCurrentParameters() const
    {
        return m_currentParams;
    }

    void forceSave()
    {
        savePatch(m_currentPatch);
        m_currentParams.clearModified();
    }

    [[nodiscard]] std::vector<std::string> listPatchNames() const
    {
        std::vector<std::string> names;
        for (const auto& f : getPatchDirectory().findChildFiles(juce::File::findFiles, false, "*.json"))
        {
            names.push_back(f.getFileNameWithoutExtension().toStdString());
        }
        std::sort(names.begin(), names.end());
        return names;
    }

    [[nodiscard]] const std::string& currentPatchName() const
    {
        return m_currentPatchName;
    }

    bool savePatchNamed(const std::string& name)
    {
        const std::string filename = getNamedPatchFilename(name);
        if (filename.empty())
        {
            return false;
        }
        std::ofstream out(filename);
        if (!out)
        {
            std::cerr << "FileIo: ERROR - Failed to open " << filename << " for writing" << std::endl;
            return false;
        }
        const nlohmann::json j = m_currentParams;
        out << j.dump(2);
        m_currentPatchName = name;
        m_currentParams.clearModified();
        return true;
    }

    bool loadPatchNamed(const std::string& name)
    {
        const std::string filename = getNamedPatchFilename(name);
        std::ifstream in(filename);
        if (filename.empty() || !in)
        {
            std::cerr << "FileIo: ERROR - Failed to open " << filename << " for reading" << std::endl;
            return false;
        }
        try
        {
            nlohmann::json j;
            in >> j;
            m_currentParams = j.get<PatchParameters>();
            m_currentPatchName = name;
            m_currentParams.clearModified();
            return true;
        }
        catch (const nlohmann::json::exception& e)
        {
            reportCorruptPatch(filename, e.what());
            return false;
        }
    }

    bool deletePatchNamed(const std::string& name)
    {
        const std::string filename = getNamedPatchFilename(name);
        if (filename.empty())
        {
            return false;
        }
        if (name == m_currentPatchName)
        {
            m_currentPatchName.clear();
        }
        return juce::File(filename).deleteFile();
    }

    bool renamePatchNamed(const std::string& oldName, const std::string& newName)
    {
        const std::string oldFilename = getNamedPatchFilename(oldName);
        const std::string newFilename = getNamedPatchFilename(newName);
        if (oldFilename.empty() || newFilename.empty())
        {
            return false;
        }
        if (!juce::File(oldFilename).moveFileTo(juce::File(newFilename)))
        {
            return false;
        }
        if (oldName == m_currentPatchName)
        {
            m_currentPatchName = newName;
        }
        return true;
    }

  private:
    // JUCE's userApplicationDataDirectory is bare "~/Library" on macOS; the
    // "Application Support" segment is a convention apps must add themselves.
    // On Windows/Linux it already points at the right per-user data folder.
    static juce::File getPatchDirectory()
    {
        auto base = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory);
#if JUCE_MAC
        base = base.getChildFile("Application Support");
#endif
        const auto dir = base.getChildFile("AbacDsp").getChildFile("Minireverb");
        dir.createDirectory();
        return dir;
    }

    static std::string getPatchFilename(const std::vector<int>& patchIndex)
    {
        std::string filename = "patch";
        for (size_t i = 0; i < patchIndex.size(); ++i)
        {
            filename += "." + std::to_string(patchIndex[i] + 1);
        }
        filename += ".json";
        return getPatchDirectory().getChildFile(filename).getFullPathName().toStdString();
    }

    static int getParameterId(const std::string_view& paramName)
    {
        for (size_t i = 0; i < PatchParameters::count(); ++i)
        {
            if (PatchParameters::paramNames[i] == paramName)
            {
                return static_cast<int>(i);
            }
        }
        return -1;
    }

    // A patch file that fails to parse or no longer matches PatchParameters' fields must not
    // crash the load: report it and leave m_currentParams untouched (default at startup, or
    // whatever was last successfully loaded/edited otherwise).
    static void reportCorruptPatch(const std::string& filename, const std::string& reason)
    {
        std::cerr << "FileIo: ERROR - Corrupt or incompatible patch file, keeping current settings.\n"
                  << "  File: " << filename << "\n"
                  << "  Reason: " << reason << std::endl;
    }

    // Named patches are stored one file per name, so the name has to survive as a filename;
    // strip characters that are invalid (or awkward, e.g. path separators) across platforms.
    static std::string getNamedPatchFilename(const std::string& name)
    {
        const juce::String sanitized = juce::String(name).removeCharacters("/\\:*?\"<>|").trim();
        if (sanitized.isEmpty())
        {
            return {};
        }
        return getPatchDirectory().getChildFile(sanitized + ".json").getFullPathName().toStdString();
    }

    bool savePatch(const std::vector<int>& patchIndex)
    {
        const std::string filename = getPatchFilename(patchIndex);
        std::ofstream out(filename);
        if (!out)
        {
            std::cerr << "FileIo: ERROR - Failed to open " << filename << " for writing" << std::endl;
            return false;
        }
        const nlohmann::json j = m_currentParams;
        out << j.dump(2);
        std::cout << "FileIo: File " << filename << " saved" << std::endl;
        std::cout << j.dump() << std::endl;
        return true;
    }

    bool loadPatch(const std::vector<int>& patchIndex)
    {
        const std::string filename = getPatchFilename(patchIndex);
        std::ifstream in(filename);
        if (!in)
        {
            std::cout << "FileIo: File " << filename << " not found, using defaults" << std::endl;
            m_currentParams = PatchParameters{};
            return false;
        }
        try
        {
            nlohmann::json j;
            in >> j;
            m_currentParams = j.get<PatchParameters>();
            std::cout << "FileIo: File " << filename << " loaded" << std::endl;
            std::cout << j.dump() << std::endl << std::endl;
            return true;
        }
        catch (const nlohmann::json::exception& e)
        {
            reportCorruptPatch(filename, e.what());
            return false;
        }
    }

    bool m_isInitialized = false;
    bool m_enabled = false;
    std::vector<int> m_currentPatch;
    PatchParameters m_currentParams;
    std::string m_currentPatchName;
};
