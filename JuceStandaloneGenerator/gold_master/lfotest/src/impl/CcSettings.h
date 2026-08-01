#pragma once

#include <fstream>
#include <iostream>
#include <juce_core/juce_core.h>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

struct CcMappingOverride
{
    std::string paramId;
    int controller;
    float valueLow;
    float valueHigh;
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(CcMappingOverride, paramId, controller, valueLow, valueHigh)

class CcSettings
{
  public:
    [[nodiscard]] static std::vector<CcMappingOverride> load()
    {
        std::ifstream in(getFilename());
        if (!in)
        {
            return {};
        }
        nlohmann::json j;
        in >> j;
        return j.get<std::vector<CcMappingOverride>>();
    }

    static void save(const std::vector<CcMappingOverride>& overrides)
    {
        std::ofstream out(getFilename());
        if (!out)
        {
            std::cerr << "CcSettings: ERROR - Failed to open " << getFilename() << " for writing" << std::endl;
            return;
        }
        const nlohmann::json j = overrides;
        out << j.dump(2);
    }

  private:
    // JUCE's userApplicationDataDirectory is bare "~/Library" on macOS; the
    // "Application Support" segment is a convention apps must add themselves.
    static std::string getFilename()
    {
        auto base = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory);
#if JUCE_MAC
        base = base.getChildFile("Application Support");
#endif
        const auto dir = base.getChildFile("AbacDsp").getChildFile("Lfotest");
        dir.createDirectory();
        return dir.getChildFile("ccmapping.json").getFullPathName().toStdString();
    }
};
