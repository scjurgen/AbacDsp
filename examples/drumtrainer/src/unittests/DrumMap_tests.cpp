#include <fstream>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <sstream>
#include <string>

#include "impl/DrumMap.h"

using namespace DrumInput;

namespace
{
uint8_t noteOf(const DrumMap& map, const Pad pad)
{
    return map.notes[static_cast<size_t>(pad)];
}

std::string loadErrorOf(const std::string& json)
{
    const auto load = parseDrumMap(json);
    EXPECT_FALSE(load.file.has_value()) << json;
    return load.error;
}
}

TEST(DrumMapNoteNameTest, NamesFollowTheMiddleCOctave)
{
    EXPECT_EQ(parseNoteName("C4", 4), 60);
    EXPECT_EQ(parseNoteName("C3", 3), 60);
    EXPECT_EQ(parseNoteName("C5", 5), 60);
    EXPECT_EQ(parseNoteName("Bb3", 4), 58);
    EXPECT_EQ(parseNoteName("D#4", 4), 63);
    EXPECT_EQ(parseNoteName("B3", 4), 59);
    EXPECT_EQ(parseNoteName("C-1", 4), 0);
    EXPECT_EQ(parseNoteName("G9", 4), 127);
}

TEST(DrumMapNoteNameTest, MalformedOrOutOfRangeNamesAreRejected)
{
    for (const auto* text : {"", "C", "C#", "4", "H4", "c4", "Cx4", "C4a", "C#-", "G#9", "Cb-1", "C100"})
    {
        EXPECT_FALSE(parseNoteName(text, 4).has_value()) << text;
    }
}

TEST(DrumMapNoteNameTest, NoteNameRoundTripsEveryNote)
{
    for (const int octave : {3, 4, 5})
    {
        for (int note = 0; note < 128; ++note)
        {
            const auto name = noteName(static_cast<uint8_t>(note), octave);
            EXPECT_EQ(parseNoteName(name, octave), note) << name;
        }
    }
}

TEST(DrumMapTest, DefaultMapMatchesTheShippedDefaultFile)
{
    std::ifstream file{std::string{DRUMTRAINER_DRUM_MAPS_DIR} + "/default.json"};
    ASSERT_TRUE(file.is_open());
    std::stringstream text;
    text << file.rdbuf();
    const auto load = parseDrumMap(text.str());
    ASSERT_TRUE(load.file.has_value()) << load.error;
    const DrumMap& shipped = load.file->map;
    const DrumMap builtIn = defaultDrumMapFile().map;
    EXPECT_EQ(shipped.inputChannel, builtIn.inputChannel);
    EXPECT_EQ(shipped.middleCOctave, builtIn.middleCOctave);
    EXPECT_EQ(shipped.notes, builtIn.notes);
    EXPECT_EQ(shipped.hihat.pedalCc, builtIn.hihat.pedalCc);
    EXPECT_EQ(shipped.hihat.halfOpenAbove, builtIn.hihat.halfOpenAbove);
    EXPECT_EQ(shipped.hihat.closedAbove, builtIn.hihat.closedAbove);
    EXPECT_FLOAT_EQ(shipped.hihat.ccGraceMs, builtIn.hihat.ccGraceMs);
    EXPECT_EQ(shipped.click.outputChannel, builtIn.click.outputChannel);
    EXPECT_EQ(shipped.click.downbeatNote, builtIn.click.downbeatNote);
    EXPECT_EQ(shipped.click.beatNote, builtIn.click.beatNote);
}

TEST(DrumMapTest, BuiltInDefaultIsTheReferenceKit)
{
    const DrumMap map = defaultDrumMapFile().map;
    EXPECT_EQ(noteOf(map, Pad::Snare), 60);
    EXPECT_EQ(noteOf(map, Pad::TomLow), 62);
    EXPECT_EQ(noteOf(map, Pad::TomHigh), 63);
    EXPECT_EQ(noteOf(map, Pad::Hihat), 64);
    EXPECT_EQ(noteOf(map, Pad::Crash), 70);
    EXPECT_EQ(noteOf(map, Pad::TomFloor), 72);
    EXPECT_EQ(noteOf(map, Pad::Kick), 74);
    EXPECT_EQ(noteOf(map, Pad::HihatPedal), 76);
    EXPECT_EQ(map.click.downbeatNote, 58);
    EXPECT_EQ(map.click.beatNote, 59);
}

TEST(DrumMapTest, NumbersAndNamesMayBeMixed)
{
    const auto load = parseDrumMap(R"({"name": "GM", "notes": {"kick": 36, "snare": "D2"}})");
    ASSERT_TRUE(load.file.has_value()) << load.error;
    EXPECT_EQ(load.file->name, "GM");
    EXPECT_EQ(noteOf(load.file->map, Pad::Kick), 36);
    EXPECT_EQ(noteOf(load.file->map, Pad::Snare), 38);
}

TEST(DrumMapTest, MiddleCOctaveAppliesToNotesAndClick)
{
    const auto load =
        parseDrumMap(R"({"middleCOctave": 3, "notes": {"snare": "C3"}, "click": {"downbeat": "C3", "beat": "D3"}})");
    ASSERT_TRUE(load.file.has_value()) << load.error;
    EXPECT_EQ(noteOf(load.file->map, Pad::Snare), 60);
    EXPECT_EQ(load.file->map.click.downbeatNote, 60);
    EXPECT_EQ(load.file->map.click.beatNote, 62);
}

