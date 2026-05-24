#pragma once

#include <fstream>
#include <iostream>
#include <string>
#include <functional>
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

    void updateParameter(const std::string& paramName, const float value)
    {
        if (!m_isInitialized)
        {
            return;
        }
        for (size_t i = 0; i < PatchParameters::count(); ++i)
        {
            if (PatchParameters::paramNames[i] == paramName)
            {
                m_currentParams.updateById(static_cast<PatchParameters::Id>(i), value);
                return;
            }
        }
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

  private:
    static std::string getPatchFilename(const std::vector<int>& patchIndex)
    {
        std::string filename = "patch";
        for (size_t i = 0; i < patchIndex.size(); ++i)
        {
            filename += "." + std::to_string(patchIndex[i] + 1);
        }
        filename += ".json";
        return filename;
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
        std::cout << j.dump();
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
        nlohmann::json j;
        in >> j;
        m_currentParams = j.get<PatchParameters>();
        std::cout << "FileIo: File " << filename << " loaded" << std::endl;
        std::cout << j.dump();
        return true;
    }

    bool m_isInitialized = false;
    bool m_enabled = false;
    std::vector<int> m_currentPatch;
    PatchParameters m_currentParams;
};
