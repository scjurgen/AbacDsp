#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <mutex>
#include <nlohmann/json.hpp>
#include <optional>
#include <span>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "AudioFile/LoadWav.h"
#include "AudioFile/SaveWav.h"

/**
 * @brief Named loop save/load ("Loops" menu) for tapelooper: background-threaded WAV+JSON
 * encode/decode of <loopsDirectory>/<name>_track{A,B,...}.wav + <name>.json, scoped to
 * tapelooper's NumTracks fixed tracks (no parts/sequencer/slices/overdub, unlike looper's own
 * LoopStorageService). This class only decodes/encodes on its own background threads (one for
 * save, one for load, each with a generation-counter handshake) - (possibly chunked) extraction
 * and installation of the live tape tracks is the caller's job, since only the audio thread may
 * safely touch their ring buffers (see TapeLooperImpl.h).
 */
template <size_t NumTracks>
class TapeLooperLoopStorageService
{
  public:
    struct LoopLoadOutcome
    {
        bool attempted{false};
        bool success{false};
        // Always false - kept only so the generic Loops-menu template's interface is
        // satisfied; tapelooper doesn't embed a second, WAV-side BPM to conflict with.
        bool hasConflict{false};
        float wavBpm{0.f};
        float jsonBpm{0.f};
        std::string patchParamsJson;
    };

    struct LoopLoadResult
    {
        std::array<std::vector<float>, NumTracks> trackLeft;
        std::array<std::vector<float>, NumTracks> trackRight;
        float bars{8.f};
        float bpm{120.f};
    };

    TapeLooperLoopStorageService()
    {
        m_saveThread = std::jthread([this](const std::stop_token& stopToken) { runSaveThread(stopToken); });
        m_loadThread = std::jthread([this](const std::stop_token& stopToken) { runLoadThread(stopToken); });
    }

    // See GrooveKit's own destructor comment: request_stop()+notify_all() must happen before
    // std::jthread's own destructor, or a lost wakeup can leave the wait blocked forever.
    ~TapeLooperLoopStorageService()
    {
        m_saveThread.request_stop();
        m_saveCv.notify_all();
        m_loadThread.request_stop();
        m_loadCv.notify_all();
    }

    void setLoopsDirectory(std::string dir)
    {
        m_loopsDirectory = std::move(dir);
    }

    // A "/" in a loop name denotes a subfolder, mirroring FileIo's named-patch convention.
    [[nodiscard]] std::vector<std::string> listLoopNames() const
    {
        std::vector<std::string> names;
        if (m_loopsDirectory.empty() || !std::filesystem::exists(m_loopsDirectory))
        {
            return names;
        }
        const std::filesystem::path root(m_loopsDirectory);
        for (const auto& entry : std::filesystem::recursive_directory_iterator(root))
        {
            if (entry.path().extension() == ".json")
            {
                auto relative = entry.path().lexically_relative(root).replace_extension().generic_string();
                names.push_back(std::move(relative));
            }
        }
        std::ranges::sort(names);
        return names;
    }

    bool deleteLoopNamed(const std::string& name)
    {
        if (sanitizeLoopName(name).empty())
        {
            return false;
        }
        std::error_code ec;
        const bool removedJson = std::filesystem::remove(jsonPath(name), ec);
        for (size_t t = 0; t < NumTracks; ++t)
        {
            std::filesystem::remove(trackWavPath(name, t), ec);
        }
        if (removedJson)
        {
            std::lock_guard<std::mutex> lock(m_currentLoopMutex);
            if (name == m_currentLoopName)
            {
                m_currentLoopName.clear();
            }
        }
        return removedJson;
    }

    bool renameLoopNamed(const std::string& oldName, const std::string& newName)
    {
        if (sanitizeLoopName(oldName).empty() || sanitizeLoopName(newName).empty() || oldName == newName)
        {
            return false;
        }
        std::error_code ec;
        std::filesystem::create_directories(jsonPath(newName).parent_path(), ec);
        std::filesystem::rename(jsonPath(oldName), jsonPath(newName), ec);
        if (ec)
        {
            return false;
        }
        for (size_t t = 0; t < NumTracks; ++t)
        {
            std::error_code trackEc;
            std::filesystem::rename(trackWavPath(oldName, t), trackWavPath(newName, t), trackEc);
        }
        std::lock_guard<std::mutex> lock(m_currentLoopMutex);
        if (oldName == m_currentLoopName)
        {
            m_currentLoopName = newName;
        }
        return true;
    }