TEST(DrumMapTest, PadsMissingFromNotesStayUnmapped)
{
    const auto load = parseDrumMap(R"({"notes": {"kick": 36}})");
    ASSERT_TRUE(load.file.has_value()) << load.error;
    const DrumMap& map = load.file->map;
    EXPECT_EQ(noteOf(map, Pad::Snare), kNoNote);
    EXPECT_EQ(map.padForNote(36), Pad::Kick);
    EXPECT_FALSE(map.padForNote(60).has_value());
    EXPECT_FALSE(map.padForNote(kNoNote).has_value());
}

TEST(DrumMapTest, OmittedSectionsKeepTheirDefaults)
{
    const auto load = parseDrumMap(R"({"notes": {"kick": 36}})");
    ASSERT_TRUE(load.file.has_value()) << load.error;
    EXPECT_EQ(load.file->name, "Unnamed drum map");
    EXPECT_EQ(load.file->map.inputChannel, kAnyChannel);
    EXPECT_EQ(load.file->map.hihat.closedAbove, 125);
    EXPECT_EQ(load.file->map.click.beatNote, 59);
}

TEST(DrumMapTest, AcceptsChannelHonoursAnyChannel)
{
    DrumMap map{};
    map.inputChannel = kAnyChannel;
    EXPECT_TRUE(map.acceptsChannel(1));
    EXPECT_TRUE(map.acceptsChannel(16));
    map.inputChannel = 10;
    EXPECT_TRUE(map.acceptsChannel(10));
    EXPECT_FALSE(map.acceptsChannel(9));
}

TEST(DrumMapTest, InvalidJsonIsReported)
{
    EXPECT_THAT(loadErrorOf("{"), ::testing::StartsWith("invalid JSON"));
}

TEST(DrumMapTest, UnknownKeysAreErrors)
{
    EXPECT_THAT(loadErrorOf(R"({"notes": {"tomhigh": 63}})"), ::testing::HasSubstr("unknown key \"tomhigh\" in notes"));
    EXPECT_THAT(loadErrorOf(R"({"notes": {}, "colour": 1})"), ::testing::HasSubstr("unknown key \"colour\""));
    EXPECT_THAT(loadErrorOf(R"({"notes": {}, "hihat": {"closed": 1}})"), ::testing::HasSubstr("in hihat"));
    EXPECT_THAT(loadErrorOf(R"({"notes": {}, "click": {"accent": 1}})"), ::testing::HasSubstr("in click"));
}

TEST(DrumMapTest, NotesSectionIsRequired)
{
    EXPECT_THAT(loadErrorOf(R"({"name": "x"})"), ::testing::HasSubstr("\"notes\""));
    EXPECT_THAT(loadErrorOf(R"([1, 2])"), ::testing::HasSubstr("must be an object"));
}

TEST(DrumMapTest, ANoteUsedByTwoPadsIsAnError)
{
    EXPECT_THAT(loadErrorOf(R"({"notes": {"kick": 36, "snare": "C2"}})"), ::testing::HasSubstr("already used by"));
}

TEST(DrumMapTest, OutOfRangeValuesAreErrors)
{
    EXPECT_THAT(loadErrorOf(R"({"notes": {"kick": 128}})"), ::testing::HasSubstr("notes.kick"));
    EXPECT_THAT(loadErrorOf(R"({"notes": {"kick": "X4"}})"), ::testing::HasSubstr("not a note name"));
    EXPECT_THAT(loadErrorOf(R"({"notes": {"kick": 36.5}})"), ::testing::HasSubstr("whole number"));
    EXPECT_THAT(loadErrorOf(R"({"inputChannel": 17, "notes": {}})"), ::testing::HasSubstr("inputChannel"));
    EXPECT_THAT(loadErrorOf(R"({"middleCOctave": 7, "notes": {}})"), ::testing::HasSubstr("middleCOctave"));
    EXPECT_THAT(loadErrorOf(R"({"notes": {}, "click": {"outputChannel": 0}})"),
                ::testing::HasSubstr("click.outputChannel"));
    EXPECT_THAT(loadErrorOf(R"({"notes": {}, "hihat": {"ccGraceMs": 21}})"), ::testing::HasSubstr("ccGraceMs"));
    EXPECT_THAT(loadErrorOf(R"({"notes": {}, "hihat": {"pedalCc": -1}})"), ::testing::HasSubstr("pedalCc"));
}

TEST(DrumMapTest, HalfOpenThresholdMustNotExceedClosedThreshold)
{
    EXPECT_THAT(loadErrorOf(R"({"notes": {}, "hihat": {"halfOpenAbove": 126, "closedAbove": 125}})"),
                ::testing::HasSubstr("halfOpenAbove"));
}

TEST(DrumMapTest, NameMustBeShortPrintableText)
{
    EXPECT_THAT(loadErrorOf(R"({"name": "", "notes": {}})"), ::testing::HasSubstr("name"));
    EXPECT_THAT(loadErrorOf(R"({"name": 5, "notes": {}})"), ::testing::HasSubstr("name must be a string"));
    EXPECT_THAT(loadErrorOf("{\"name\": \"a\\tb\", \"notes\": {}}"), ::testing::HasSubstr("control characters"));
}
