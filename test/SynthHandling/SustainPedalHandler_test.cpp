#include <vector>

#include "gtest/gtest.h"

#include "SynthHandling/SustainPedalHandler.h"

namespace AbacDsp::Test
{

namespace
{
struct MidiEvent
{
    int channel;
    int note;
    int velocity;
};
}

TEST(SustainPedalHandler, noteOnAndOffWithoutSustainFireImmediately)
{
    SustainPedalHandler handler(4);
    std::vector<MidiEvent> onEvents;
    std::vector<MidiEvent> offEvents;
    handler.configureCallbacks([&](int ch, int note, int vel) { onEvents.push_back({ch, note, vel}); },
                               [&](int ch, int note, int vel) { offEvents.push_back({ch, note, vel}); });

    handler.noteOn(0, 60, 100);
    ASSERT_EQ(onEvents.size(), 1u);
    EXPECT_EQ(onEvents[0].note, 60);
    EXPECT_TRUE(offEvents.empty());

    handler.noteOff(0, 60, 0);
    ASSERT_EQ(offEvents.size(), 1u);
    EXPECT_EQ(offEvents[0].note, 60);
}

TEST(SustainPedalHandler, RetriggeringAnActiveNoteWithoutSustainClosesTheOldSlotFirst)
{
    SustainPedalHandler handler(4);
    std::vector<MidiEvent> onEvents;
    std::vector<MidiEvent> offEvents;
    handler.configureCallbacks([&](int ch, int note, int vel) { onEvents.push_back({ch, note, vel}); },
                               [&](int ch, int note, int vel) { offEvents.push_back({ch, note, vel}); });

    handler.noteOn(0, 60, 100);
    handler.noteOn(0, 60, 110); // retrigger before any note-off arrives
    ASSERT_EQ(onEvents.size(), 2u);
    ASSERT_EQ(offEvents.size(), 1u) << "the retrigger must close the first instance's slot itself";

    handler.noteOff(0, 60, 0);
    EXPECT_EQ(offEvents.size(), 2u) << "the single physical note-off must still reach the retriggered instance";
}

TEST(SustainPedalHandler, sustainHoldsNoteOffUntilPedalReleased)
{
    SustainPedalHandler handler(4);
    std::vector<MidiEvent> offEvents;
    handler.configureCallbacks([&](int, int, int) {},
                               [&](int ch, int note, int vel) { offEvents.push_back({ch, note, vel}); });

    handler.setSustain(true);
    handler.noteOn(0, 60, 100);
    handler.noteOff(0, 60, 0);
    EXPECT_TRUE(offEvents.empty()) << "note-off should be deferred while sustaining";

    handler.setSustain(false);
    ASSERT_EQ(offEvents.size(), 1u);
    EXPECT_EQ(offEvents[0].note, 60);
}

TEST(SustainPedalHandler, voiceStealingReplacesOldestNoteWhenFull)
{
    constexpr size_t maxVoices{2};
    SustainPedalHandler handler(maxVoices);
    std::vector<MidiEvent> onEvents;
    std::vector<MidiEvent> offEvents;
    handler.configureCallbacks([&](int ch, int note, int vel) { onEvents.push_back({ch, note, vel}); },
                               [&](int ch, int note, int vel) { offEvents.push_back({ch, note, vel}); });

    handler.noteOn(0, 60, 100);
    handler.noteOn(0, 61, 100);
    handler.noteOn(0, 62, 100); // queue full, must steal the oldest (60)

    ASSERT_EQ(onEvents.size(), 3u);
    ASSERT_EQ(offEvents.size(), 1u);
    EXPECT_EQ(offEvents[0].note, 60);
}

TEST(SustainPedalHandler, allNotesOffClearsEverythingAndResetsSustain)
{
    SustainPedalHandler handler(4);
    std::vector<MidiEvent> offEvents;
    handler.configureCallbacks([&](int, int, int) {},
                               [&](int ch, int note, int vel) { offEvents.push_back({ch, note, vel}); });

    handler.setSustain(true);
    handler.noteOn(0, 60, 100);
    handler.noteOn(0, 64, 100);

    handler.allNotesOff();
    ASSERT_EQ(offEvents.size(), 2u);

    // sustain should be reset, so a fresh note-off fires immediately, not deferred
    offEvents.clear();
    handler.noteOn(0, 67, 100);
    handler.noteOff(0, 67, 0);
    ASSERT_EQ(offEvents.size(), 1u);
    EXPECT_EQ(offEvents[0].note, 67);
}

}
