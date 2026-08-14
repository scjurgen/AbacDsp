#pragma once

#include <functional>
#include <juce_core/juce_core.h>
#include <optional>
#include <vector>

#include "LlmAssistWatcherCore.h"

enum class LlmAssistResultKind
{
    Script,
    Library
};

// Watches a folder for dropped Lua scripts. Each poll() applies the newest-mtime
// "*.lua" file not already marked "pulled-" from the folder itself (a full patch script)
// or, failing that, from its "libraries" subfolder (a shared import "name" library) - see
// applyLibraryAndPull() for what a library drop actually does. Either way, the result is
// reported via a sibling "state-{name}.json" and the source is renamed to
// "pulled-{name}-{epoch-ms}.lua" so it is never reapplied.
class LlmAssistWatcher
{
  public:
    std::function<bool(const juce::String&)> applyScriptText;
    std::function<juce::String()> scriptErrorMessage;
    std::function<juce::String()> currentPatchName;
    // Only needed for library drops: the currently active patch script's own source, and
    // a way to persist a library into Library/User/. See applyLibraryAndPull().
    std::function<juce::String()> currentScriptText;
    std::function<bool(const juce::String& name, const juce::String& content)> saveUserLibraryScript;

    struct Result
    {
        LlmAssistResultKind kind{LlmAssistResultKind::Script};
        juce::String scriptName;
        bool compiled{false};
        juce::String error;
    };

    std::optional<Result> poll(const juce::File& folder)
    {
        if (!applyScriptText)
        {
            return std::nullopt;
        }
        if (const auto script = findCandidate(folder); script.existsAsFile())
        {
            return applyAndPull(script);
        }
        if (const auto library = findCandidate(folder.getChildFile("libraries")); library.existsAsFile())
        {
            return applyLibraryAndPull(library);
        }
        return std::nullopt;
    }

  private:
    [[nodiscard]] static juce::File findCandidate(const juce::File& folder)
    {
        const auto entries = folder.findChildFiles(juce::File::findFiles, false, "*.lua");
        std::vector<std::pair<std::string, std::int64_t>> files;
        files.reserve(static_cast<std::size_t>(entries.size()));
        for (const auto& f : entries)
        {
            files.emplace_back(f.getFileName().toStdString(), f.getLastModificationTime().toMilliseconds());
        }
        const auto index = selectLlmAssistCandidate(files);
        return index < 0 ? juce::File{} : entries[index];
    }

    Result applyAndPull(const juce::File& source)
    {
        Result result;
        result.scriptName = source.getFileNameWithoutExtension();
        result.compiled = applyScriptText(source.loadFileAsString());
        result.error = result.compiled ? juce::String{} : (scriptErrorMessage ? scriptErrorMessage() : juce::String{});

        writeStateFile(source.getParentDirectory(), result);
        pull(source, result.scriptName);
        return result;
    }

    // A library drop never touches the running script directly: it saves the file into
    // Library/User/, then re-applies whatever patch script is currently active so a real
    // `import "name"` for it (if any) is re-resolved against the new content and actually
    // validated - not an isolated syntax check. compiled/error below describe that
    // re-apply's outcome, not the library file in isolation; state-{name}.json's "kind"
    // field makes this distinction explicit to whatever reads it.
    Result applyLibraryAndPull(const juce::File& source)
    {
        Result result;
        result.kind = LlmAssistResultKind::Library;
        result.scriptName = source.getFileNameWithoutExtension();

        const bool saved = saveUserLibraryScript && saveUserLibraryScript(result.scriptName, source.loadFileAsString());
        if (!saved)
        {
            result.error = "failed to save library script (invalid name or write error)";
        }
        else if (currentScriptText)
        {
            result.compiled = applyScriptText(currentScriptText());
            result.error =
                result.compiled ? juce::String{} : (scriptErrorMessage ? scriptErrorMessage() : juce::String{});
        }
        else
        {
            result.compiled = true; // saved fine; nothing to re-validate against
        }

        writeStateFile(source.getParentDirectory(), result);
        pull(source, result.scriptName);
        return result;
    }

    void writeStateFile(const juce::File& folder, const Result& result) const
    {
        auto* obj = new juce::DynamicObject();
        obj->setProperty("compiled", result.compiled);
        obj->setProperty("error", result.error);
        obj->setProperty("patchName", currentPatchName ? currentPatchName() : juce::String{});
        obj->setProperty("scriptName", result.scriptName);
        obj->setProperty("kind", result.kind == LlmAssistResultKind::Library ? "library" : "script");
        const juce::var json(obj);
        const auto stateFile = folder.getChildFile(makeStateFilename(result.scriptName.toStdString()));
        stateFile.replaceWithText(juce::JSON::toString(json));
    }

    static void pull(const juce::File& source, const juce::String& baseName)
    {
        const auto pulled =
            source.getSiblingFile(makePulledFilename(baseName.toStdString(), juce::Time::currentTimeMillis()));
        if (pulled.existsAsFile())
        {
            pulled.deleteFile();
        }
        source.moveFileTo(pulled);
    }
};
