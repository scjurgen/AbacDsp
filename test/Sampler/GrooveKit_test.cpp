#include <array>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include "AudioFile/SaveWav.h"

#include "Sampler/GrooveKit.h"

namespace AbacDsp::test
{

// Loads a synthetic kit + groove file from a temp directory, exercising the
// note -> tag -> loaded-piece fallback end to end (not just via mocks), so the
// test stays independent of the real (gitignored, user-supplied) sample/MIDI content.
namespace
{
// Minimal JsonLike stand-in (see Sampler/LoopFile_test.cpp's own FakeJson): exercises
// the concept/template contract without pulling a real JSON library into the core
// test build. Its own text format is "<feel>|<timeSignature>|<sound1,sound2,...>",
// not real JSON - GrooveKit.h never inspects the text itself, only what Json::parse()
// and .get<GrooveSidecar>() hand back, so any format both sides agree on is valid.
// Own text format: "<feel>|<timeSignature>|<idealBpm>|<sound1,sound2,...>".
class FakeJson
{
  public:
    [[nodiscard]] static FakeJson parse(const std::string& text)
    {
        FakeJson j;
        j.m_text = text;
        return j;
    }

    template <typename T>
    [[nodiscard]] T get() const
    {
        const auto firstBar = m_text.find('|');
        const auto secondBar = firstBar == std::string::npos ? std::string::npos : m_text.find('|', firstBar + 1);
        const auto thirdBar = secondBar == std::string::npos ? std::string::npos : m_text.find('|', secondBar + 1);
        if (firstBar == std::string::npos || secondBar == std::string::npos || thirdBar == std::string::npos)
        {
            throw std::runtime_error("FakeJson: malformed groove sidecar");
        }
        T sidecar{};
        sidecar.rhythm.feel = m_text.substr(0, firstBar);
        sidecar.rhythm.timeSignature = m_text.substr(firstBar + 1, secondBar - firstBar - 1);
        const auto idealBpmText = m_text.substr(secondBar + 1, thirdBar - secondBar - 1);
        sidecar.idealBpm = idealBpmText.empty() ? 0.f : std::stof(idealBpmText);
        const auto sounds = m_text.substr(thirdBar + 1);
        size_t start = 0;
        while (!sounds.empty() && start <= sounds.size())
        {
            const auto comma = sounds.find(',', start);
            const auto token = sounds.substr(start, comma == std::string::npos ? std::string::npos : comma - start);
            if (!token.empty())
            {
                sidecar.dominantSounds.push_back(token);
            }
            if (comma == std::string::npos)
            {
                break;
            }
            start = comma + 1;
        }
        return sidecar;
    }

  private:
    std::string m_text;
};

using GrooveKit = AbacDsp::GrooveKit<FakeJson>;

class TempGrooveKitDir
{
  public:
    TempGrooveKitDir()
        : m_dir(std::filesystem::temp_directory_path() / "abacdsp_groovekit_test")
    {
        std::error_code ec;
        std::filesystem::remove_all(m_dir, ec);
        std::filesystem::create_directories(m_dir);
    }

    ~TempGrooveKitDir()
    {
        std::error_code ec;
        std::filesystem::remove_all(m_dir, ec);
    }

    [[nodiscard]] std::string dir() const
    {
        return m_dir.string();
    }

    [[nodiscard]] std::string filePath(const std::string& filename) const
    {
        return (m_dir / filename).string();
    }

