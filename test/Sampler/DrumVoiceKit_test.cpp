#include <chrono>
#include <filesystem>
#include <gtest/gtest.h>
#include <string>
#include <thread>
#include <vector>

#include "AudioFile/SaveWav.h"

#include "Sampler/DrumVoiceKit.h"

namespace AbacDsp::test
{

namespace
{
class TempDrumVoiceKitDir
{
  public:
    TempDrumVoiceKitDir()
        : m_dir(std::filesystem::temp_directory_path() / "abacdsp_drumvoicekit_test")
    {
        std::error_code ec;
        std::filesystem::remove_all(m_dir, ec);
        std::filesystem::create_directories(m_dir);
    }

    ~TempDrumVoiceKitDir()
    {
        std::error_code ec;
        std::filesystem::remove_all(m_dir, ec);
    }

    [[nodiscard]] std::string dir() const
    {
        return m_dir.string();
    }

  private:
    std::filesystem::path m_dir;
};

// Constant-valued takes so a test can tell which take was returned just by its
// sample value, without decoding a real drum sample.
void writeTake(const TempDrumVoiceKitDir& dir, const std::string& code, const unsigned index, const float value)
{
    const std::vector<float> mono(8, value);
    AudioUtility::SaveWav::saveStereoAs(
        (std::filesystem::path(dir.dir()) / (code + "_" + std::to_string(index) + ".wav")).string(), mono, mono);
}

[[nodiscard]] bool waitUntilReady(DrumVoiceKit& kit)
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

TEST(DrumVoiceKitTest, NotReadyBeforeAnyLoadRequested)
{
    DrumVoiceKit kit;
    kit.pollAndInstall();
    EXPECT_FALSE(kit.isReady());
    EXPECT_EQ(kit.nextTake("bd"), nullptr);
}

TEST(DrumVoiceKitTest, LoadsSingleTakeCode)
{
    const TempDrumVoiceKitDir dir;
    writeTake(dir, "bd", 1, 0.25f);

    DrumVoiceKit kit;
    kit.requestLoad(dir.dir());
    ASSERT_TRUE(waitUntilReady(kit));

    const auto take = kit.nextTake("bd");
    ASSERT_NE(take, nullptr);
    EXPECT_NEAR((*take)[0], 0.25f, 1e-4f);
}

TEST(DrumVoiceKitTest, UnknownCodeReturnsNull)
{
    const TempDrumVoiceKitDir dir;
    writeTake(dir, "bd", 1, 0.25f);

    DrumVoiceKit kit;
    kit.requestLoad(dir.dir());
    ASSERT_TRUE(waitUntilReady(kit));

    EXPECT_EQ(kit.nextTake("sd"), nullptr);
}

TEST(DrumVoiceKitTest, CyclesRoundRobinTakesInOrderThenWraps)
{
    const TempDrumVoiceKitDir dir;
    writeTake(dir, "hh", 1, 0.1f);
    writeTake(dir, "hh", 2, 0.2f);
    writeTake(dir, "hh", 3, 0.3f);

    DrumVoiceKit kit;
    kit.requestLoad(dir.dir());
    ASSERT_TRUE(waitUntilReady(kit));

    EXPECT_NEAR((*kit.nextTake("hh"))[0], 0.1f, 1e-4f);
    EXPECT_NEAR((*kit.nextTake("hh"))[0], 0.2f, 1e-4f);
    EXPECT_NEAR((*kit.nextTake("hh"))[0], 0.3f, 1e-4f);
    EXPECT_NEAR((*kit.nextTake("hh"))[0], 0.1f, 1e-4f); // wrapped
}

TEST(DrumVoiceKitTest, MissingDirectoryLeavesKitEmptyNotCrashing)
{
    DrumVoiceKit kit;
    kit.requestLoad("/does/not/exist/anywhere");
    for (int i = 0; i < 50; ++i)
    {
        kit.pollAndInstall();
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    EXPECT_FALSE(kit.isReady());
    EXPECT_EQ(kit.nextTake("bd"), nullptr);
}

TEST(DrumVoiceKitTest, LaterRequestSupersedesEarlierOne)
{
    const TempDrumVoiceKitDir dirA;
    writeTake(dirA, "bd", 1, 0.4f);
    const TempDrumVoiceKitDir dirB;
    writeTake(dirB, "sd", 1, 0.6f);

    DrumVoiceKit kit;
    kit.requestLoad(dirA.dir());
    kit.requestLoad(dirB.dir());
    ASSERT_TRUE(waitUntilReady(kit));

    // Only one of the two loads can have won the race, but eventually the kit
    // settles on the second request's directory.
    for (int i = 0; i < 200 && kit.nextTake("sd") == nullptr; ++i)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        kit.pollAndInstall();
    }
    EXPECT_NE(kit.nextTake("sd"), nullptr);
}

}
