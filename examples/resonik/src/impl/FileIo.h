#pragma once

#include <algorithm>
#include <fstream>
#include <functional>
#include <iostream>
#include <juce_core/juce_core.h>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include "../inc/LuaScriptEngineBase.h"
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
        syncBaseLibraryScripts();
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

    [[nodiscard]] std::string currentParametersAsJson() const
    {
        return nlohmann::json(m_currentParams).dump();
    }

    // Applies a full parameter snapshot captured elsewhere (e.g. embedded in a
    // saved loop) rather than one of the on-disk patch slots/names.
    bool loadParametersFromJson(const std::string& text)
    {
        try
        {
            m_currentParams = nlohmann::json::parse(text).get<PatchParameters>();
            m_currentParams.clearModified();
            return true;
        }
        catch (const nlohmann::json::exception& e)
        {
            reportCorruptPatch("<embedded>", e.what());
            return false;
        }
    }

    void forceSave()
    {
        savePatch(m_currentPatch);
        m_currentParams.clearModified();
    }

    [[nodiscard]] std::vector<std::string> listPatchNames() const
    {
        std::vector<std::string> names;
        const auto rootDir = getPatchDirectory();
        for (const auto& f : rootDir.findChildFiles(juce::File::findFiles, true, "*.json"))
        {
            const auto relative = f.getRelativePathFrom(rootDir).replaceCharacter('\\', '/');
            names.push_back(relative.upToLastOccurrenceOf(".json", false, false).toStdString());
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


    void updateScript(const std::string& value)
    {
        if (!m_isInitialized)
        {
            return;
        }
        m_currentParams.updateScript(value);
    }

    [[nodiscard]] const std::string& currentScript() const
    {
        return m_currentParams.script;
    }

    [[nodiscard]] std::vector<std::string> listScriptNames() const
    {
        std::vector<std::string> names;
        const auto rootDir = getScriptDirectory();
        for (const auto& f : rootDir.findChildFiles(juce::File::findFiles, true, "*.lua"))
        {
            const auto relative = f.getRelativePathFrom(rootDir).replaceCharacter('\\', '/');
            names.push_back(relative.upToLastOccurrenceOf(".lua", false, false).toStdString());
        }
        std::sort(names.begin(), names.end());
        return names;
    }

    [[nodiscard]] const std::string& currentScriptName() const
    {
        return m_currentScriptName;
    }

    bool saveScriptNamed(const std::string& name)
    {
        const std::string filename = getScriptFilename(name);
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
        out << m_currentParams.script;
        m_currentScriptName = name;
        return true;
    }

    bool loadScriptNamed(const std::string& name)
    {
        const std::string filename = getScriptFilename(name);
        std::ifstream in(filename);
        if (filename.empty() || !in)
        {
            std::cerr << "FileIo: ERROR - Failed to open " << filename << " for reading" << std::endl;
            return false;
        }
        std::ostringstream buffer;
        buffer << in.rdbuf();
        updateScript(buffer.str());
        m_currentScriptName = name;
        return true;
    }

    bool deleteScriptNamed(const std::string& name)
    {
        const std::string filename = getScriptFilename(name);
        if (filename.empty())
        {
            return false;
        }
        if (name == m_currentScriptName)
        {
            m_currentScriptName.clear();
        }
        return juce::File(filename).deleteFile();
    }

    bool renameScriptNamed(const std::string& oldName, const std::string& newName)
    {
        const std::string oldFilename = getScriptFilename(oldName);
        const std::string newFilename = getScriptFilename(newName);
        if (oldFilename.empty() || newFilename.empty())
        {
            return false;
        }
        if (!juce::File(oldFilename).moveFileTo(juce::File(newFilename)))
        {
            return false;
        }
        if (oldName == m_currentScriptName)
        {
            m_currentScriptName = newName;
        }
        return true;
    }

    // Resolves an `import "name"` library lookup: the user's own Library/User/ directory
    // takes precedence over the repo-synced Library/Base/ one, so a user copy of the same
    // name overrides the built-in. On failure, notFoundDetail lists the full paths checked.
    [[nodiscard]] static ImportLookup resolveLibraryScript(const std::string_view name)
    {
        const juce::String sanitized = juce::String(std::string(name)).removeCharacters("\\/:*?\"<>|");
        if (sanitized.isEmpty())
        {
            return {std::nullopt, "\"" + std::string(name) + "\" is not a valid library file name"};
        }
        juce::StringArray checkedPaths;
        for (const auto& dir : {getLibraryUserDirectory(), getLibraryBaseDirectory()})
        {
            const juce::File file = dir.getChildFile(sanitized + ".lua");
            if (file.existsAsFile())
            {
                return {file.loadFileAsString().toStdString(), {}};
            }
            checkedPaths.add(file.getFullPathName());
        }
        return {std::nullopt, "looked in " + checkedPaths.joinIntoString("; ").toStdString()};
    }

    // Writes/overwrites a Library/User/ script, e.g. from the LLM-Assist watched-folder
    // workflow. Rejects a name normalizeImportName() would itself reject, so nothing is
    // ever written here that could never actually be import "..."-ed back out.
    [[nodiscard]] static bool saveUserLibraryScript(const std::string_view name, const std::string_view content)
    {
        const std::optional<std::string> normalized = normalizeImportName(name);
        if (!normalized)
        {
            return false;
        }
        const juce::File file = getLibraryUserDirectory().getChildFile(juce::String(*normalized) + ".lua");
        return file.replaceWithText(juce::String(std::string(content)));
    }

    // Every installed library name, User and Base combined - a name in both is listed
    // once, matching resolveLibraryScript()'s own "User overrides Base" resolution.
    [[nodiscard]] static std::vector<std::string> listLibraryScriptNames()
    {
        std::vector<std::string> names;
        for (const auto& dir : {getLibraryUserDirectory(), getLibraryBaseDirectory()})
        {
            for (const auto& f : dir.findChildFiles(juce::File::findFiles, false, "*.lua"))
            {
                names.push_back(f.getFileNameWithoutExtension().toStdString());
            }
        }
        std::sort(names.begin(), names.end());
        names.erase(std::unique(names.begin(), names.end()), names.end());
        return names;
    }

    // Repo-synced (see syncBaseLibraryScripts()) - not meant to be hand-edited by users.
    static juce::File getLibraryBaseDirectory()
    {
        const auto dir = getLibraryDirectory().getChildFile("Base");
        dir.createDirectory();
        return dir;
    }

    // The user's own import-able library scripts; never touched by syncBaseLibraryScripts().
    static juce::File getLibraryUserDirectory()
    {
        const auto dir = getLibraryDirectory().getChildFile("User");
        dir.createDirectory();
        return dir;
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
        const auto dir = base.getChildFile("AbacDsp").getChildFile("Resonik");
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

    // Named patches are stored one file per name. A "/" in the name denotes a subfolder
    // (e.g. "chorus/classic tri chorus"), created on demand; each path segment is otherwise
    // sanitized to strip characters invalid in filenames across platforms.
    static std::string getNamedPatchFilename(const std::string& name)
    {
        juce::StringArray segments;
        segments.addTokens(juce::String(name), "/", "");
        segments.trim();
        segments.removeEmptyStrings();
        if (segments.isEmpty())
        {
            return {};
        }
        juce::File dir = getPatchDirectory();
        for (int i = 0; i < segments.size() - 1; ++i)
        {
            const juce::String sanitized = segments[i].removeCharacters("\\:*?\"<>|");
            if (sanitized.isEmpty())
            {
                return {};
            }
            dir = dir.getChildFile(sanitized);
        }
        const juce::String fileName = segments[segments.size() - 1].removeCharacters("\\:*?\"<>|");
        if (fileName.isEmpty())
        {
            return {};
        }
        dir.createDirectory();
        return dir.getChildFile(fileName + ".json").getFullPathName().toStdString();
    }


    static juce::File getScriptDirectory()
    {
        auto base = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory);
#if JUCE_MAC
        base = base.getChildFile("Application Support");
#endif
        const auto dir = base.getChildFile("AbacDsp").getChildFile("Resonik").getChildFile("Scripts");
        dir.createDirectory();
        return dir;
    }

    static juce::File getLibraryDirectory()
    {
        auto base = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory);
#if JUCE_MAC
        base = base.getChildFile("Application Support");
#endif
        const auto dir = base.getChildFile("AbacDsp").getChildFile("Resonik").getChildFile("Library");
        dir.createDirectory();
        return dir;
    }

    // Refreshes Library/Base/ from the repo's base-scripts/ directory (only available in a
    // dev build from a real checkout - RESONIK_BASE_SCRIPTS_DIR is undefined
    // otherwise, in which case this is a no-op and whatever is already on disk is used).
    static void syncBaseLibraryScripts()
    {
#ifdef RESONIK_BASE_SCRIPTS_DIR
        const juce::File repoDir(RESONIK_BASE_SCRIPTS_DIR);
        if (!repoDir.isDirectory())
        {
            return;
        }
        const juce::File targetDir = getLibraryBaseDirectory();
        for (const auto& source : repoDir.findChildFiles(juce::File::findFiles, false, "*.lua"))
        {
            source.copyFileTo(targetDir.getChildFile(source.getFileName()));
        }
#endif
    }

    static std::string getScriptFilename(const std::string& name)
    {
        juce::StringArray segments;
        segments.addTokens(juce::String(name), "/", "");
        segments.trim();
        segments.removeEmptyStrings();
        if (segments.isEmpty())
        {
            return {};
        }
        juce::File dir = getScriptDirectory();
        for (int i = 0; i < segments.size() - 1; ++i)
        {
            const juce::String sanitized = segments[i].removeCharacters("\\:*?\"<>|");
            if (sanitized.isEmpty())
            {
                return {};
            }
            dir = dir.getChildFile(sanitized);
        }
        const juce::String fileName = segments[segments.size() - 1].removeCharacters("\\:*?\"<>|");
        if (fileName.isEmpty())
        {
            return {};
        }
        dir.createDirectory();
        return dir.getChildFile(fileName + ".lua").getFullPathName().toStdString();
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
    std::string m_currentScriptName;
};
