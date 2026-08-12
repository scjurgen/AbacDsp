#include <gtest/gtest.h>
#include <juce_core/juce_core.h>
#include <string>

#include "impl/FileIo.h"

namespace
{
// Creates a library script file for the duration of one test and removes it afterward,
// regardless of outcome, so these tests never depend on state left by a previous run.
class ScopedLibraryFile
{
  public:
    ScopedLibraryFile(const juce::File& dir, const std::string& name, const std::string& content)
        : m_file(dir.getChildFile(juce::String(name) + ".lua"))
    {
        m_file.deleteFile();
        m_file.replaceWithText(content);
    }

    ~ScopedLibraryFile()
    {
        m_file.deleteFile();
    }

    ScopedLibraryFile(const ScopedLibraryFile&) = delete;
    ScopedLibraryFile& operator=(const ScopedLibraryFile&) = delete;

  private:
    juce::File m_file;
};
}

TEST(FileIo, ResolveLibraryScriptReturnsNulloptForUnknownName)
{
    const auto lookup = FileIo::resolveLibraryScript("zz_fileio_test_definitely_missing");
    EXPECT_FALSE(lookup.source.has_value());
    EXPECT_NE(lookup.notFoundDetail.find("Library"), std::string::npos)
        << "should describe where it looked: " << lookup.notFoundDetail;
}

TEST(FileIo, ResolveLibraryScriptRejectsEmptyName)
{
    EXPECT_FALSE(FileIo::resolveLibraryScript("").source.has_value());
}

TEST(FileIo, ResolveLibraryScriptFindsFileInBaseDirectory)
{
    const ScopedLibraryFile file(FileIo::getLibraryBaseDirectory(), "zz_fileio_test_base_only", "BaseMarker");
    const auto lookup = FileIo::resolveLibraryScript("zz_fileio_test_base_only");
    ASSERT_TRUE(lookup.source.has_value());
    EXPECT_EQ(*lookup.source, "BaseMarker");
}

TEST(FileIo, UserDirectoryOverridesBaseDirectoryForSameName)
{
    const ScopedLibraryFile baseFile(FileIo::getLibraryBaseDirectory(), "zz_fileio_test_override", "BaseVersion");
    const ScopedLibraryFile userFile(FileIo::getLibraryUserDirectory(), "zz_fileio_test_override", "UserVersion");
    const auto lookup = FileIo::resolveLibraryScript("zz_fileio_test_override");
    ASSERT_TRUE(lookup.source.has_value());
    EXPECT_EQ(*lookup.source, "UserVersion");
}

// Integration-style: DRONESEQUENCER_BASE_SCRIPTS_DIR (set for this test target only, see
// CMakeLists.txt) points at fixtures/base-scripts-fixture/, and initialize() really does
// sync it into the real per-user Library/Base/ directory - there is no seam to fake that.
TEST(FileIo, InitializeSyncsBaseLibraryScriptsFromFixtureDirectory)
{
    FileIo fileIo;
    fileIo.initialize({0});
    const auto lookup = FileIo::resolveLibraryScript("fileio_sync_fixture");
    ASSERT_TRUE(lookup.source.has_value());
    EXPECT_NE(lookup.source->find("FileIoSyncFixtureMarker"), std::string::npos);
}
