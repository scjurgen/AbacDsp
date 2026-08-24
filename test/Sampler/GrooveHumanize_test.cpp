#include <cstdint>
#include <gtest/gtest.h>
#include <vector>

#include "Sampler/GrooveHumanize.h"
#include "Sampler/GrooveMidiFile.h"

namespace AbacDsp::Test
{

TEST(GrooveHumanize, VoiceClassificationCoversMainGrooveVoices)
{
    EXPECT_EQ(voiceForGrooveNote(36), GrooveVoice::Kick);
    EXPECT_EQ(voiceForGrooveNote(38), GrooveVoice::Snare);
    EXPECT_EQ(voiceForGrooveNote(42), GrooveVoice::Hihat);
    EXPECT_EQ(voiceForGrooveNote(27), GrooveVoice::Cymbal); // crash
    EXPECT_EQ(voiceForGrooveNote(45), GrooveVoice::Tom);
    EXPECT_EQ(voiceForGrooveNote(56), GrooveVoice::Other);  // woodblock: no bucket
    EXPECT_EQ(voiceForGrooveNote(200), GrooveVoice::Other); // unknown note
}

TEST(GrooveHumanize, GhostDetectionFlagsWeakPositionAndLowVelocitySnareHits)
{
    const std::vector<GrooveNoteEvent> events{
        {480, 38, 100}, // backbeat position, loud: not a ghost
        {600, 38, 100}, // weak position: a ghost regardless of velocity
        {1440, 38, 20}, // backbeat position, quiet: a ghost by velocity
    };
    const auto notes = analyzeNotes(events, 480, {});
    ASSERT_EQ(notes.size(), 3u);
    EXPECT_FALSE(notes[0].isGhost);
    EXPECT_TRUE(notes[1].isGhost);
    EXPECT_TRUE(notes[2].isGhost);
    EXPECT_TRUE(notes[0].isBackbeat);
    EXPECT_FALSE(notes[1].isBackbeat);
    EXPECT_TRUE(notes[2].isBackbeat);
}

TEST(GrooveHumanize, PushZeroLifeOneReproducesSourceExactly)
{
    GrooveAnalysisNote note;
    note.sourceTick = 3613;
    note.note = 36;
    note.velocity = 77;
    note.voice = GrooveVoice::Kick;
    note.gridTick = 3600;

    const auto humanized = applyHumanize({note}, 480, GridResolution::Sixteenth, 0.f, 1.f);
    ASSERT_EQ(humanized.size(), 1u);
    EXPECT_EQ(humanized[0].tick, note.sourceTick);
    EXPECT_EQ(humanized[0].velocity, note.velocity);
}

TEST(GrooveHumanize, LifeZeroReproducesGridPlusPushBiasExactlyEvenWithNonzeroPush)
{
    GrooveAnalysisNote note;
    note.sourceTick = 3600; // irrelevant at life = 0
    note.note = 36;
    note.velocity = 5; // irrelevant at life = 0
    note.voice = GrooveVoice::Kick;
    note.gridTick = 3600;

    const auto humanized = applyHumanize({note}, 480, GridResolution::Sixteenth, -1.f, 0.f);
    ASSERT_EQ(humanized.size(), 1u);
    const float expectedBias = pushBias(GrooveVoice::Kick, false, false, GridStrength::Strong, -1.f, 120);
    const auto expectedTick = static_cast<uint32_t>(std::lround(3600.f + expectedBias));
    EXPECT_EQ(humanized[0].tick, expectedTick);
    EXPECT_EQ(humanized[0].velocity, roleFlatVelocity(GrooveVoice::Kick, false, false, GridStrength::Strong, -1.f));
}

namespace
{
// Two identical "normal" bars (kick on 0/8, snare backbeat on 4/12), a third
// bar of toms on cells never otherwise used (a fill), then a fourth bar
// whose first hit lands right after it.
[[nodiscard]] std::vector<GrooveNoteEvent> buildFillPatternEvents()
{
    std::vector<GrooveNoteEvent> events;
    for (const uint32_t barStart : {0u, 1920u})
    {
        events.push_back({barStart + 0, 36, 100});
        events.push_back({barStart + 480, 38, 100});
        events.push_back({barStart + 960, 36, 100});
        events.push_back({barStart + 1440, 38, 100});
    }
    for (const uint32_t step : {1u, 3u, 5u, 7u, 9u, 11u, 13u, 15u})
    {
        events.push_back({3840 + step * 120, 45, 100}); // tom fill
    }
    events.push_back({5760, 36, 100}); // landing bar's first hit
    events.push_back({6240, 38, 100});
    return events;
}
}

TEST(GrooveHumanize, FillBarInteriorIsUntouchedWhileLandingNoteIsFullyAffected)
{
    const auto notes = analyzeNotes(buildFillPatternEvents(), 480, {});
    ASSERT_EQ(notes.size(), 8u + 8u + 2u);

    for (size_t i = 0; i < 8; ++i)
    {
        EXPECT_FALSE(notes[i].isFillBar) << i;
    }
    for (size_t i = 8; i < 16; ++i)
    {
        EXPECT_TRUE(notes[i].isFillBar) << i;
        EXPECT_FALSE(notes[i].isFillLanding) << i;
    }
    EXPECT_TRUE(notes[16].isFillLanding); // tick 5760: first hit of the landing bar
    EXPECT_FALSE(notes[17].isFillLanding);

    const auto humanized = applyHumanize(notes, 480, GridResolution::Sixteenth, 1.f, 0.f);
    for (size_t i = 8; i < 16; ++i)
    {
        EXPECT_EQ(humanized[i].tick, notes[i].sourceTick) << i;
        EXPECT_EQ(humanized[i].velocity, notes[i].velocity) << i;
    }
    EXPECT_NE(humanized[16].tick, notes[16].sourceTick);
}

TEST(GrooveHumanize, ToCsvHasHeaderAndOneRowPerNote)
{
    const auto notes = analyzeNotes({{0, 36, 100}, {480, 38, 90}}, 480, {});
    const auto humanized = applyHumanize(notes, 480, GridResolution::Sixteenth, 0.f, 1.f);
    const auto csv = toCsv(notes, humanized);

    const auto firstNewline = csv.find('\n');
    ASSERT_NE(firstNewline, std::string::npos);
    EXPECT_EQ(csv.substr(0, firstNewline),
              "voice,pattern_position,strength,backbeat,ghost,fill_bar,fill_landing,source_tick,output_tick,"
              "source_velocity,output_velocity");
    EXPECT_EQ(std::ranges::count(csv, '\n'), 3); // header + 2 rows
}

}
