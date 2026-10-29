#pragma once

#include <bit>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#include "TakeAnalysis.h"

// A scored take on disk: the recorded audio, what was expected and what was heard, and the report.
namespace TakeExport
{

struct Context
{
    std::string instrument;
    float inputGainDb{0.f};
};

namespace Detail
{

inline void put16(std::vector<uint8_t>& out, const uint16_t value)
{
    out.push_back(static_cast<uint8_t>(value & 0xFF));
    out.push_back(static_cast<uint8_t>(value >> 8));
}

inline void put32(std::vector<uint8_t>& out, const uint32_t value)
{
    put16(out, static_cast<uint16_t>(value & 0xFFFF));
    put16(out, static_cast<uint16_t>(value >> 16));
}

inline void putTag(std::vector<uint8_t>& out, const std::string_view tag)
{
    out.insert(out.end(), tag.begin(), tag.end());
}

inline bool writeBytes(const std::filesystem::path& path, const std::span<const char> bytes)
{
    std::ofstream file{path, std::ios::binary};
    file.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    return static_cast<bool>(file);
}

}

// Mono 32 bit float WAV, lossless for the captured samples.
[[nodiscard]] inline std::vector<uint8_t> wavBytes(const std::span<const float> samples, const float sampleRate)
{
    constexpr uint32_t kFormatChunk{18};
    constexpr uint16_t kFloatFormat{3};
    constexpr uint16_t kBytesPerSample{4};
    const auto rate = static_cast<uint32_t>(sampleRate);
    const auto dataBytes = static_cast<uint32_t>(samples.size() * kBytesPerSample);
    std::vector<uint8_t> out;
    out.reserve(dataBytes + 64);
    Detail::putTag(out, "RIFF");
    Detail::put32(out, 4 + (8 + kFormatChunk) + (8 + 4) + (8 + dataBytes));
    Detail::putTag(out, "WAVE");
    Detail::putTag(out, "fmt ");
    Detail::put32(out, kFormatChunk);
    Detail::put16(out, kFloatFormat);
    Detail::put16(out, 1);
    Detail::put32(out, rate);
    Detail::put32(out, rate * kBytesPerSample);
    Detail::put16(out, kBytesPerSample);
    Detail::put16(out, 8 * kBytesPerSample);
    Detail::put16(out, 0);
    Detail::putTag(out, "fact");
    Detail::put32(out, 4);
    Detail::put32(out, static_cast<uint32_t>(samples.size()));
    Detail::putTag(out, "data");
    Detail::put32(out, dataBytes);
    for (const float sample : samples)
    {
        Detail::put32(out, std::bit_cast<uint32_t>(sample));
    }
    return out;
}

[[nodiscard]] inline std::string describe(const TakeAnalysis::TakeResult& result, const Context& context)
{
    const auto& setup = result.setup;
    const auto& pattern = setup.pattern;
    nlohmann::json root;
    root["lesson"] = {{"name", result.lessonName}, {"stem", result.lessonStem}};
    root["take"] = {{"midi", result.midi},
                    {"sampleRate", result.sampleRate},
                    {"bpm", setup.bpm},
                    {"clickOn", setup.clickOn},
                    {"latencyMs", setup.latencyMs},
                    {"downbeatSeconds", setup.downbeatSeconds},
                    {"instrument", context.instrument},
                    {"inputGainDb", context.inputGainDb}};
    root["phrase"] = {{"pickupBeats", pattern.pickupBeats},
                      {"patternBars", pattern.patternBars},
                      {"beatsPerBar", pattern.beatsPerBar},
                      {"octaveFree", pattern.scoring.octaveFree},
                      {"notes", nlohmann::json::array()}};
    for (size_t i = 0; i < pattern.noteCount; ++i)
    {
        const auto& note = pattern.notes[i];
        root["phrase"]["notes"].push_back({{"positionBeats", note.positionBeats},
                                           {"durationBeats", note.durationBeats},
                                           {"pitch", note.pitch},
                                           {"velocity", note.velocity}});
    }
    root["detected"] = nlohmann::json::array();
    for (const auto& played : result.played)
    {
        root["detected"].push_back({{"onsetSeconds", played.onsetSeconds},
                                    {"offsetSeconds", played.offsetSeconds},
                                    {"midiNote", played.midiNote},
                                    {"pitchConfidence", played.pitchConfidence},
                                    {"peakDb", played.peakDb},
                                    {"attackRmsDb", played.attackRmsDb}});
    }
    const auto& score = result.score;
    root["score"] = {{"overall", score.overall},
                     {"notes", score.notes},
                     {"rhythm", score.rhythm},
                     {"dynamics", score.dynamics},
                     {"passed", score.passed},
                     {"expected", score.expectedCount},
                     {"matched", score.matchedCount},
                     {"missed", score.missedCount},
                     {"extra", score.extraCount},
                     {"octaveShift", score.octaveShift},
                     {"meanTimingMs", score.meanTimingMs}};
    return root.dump(2);
}

[[nodiscard]] inline std::string folderName(const std::string_view timestamp, const std::string_view lessonStem)
{
    return std::string{timestamp} + "-" + (lessonStem.empty() ? std::string{"script"} : std::string{lessonStem});
}

// Writes take.wav (audio takes), take.json and report.html below root; returns the folder, or nothing on failure.
[[nodiscard]] inline std::optional<std::filesystem::path> writeTake(const std::filesystem::path& root,
                                                                    const std::string_view timestamp,
                                                                    const TakeAnalysis::TakeResult& result,
                                                                    const Context& context)
{
    const auto folder = root / folderName(timestamp, result.lessonStem);
    std::error_code error;
    std::filesystem::create_directories(folder, error);
    if (error)
    {
        return std::nullopt;
    }
    const auto text = [](const std::string& s) { return std::span<const char>{s.data(), s.size()}; };
    bool ok = Detail::writeBytes(folder / "take.json", text(describe(result, context)));
    ok = Detail::writeBytes(folder / "report.html", text(result.reportHtml)) && ok;
    if (!result.audio.empty())
    {
        const auto wav = wavBytes(result.audio, result.sampleRate);
        ok = Detail::writeBytes(folder / "take.wav",
                                std::span<const char>{reinterpret_cast<const char*>(wav.data()), wav.size()}) &&
             ok;
    }
    return ok ? std::optional{folder} : std::nullopt;
}

}
