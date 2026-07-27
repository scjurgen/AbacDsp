#include <filesystem>
#include <string>

#include "gtest/gtest.h"

#include "Sampler/MidiFile.h"

namespace AbacDsp::Test
{

namespace
{
class TempMidiFile
{
  public:
    TempMidiFile()
        : m_path(std::filesystem::temp_directory_path() / "abacdsp_midifile_test.mid")
    {
    }

    ~TempMidiFile()
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

TEST(MidiFile, WrittenBufferStartsWithValidHeaderChunks)
{
    MidiFile midi;
    midi.setTempoBpm(120.f);
    midi.addTimeSignature(0, 4, 2);
    const auto bytes = midi.writeToBuffer();

    ASSERT_GE(bytes.size(), 14u + 8u);
    EXPECT_EQ(bytes[0], 'M');
    EXPECT_EQ(bytes[1], 'T');
    EXPECT_EQ(bytes[2], 'h');
    EXPECT_EQ(bytes[3], 'd');
    EXPECT_EQ(bytes[14], 'M');
    EXPECT_EQ(bytes[15], 'T');
    EXPECT_EQ(bytes[16], 'r');
    EXPECT_EQ(bytes[17], 'k');
}

TEST(MidiFile, RoundTripsThroughBufferPreservingTempoAndTimeSignatures)
{
    MidiFile writer;
    writer.setTempoBpm(140.f);
    writer.addTimeSignature(0, 4, 2);    // 4/4 from bar 0
    writer.addTimeSignature(1920, 7, 3); // 7/8 from tick 1920 (4 bars of 4/4 at 480 ticks/quarter)
    const auto bytes = writer.writeToBuffer();

    MidiFile reader;
    ASSERT_TRUE(reader.readFromBuffer(bytes));
    EXPECT_NEAR(reader.tempoBpm(), 140.f, 0.1f);
    ASSERT_EQ(reader.timeSignatures().size(), 2u);
    EXPECT_EQ(reader.timeSignatures()[0].tick, 0u);
    EXPECT_EQ(reader.timeSignatures()[0].numerator, 4u);
    EXPECT_EQ(reader.timeSignatures()[0].denominatorPower, 2u);
    EXPECT_EQ(reader.timeSignatures()[1].tick, 1920u);
    EXPECT_EQ(reader.timeSignatures()[1].numerator, 7u);
    EXPECT_EQ(reader.timeSignatures()[1].denominatorPower, 3u);
}

TEST(MidiFile, RoundTripsThroughFile)
{
    const TempMidiFile temp;
    MidiFile writer;
    writer.setTempoBpm(90.f);
    writer.addTimeSignature(0, 3, 2);
    ASSERT_TRUE(writer.writeToFile(temp.path()));

    MidiFile reader;
    ASSERT_TRUE(reader.readFromFile(temp.path()));
    EXPECT_NEAR(reader.tempoBpm(), 90.f, 0.1f);
    ASSERT_EQ(reader.timeSignatures().size(), 1u);
    EXPECT_EQ(reader.timeSignatures()[0].numerator, 3u);
}

TEST(MidiFile, ClearResetsToDefaults)
{
    MidiFile midi;
    midi.setTempoBpm(200.f);
    midi.addTimeSignature(0, 6, 3);
    midi.clear();

    EXPECT_FLOAT_EQ(midi.tempoBpm(), 120.f);
    EXPECT_TRUE(midi.timeSignatures().empty());
}

TEST(MidiFile, ReadFromEmptyBufferFails)
{
    MidiFile midi;
    EXPECT_FALSE(midi.readFromBuffer({}));
}

TEST(MidiFile, ReadFromGarbageBufferFails)
{
    MidiFile midi;
    const std::vector<uint8_t> garbage{1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    EXPECT_FALSE(midi.readFromBuffer(garbage));
}

TEST(MidiFile, ReadFromNonexistentFileFails)
{
    MidiFile midi;
    EXPECT_FALSE(midi.readFromFile("/nonexistent/path/abacdsp_no_such_file.mid"));
}

TEST(MidiFile, HandlesManyTimeSignatureChangesInOrder)
{
    MidiFile writer;
    writer.setTempoBpm(100.f);
    uint32_t tick = 0;
    for (uint8_t numerator = 2; numerator <= 9; ++numerator)
    {
        writer.addTimeSignature(tick, numerator, 2);
        tick += 960;
    }
    const auto bytes = writer.writeToBuffer();

    MidiFile reader;
    ASSERT_TRUE(reader.readFromBuffer(bytes));
    ASSERT_EQ(reader.timeSignatures().size(), 8u);
    for (size_t i = 0; i < reader.timeSignatures().size(); ++i)
    {
        EXPECT_EQ(reader.timeSignatures()[i].tick, static_cast<uint32_t>(i) * 960u);
        EXPECT_EQ(reader.timeSignatures()[i].numerator, static_cast<uint8_t>(i + 2));
    }
}

}