    [[nodiscard]] std::string currentLoopName() const
    {
        std::lock_guard<std::mutex> lock(m_currentLoopMutex);
        return m_currentLoopName;
    }

    // Message thread: sizes this save's own scratch buffers to loopFrames (a real, one-time
    // allocation - safe here, never on the audio thread). writeSaveChunk()/finishSave() below
    // then fill and dispatch them from the audio thread with no further allocation.
    void beginSave(const std::string& name, const std::string& patchParamsJson, const size_t loopFrames,
                   const float bars, const float bpm, const float sampleRate)
    {
        if (m_savePending)
        {
            return;
        }
        m_saveName = name;
        m_savePatchParamsJson = patchParamsJson;
        for (auto& v : m_saveTrackLeft)
        {
            v.assign(loopFrames, 0.f);
        }
        for (auto& v : m_saveTrackRight)
        {
            v.assign(loopFrames, 0.f);
        }
        m_saveBars = bars;
        m_saveBpm = bpm;
        m_saveSampleRate = sampleRate;
    }

    // Audio thread: writes one chunk of one track's audio into this save's own scratch buffer
    // (already sized by beginSave()) - no allocation, safe to call across many blocks.
    void writeSaveChunk(const size_t track, const size_t offset, const std::span<const float> left,
                        const std::span<const float> right) noexcept
    {
        std::ranges::copy(left, m_saveTrackLeft[track].begin() + static_cast<std::ptrdiff_t>(offset));
        std::ranges::copy(right, m_saveTrackRight[track].begin() + static_cast<std::ptrdiff_t>(offset));
    }

    // Audio thread: every track's chunks are written - dispatches the background encode.
    void finishSave() noexcept
    {
        if (m_savePending || m_loopsDirectory.empty() || sanitizeLoopName(m_saveName).empty())
        {
            return;
        }
        m_saveRequestedGen = m_saveRequestGen.load(std::memory_order_relaxed) + 1;
        m_savePending = true;
        m_saveRequestGen.store(m_saveRequestedGen, std::memory_order_release);
        m_saveCv.notify_one();
    }

    [[nodiscard]] bool isSavePending() const noexcept
    {
        return m_savePending;
    }

    // Audio thread, polled every block: clears the pending flag once the worker's
    // done-generation catches up.
    void checkSaveCompletion() noexcept
    {
        if (!m_savePending || m_saveDoneGen.load(std::memory_order_acquire) != m_saveRequestedGen)
        {
            return;
        }
        m_savePending = false;
    }

    void requestLoad(const std::string& name)
    {
        if (m_loadPending || m_loopsDirectory.empty() || sanitizeLoopName(name).empty())
        {
            return;
        }
        m_loadName = name;
        m_loadRequestedGen = m_loadRequestGen.load(std::memory_order_relaxed) + 1;
        m_loadPending = true;
        m_loadRequestGen.store(m_loadRequestedGen, std::memory_order_release);
        m_loadCv.notify_one();
    }

    [[nodiscard]] bool isLoadPending() const noexcept
    {
        return m_loadPending;
    }

    [[nodiscard]] LoopLoadOutcome consumeLoadOutcome()
    {
        std::lock_guard<std::mutex> lock(m_loadOutcomeMutex);
        return std::exchange(m_loadOutcome, LoopLoadOutcome{});
    }

    // Audio thread, polled every block; returns the decoded payload once the worker's
    // done-generation catches up, for the caller to install (chunked - see TapeLooperImpl).
    [[nodiscard]] std::optional<LoopLoadResult> pollLoadCompletion()
    {
        if (!m_loadPending || m_loadDoneGen.load(std::memory_order_acquire) != m_loadRequestedGen)
        {
            return std::nullopt;
        }
        m_loadPending = false;
        LoopLoadResult result;
        result.trackLeft = std::move(m_loadTrackLeft);
        result.trackRight = std::move(m_loadTrackRight);
        result.bars = m_loadBars;
        result.bpm = m_loadBpm;
        return result;
    }

