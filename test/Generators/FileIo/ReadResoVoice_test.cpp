#include <filesystem>
#include <fstream>

#include "gtest/gtest.h"

#include "Generators/FileIo/ReadResoVoice.h"

namespace AbacDsp::Test
{

namespace
{
class TempCsvFile
{
  public:
    explicit TempCsvFile(const std::string_view content)
        : m_path(std::filesystem::temp_directory_path() / "abacdsp_read_reso_voice_test.csv")
    {
        std::ofstream file(m_path);
        file << content;
    }

    ~TempCsvFile()
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

TEST(ReadResoVoice, parsesValidRowsAndSkipsCommentsAndBlankLines)
{
    const TempCsvFile csv("# ratio decayMs impulseFeedLevel sustainFeedFactor pan waitMs\n"
                          "1.0 250.5 0.8 0.5 -0.2 10.0\n"
                          "\n"
                          "2.0 500.0 0.6 0.4 0.3 0.0\n");

    std::vector<CsvVoice> settings;
    ASSERT_TRUE(readVoiceSettings(csv.path(), settings));

    ASSERT_EQ(settings.size(), 2u);
    EXPECT_FLOAT_EQ(settings[0].ratio, 1.0f);
    EXPECT_FLOAT_EQ(settings[0].decayMs, 250.5f);
    EXPECT_FLOAT_EQ(settings[0].impulseFeedLevel, 0.8f);
    EXPECT_FLOAT_EQ(settings[0].sustainFeedFactor, 0.5f);
    EXPECT_FLOAT_EQ(settings[0].pan, -0.2f);
    EXPECT_FLOAT_EQ(settings[0].waitMs, 10.0f);
    EXPECT_FLOAT_EQ(settings[1].ratio, 2.0f);
}

TEST(ReadResoVoice, skipsMalformedLines)
{
    const TempCsvFile csv("1.0 250.5 0.8 0.5 -0.2 10.0\n"
                          "not a valid row\n"
                          "2.0 500.0 0.6 0.4 0.3 0.0\n");

    std::vector<CsvVoice> settings;
    ASSERT_TRUE(readVoiceSettings(csv.path(), settings));
    EXPECT_EQ(settings.size(), 2u);
}

TEST(ReadResoVoice, returnsFalseForMissingFile)
{
    std::vector<CsvVoice> settings;
    EXPECT_FALSE(readVoiceSettings("/nonexistent/path/abacdsp_missing.csv", settings));
}

}
