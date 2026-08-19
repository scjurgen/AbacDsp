#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

// Picks the newest-mtime entry whose name does not start with "pulled-", tie-broken
// alphabetically. Returns -1 if no candidate exists. Kept JUCE-free for unit testing.
[[nodiscard]] inline int selectLlmAssistCandidate(const std::vector<std::pair<std::string, std::int64_t>>& files)
{
    int best = -1;
    for (std::size_t i = 0; i < files.size(); ++i)
    {
        const auto& [name, modTime] = files[i];
        if (name.starts_with("pulled-"))
        {
            continue;
        }
        if (best < 0)
        {
            best = static_cast<int>(i);
            continue;
        }
        const auto& [bestName, bestTime] = files[static_cast<std::size_t>(best)];
        if (modTime > bestTime || (modTime == bestTime && name < bestName))
        {
            best = static_cast<int>(i);
        }
    }
    return best;
}

[[nodiscard]] inline std::string makePulledFilename(const std::string& baseName, std::int64_t epochMs)
{
    return "pulled-" + baseName + "-" + std::to_string(epochMs) + ".lua";
}

[[nodiscard]] inline std::string makeStateFilename(const std::string& baseName)
{
    return "state-" + baseName + ".json";
}