  private:
    // Filesystem-unsafe characters stripped by hand (JUCE-free, mirroring LoopStorageService's
    // own reasoning); a "/" denotes a subfolder, each segment sanitized on its own.
    [[nodiscard]] static std::string sanitizeLoopName(const std::string& name)
    {
        std::vector<std::string> segments;
        size_t start = 0;
        while (start <= name.size())
        {
            const auto slash = name.find('/', start);
            const auto end = (slash == std::string::npos) ? name.size() : slash;
            std::string segment;
            for (size_t i = start; i < end; ++i)
            {
                if (std::string_view("\\:*?\"<>|").find(name[i]) == std::string_view::npos)
                {
                    segment += name[i];
                }
            }
            const auto first = segment.find_first_not_of(" \t");
            if (first != std::string::npos)
            {
                const auto last = segment.find_last_not_of(" \t");
                segments.push_back(segment.substr(first, last - first + 1));
            }
            if (slash == std::string::npos)
            {
                break;
            }
            start = slash + 1;
        }
        if (segments.empty())
        {
            return {};
        }
        std::string joined = segments.front();
        for (size_t i = 1; i < segments.size(); ++i)
        {
            joined += "/" + segments[i];
        }
        return joined;
    }

    [[nodiscard]] std::filesystem::path loopBasePath(const std::string& name, const bool createDirs = false) const
    {
        const auto sanitized = sanitizeLoopName(name);
        if (sanitized.empty())
        {
            return {};
        }
        std::filesystem::path path(m_loopsDirectory);
        size_t start = 0;
        while (true)
        {
            const auto slash = sanitized.find('/', start);
            path /= sanitized.substr(start, slash == std::string::npos ? std::string::npos : slash - start);
            if (slash == std::string::npos)
            {
                break;
            }
            start = slash + 1;
        }
        if (createDirs)
        {
            std::filesystem::create_directories(path.parent_path());
        }
        return path;
    }

    [[nodiscard]] std::filesystem::path jsonPath(const std::string& name, const bool createDirs = false) const
    {
        auto path = loopBasePath(name, createDirs);
        path += ".json";
        return path;
    }

    [[nodiscard]] std::filesystem::path trackWavPath(const std::string& name, const size_t track,
                                                     const bool createDirs = false) const
    {
        auto path = loopBasePath(name, createDirs);
        path += "_track";
        path += static_cast<char>('A' + track);
        path += ".wav";
        return path;
    }

    void runSaveThread(const std::stop_token& stopToken)
    {
        uint64_t lastHandled = 0;
        while (!stopToken.stop_requested())
        {
            std::unique_lock lock(m_saveWaitMutex);
            m_saveCv.wait(lock, stopToken, [this, lastHandled]
                          { return m_saveRequestGen.load(std::memory_order_acquire) != lastHandled; });
            if (stopToken.stop_requested())
            {
                return;
            }
            lastHandled = m_saveRequestGen.load(std::memory_order_acquire);
            lock.unlock();
            runSave(lastHandled);
        }
    }

    // Worker thread only.
    void runSave(const uint64_t gen)
    {
        for (size_t t = 0; t < NumTracks; ++t)
        {
            AudioUtility::SaveWav::saveStereoAs(trackWavPath(m_saveName, t, true).string(), m_saveTrackLeft[t],
                                                m_saveTrackRight[t], m_saveSampleRate);
        }
        nlohmann::json j;
        j["bars"] = m_saveBars;
        j["bpm"] = m_saveBpm;
        if (!m_savePatchParamsJson.empty())
        {
            try
            {
                j["patchParams"] = nlohmann::json::parse(m_savePatchParamsJson);
            }
            catch (const nlohmann::json::exception& e)
            {
                std::cerr << "TapeLooperLoopStorageService: failed to embed patch params: " << e.what() << std::endl;
            }
        }
        std::ofstream jsonOut(jsonPath(m_saveName));
        if (jsonOut)
        {
            jsonOut << j.dump(2);
        }
        {
            std::lock_guard<std::mutex> lock(m_currentLoopMutex);
            m_currentLoopName = m_saveName;
        }
        m_saveDoneGen.store(gen, std::memory_order_release);
    }

