#include <cctype>
#include <cstdint>
#include <gtest/gtest.h>
#include <nlohmann/json.hpp>
#include <string>

#include "inc/AuthoringHttpServerCore.h"

TEST(AuthoringHttpServerCore, TokenIsThirtyTwoHexChars)
{
    const auto token = generateAuthoringToken();
    EXPECT_EQ(token.size(), 32u);
    for (const char c : token)
    {
        EXPECT_TRUE(std::isxdigit(static_cast<unsigned char>(c))) << token;
    }
}

TEST(AuthoringHttpServerCore, TwoGeneratedTokensDiffer)
{
    EXPECT_NE(generateAuthoringToken(), generateAuthoringToken());
}

TEST(AuthoringHttpServerCore, TokensMatchRequiresExactEquality)
{
    EXPECT_TRUE(authoringTokensMatch("abc123", "abc123"));
    EXPECT_FALSE(authoringTokensMatch("abc123", "abc124"));
    EXPECT_FALSE(authoringTokensMatch("abc123", "abc12"));
    EXPECT_FALSE(authoringTokensMatch("", "abc123"));
    EXPECT_TRUE(authoringTokensMatch("", ""));
}

TEST(AuthoringHttpServerCore, DiscoveryFileJsonRoundTripsExpectedFields)
{
    AuthoringInstanceInfo info;
    info.module = "dronesequencer";
    info.pid = 4242;
    info.port = 55555;
    info.token = "deadbeef";
    info.startedAtEpochMs = 1700000000000;

    const auto j = nlohmann::json::parse(makeDiscoveryFileJson(info));
    EXPECT_EQ(j.at("module").get<std::string>(), "dronesequencer");
    EXPECT_EQ(j.at("pid").get<int>(), 4242);
    EXPECT_EQ(j.at("port").get<int>(), 55555);
    EXPECT_EQ(j.at("token").get<std::string>(), "deadbeef");
    EXPECT_EQ(j.at("baseUrl").get<std::string>(), "http://127.0.0.1:55555");
    EXPECT_EQ(j.at("startedAtEpochMs").get<std::int64_t>(), 1700000000000);
}

TEST(AuthoringHttpServerCore, DiscoveryFilenameCombinesPidAndPort)
{
    EXPECT_EQ(makeDiscoveryFilename(4242, 55555), "4242-55555.json");
}

TEST(AuthoringHttpServerCore, StatusJsonRoundTripsExpectedFields)
{
    AuthoringStatusSnapshot s;
    s.module = "dronesequencer";
    s.pid = 1;
    s.port = 2;
    s.wrapperType = "Standalone";
    s.currentScriptName = "my-script";
    s.currentPatchName = "my-patch";
    s.hasScriptError = true;
    s.scriptErrorMessage = "boom";
    s.cpuLoadPercent = 12.5f;
    s.poolBytesInUse = 4096;

    const auto j = nlohmann::json::parse(makeStatusJson(s));
    EXPECT_EQ(j.at("module").get<std::string>(), "dronesequencer");
    EXPECT_EQ(j.at("wrapperType").get<std::string>(), "Standalone");
    EXPECT_EQ(j.at("currentScriptName").get<std::string>(), "my-script");
    EXPECT_EQ(j.at("currentPatchName").get<std::string>(), "my-patch");
    EXPECT_TRUE(j.at("hasScriptError").get<bool>());
    EXPECT_EQ(j.at("scriptErrorMessage").get<std::string>(), "boom");
    EXPECT_FLOAT_EQ(j.at("cpuLoadPercent").get<float>(), 12.5f);
    EXPECT_EQ(j.at("poolBytesInUse").get<std::size_t>(), 4096u);
}

TEST(AuthoringHttpServerCore, ApplyScriptResultJsonReflectsCompiledAndError)
{
    const auto ok = nlohmann::json::parse(makeApplyScriptResultJson(true, ""));
    EXPECT_TRUE(ok.at("compiled").get<bool>());
    EXPECT_EQ(ok.at("error").get<std::string>(), "");

    const auto failed = nlohmann::json::parse(makeApplyScriptResultJson(false, "syntax error"));
    EXPECT_FALSE(failed.at("compiled").get<bool>());
    EXPECT_EQ(failed.at("error").get<std::string>(), "syntax error");
}
