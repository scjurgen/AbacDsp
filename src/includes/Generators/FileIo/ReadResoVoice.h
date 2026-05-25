#pragma once

#include <fstream>
#include <sstream>
#include <string>
#include <vector>

struct CsvVoice
{
    float ratio;
    float decayMs;
    float impulseFeedLevel;
    float sustainFeedFactor;
    float pan;
    float waitMs;
};

[[nodiscard]] inline bool readVoiceSettings(const std::string& filename, std::vector<CsvVoice>& settings)
{
    std::ifstream file(filename);

    if (!file.is_open())
    {
        return false;
    }
    settings.clear();
    std::string line;
    while (std::getline(file, line))
    {
        if (line.empty() || line[0] == '#')
        {
            continue;
        }

        std::istringstream stream(line);
        CsvVoice setting{};
        if (stream >> setting.ratio >> setting.decayMs >> setting.impulseFeedLevel >> setting.sustainFeedFactor >>
            setting.pan >> setting.waitMs)
        {
            settings.push_back(setting);
        }
    }
    return true;
}
