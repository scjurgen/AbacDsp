#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <nlohmann/json.hpp>
#include <random>
#include <string>
#include <string_view>

// Fixed, always-copied shared component (CPP_SOURCE_FILES_LUA) - must stay JUCE-free so
// the token/discovery/status-shaping logic can be unit-tested without a real socket or a
// JUCE event loop. AuthoringHttpServer.h is the JUCE glue that owns the actual listener.

// A fresh 32-hex-character token (16 random bytes) - regenerated every time Authoring
// Mode is (re-)enabled, so a stale bookmarked dashboard URL from an earlier session stops
// working once the instance restarts.
[[nodiscard]] inline std::string generateAuthoringToken()
{
    std::random_device rd;
    std::array<std::uint8_t, 16> bytes{};
    for (auto& b : bytes)
    {
        b = static_cast<std::uint8_t>(rd() & 0xFF);
    }
    static constexpr std::string_view kHex = "0123456789abcdef";
    std::string token;
    token.reserve(bytes.size() * 2);
    for (const auto b : bytes)
    {
        token += kHex[(b >> 4) & 0x0F];
        token += kHex[b & 0x0F];
    }
    return token;
}

// Fixed-time comparison (no early exit on the first mismatch) so a wrong token's response
// latency can't leak how many of its leading characters were guessed correctly.
[[nodiscard]] inline bool authoringTokensMatch(const std::string_view expected, const std::string_view given) noexcept
{
    if (expected.size() != given.size())
    {
        return false;
    }
    unsigned char diff = 0;
    for (std::size_t i = 0; i < expected.size(); ++i)
    {
        diff |= static_cast<unsigned char>(expected[i]) ^ static_cast<unsigned char>(given[i]);
    }
    return diff == 0;
}

// What one running instance writes into the shared, per-machine discovery directory so a
// client can enumerate every Authoring-Mode instance of every plugin at once.
struct AuthoringInstanceInfo
{
    std::string module;
    int pid{0};
    int port{0};
    std::string token;
    std::int64_t startedAtEpochMs{0};
};

[[nodiscard]] inline std::string makeDiscoveryFileJson(const AuthoringInstanceInfo& info)
{
    const nlohmann::json j{
        {"module", info.module},
        {"pid", info.pid},
        {"port", info.port},
        {"token", info.token},
        {"baseUrl", "http://127.0.0.1:" + std::to_string(info.port)},
        {"startedAtEpochMs", info.startedAtEpochMs},
    };
    return j.dump(2);
}

[[nodiscard]] inline std::string makeDiscoveryFilename(const int pid, const int port)
{
    return std::to_string(pid) + "-" + std::to_string(port) + ".json";
}

// The payload for both GET /status and (content-negotiated) GET /, and what the
// dashboard's own polling JS fetches - one shape, one place it's built.
struct AuthoringStatusSnapshot
{
    std::string module;
    int pid{0};
    int port{0};
    std::string wrapperType;
    std::string currentScriptName;
    std::string currentPatchName;
    bool hasScriptError{false};
    std::string scriptErrorMessage;
    float cpuLoadPercent{0.f};
    std::size_t poolBytesInUse{0};
    bool isRecording{false};
    float recordingElapsedSeconds{0.f};
};

[[nodiscard]] inline std::string makeStatusJson(const AuthoringStatusSnapshot& s)
{
    const nlohmann::json j{
        {"module", s.module},
        {"pid", s.pid},
        {"port", s.port},
        {"wrapperType", s.wrapperType},
        {"currentScriptName", s.currentScriptName},
        {"currentPatchName", s.currentPatchName},
        {"hasScriptError", s.hasScriptError},
        {"scriptErrorMessage", s.scriptErrorMessage},
        {"cpuLoadPercent", s.cpuLoadPercent},
        {"poolBytesInUse", s.poolBytesInUse},
        {"isRecording", s.isRecording},
        {"recordingElapsedSeconds", s.recordingElapsedSeconds},
    };
    return j.dump();
}

// POST /script's response - identical semantics to the old folder watchdog's
// state-<name>.json, just returned synchronously instead of polled for.
[[nodiscard]] inline std::string makeApplyScriptResultJson(const bool compiled, const std::string_view error)
{
    const nlohmann::json j{{"compiled", compiled}, {"error", std::string(error)}};
    return j.dump();
}

// POST /record/start's response - a caller-chosen filename is never accepted (no
// path-traversal surface to validate), so `path` is always the server's own choice.
struct AuthoringRecordStartResult
{
    bool started{false};
    std::string path;
    std::string error;
};

[[nodiscard]] inline std::string makeRecordStartResultJson(const AuthoringRecordStartResult& r)
{
    const nlohmann::json j{{"started", r.started}, {"path", r.path}, {"error", r.error}};
    return j.dump();
}

// POST /record/stop's response. `capped` means the safety-duration ceiling was hit before
// this call - the file is still valid, just shorter than whatever was actually requested.
struct AuthoringRecordStopResult
{
    bool wasRecording{false};
    std::string path;
    double durationSeconds{0.0};
    bool capped{false};
};

[[nodiscard]] inline std::string makeRecordStopResultJson(const AuthoringRecordStopResult& r)
{
    const nlohmann::json j{{"wasRecording", r.wasRecording},
                           {"path", r.path},
                           {"durationSeconds", r.durationSeconds},
                           {"capped", r.capped}};
    return j.dump();
}
