#include <algorithm>
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

TEST(FileIo, SaveUserLibraryScriptWritesReadableFile)
{
    const ScopedLibraryFile guard(FileIo::getLibraryUserDirectory(), "zz_fileio_test_save", "");
    ASSERT_TRUE(FileIo::saveUserLibraryScript("zz_fileio_test_save", "function Foo() end"));
    const auto lookup = FileIo::resolveLibraryScript("zz_fileio_test_save");
    ASSERT_TRUE(lookup.source.has_value());
    EXPECT_EQ(*lookup.source, "function Foo() end");
}

TEST(FileIo, SaveUserLibraryScriptOverwritesExistingContent)
{
    const ScopedLibraryFile guard(FileIo::getLibraryUserDirectory(), "zz_fileio_test_overwrite", "old");
    ASSERT_TRUE(FileIo::saveUserLibraryScript("zz_fileio_test_overwrite", "new"));
    const auto lookup = FileIo::resolveLibraryScript("zz_fileio_test_overwrite");
    ASSERT_TRUE(lookup.source.has_value());
    EXPECT_EQ(*lookup.source, "new");
}

TEST(FileIo, SaveUserLibraryScriptRejectsInvalidName)
{
    EXPECT_FALSE(FileIo::saveUserLibraryScript("bad name!", "function Foo() end"));
    EXPECT_FALSE(FileIo::saveUserLibraryScript("", "function Foo() end"));
}

TEST(FileIo, ListLibraryScriptNamesIncludesBothTiers)
{
    const ScopedLibraryFile userFile(FileIo::getLibraryUserDirectory(), "zz_fileio_test_list_user", "x");
    const ScopedLibraryFile baseFile(FileIo::getLibraryBaseDirectory(), "zz_fileio_test_list_base", "x");
    const auto names = FileIo::listLibraryScriptNames();
    EXPECT_NE(std::find(names.begin(), names.end(), "zz_fileio_test_list_user"), names.end());
    EXPECT_NE(std::find(names.begin(), names.end(), "zz_fileio_test_list_base"), names.end());
}

TEST(FileIo, ListLibraryScriptNamesListsANameInBothTiersOnce)
{
    const ScopedLibraryFile userFile(FileIo::getLibraryUserDirectory(), "zz_fileio_test_list_both", "user");
    const ScopedLibraryFile baseFile(FileIo::getLibraryBaseDirectory(), "zz_fileio_test_list_both", "base");
    const auto names = FileIo::listLibraryScriptNames();
    EXPECT_EQ(std::count(names.begin(), names.end(), "zz_fileio_test_list_both"), 1);
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

// Same shape as the sync test above, but for DRONESEQUENCER_FACTORY_PATCHES_DIR ->
// Patches/Factory/, exercising readPatchJson()'s "/"-splitting on a synced subfolder name.
TEST(FileIo, InitializeSyncsFactoryPatchesFromFixtureDirectoryAndReadPatchJsonFindsIt)
{
    FileIo fileIo;
    fileIo.initialize({0});
    const auto names = fileIo.listPatchNames();
    ASSERT_NE(std::find(names.begin(), names.end(), "Factory/fileio-sync-fixture"), names.end());

    const auto json = fileIo.readPatchJson("Factory/fileio-sync-fixture");
    ASSERT_TRUE(json.has_value());
    EXPECT_NE(json->find("FileIoFactoryPatchSyncFixtureMarker"), std::string::npos);
}

TEST(FileIo, ReadPatchJsonReturnsNulloptForUnknownName)
{
    FileIo fileIo;
    fileIo.initialize({0});
    EXPECT_FALSE(fileIo.readPatchJson("zz_fileio_test_nonexistent_patch").has_value());
}

TEST(FileIo, ReadPatchJsonRoundTripsASavedPatch)
{
    FileIo fileIo;
    fileIo.initialize({0});
    ASSERT_TRUE(fileIo.savePatchNamed("zz_fileio_test_roundtrip"));
    const auto json = fileIo.readPatchJson("zz_fileio_test_roundtrip");
    ASSERT_TRUE(json.has_value());
    // Parsed comparison, not string equality: savePatchNamed() pretty-prints (dump(2)),
    // currentParametersAsJson() doesn't (dump()) - same content, different formatting.
    EXPECT_EQ(nlohmann::json::parse(*json), nlohmann::json::parse(fileIo.currentParametersAsJson()));
    fileIo.deletePatchNamed("zz_fileio_test_roundtrip");
}
