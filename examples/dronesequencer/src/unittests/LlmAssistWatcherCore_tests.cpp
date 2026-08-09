#include <gtest/gtest.h>

#include "inc/LlmAssistWatcherCore.h"

TEST(LlmAssistWatcherCore, NoFilesYieldsNoCandidate)
{
    EXPECT_EQ(selectLlmAssistCandidate({}), -1);
}

TEST(LlmAssistWatcherCore, PicksNewestMtime)
{
    const std::vector<std::pair<std::string, std::int64_t>> files{
        {"a.lua", 100},
        {"b.lua", 300},
        {"c.lua", 200},
    };
    EXPECT_EQ(selectLlmAssistCandidate(files), 1);
}

TEST(LlmAssistWatcherCore, ExcludesPulledFiles)
{
    const std::vector<std::pair<std::string, std::int64_t>> files{
        {"pulled-a-999.lua", 500},
        {"b.lua", 200},
    };
    EXPECT_EQ(selectLlmAssistCandidate(files), 1);
}

TEST(LlmAssistWatcherCore, AllPulledYieldsNoCandidate)
{
    const std::vector<std::pair<std::string, std::int64_t>> files{
        {"pulled-a-1.lua", 100},
        {"pulled-b-2.lua", 200},
    };
    EXPECT_EQ(selectLlmAssistCandidate(files), -1);
}

TEST(LlmAssistWatcherCore, TiesBreakAlphabetically)
{
    const std::vector<std::pair<std::string, std::int64_t>> files{
        {"b.lua", 100},
        {"a.lua", 100},
    };
    EXPECT_EQ(selectLlmAssistCandidate(files), 1);
}

TEST(LlmAssistWatcherCore, MakesPulledFilename)
{
    EXPECT_EQ(makePulledFilename("drone", 1234), "pulled-drone-1234.lua");
}

TEST(LlmAssistWatcherCore, MakesStateFilename)
{
    EXPECT_EQ(makeStateFilename("drone"), "state-drone.json");
}
