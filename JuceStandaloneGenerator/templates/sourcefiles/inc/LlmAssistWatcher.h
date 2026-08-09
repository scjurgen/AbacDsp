#pragma once

#include <functional>
#include <juce_core/juce_core.h>
#include <optional>
#include <vector>

#include "LlmAssistWatcherCore.h"

// Watches a folder for dropped Lua scripts. Each poll() applies the newest-mtime
// "*.lua" file not already marked "pulled-", reports the result via a sibling
// "state-{name}.json", and renames the source to "pulled-{name}-{epoch-ms}.lua" so it
// is never reapplied.
class LlmAssistWatcher
{
  public:
    std::function<bool(const juce::String&)> applyScriptText;
    std::function<juce::String()> scriptErrorMessage;
    std::function<juce::String()> currentPatchName;

    struct Result
    {
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
        const auto candidate = findCandidate(folder);
        if (!candidate.existsAsFile())
        {
            return std::nullopt;
        }
        return applyAndPull(candidate);
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

    void writeStateFile(const juce::File& folder, const Result& result) const
    {
        auto* obj = new juce::DynamicObject();
        obj->setProperty("compiled", result.compiled);
        obj->setProperty("error", result.error);
        obj->setProperty("patchName", currentPatchName ? currentPatchName() : juce::String{});
        obj->setProperty("scriptName", result.scriptName);
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
