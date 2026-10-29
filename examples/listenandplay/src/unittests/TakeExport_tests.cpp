#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <nlohmann/json.hpp>
#include <sstream>
#include <string>
#include <vector>

#include "TestTakes.h"
#include "impl/TakeExport.h"

namespace
{
constexpr float kSampleRate{48000.f};

uint32_t read32(const std::vector<uint8_t>& bytes, const size_t at)
{
    return static_cast<uint32_t>(bytes[at]) | (static_cast<uint32_t>(bytes[at + 1]) << 8) |
           (static_cast<uint32_t>(bytes[at + 2]) << 16) | (static_cast<uint32_t>(bytes[at + 3]) << 24);
}

std::string readFile(const std::filesystem::path& path)
{
    std::ifstream in(path, std::ios::binary);
    std::stringstream text;
    text << in.rdbuf();
    return text.str();
}

TakeAnalysis::TakeResult makeResult(const bool midi)
{
    const auto setup = TestTakes::makeSetup(TestTakes::makePattern());
    auto result = TakeAnalysis::scoreTake(setup, TestTakes::perfectTake(setup), "Scales", 7, "a06-major-scale");
    result.sampleRate = kSampleRate;
    result.midi = midi;
    if (!midi)
    {
        result.audio = {0.f, 0.5f, -0.25f, 1.f};
    }
    return result;
}

const TakeExport::Context kContext{"Guitar", -3.f};

struct ScratchFolder
{
    std::filesystem::path path{std::filesystem::current_path() / "takeexport-scratch"};

    ScratchFolder()
    {
        std::filesystem::remove_all(path);
    }

    ~ScratchFolder()
    {
        std::filesystem::remove_all(path);
    }
};
}

TEST(TakeExportTest, TheWavIsAMonoFloatFileThatHoldsTheSamplesExactly)
{
    const std::vector<float> samples{0.f, 0.5f, -0.25f, 1.f, 1e-7f};
    const auto bytes = TakeExport::wavBytes(samples, kSampleRate);
    ASSERT_EQ(bytes.size(), 12u + 26u + 12u + 8u + samples.size() * 4);
    EXPECT_EQ(std::string(bytes.begin(), bytes.begin() + 4), "RIFF");
    EXPECT_EQ(std::string(bytes.begin() + 8, bytes.begin() + 12), "WAVE");
    EXPECT_EQ(read32(bytes, 4), bytes.size() - 8);
    EXPECT_EQ(bytes[20], 3);
    EXPECT_EQ(bytes[22], 1);
    EXPECT_EQ(read32(bytes, 24), 48000u);
    const size_t data = bytes.size() - samples.size() * 4;
    EXPECT_EQ(std::string(bytes.begin() + static_cast<std::ptrdiff_t>(data) - 8,
                          bytes.begin() + static_cast<std::ptrdiff_t>(data) - 4),
              "data");
    EXPECT_EQ(read32(bytes, data - 4), samples.size() * 4);
    for (size_t i = 0; i < samples.size(); ++i)
    {
        float back{};
        const uint32_t word = read32(bytes, data + i * 4);
        std::memcpy(&back, &word, sizeof back);
        EXPECT_EQ(back, samples[i]);
    }
}

TEST(TakeExportTest, TheDescriptionNamesTheLessonTheSettingsTheExpectedAndTheDetectedNotes)
{
    const auto result = makeResult(false);
    const auto json = nlohmann::json::parse(TakeExport::describe(result, kContext));
    EXPECT_EQ(json["lesson"]["stem"], "a06-major-scale");
    EXPECT_EQ(json["lesson"]["name"], "Scales");
    EXPECT_EQ(json["take"]["instrument"], "Guitar");
    EXPECT_FLOAT_EQ(json["take"]["inputGainDb"].get<float>(), -3.f);
    EXPECT_FLOAT_EQ(json["take"]["downbeatSeconds"].get<float>(), 1.f);
    EXPECT_FLOAT_EQ(json["take"]["sampleRate"].get<float>(), kSampleRate);
    EXPECT_FALSE(json["take"]["midi"].get<bool>());
    ASSERT_EQ(json["phrase"]["notes"].size(), 8u);
    EXPECT_EQ(json["phrase"]["notes"][2]["pitch"], 64);
    EXPECT_EQ(json["detected"].size(), result.played.size());
    EXPECT_EQ(json["score"]["expected"], 8);
}

TEST(TakeExportTest, TheFolderIsNamedAfterTheTimeAndTheLesson)
{
    EXPECT_EQ(TakeExport::folderName("20261025-101500", "e01-cdef-c3"), "20261025-101500-e01-cdef-c3");
    EXPECT_EQ(TakeExport::folderName("20261025-101500", ""), "20261025-101500-script");
}

TEST(TakeExportTest, AnAudioTakeIsWrittenAsWavJsonAndReport)
{
    const ScratchFolder scratch;
    const auto result = makeResult(false);
    const auto folder = TakeExport::writeTake(scratch.path, "20261025-101500", result, kContext);
    ASSERT_TRUE(folder.has_value());
    EXPECT_EQ(folder->filename().string(), "20261025-101500-a06-major-scale");
    EXPECT_EQ(std::filesystem::file_size(*folder / "take.wav"), TakeExport::wavBytes(result.audio, kSampleRate).size());
    EXPECT_NO_THROW(static_cast<void>(nlohmann::json::parse(readFile(*folder / "take.json"))));
    EXPECT_EQ(readFile(*folder / "report.html"), result.reportHtml);
}

TEST(TakeExportTest, AMidiTakeHasNoWav)
{
    const ScratchFolder scratch;
    const auto folder = TakeExport::writeTake(scratch.path, "20261025-101500", makeResult(true), kContext);
    ASSERT_TRUE(folder.has_value());
    EXPECT_FALSE(std::filesystem::exists(*folder / "take.wav"));
    EXPECT_TRUE(std::filesystem::exists(*folder / "take.json"));
}
