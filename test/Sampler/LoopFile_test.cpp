#include <filesystem>
#include <string>

#include "gtest/gtest.h"

#include "Sampler/LoopFile.h"

namespace AbacDsp::Test
{

namespace
{
// Minimal JsonLike stand-in: exercises the concept/template contract without
// pulling a real JSON library into the core test build.
class FakeJson
{
  public:
    FakeJson() = default;

    explicit FakeJson(const LoopMetadata& meta)
        : m_text("v=" + std::to_string(meta.version) + ";bpm=" + std::to_string(meta.bpm) +
                 ";bars=" + std::to_string(meta.bars) + ";beats=" + std::to_string(meta.beats))
    {
    }

    [[nodiscard]] static FakeJson parse(const std::string& text)
    {
        FakeJson j;
        j.m_text = text;
        return j;
    }

    [[nodiscard]] std::string dump() const
    {
        return m_text;
    }

    template <typename T>
    [[nodiscard]] T get() const
    {
        T meta{};
        const auto bpmPos = m_text.find("bpm=");
        const auto barsPos = m_text.find("bars=");
        const auto beatsPos = m_text.find("beats=");
        if (m_text.rfind("v=", 0) != 0 || bpmPos == std::string::npos || barsPos == std::string::npos ||
            beatsPos == std::string::npos)
        {
            throw std::runtime_error("FakeJson: malformed text");
        }
        meta.version = std::stoi(m_text.substr(2, bpmPos - 3));
        meta.bpm = std::stof(m_text.substr(bpmPos + 4, barsPos - (bpmPos + 4) - 1));
        meta.bars = std::stof(m_text.substr(barsPos + 5, beatsPos - (barsPos + 5) - 1));
        meta.beats = std::stof(m_text.substr(beatsPos + 6));
        return meta;
    }

  private:
    std::string m_text;
};

class TempWavFile
{
  public:
    TempWavFile()
        : m_path(std::filesystem::temp_directory_path() / "abacdsp_loopfile_test.wav")
    {
    }

    ~TempWavFile()
    {
        std::filesystem::remove(m_path);
    }

    [[nodiscard]] std::string path() const
    {
        return m_path.string();
    }

  private:
    std::filesystem::path m_path;
};
}

TEST(LoopFile, MetadataRoundTripsThroughFakeJson)
{
    const LoopMetadata meta{2, 128.5f, 4.f, 16.f};
    const auto text = LoopFile<FakeJson>::serializeMetadata(meta);
    const auto parsed = LoopFile<FakeJson>::deserializeMetadata(text);
    ASSERT_TRUE(parsed.has_value());
    EXPECT_EQ(parsed->version, 2);
    EXPECT_FLOAT_EQ(parsed->bpm, 128.5f);
    EXPECT_FLOAT_EQ(parsed->bars, 4.f);
    EXPECT_FLOAT_EQ(parsed->beats, 16.f);
}

TEST(LoopFile, DeserializeMalformedTextReturnsNullopt)
{
    const auto parsed = LoopFile<FakeJson>::deserializeMetadata("not metadata");
    EXPECT_FALSE(parsed.has_value());
}

TEST(LoopFile, SaveThenLoadRoundTripsAudioAndEmbeddedMetadata)
{
    const TempWavFile wav;
    const std::vector<float> left{0.1f, 0.2f, -0.3f, 0.4f};
    const std::vector<float> right{-0.1f, -0.2f, 0.3f, -0.4f};
    const LoopMetadata meta{1, 140.f, 2.f, 8.f};

    LoopFile<FakeJson>::saveStereoWav(wav.path(), left, right, 48000.f, meta);
    const auto result = LoopFile<FakeJson>::loadStereoWav(wav.path());

    ASSERT_EQ(result.left.size(), left.size());
    ASSERT_EQ(result.right.size(), right.size());
    for (size_t i = 0; i < left.size(); ++i)
    {
        EXPECT_NEAR(result.left[i], left[i], 1e-3f);
        EXPECT_NEAR(result.right[i], right[i], 1e-3f);
    }
    EXPECT_FLOAT_EQ(result.sampleRate, 48000.f);
    ASSERT_TRUE(result.embeddedMetadata.has_value());
    EXPECT_EQ(result.embeddedMetadata->version, 1);
    EXPECT_FLOAT_EQ(result.embeddedMetadata->bpm, 140.f);
    EXPECT_FLOAT_EQ(result.embeddedMetadata->bars, 2.f);
    EXPECT_FLOAT_EQ(result.embeddedMetadata->beats, 8.f);
}

TEST(LoopFile, LoadMissingFileReturnsEmptyResult)
{
    const auto result = LoopFile<FakeJson>::loadStereoWav("/nonexistent/path/abacdsp_missing.wav");
    EXPECT_TRUE(result.left.empty());
    EXPECT_TRUE(result.right.empty());
}

}
