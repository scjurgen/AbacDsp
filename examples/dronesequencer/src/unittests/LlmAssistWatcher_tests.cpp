#include <gtest/gtest.h>
#include <juce_core/juce_core.h>
#include <string>
#include <utility>
#include <vector>

#include "inc/LlmAssistWatcher.h"

namespace
{
// Creates a fresh, empty temp directory for one test and recursively deletes it afterward.
// All watcher callbacks in these tests are faked, so nothing here ever touches the real
// per-user Application Support paths FileIo/Processor would otherwise write to.
class ScopedTempFolder
{
  public:
    ScopedTempFolder()
        : m_dir(juce::File::getSpecialLocation(juce::File::tempDirectory)
                    .getChildFile("LlmAssistWatcherTests_" + juce::String(juce::Time::currentTimeMillis()) + "_" +
                                  juce::String(juce::Random::getSystemRandom().nextInt())))
    {
        m_dir.deleteRecursively();
        m_dir.createDirectory();
    }

    ~ScopedTempFolder()
    {
        m_dir.deleteRecursively();
    }

    ScopedTempFolder(const ScopedTempFolder&) = delete;
    ScopedTempFolder& operator=(const ScopedTempFolder&) = delete;

    [[nodiscard]] const juce::File& dir() const
    {
        return m_dir;
    }

  private:
    juce::File m_dir;
};

void writeFile(const juce::File& file, const juce::String& content)
{
    file.getParentDirectory().createDirectory();
    file.replaceWithText(content);
}
}

TEST(LlmAssistWatcher, PatchScriptCandidateTakesPriorityOverLibraryCandidate)
{
    const ScopedTempFolder folder;
    writeFile(folder.dir().getChildFile("mypatch.lua"), "function NextNotes() return {} end");
    writeFile(folder.dir().getChildFile("libraries/mylib.lua"), "function Helper() end");

    LlmAssistWatcher watcher;
    watcher.applyScriptText = [](const juce::String&) { return true; };

    const auto result = watcher.poll(folder.dir());
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->kind, LlmAssistResultKind::Script);
    EXPECT_EQ(result->scriptName, "mypatch");
}

TEST(LlmAssistWatcher, LibraryDropIsSavedAndTriggersReapplyOfCurrentScript)
{
    const ScopedTempFolder folder;
    writeFile(folder.dir().getChildFile("libraries/mylib.lua"), "function Helper() end");

    std::vector<std::pair<juce::String, juce::String>> saved;
    juce::String appliedWith;

    LlmAssistWatcher watcher;
    watcher.saveUserLibraryScript = [&saved](const juce::String& name, const juce::String& content)
    {
        saved.emplace_back(name, content);
        return true;
    };
    watcher.currentScriptText = [] { return juce::String("import \"mylib\"\nfunction NextNotes() return {} end"); };
    watcher.applyScriptText = [&appliedWith](const juce::String& text)
    {
        appliedWith = text;
        return true;
    };

    const auto result = watcher.poll(folder.dir());
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->kind, LlmAssistResultKind::Library);
    EXPECT_EQ(result->scriptName, "mylib");
    EXPECT_TRUE(result->compiled);
    ASSERT_EQ(saved.size(), 1u);
    EXPECT_EQ(saved[0].first, "mylib");
    EXPECT_EQ(saved[0].second, "function Helper() end");
    EXPECT_EQ(appliedWith, "import \"mylib\"\nfunction NextNotes() return {} end");
}

TEST(LlmAssistWatcher, LibraryDropReportsCurrentScriptCompileFailure)
{
    const ScopedTempFolder folder;
    writeFile(folder.dir().getChildFile("libraries/mylib.lua"), "function Helper() end");

    LlmAssistWatcher watcher;
    watcher.saveUserLibraryScript = [](const juce::String&, const juce::String&) { return true; };
    watcher.currentScriptText = [] { return juce::String("this is not lua"); };
    watcher.applyScriptText = [](const juce::String&) { return false; };
    watcher.scriptErrorMessage = [] { return juce::String("syntax error"); };

    const auto result = watcher.poll(folder.dir());
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->kind, LlmAssistResultKind::Library);
    EXPECT_FALSE(result->compiled);
    EXPECT_EQ(result->error, "syntax error");
}

TEST(LlmAssistWatcher, LibraryDropWithFailedSaveSkipsReapply)
{
    const ScopedTempFolder folder;
    writeFile(folder.dir().getChildFile("libraries/mylib.lua"), "function Helper() end");

    bool applyCalled = false;
    LlmAssistWatcher watcher;
    watcher.saveUserLibraryScript = [](const juce::String&, const juce::String&) { return false; };
    watcher.currentScriptText = [] { return juce::String("function NextNotes() return {} end"); };
    watcher.applyScriptText = [&applyCalled](const juce::String&)
    {
        applyCalled = true;
        return true;
    };

    const auto result = watcher.poll(folder.dir());
    ASSERT_TRUE(result.has_value());
    EXPECT_FALSE(result->compiled);
    EXPECT_FALSE(result->error.isEmpty());
    EXPECT_FALSE(applyCalled) << "must not re-apply the current script if the library itself failed to save";
}

TEST(LlmAssistWatcher, LibraryFileIsPulledAndStateFileHasLibraryKind)
{
    const ScopedTempFolder folder;
    const auto librariesDir = folder.dir().getChildFile("libraries");
    writeFile(librariesDir.getChildFile("mylib.lua"), "function Helper() end");

    LlmAssistWatcher watcher;
    watcher.saveUserLibraryScript = [](const juce::String&, const juce::String&) { return true; };
    watcher.currentScriptText = [] { return juce::String("function NextNotes() return {} end"); };
    watcher.applyScriptText = [](const juce::String&) { return true; };

    ASSERT_TRUE(watcher.poll(folder.dir()).has_value());

    EXPECT_FALSE(librariesDir.getChildFile("mylib.lua").existsAsFile());
    const auto pulled = librariesDir.findChildFiles(juce::File::findFiles, false, "pulled-mylib-*.lua");
    EXPECT_EQ(pulled.size(), 1);

    const auto stateFile = librariesDir.getChildFile("state-mylib.json");
    ASSERT_TRUE(stateFile.existsAsFile());
    const auto parsed = juce::JSON::parse(stateFile.loadFileAsString());
    EXPECT_EQ(parsed["kind"].toString(), "library");
    EXPECT_EQ(parsed["scriptName"].toString(), "mylib");
}

TEST(LlmAssistWatcher, PlainPatchScriptPollStillWorksEndToEnd)
{
    const ScopedTempFolder folder;
    writeFile(folder.dir().getChildFile("mypatch.lua"), "function NextNotes() return {} end");

    LlmAssistWatcher watcher;
    watcher.applyScriptText = [](const juce::String&) { return true; };

    const auto result = watcher.poll(folder.dir());
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->kind, LlmAssistResultKind::Script);
    EXPECT_TRUE(result->compiled);

    EXPECT_FALSE(folder.dir().getChildFile("mypatch.lua").existsAsFile());
    const auto stateFile = folder.dir().getChildFile("state-mypatch.json");
    ASSERT_TRUE(stateFile.existsAsFile());
    const auto parsed = juce::JSON::parse(stateFile.loadFileAsString());
    EXPECT_EQ(parsed["kind"].toString(), "script");
}
