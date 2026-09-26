#pragma once

#include <algorithm>
#include <atomic>
#include <cctype>
#include <condition_variable>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <vector>

#include "AudioFile/LoadWav.h"

namespace AbacDsp
{

/**
 * @ingroup sampler
 * @brief Loads a directory of "<code>_<n>.wav" round-robin drum takes off the
 * audio thread and hands out one take per code at a time, cycling round-robin.
 *
 * Unlike GrooveKit, this does not resolve MIDI notes or tags - it is a thin,
 * self-contained loader for callers (e.g. a metronome voicing) that already
 * know which code they want at each trigger. requestLoad() is safe to call
 * repeatedly; a completed load is only installed by the audio thread's own
 * pollAndInstall(), never reached into mid-load by the background thread.
 */
class DrumVoiceKit
{
  public:
    DrumVoiceKit()
    {
        m_thread = std::jthread(
            [this](const std::stop_token& stopToken)
            {
                uint64_t lastHandled = 0;
                while (!stopToken.stop_requested())
                {
                    std::unique_lock lock(m_waitMutex);
                    m_cv.wait(lock, stopToken, [this, lastHandled]
                              { return m_requestGen.load(std::memory_order_acquire) != lastHandled; });
                    if (stopToken.stop_requested())
                    {
                        return;
                    }
                    lastHandled = m_requestGen.load(std::memory_order_acquire);
                    lock.unlock();
                    runLoad(lastHandled);
                }
            });
    }

    // See GrooveKit::~GrooveKit()'s own doc comment for why this precedes m_thread's
    // own destructor: guards against a lost-wakeup race on stop_token-based waits.
    ~DrumVoiceKit()
    {
        m_thread.request_stop();
        m_cv.notify_all();
    }

    DrumVoiceKit(const DrumVoiceKit&) = delete;
    DrumVoiceKit& operator=(const DrumVoiceKit&) = delete;

    // Queues a (re)load from sampleDir. A later call while one is in flight
    // simply supersedes it.
    void requestLoad(std::string sampleDir)
    {
        {
            std::lock_guard lock(m_requestMutex);
            m_requestedSampleDir = std::move(sampleDir);
        }
        m_requestGen.fetch_add(1, std::memory_order_acq_rel);
        m_cv.notify_one();
    }

    // Call every audio block; installs a newly completed load if one is pending
    // (cheap no-op otherwise: one atomic load and a comparison).
    void pollAndInstall() noexcept
    {
        const uint64_t doneGen = m_doneGen.load(std::memory_order_acquire);
        if (doneGen == m_installedGen)
        {
            return;
        }
        {
            std::lock_guard lock(m_resultMutex);
            m_installedPieces = m_result;
        }
        m_installedGen = doneGen;
    }

    [[nodiscard]] bool isReady() const noexcept
    {
        return !m_installedPieces.empty();
    }

    // The next round-robin take for code, or nullptr if code has no loaded
    // pieces. Audio-thread only: mutates that code's own round-robin cursor.
    [[nodiscard]] std::shared_ptr<std::vector<float>> nextTake(const std::string_view code) noexcept
    {
        const auto it = m_installedPieces.find(std::string(code));
        if (it == m_installedPieces.end() || it->second.takes.empty())
        {
            return nullptr;
        }
        auto& piece = it->second;
        const auto take = piece.takes[piece.nextIndex];
        piece.nextIndex = (piece.nextIndex + 1) % piece.takes.size();
        return take;
    }

  private:
    // One code's round-robin takes plus its own playback cursor - audio thread
    // only, so a plain index (not atomic) is enough.
    struct Piece
    {
        std::vector<std::shared_ptr<std::vector<float>>> takes;
        size_t nextIndex{0};
    };
    using Pieces = std::unordered_map<std::string, Piece>;

    struct RoundRobinName
    {
        std::string code;
        unsigned index;
    };

    // "<code>_<n>.wav" -> code/n, or nullopt if filename doesn't match that shape.
    // Mirrors GrooveKit::parseRoundRobinName() - same on-disk naming convention.
    [[nodiscard]] static std::optional<RoundRobinName> parseRoundRobinName(const std::string& filename)
    {
        constexpr std::string_view kSuffix = ".wav";
        if (filename.size() <= kSuffix.size() || !filename.ends_with(kSuffix))
        {
            return std::nullopt;
        }
        const std::string stem = filename.substr(0, filename.size() - kSuffix.size());
        const auto underscore = stem.rfind('_');
        if (underscore == std::string::npos || underscore == 0 || underscore + 1 >= stem.size())
        {
            return std::nullopt;
        }
        const std::string digits = stem.substr(underscore + 1);
        const bool allDigits =
            std::ranges::all_of(digits, [](const unsigned char c) noexcept { return std::isdigit(c) != 0; });
        if (!allDigits)
        {
            return std::nullopt;
        }
        return RoundRobinName{stem.substr(0, underscore), static_cast<unsigned>(std::stoul(digits))};
    }

    // Background thread only. Scans sampleDir for "<code>_<n>.wav" files and
    // loads each code's takes (in ascending n) as interleaved stereo buffers.
    [[nodiscard]] static Pieces loadPieces(const std::string& sampleDir)
    {
        std::unordered_map<std::string, std::vector<std::pair<unsigned, std::filesystem::path>>> byCode;
        if (!sampleDir.empty() && std::filesystem::exists(sampleDir))
        {
            for (const auto& entry : std::filesystem::directory_iterator(sampleDir))
            {
                if (const auto parsed = parseRoundRobinName(entry.path().filename().string()))
                {
                    byCode[parsed->code].emplace_back(parsed->index, entry.path());
                }
            }
        }
        Pieces pieces;
        for (auto& [code, indexed] : byCode)
        {
            std::ranges::sort(indexed, {}, &std::pair<unsigned, std::filesystem::path>::first);
            Piece piece;
            for (const auto& [index, path] : indexed)
            {
                const auto [left, right] = AudioUtility::LoadWav::loadStereoFromFile(path.string());
                if (left.empty())
                {
                    continue;
                }
                auto interleaved = std::make_shared<std::vector<float>>(left.size() * 2);
                for (size_t i = 0; i < left.size(); ++i)
                {
                    (*interleaved)[i * 2] = left[i];
                    (*interleaved)[i * 2 + 1] = right[i];
                }
                piece.takes.push_back(std::move(interleaved));
            }
            if (!piece.takes.empty())
            {
                pieces.emplace(code, std::move(piece));
            }
        }
        return pieces;
    }

    // Background thread only.
    void runLoad(const uint64_t gen)
    {
        std::string sampleDir;
        {
            std::lock_guard lock(m_requestMutex);
            sampleDir = m_requestedSampleDir;
        }
        auto pieces = loadPieces(sampleDir);
        {
            std::lock_guard lock(m_resultMutex);
            m_result = std::move(pieces);
        }
        m_doneGen.store(gen, std::memory_order_release);
    }

    std::mutex m_waitMutex;
    std::condition_variable_any m_cv;
    std::atomic<uint64_t> m_requestGen{0};
    std::atomic<uint64_t> m_doneGen{0};
    uint64_t m_installedGen{0}; // audio thread only

    std::mutex m_requestMutex;
    std::string m_requestedSampleDir;

    std::mutex m_resultMutex;
    Pieces m_result;

    // Audio thread only.
    Pieces m_installedPieces;

    // Declared last so it is destroyed first - see ~DrumVoiceKit()'s own doc comment.
    std::jthread m_thread;
};

}