  private:
    std::filesystem::path m_dir;
};

void appendGrooveVlq(std::vector<uint8_t>& buf, uint32_t value)
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

void appendGrooveU16BE(std::vector<uint8_t>& buf, const uint16_t value)
{
    buf.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
    buf.push_back(static_cast<uint8_t>(value & 0xFF));
}

void appendGrooveU32BE(std::vector<uint8_t>& buf, const uint32_t value)
{
    buf.push_back(static_cast<uint8_t>((value >> 24) & 0xFF));
    buf.push_back(static_cast<uint8_t>((value >> 16) & 0xFF));
    buf.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
    buf.push_back(static_cast<uint8_t>(value & 0xFF));
}

// One-track, format-0 SMF with a note-on (velocity 100) for every {tick, note}
// pair given, in ascending tick order.
[[nodiscard]] std::vector<uint8_t> buildGrooveMidiBytes(const std::vector<std::pair<uint32_t, uint8_t>>& notes,
                                                        const uint16_t division = 480)
{
    std::vector<uint8_t> body;
    uint32_t lastTick = 0;
    for (const auto& [tick, note] : notes)
    {
        appendGrooveVlq(body, tick - lastTick);
        body.push_back(0x90);
        body.push_back(note);
        body.push_back(100);
        lastTick = tick;
    }
    appendGrooveVlq(body, 0);
    body.push_back(0xFF);
    body.push_back(0x2F);
    body.push_back(0x00);

    std::vector<uint8_t> bytes{'M', 'T', 'h', 'd'};
    appendGrooveU32BE(bytes, 6);
    appendGrooveU16BE(bytes, 0);
    appendGrooveU16BE(bytes, 1);
    appendGrooveU16BE(bytes, division);
    bytes.push_back('M');
    bytes.push_back('T');
    bytes.push_back('r');
    bytes.push_back('k');
    appendGrooveU32BE(bytes, static_cast<uint32_t>(body.size()));
    bytes.insert(bytes.end(), body.begin(), body.end());
    return bytes;
}

void writeGrooveMidiFile(const std::string& path, const std::vector<std::pair<uint32_t, uint8_t>>& notes,
                         const uint16_t division = 480)
{
    const auto bytes = buildGrooveMidiBytes(notes, division);
    std::ofstream out(path, std::ios::binary);
    out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
}

void writeGrooveTake(const TempGrooveKitDir& dir, const std::string& code, const unsigned index)
{
    const std::vector<float> mono(32, 0.1f);
    AudioUtility::SaveWav::saveStereoAs(dir.filePath(code + "_" + std::to_string(index) + ".wav"), mono, mono);
}

[[nodiscard]] bool waitUntilGrooveKitReady(GrooveKit& kit)
{
    for (int i = 0; i < 2000; ++i)
    {
        kit.pollAndInstall();
        if (kit.isReady())
        {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    return false;
}
}

TEST(GrooveKitTest, ResolvesDirectAndFallbackMatchesAndSkipsUnmatchedNotes)
{
    const TempGrooveKitDir dir;
    writeGrooveTake(dir, "bd", 1); // kick: single take
    writeGrooveTake(dir, "hh", 1); // closed hihat: two round-robin takes
    writeGrooveTake(dir, "hh", 2); // (no "hhopen", no snare/crash/etc at all)

    writeGrooveMidiFile(dir.filePath("groove.mid"),
                        {
                            {0, 36},   // Kick: direct match ("bd")
                            {480, 21}, // HihatClosedPedal -> HihatClosed: direct match ("hh")
                            {960, 49}, // HihatOpen -> Hihat (umbrella fallback): matches "hh"
                            {1440, 38} // Snare: no snare piece loaded at all -> must be skipped
                        });

    GrooveKit kit;
    kit.requestLoad(dir.dir(), dir.dir(), "groove.mid");
    ASSERT_TRUE(waitUntilGrooveKitReady(kit));

    const auto* library = kit.library();
    const auto* program = kit.program();
    ASSERT_NE(library, nullptr);
    ASSERT_NE(program, nullptr);
    EXPECT_EQ(program->ticksPerQuarterNote, 480u);
    EXPECT_EQ(program->loopLengthTicks, 1920u); // last note at tick 1440 -> 4 beats
    ASSERT_EQ(program->triggers.size(), 3u) << "the unmatched snare note must not become a trigger";

    size_t kickTrack = 0;
    size_t hihatTrack = 0;
    bool foundKick = false;
    bool foundHihatDirect = false;
    bool foundHihatFallback = false;
    for (const auto& trigger : program->triggers)
    {
        const size_t sliceCount = library->sliceCountInTrack(trigger.track);
        if (sliceCount == 1)
        {
            kickTrack = trigger.track;
            foundKick = true;
        }
        else if (sliceCount == 2)
        {
            hihatTrack = trigger.track;
            if (foundHihatDirect)
            {
                foundHihatFallback = true;
            }
            foundHihatDirect = true;
        }
    }
    EXPECT_TRUE(foundKick);
    EXPECT_TRUE(foundHihatDirect);
    EXPECT_TRUE(foundHihatFallback);
    EXPECT_NE(kickTrack, hihatTrack);
}

TEST(GrooveKitTest, MissingSampleDirectoryStillBecomesReadyWithEmptyProgram)
{
    const TempGrooveKitDir dir; // never populated with any WAV or MIDI file

    GrooveKit kit;
    kit.requestLoad(dir.dir() + "/does_not_exist", dir.dir(), "missing.mid");
    ASSERT_TRUE(waitUntilGrooveKitReady(kit));

    ASSERT_NE(kit.library(), nullptr);
    ASSERT_NE(kit.program(), nullptr);
    EXPECT_EQ(kit.program()->triggers.size(), 0u);
}

TEST(GrooveKitTest, ListAvailableGroovesGroupsByStyleAndSortsVariations)
{
    const TempGrooveKitDir dir;
    const auto genreDir = dir.dir() + "/Session Drums";
    std::filesystem::create_directories(genreDir);
    for (const char* file : {"str_4#4_1_c_v1.mid", "str_4#4_1_c_v3.mid", "swg_4#4_2_c_v1.mid"})
    {
        std::ofstream(std::filesystem::path(genreDir) / file).put('\0'); // content irrelevant to listing
    }

    const auto names = GrooveKit::listAvailableGrooves(dir.dir());
    ASSERT_EQ(names.size(), 2u);
    EXPECT_EQ(names[0], "Session Drums/str_4#4_1_c");
    EXPECT_EQ(names[1], "Session Drums/swg_4#4_2_c");

    EXPECT_EQ(GrooveKit::countVariations(dir.dir(), "Session Drums/str_4#4_1_c"), 2u);
    EXPECT_EQ(GrooveKit::countVariations(dir.dir(), "Session Drums/swg_4#4_2_c"), 1u);
    EXPECT_EQ(GrooveKit::countVariations(dir.dir(), "Session Drums/does_not_exist"), 0u);

    EXPECT_EQ(GrooveKit::resolveGrooveName(dir.dir(), "Session Drums/str_4#4_1_c", 0),
              "Session Drums/str_4#4_1_c_v1.mid");
    EXPECT_EQ(GrooveKit::resolveGrooveName(dir.dir(), "Session Drums/str_4#4_1_c", 1),
              "Session Drums/str_4#4_1_c_v3.mid");
    EXPECT_FALSE(GrooveKit::resolveGrooveName(dir.dir(), "Session Drums/str_4#4_1_c", 2).has_value());
}

TEST(GrooveKitTest, RequestLoadStyleResolvesFilenameOnBackgroundThread)
{
    const TempGrooveKitDir dir;
    writeGrooveTake(dir, "bd", 1);
    const auto genreDir = dir.dir() + "/Session Drums";
    std::filesystem::create_directories(genreDir);
    writeGrooveMidiFile((std::filesystem::path(genreDir) / "str_4#4_1_c_v1.mid").string(), {{0, 36}});

    GrooveKit kit;
    kit.requestLoadStyle(dir.dir(), dir.dir(), "Session Drums/str_4#4_1_c", 0);
    ASSERT_TRUE(waitUntilGrooveKitReady(kit));

    EXPECT_EQ(kit.program()->triggers.size(), 1u);
    EXPECT_EQ(kit.currentGrooveName(), "Session Drums/str_4#4_1_c_v1.mid");
}

TEST(GrooveKitTest, MetadataParsesSidecarAndComputesBarsFromBeatCount)
{
    const TempGrooveKitDir dir;
    writeGrooveTake(dir, "bd", 1);
    writeGrooveMidiFile(dir.filePath("groove.mid"), {{0, 36}, {3360, 36}}); // tick 3360/480=7 -> 8 beats
    std::ofstream(dir.filePath("groove.json")) << "even|4/4|91|kick,hihat";

    GrooveKit kit;
    kit.requestLoad(dir.dir(), dir.dir(), "groove.mid");
    ASSERT_TRUE(waitUntilGrooveKitReady(kit));

    const auto& metadata = kit.installedMetadata();
    EXPECT_EQ(metadata.feel, "even");
    EXPECT_EQ(metadata.timeSignature, "4/4");
    EXPECT_FLOAT_EQ(metadata.idealBpm, 91.f);
    EXPECT_EQ(metadata.bars, 2u); // 8 beats / 4 per bar
    ASSERT_EQ(metadata.dominantSounds.size(), 2u);
    EXPECT_EQ(metadata.dominantSounds[0], "kick");
    EXPECT_EQ(metadata.dominantSounds[1], "hihat");
}

TEST(GrooveKitTest, BarCountUsesTimeSignatureNumeratorAsDivisor)
{
    const TempGrooveKitDir dir;
    writeGrooveTake(dir, "bd", 1);
    writeGrooveMidiFile(dir.filePath("groove.mid"), {{0, 36}, {2400, 36}}); // tick 2400/480=5 -> 6 beats
    std::ofstream(dir.filePath("groove.json")) << "straight|3/4||";

    GrooveKit kit;
    kit.requestLoad(dir.dir(), dir.dir(), "groove.mid");
    ASSERT_TRUE(waitUntilGrooveKitReady(kit));

    EXPECT_EQ(kit.installedMetadata().bars, 2u); // 6 beats / 3 per bar
}

TEST(GrooveKitTest, MetadataDefaultsWhenSidecarMissing)
{
    const TempGrooveKitDir dir;
    writeGrooveTake(dir, "bd", 1);
    writeGrooveMidiFile(dir.filePath("groove.mid"), {{0, 36}}); // no groove.json written

    GrooveKit kit;
    kit.requestLoad(dir.dir(), dir.dir(), "groove.mid");
    ASSERT_TRUE(waitUntilGrooveKitReady(kit));

    const auto& metadata = kit.installedMetadata();
    EXPECT_EQ(metadata.bars, 0u);
    EXPECT_TRUE(metadata.feel.empty());
    EXPECT_TRUE(metadata.timeSignature.empty());
    EXPECT_TRUE(metadata.dominantSounds.empty());
}

TEST(GrooveKitTest, MetadataDefaultsWhenSidecarIsMalformed)
{
    const TempGrooveKitDir dir;
    writeGrooveTake(dir, "bd", 1);
    writeGrooveMidiFile(dir.filePath("groove.mid"), {{0, 36}});
    std::ofstream(dir.filePath("groove.json")) << "not a valid sidecar";

    GrooveKit kit;
    kit.requestLoad(dir.dir(), dir.dir(), "groove.mid");
    ASSERT_TRUE(waitUntilGrooveKitReady(kit));

    EXPECT_EQ(kit.installedMetadata().bars, 0u);
}

TEST(GrooveKitTest, FormatGrooveInfoTextBuildsExpectedShape)
{
    GrooveMetadata metadata;
    metadata.bars = 4;
    metadata.feel = "even";
    metadata.timeSignature = "4/4";
    metadata.idealBpm = 91.f;
    metadata.dominantSounds = {"hihat", "snare"};

    EXPECT_EQ(GrooveKit::formatGrooveInfoText("Session Drums/str_4#4_1_c_v1", metadata),
              "Session Drums/str_4#4_1_c_v1: 4 bars, even feel, 4/4, 91 BPM - hihat, snare");
}

TEST(GrooveKitTest, FormatGrooveInfoTextOmitsEmptyFields)
{
    GrooveMetadata metadata;
    metadata.bars = 2;
    EXPECT_EQ(GrooveKit::formatGrooveInfoText("groove", metadata), "groove: 2 bars");
}

TEST(GrooveKitTest, DefaultBurstConfigRendersNoBurst)
{
    const TempGrooveKitDir dir;
    writeGrooveTake(dir, "bd", 1);
    writeGrooveMidiFile(dir.filePath("groove.mid"), {{0, 36}});

    GrooveKit kit;
    kit.requestLoad(dir.dir(), dir.dir(), "groove.mid"); // default BurstConfig{}: sampleRate == 0
    ASSERT_TRUE(waitUntilGrooveKitReady(kit));

    EXPECT_EQ(kit.installedBurstAudio(), nullptr);
}

TEST(GrooveKitTest, DefaultPushLifeProducesTriggersIdenticalToRawSourceTicks)
{
    const TempGrooveKitDir dir;
    writeGrooveTake(dir, "bd", 1);
    writeGrooveTake(dir, "sd", 1);
    // Slightly off-grid ticks: the point is that push = 0, life = 1 must not quantize.
    writeGrooveMidiFile(dir.filePath("groove.mid"), {{0, 36}, {487, 38}, {965, 36}, {1441, 38}});

    GrooveKit kit;
    kit.requestLoad(dir.dir(), dir.dir(), "groove.mid");
    ASSERT_TRUE(waitUntilGrooveKitReady(kit));

    const auto* program = kit.program();
    ASSERT_NE(program, nullptr);
    ASSERT_EQ(program->triggers.size(), 4u);
    const std::array<uint32_t, 4> expectedTicks{0, 487, 965, 1441};
    for (size_t i = 0; i < expectedTicks.size(); ++i)
    {
        EXPECT_EQ(program->triggers[i].tick, expectedTicks[i]) << i;
    }
}

TEST(GrooveKitTest, RequestHumanizeChangeDoesNotReloadTheSampleKit)
{
    const TempGrooveKitDir dir;
    writeGrooveTake(dir, "bd", 1);
    writeGrooveMidiFile(dir.filePath("groove.mid"), {{0, 36}, {960, 36}});

    GrooveKit kit;
    kit.requestLoad(dir.dir(), dir.dir(), "groove.mid");
    ASSERT_TRUE(waitUntilGrooveKitReady(kit));
    const auto* libraryBeforeChange = kit.library();

    // push = -1 (driving), life = 0 (dead): kick's 0.15-of-a-step bias against
    // a 120-tick (16th note @ 480 tpqn) grid nudges tick 960 to 942.
    kit.requestHumanizeChange(-1.f, 0.f);
    bool sawUpdatedTick = false;
    for (int i = 0; i < 2000; ++i)
    {
        kit.pollAndInstall();
        if (kit.program()->triggers.size() == 2 && kit.program()->triggers[1].tick == 942)
        {
            sawUpdatedTick = true;
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    EXPECT_TRUE(sawUpdatedTick);
    EXPECT_EQ(kit.library(), libraryBeforeChange);
}

TEST(GrooveKitTest, BurstConfigRendersRequestedBurst)
{
    const TempGrooveKitDir dir;
    writeGrooveTake(dir, "bd", 1);
    writeGrooveMidiFile(dir.filePath("groove.mid"), {{0, 36}});

    GrooveKit kit;
    kit.requestLoad(dir.dir(), dir.dir(), "groove.mid", BurstConfig{1000.f, 120.f});
    ASSERT_TRUE(waitUntilGrooveKitReady(kit));

    const auto burst = kit.installedBurstAudio();
    ASSERT_NE(burst, nullptr);
    EXPECT_EQ(burst->size(), 1000u * 2); // 1 s @ 1000 Hz, stereo
}

}