    void runLoadThread(const std::stop_token& stopToken)
    {
        uint64_t lastHandled = 0;
        while (!stopToken.stop_requested())
        {
            std::unique_lock lock(m_loadWaitMutex);
            m_loadCv.wait(lock, stopToken, [this, lastHandled]
                          { return m_loadRequestGen.load(std::memory_order_acquire) != lastHandled; });
            if (stopToken.stop_requested())
            {
                return;
            }
            lastHandled = m_loadRequestGen.load(std::memory_order_acquire);
            lock.unlock();
            runLoad(lastHandled);
        }
    }

    // Worker thread only.
    void runLoad(const uint64_t gen)
    {
        LoopLoadOutcome outcome;
        outcome.attempted = true;
        std::ifstream jsonIn(jsonPath(m_loadName));
        if (jsonIn)
        {
            try
            {
                nlohmann::json j;
                jsonIn >> j;
                m_loadBars = j.value("bars", 8.f);
                m_loadBpm = j.value("bpm", 120.f);
                if (j.contains("patchParams"))
                {
                    outcome.patchParamsJson = j.at("patchParams").dump();
                }
                bool allTracksLoaded = true;
                for (size_t t = 0; t < NumTracks; ++t)
                {
                    auto [left, right] =
                        AudioUtility::LoadWav::loadStereoFromFile(trackWavPath(m_loadName, t).string());
                    allTracksLoaded = allTracksLoaded && !left.empty();
                    m_loadTrackLeft[t] = std::move(left);
                    m_loadTrackRight[t] = std::move(right);
                }
                outcome.success = allTracksLoaded;
                if (outcome.success)
                {
                    std::lock_guard<std::mutex> lock(m_currentLoopMutex);
                    m_currentLoopName = m_loadName;
                }
            }
            catch (const std::exception& e)
            {
                std::cerr << "TapeLooperLoopStorageService: failed to parse " << jsonPath(m_loadName) << ": "
                          << e.what() << std::endl;
            }
        }
        {
            std::lock_guard<std::mutex> lock(m_loadOutcomeMutex);
            m_loadOutcome = outcome;
        }
        m_loadDoneGen.store(gen, std::memory_order_release);
    }

    std::string m_loopsDirectory;

    std::string m_saveName;
    std::string m_savePatchParamsJson;
    std::array<std::vector<float>, NumTracks> m_saveTrackLeft;
    std::array<std::vector<float>, NumTracks> m_saveTrackRight;
    float m_saveBars{8.f};
    float m_saveBpm{120.f};
    float m_saveSampleRate{48000.f};
    bool m_savePending{false};
    uint64_t m_saveRequestedGen{0};
    std::atomic<uint64_t> m_saveRequestGen{0};
    std::atomic<uint64_t> m_saveDoneGen{0};
    std::mutex m_saveWaitMutex;
    std::condition_variable_any m_saveCv;

    std::string m_loadName;
    bool m_loadPending{false};
    uint64_t m_loadRequestedGen{0};
    std::atomic<uint64_t> m_loadRequestGen{0};
    std::atomic<uint64_t> m_loadDoneGen{0};
    std::mutex m_loadWaitMutex;
    std::condition_variable_any m_loadCv;
    std::array<std::vector<float>, NumTracks> m_loadTrackLeft; // worker-owned until pollLoadCompletion()
    std::array<std::vector<float>, NumTracks> m_loadTrackRight;
    float m_loadBars{8.f};
    float m_loadBpm{120.f};
    std::mutex m_loadOutcomeMutex;
    LoopLoadOutcome m_loadOutcome;

    mutable std::mutex m_currentLoopMutex;
    std::string m_currentLoopName;

    // Declared last so they are destroyed first: each thread's own stop/join must complete
    // before any member above it might still be read from inside its loop.
    std::jthread m_saveThread;
    std::jthread m_loadThread;
};
