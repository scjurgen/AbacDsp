#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "gtest/gtest.h"

#include "Sampler/GrooveMidiFile.h"

namespace AbacDsp::Test
{

namespace
{
class TempMidiFile
{
  public:
    TempMidiFile()
        : m_path(std::filesystem::temp_directory_path() / "abacdsp_groovemidifile_test.mid")
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

void appendVlq(std::vector<uint8_t>& buf, uint32_t value)
{
    std::array<uint8_t, 5> stack{};
    size_t count = 0;
    stack[count++] = static_cast<uint8_t>(value & 0x7F);
    value >>= 7;
    while (value > 0)
    {
        stack[count++] = static_cast<uint8_t>(value & 0x7F);
        value >>= 7;
    }
    for (size_t i = count; i-- > 0;)
    {
        buf.push_back(static_cast<uint8_t>(stack[i] | (i != 0 ? 0x80 : 0x00)));
    }
}

void appendU16BE(std::vector<uint8_t>& buf, const uint16_t value)
{
    buf.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
    buf.push_back(static_cast<uint8_t>(value & 0xFF));
}

void appendU32BE(std::vector<uint8_t>& buf, const uint32_t value)
{
    buf.push_back(static_cast<uint8_t>((value >> 24) & 0xFF));
    buf.push_back(static_cast<uint8_t>((value >> 16) & 0xFF));
    buf.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
    buf.push_back(static_cast<uint8_t>(value & 0xFF));
}

// Wraps one track's already-encoded event bytes in an "MTrk" chunk header.
void appendTrackChunk(std::vector<uint8_t>& buf, const std::vector<uint8_t>& body)
{
    buf.push_back('M');
    buf.push_back('T');
    buf.push_back('r');
    buf.push_back('k');
    appendU32BE(buf, static_cast<uint32_t>(body.size()));
    buf.insert(buf.end(), body.begin(), body.end());
}

[[nodiscard]] std::vector<uint8_t> buildHeader(const uint16_t format, const uint16_t trackCount,
                                               const uint16_t division)
{
    std::vector<uint8_t> buf;
    buf.push_back('M');
    buf.push_back('T');
    buf.push_back('h');
    buf.push_back('d');
    appendU32BE(buf, 6);
    appendU16BE(buf, format);
    appendU16BE(buf, trackCount);
    appendU16BE(buf, division);
    return buf;
}

void appendNoteOn(std::vector<uint8_t>& body, const uint32_t delta, const uint8_t note, const uint8_t velocity,
                  const bool withStatusByte = true)
{
    appendVlq(body, delta);
    if (withStatusByte)
    {
        body.push_back(0x90);
    }
    body.push_back(note);
    body.push_back(velocity);
}

void appendEndOfTrack(std::vector<uint8_t>& body, const uint32_t delta = 0)
{
    appendVlq(body, delta);
    body.push_back(0xFF);
    body.push_back(0x2F);
    body.push_back(0x00);
}

// A tempo meta event (0xFF 0x51 0x03 <3 bytes>), used to check meta events
// don't corrupt running status for the channel events around them.
void appendTempoMeta(std::vector<uint8_t>& body, const uint32_t delta = 0)
{
    appendVlq(body, delta);
    body.push_back(0xFF);
    body.push_back(0x51);
    body.push_back(0x03);
    body.push_back(0x07);
    body.push_back(0xA1);
    body.push_back(0x20);
}

void appendTimeSignatureMeta(std::vector<uint8_t>& body, const uint32_t delta, const uint8_t numerator,
                             const uint8_t denominatorPower)
{
    appendVlq(body, delta);
    body.push_back(0xFF);
    body.push_back(0x58);
    body.push_back(0x04);
    body.push_back(numerator);
    body.push_back(denominatorPower);
    body.push_back(24);
    body.push_back(8);
}
}

TEST(GrooveMidiFile, ReadsSingleTrackNoteOnEvents)
{
    std::vector<uint8_t> body;
    appendNoteOn(body, 0, 36, 100);
    appendNoteOn(body, 240, 38, 90);
    appendEndOfTrack(body);

    std::vector<uint8_t> bytes = buildHeader(0, 1, 480);
    appendTrackChunk(bytes, body);

    GrooveMidiFile midi;
    ASSERT_TRUE(midi.readFromBuffer(bytes));
    EXPECT_EQ(midi.ticksPerQuarterNote(), 480u);
    ASSERT_EQ(midi.noteEvents().size(), 2u);
    EXPECT_EQ(midi.noteEvents()[0].tick, 0u);
    EXPECT_EQ(midi.noteEvents()[0].note, 36u);
    EXPECT_EQ(midi.noteEvents()[0].velocity, 100u);
    EXPECT_EQ(midi.noteEvents()[1].tick, 240u);
    EXPECT_EQ(midi.noteEvents()[1].note, 38u);
    EXPECT_EQ(midi.noteEvents()[1].velocity, 90u);
}

TEST(GrooveMidiFile, RunningStatusOmitsRepeatedStatusByte)
{
    std::vector<uint8_t> body;
    appendNoteOn(body, 0, 36, 100);
    appendNoteOn(body, 120, 38, 80, false); // no status byte: relies on running status
    appendEndOfTrack(body);

    std::vector<uint8_t> bytes = buildHeader(0, 1, 480);
    appendTrackChunk(bytes, body);

    GrooveMidiFile midi;
    ASSERT_TRUE(midi.readFromBuffer(bytes));
    ASSERT_EQ(midi.noteEvents().size(), 2u);
    EXPECT_EQ(midi.noteEvents()[1].note, 38u);
    EXPECT_EQ(midi.noteEvents()[1].velocity, 80u);
}

TEST(GrooveMidiFile, MetaEventDoesNotDisturbSurroundingRunningStatus)
{
    std::vector<uint8_t> body;
    appendNoteOn(body, 0, 36, 100);
    appendTempoMeta(body, 10);
    appendNoteOn(body, 110, 38, 80, false); // running status must still be 0x90 after the meta event
    appendEndOfTrack(body);

    std::vector<uint8_t> bytes = buildHeader(0, 1, 480);
    appendTrackChunk(bytes, body);

    GrooveMidiFile midi;
    ASSERT_TRUE(midi.readFromBuffer(bytes));
    ASSERT_EQ(midi.noteEvents().size(), 2u);
    EXPECT_EQ(midi.noteEvents()[1].note, 38u);
    EXPECT_EQ(midi.noteEvents()[1].tick, 120u);
}

TEST(GrooveMidiFile, ZeroVelocityNoteOnIsTreatedAsNoteOffAndDropped)
{
    std::vector<uint8_t> body;
    appendNoteOn(body, 0, 36, 100);
    appendNoteOn(body, 100, 36, 0); // note-off in disguise
    appendEndOfTrack(body);

    std::vector<uint8_t> bytes = buildHeader(0, 1, 480);
    appendTrackChunk(bytes, body);

    GrooveMidiFile midi;
    ASSERT_TRUE(midi.readFromBuffer(bytes));
    ASSERT_EQ(midi.noteEvents().size(), 1u);
    EXPECT_EQ(midi.noteEvents()[0].note, 36u);
}

TEST(GrooveMidiFile, ExplicitNoteOffEventIsDropped)
{
    std::vector<uint8_t> body;
    appendVlq(body, 0);
    body.push_back(0x90);
    body.push_back(36);
    body.push_back(100);
    appendVlq(body, 100);
    body.push_back(0x80); // explicit Note Off
    body.push_back(36);
    body.push_back(64);
    appendEndOfTrack(body);

    std::vector<uint8_t> bytes = buildHeader(0, 1, 480);
    appendTrackChunk(bytes, body);

    GrooveMidiFile midi;
    ASSERT_TRUE(midi.readFromBuffer(bytes));
    ASSERT_EQ(midi.noteEvents().size(), 1u);
}

TEST(GrooveMidiFile, MergesAndSortsEventsAcrossMultipleTracks)
{
    std::vector<uint8_t> trackA;
    appendNoteOn(trackA, 0, 36, 100);
    appendNoteOn(trackA, 480, 36, 100);
    appendEndOfTrack(trackA);

    std::vector<uint8_t> trackB;
    appendNoteOn(trackB, 240, 42, 90);
    appendEndOfTrack(trackB);

    std::vector<uint8_t> bytes = buildHeader(1, 2, 480);
    appendTrackChunk(bytes, trackA);
    appendTrackChunk(bytes, trackB);

    GrooveMidiFile midi;
    ASSERT_TRUE(midi.readFromBuffer(bytes));
    ASSERT_EQ(midi.noteEvents().size(), 3u);
    EXPECT_EQ(midi.noteEvents()[0].tick, 0u);
    EXPECT_EQ(midi.noteEvents()[1].tick, 240u);
    EXPECT_EQ(midi.noteEvents()[1].note, 42u);
    EXPECT_EQ(midi.noteEvents()[2].tick, 480u);
}

TEST(GrooveMidiFile, KeepsEarlierTracksWhenALaterTrackIsTruncated)
{
    std::vector<uint8_t> trackA;
    appendNoteOn(trackA, 0, 36, 100);
    appendEndOfTrack(trackA);

    std::vector<uint8_t> bytes = buildHeader(1, 2, 480);
    appendTrackChunk(bytes, trackA);
    // Second track: claims a length far larger than the bytes actually present.
    bytes.push_back('M');
    bytes.push_back('T');
    bytes.push_back('r');
    bytes.push_back('k');
    appendU32BE(bytes, 1000);
    bytes.push_back(0x00); // a few real bytes, nowhere near the claimed length

    GrooveMidiFile midi;
    EXPECT_TRUE(midi.readFromBuffer(bytes));
    ASSERT_EQ(midi.noteEvents().size(), 1u);
    EXPECT_EQ(midi.noteEvents()[0].note, 36u);
}

TEST(GrooveMidiFile, RejectsSmpteDivision)
{
    std::vector<uint8_t> bytes = buildHeader(0, 1, 0x8000 | 0x1E28); // top bit set: SMPTE frames
    std::vector<uint8_t> body;
    appendEndOfTrack(body);
    appendTrackChunk(bytes, body);

    GrooveMidiFile midi;
    EXPECT_FALSE(midi.readFromBuffer(bytes));
}

TEST(GrooveMidiFile, ReadFromEmptyBufferFails)
{
    GrooveMidiFile midi;
    EXPECT_FALSE(midi.readFromBuffer({}));
}

TEST(GrooveMidiFile, ReadFromGarbageBufferFails)
{
    GrooveMidiFile midi;
    const std::vector<uint8_t> garbage{1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    EXPECT_FALSE(midi.readFromBuffer(garbage));
}

TEST(GrooveMidiFile, ReadFromNonexistentFileFails)
{
    GrooveMidiFile midi;
    EXPECT_FALSE(midi.readFromFile("/nonexistent/path/abacdsp_no_such_file.mid"));
}

TEST(GrooveMidiFile, RoundTripsThroughFile)
{
    const TempMidiFile temp;
    std::vector<uint8_t> body;
    appendNoteOn(body, 0, 36, 100);
    appendEndOfTrack(body);
    std::vector<uint8_t> bytes = buildHeader(0, 1, 240);
    appendTrackChunk(bytes, body);

    std::ofstream out(temp.path(), std::ios::binary);
    out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    out.close();

    GrooveMidiFile midi;
    ASSERT_TRUE(midi.readFromFile(temp.path()));
    EXPECT_EQ(midi.ticksPerQuarterNote(), 240u);
    ASSERT_EQ(midi.noteEvents().size(), 1u);
}

TEST(GrooveMidiFile, ReadsTempoMetaEvent)
{
    std::vector<uint8_t> body;
    appendTempoMeta(body, 0); // 0x07A120 microseconds/quarter = 120 BPM
    appendEndOfTrack(body);

    std::vector<uint8_t> bytes = buildHeader(0, 1, 480);
    appendTrackChunk(bytes, body);

    GrooveMidiFile midi;
    ASSERT_TRUE(midi.readFromBuffer(bytes));
    ASSERT_EQ(midi.tempoEvents().size(), 1u);
    EXPECT_EQ(midi.tempoEvents()[0].tick, 0u);
    EXPECT_EQ(midi.tempoEvents()[0].microsecondsPerQuarterNote, 500000u);
}

TEST(GrooveMidiFile, ReadsTimeSignatureMetaEvent)
{
    std::vector<uint8_t> body;
    appendTimeSignatureMeta(body, 0, 3, 2); // 3/4
    appendEndOfTrack(body);

    std::vector<uint8_t> bytes = buildHeader(0, 1, 480);
    appendTrackChunk(bytes, body);

    GrooveMidiFile midi;
    ASSERT_TRUE(midi.readFromBuffer(bytes));
    ASSERT_EQ(midi.timeSignatures().size(), 1u);
    EXPECT_EQ(midi.timeSignatures()[0].tick, 0u);
    EXPECT_EQ(midi.timeSignatures()[0].numerator, 3u);
    EXPECT_EQ(midi.timeSignatures()[0].denominatorPower, 2u);
}

TEST(GrooveMidiFile, EndOfTrackTickReflectsDeclaredLengthNotLastNote)
{
    std::vector<uint8_t> body;
    appendNoteOn(body, 0, 36, 100); // last note at tick 0
    appendEndOfTrack(body, 1920);   // EOT a full bar later - trailing silence

    std::vector<uint8_t> bytes = buildHeader(0, 1, 480);
    appendTrackChunk(bytes, body);

    GrooveMidiFile midi;
    ASSERT_TRUE(midi.readFromBuffer(bytes));
    EXPECT_EQ(midi.endOfTrackTick(), 1920u);
}

TEST(GrooveMidiFile, EndOfTrackTickIsTheLatestAcrossMultipleTracks)
{
    std::vector<uint8_t> trackA;
    appendNoteOn(trackA, 0, 36, 100);
    appendEndOfTrack(trackA, 480);

    std::vector<uint8_t> trackB;
    appendNoteOn(trackB, 0, 42, 90);
    appendEndOfTrack(trackB, 960);

    std::vector<uint8_t> bytes = buildHeader(1, 2, 480);
    appendTrackChunk(bytes, trackA);
    appendTrackChunk(bytes, trackB);

    GrooveMidiFile midi;
    ASSERT_TRUE(midi.readFromBuffer(bytes));
    EXPECT_EQ(midi.endOfTrackTick(), 960u);
}

TEST(GrooveMidiFile, TempoAndTimeSignatureFromAConductorTrackMergeWithNoteTracks)
{
    std::vector<uint8_t> conductorTrack;
    appendTempoMeta(conductorTrack, 0);
    appendTimeSignatureMeta(conductorTrack, 0, 4, 2);
    appendTimeSignatureMeta(conductorTrack, 1920, 3, 2);
    appendEndOfTrack(conductorTrack);

    std::vector<uint8_t> noteTrack;
    appendNoteOn(noteTrack, 0, 36, 100);
    appendEndOfTrack(noteTrack);

    std::vector<uint8_t> bytes = buildHeader(1, 2, 480);
    appendTrackChunk(bytes, conductorTrack);
    appendTrackChunk(bytes, noteTrack);

    GrooveMidiFile midi;
    ASSERT_TRUE(midi.readFromBuffer(bytes));
    ASSERT_EQ(midi.tempoEvents().size(), 1u);
    ASSERT_EQ(midi.timeSignatures().size(), 2u);
    EXPECT_EQ(midi.timeSignatures()[1].tick, 1920u);
    EXPECT_EQ(midi.timeSignatures()[1].numerator, 3u);
    ASSERT_EQ(midi.noteEvents().size(), 1u);
}

TEST(GrooveMidiFile, ClearResetsToDefaults)
{
    std::vector<uint8_t> body;
    appendNoteOn(body, 0, 36, 100);
    appendEndOfTrack(body, 240);
    std::vector<uint8_t> bytes = buildHeader(0, 1, 96);
    appendTrackChunk(bytes, body);

    GrooveMidiFile midi;
    ASSERT_TRUE(midi.readFromBuffer(bytes));
    midi.clear();
    EXPECT_TRUE(midi.noteEvents().empty());
    EXPECT_EQ(midi.ticksPerQuarterNote(), 480u);
    EXPECT_EQ(midi.endOfTrackTick(), 0u);
}

}
