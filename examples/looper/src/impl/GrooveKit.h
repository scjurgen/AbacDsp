#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <cctype>
#include <condition_variable>
#include <cstdint>
#include <filesystem>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <vector>

#include "AudioFile/LoadWav.h"

#include "GrooveNoteMap.h"
#include "Sampler/GrooveDrumPlayer.h"
#include "Sampler/GrooveMidiFile.h"
#include "Sampler/SliceLibrary.h"

// Loads a round-robin drum sample kit + one groove MIDI file off the audio thread,
// resolving each note to a loaded piece via GrooveNoteMap's tag fallback (closest
// available articulation, not the exact one, when the kit lacks it). requestLoad() is
// safe to call repeatedly at runtime (switching grooves): the sample kit itself only
// reloads when sampleDir actually changes, and a completed load is installed by the
// audio thread's own pollAndInstall(), never by the background thread reaching into
// state the audio thread might be reading mid-block.
class GrooveKit
{
  public:
    GrooveKit()
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

    // A lost-wakeup race has been observed between request_stop() and
    // condition_variable_any's stop-token wait (stop_requested() true, wait still
    // blocked) - request and notify explicitly before m_thread's own destructor.
    ~GrooveKit()
    {
        m_thread.request_stop();
        m_cv.notify_all();
    }

    // Queues a load by its exact relative name (default groove, or a saved loop's
    // own recorded name). A later call while one is in flight simply supersedes it
    // (same coalescing as LoopStorageService's own request/generation handshake).
    void requestLoad(std::string sampleDir, std::string midiDrumsRootDir, std::string relativeGrooveName)
    {
        Request request;
        request.sampleDir = std::move(sampleDir);
        request.midiDrumsRootDir = std::move(midiDrumsRootDir);
        request.relativeGrooveName = std::move(relativeGrooveName);
        submitRequest(std::move(request));
    }

    // Queues a load by style + 0-based variation index (a Groove-menu click, or the
    // Variation dial - which may fire from the audio thread, so the filename
    // resolution happens on the background thread, never here or in the caller).
    void requestLoadStyle(std::string sampleDir, std::string midiDrumsRootDir, std::string styleName,
                          const unsigned variationIndex)
    {
        Request request;
        request.sampleDir = std::move(sampleDir);
        request.midiDrumsRootDir = std::move(midiDrumsRootDir);
        request.styleName = std::move(styleName);
        request.variationIndex = variationIndex;
        submitRequest(std::move(request));
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
        LoadResult result;
        {
            std::lock_guard lock(m_resultMutex);
            result = m_result;
        }
        m_installedLibrary = std::move(result.library);
        m_installedProgram = std::move(result.program);
        m_installedGrooveName = std::move(result.grooveName);
        m_installedTrackNames = std::move(result.trackNames);
        m_installedGen = doneGen;
    }

    [[nodiscard]] bool isReady() const noexcept
    {
        return m_installedLibrary != nullptr;
    }

    [[nodiscard]] const AbacDsp::SliceLibrary* library() const noexcept
    {
        return m_installedLibrary.get();
    }

    [[nodiscard]] const AbacDsp::GrooveProgram* program() const noexcept
    {
        return m_installedProgram.get();
    }

    // Audio-thread-safe counterpart to currentGrooveName(): the name of the
    // groove pollAndInstall() most recently installed, not the message thread's
    // most-recently-finished-loading view.
    [[nodiscard]] const std::string& installedGrooveName() const noexcept
    {
        return m_installedGrooveName;
    }

    // Per-track display names ("bd", "sd", "hh", ...) matching library()'s tracks.
    [[nodiscard]] std::span<const std::string> installedTrackNames() const noexcept
    {
        return m_installedTrackNames ? std::span<const std::string>(*m_installedTrackNames)
                                     : std::span<const std::string>{};
    }

    // Relative-to-MidiDrums-root name of the groove most recently finished loading
    // (not yet necessarily installed - see pollAndInstall()). Message-thread only,
    // like LoopStorageService::currentLoopName() - no extra sync beyond that split.
    [[nodiscard]] std::string currentGrooveName() const
    {
        return m_currentGrooveName;
    }

    // "<Genre>/<style>" for every groove under midiDrumsRootDir, deduplicated across
    // variations and sorted. For the Groove menu.
    [[nodiscard]] static std::vector<std::string> listAvailableGrooves(const std::string& midiDrumsRootDir)
    {
        std::vector<std::string> names;
        for (const auto& [style, variations] : scanMidiDrums(midiDrumsRootDir))
        {
            names.push_back(style);
        }
        return names;
    }

    // Number of distinct variations available for styleName (0 if the style is
    // unknown), for clamping the Variation control's effective range.
    [[nodiscard]] static unsigned countVariations(const std::string& midiDrumsRootDir, const std::string& styleName)
    {
        const auto styles = scanMidiDrums(midiDrumsRootDir);
        const auto it = styles.find(styleName);
        return it == styles.end() ? 0u : static_cast<unsigned>(it->second.size());
    }

    // styleName's variationIndex-th take (0-based, sorted by the file's own
    // variation number), as a path relative to midiDrumsRootDir - ready to pass
    // straight to requestLoad(). nullopt if styleName or that index doesn't exist.
    [[nodiscard]] static std::optional<std::string> resolveGrooveName(const std::string& midiDrumsRootDir,
                                                                      const std::string& styleName,
                                                                      const unsigned variationIndex)
    {
        const auto styles = scanMidiDrums(midiDrumsRootDir);
        const auto it = styles.find(styleName);
        if (it == styles.end() || variationIndex >= it->second.size())
        {
            return std::nullopt;
        }
        return it->second[variationIndex].second;
    }

  private:
    // One drum piece's round-robin takes, concatenated into one interleaved buffer
    // ready for SliceLibrary::extractTrack() (each take becomes one slice in it).
    struct PieceAudio
    {
        std::vector<float> interleaved;
        std::vector<AbacDsp::Slice> slices;
    };

    using TagToTrack = std::array<std::optional<size_t>, static_cast<size_t>(GrooveTag::Count)>;

    struct LoadResult
    {
        std::shared_ptr<const AbacDsp::SliceLibrary> library;
        std::shared_ptr<const AbacDsp::GrooveProgram> program;
        std::string grooveName;
        std::shared_ptr<const std::vector<std::string>> trackNames;
    };

    // Exactly one of relativeGrooveName or styleName is set, depending on which
    // requestXxx() was called; runLoad() branches on that to decide whether it
    // needs to resolve a style+index into a filename itself.
    struct Request
    {
        std::string sampleDir;
        std::string midiDrumsRootDir;
        std::string relativeGrooveName;
        std::string styleName;
        unsigned variationIndex{0};
    };

    void submitRequest(Request request)
    {
        {
            std::lock_guard lock(m_requestMutex);
            m_request = std::move(request);
        }
        m_requestGen.fetch_add(1, std::memory_order_acq_rel);
        m_cv.notify_one();
    }

    // Background thread only. Reloads the sample kit only when sampleDir actually
    // changed since the last call; a groove-only switch just rebuilds the pattern
    // against the already-loaded kit's classification.
    void runLoad(const uint64_t gen)
    {
        Request request;
        {
            std::lock_guard lock(m_requestMutex);
            request = m_request;
        }
        const std::string& sampleDir = request.sampleDir;
        const std::string& midiDrumsRootDir = request.midiDrumsRootDir;
        const std::string grooveName =
            request.styleName.empty() ? request.relativeGrooveName
                                      : resolveGrooveName(midiDrumsRootDir, request.styleName, request.variationIndex)
                                            .value_or(std::string());
        if (sampleDir != m_loadedSampleDir)
        {
            reloadKit(sampleDir);
            m_loadedSampleDir = sampleDir;
            m_programCache.clear(); // cached programs' track indices belong to the old kit
        }
        std::shared_ptr<const AbacDsp::GrooveProgram> program;
        if (const auto it = m_programCache.find(grooveName); it != m_programCache.end())
        {
            program = it->second;
        }
        else
        {
            const auto midiPath = (std::filesystem::path(midiDrumsRootDir) / grooveName).string();
            program = buildGrooveProgram(midiPath, m_tagToTrack);
            m_programCache.emplace(grooveName, program);
        }

        {
            std::lock_guard lock(m_resultMutex);
            m_result.library = m_currentLibrary;
            m_result.program = std::move(program);
            m_result.grooveName = grooveName;
            m_result.trackNames = m_currentTrackNames;
        }
        m_currentGrooveName = grooveName;
        m_doneGen.store(gen, std::memory_order_release);
    }

    // Background thread only.
    void reloadKit(const std::string& sampleDir)
    {
        const auto pieces = loadPieces(sampleDir);
        std::unordered_map<std::string, size_t> codeToTrack;
        size_t totalFrames = 0;
        for (const auto& [code, audio] : pieces)
        {
            totalFrames += audio.interleaved.size() / AbacDsp::SliceLibrary::kChannels;
        }
        auto library = std::make_shared<AbacDsp::SliceLibrary>(std::max<size_t>(1, totalFrames));
        std::vector<std::string> trackNames(pieces.size());
        for (const auto& [code, audio] : pieces)
        {
            const size_t track = library->extractTrack(audio.interleaved, audio.slices);
            codeToTrack[code] = track;
            trackNames[track] = code;
        }
        m_tagToTrack = classifyReggaeKit(codeToTrack);
        m_currentLibrary = std::move(library);
        m_currentTrackNames = std::make_shared<const std::vector<std::string>>(std::move(trackNames));
    }

    // Scans sampleDir for "<code>_<n>.wav" files and groups each code's round-robin
    // takes (in ascending n) into one PieceAudio.
    [[nodiscard]] static std::unordered_map<std::string, PieceAudio> loadPieces(const std::string& sampleDir)
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
        std::unordered_map<std::string, PieceAudio> pieces;
        for (auto& [code, indexed] : byCode)
        {
            std::ranges::sort(indexed, {}, &std::pair<unsigned, std::filesystem::path>::first);
            if (auto audio = concatenateTakes(indexed); !audio.slices.empty())
            {
                pieces.emplace(code, std::move(audio));
            }
        }
        return pieces;
    }

    struct RoundRobinName
    {
        std::string code;
        unsigned index;
    };

    // "<code>_<n>.wav" -> code/n, or nullopt if filename doesn't match that shape.
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

    [[nodiscard]] static PieceAudio concatenateTakes(
        const std::vector<std::pair<unsigned, std::filesystem::path>>& indexed)
    {
        PieceAudio audio;
        for (const auto& [index, path] : indexed)
        {
            const auto [left, right] = AudioUtility::LoadWav::loadStereoFromFile(path.string());
            if (left.empty())
            {
                continue;
            }
            const size_t startFrame = audio.interleaved.size() / AbacDsp::SliceLibrary::kChannels;
            audio.interleaved.reserve(audio.interleaved.size() + left.size() * 2);
            for (size_t i = 0; i < left.size(); ++i)
            {
                audio.interleaved.push_back(left[i]);
                audio.interleaved.push_back(right[i]);
            }
            audio.slices.push_back({startFrame, left.size()});
        }
        return audio;
    }

    // Per-kit classification: which loaded reggae-kit code belongs to which tag.
    // Hand-authored for this one kit; a second kit added later needs its own table.
    [[nodiscard]] static TagToTrack classifyReggaeKit(const std::unordered_map<std::string, size_t>& codeToTrack)
    {
        static constexpr std::array<std::pair<GrooveTag, std::string_view>, 40> kClassification{{
            // Umbrella tags: every note whose specific tag has no loaded piece
            // eventually falls back to one of these (see GrooveNoteMap.h).
            {GrooveTag::Hihat, "hh"},
            {GrooveTag::Cymbal, "crash"},
            {GrooveTag::Tom, "tom1"},
            {GrooveTag::Timbale, "timb1"},
            {GrooveTag::Kick, "bd"},
            {GrooveTag::Rimshot, "rs"},
            {GrooveTag::Snare, "sd"},
            {GrooveTag::SnareRoll, "sdroll"},
            {GrooveTag::SnareAlt, "sd2"},
            {GrooveTag::TomLow, "tomlo"},
            {GrooveTag::HihatClosed, "hh"},
            {GrooveTag::Tom1, "tom1"},
            {GrooveTag::HihatStep, "hhstep"},
            {GrooveTag::Tom2, "tom2"},
            {GrooveTag::HihatHalfOpen, "hhhalf"},
            {GrooveTag::Tom3, "tom3"},
            {GrooveTag::TomLeft, "tomlt"},
            {GrooveTag::HihatOpen, "hhopen"},
            {GrooveTag::CrashStopped, "crstop"},
            {GrooveTag::Ride, "ride"},
            {GrooveTag::CrashLong, "crlong"},
            {GrooveTag::RideBell, "ridebell"},
            {GrooveTag::CrashStopped, "crstop2"},
            {GrooveTag::Crash, "crash"},
            {GrooveTag::Woodblock, "wood"},
            {GrooveTag::China, "china"},
            {GrooveTag::China, "chinasht"},
            {GrooveTag::CrashLong, "crlong2"},
            {GrooveTag::Ride, "ride2"},
            {GrooveTag::HihatStopped, "hhstop"},
            {GrooveTag::HihatStep, "hhstep2"},
            {GrooveTag::HihatSoftStep, "hhsoft"},
            {GrooveTag::HihatStep, "hhstep3"},
            {GrooveTag::HihatGhost, "hhghost"},
            {GrooveTag::Timbale1, "timb1"},
            {GrooveTag::Timbale2, "timb2"},
            {GrooveTag::Timbale3, "timb3"},
            {GrooveTag::TimbaleDamped, "timbdmp"},
            {GrooveTag::Timbale4, "timb4"},
            {GrooveTag::Sidestick, "sstick"},
        }};
        TagToTrack tagToTrack{};
        for (const auto& [tag, code] : kClassification)
        {
            auto& slot = tagToTrack[static_cast<size_t>(tag)];
            if (slot.has_value())
            {
                continue; // first code claiming a tag wins
            }
            if (const auto it = codeToTrack.find(std::string(code)); it != codeToTrack.end())
            {
                slot = it->second;
            }
        }
        return tagToTrack;
    }

    // Triggers land at their exact source-file tick, so nothing here quantizes
    // or pads a loop. loopLengthTicks is the fewest whole beats containing
    // every note - a groove need not span a whole number of bars.
    [[nodiscard]] static std::shared_ptr<AbacDsp::GrooveProgram> buildGrooveProgram(const std::string& midiFile,
                                                                                    const TagToTrack& tagToTrack)
    {
        auto program = std::make_shared<AbacDsp::GrooveProgram>();
        AbacDsp::GrooveMidiFile midi;
        if (midiFile.empty() || !midi.readFromFile(midiFile) || midi.noteEvents().empty())
        {
            program->ticksPerQuarterNote = midi.ticksPerQuarterNote();
            program->loopLengthTicks = program->ticksPerQuarterNote;
            return program;
        }
        const uint16_t ticksPerQuarterNote = midi.ticksPerQuarterNote();
        const uint32_t maxTick = midi.noteEvents().back().tick;
        const uint32_t lastBeatIndex = maxTick / ticksPerQuarterNote;
        const uint32_t totalBeats = lastBeatIndex + 1;

        program->ticksPerQuarterNote = ticksPerQuarterNote;
        program->loopLengthTicks = totalBeats * static_cast<uint32_t>(ticksPerQuarterNote);
        for (const auto& note : midi.noteEvents())
        {
            const auto track = resolveTrack(note.note, tagToTrack);
            if (!track.has_value())
            {
                continue;
            }
            // Cubic velocity curve: low hits read as noticeably quieter than a linear
            // mapping would give, matching how velocity is generally perceived.
            const float velocityNorm = static_cast<float>(note.velocity) / 127.f;
            program->triggers.push_back({note.tick, *track, velocityNorm * velocityNorm * velocityNorm});
        }
        return program;
    }

    // Walks the note's tags most-specific-first, returning the first one some
    // loaded kit piece was classified under.
    [[nodiscard]] static std::optional<size_t> resolveTrack(const uint8_t note, const TagToTrack& tagToTrack)
    {
        for (const GrooveTag tag : tagsForGrooveNote(note))
        {
            if (tag == GrooveTag::None)
            {
                continue;
            }
            if (const auto& slot = tagToTrack[static_cast<size_t>(tag)]; slot.has_value())
            {
                return slot;
            }
        }
        return std::nullopt;
    }

    // "<genre>/<style>_v<n>.mid" -> {"<genre>/<style>", n}; nullopt if the filename
    // doesn't end in the expected "_v<digits>" suffix (see MidiDrums/README.md).
    [[nodiscard]] static std::optional<std::pair<std::string, unsigned>> splitStyleAndVariation(
        const std::string& genreSlashStem)
    {
        const auto vPos = genreSlashStem.rfind("_v");
        if (vPos == std::string::npos || vPos + 2 >= genreSlashStem.size())
        {
            return std::nullopt;
        }
        const std::string digits = genreSlashStem.substr(vPos + 2);
        const bool allDigits =
            std::ranges::all_of(digits, [](const unsigned char c) noexcept { return std::isdigit(c) != 0; });
        if (!allDigits)
        {
            return std::nullopt;
        }
        return std::make_pair(genreSlashStem.substr(0, vPos), static_cast<unsigned>(std::stoul(digits)));
    }

    // "<style>" -> its variations, {variation number, path relative to
    // midiDrumsRootDir}, sorted by number. Metadata-only scan, cheap enough to
    // call synchronously from whichever thread needs it rather than caching.
    [[nodiscard]] static std::map<std::string, std::vector<std::pair<unsigned, std::string>>> scanMidiDrums(
        const std::string& midiDrumsRootDir)
    {
        std::map<std::string, std::vector<std::pair<unsigned, std::string>>> styles;
        if (midiDrumsRootDir.empty() || !std::filesystem::exists(midiDrumsRootDir))
        {
            return styles;
        }
        const std::filesystem::path root(midiDrumsRootDir);
        for (const auto& entry : std::filesystem::recursive_directory_iterator(root))
        {
            if (!entry.is_regular_file() || entry.path().extension() != ".mid")
            {
                continue;
            }
            const auto relative = entry.path().lexically_relative(root);
            const auto genreSlashStem = (relative.parent_path() / relative.stem()).generic_string();
            if (const auto parsed = splitStyleAndVariation(genreSlashStem))
            {
                styles[parsed->first].emplace_back(parsed->second, relative.generic_string());
            }
        }
        for (auto& [style, variations] : styles)
        {
            std::ranges::sort(variations, {}, &std::pair<unsigned, std::string>::first);
        }
        return styles;
    }

    std::mutex m_waitMutex;
    std::condition_variable_any m_cv;
    std::atomic<uint64_t> m_requestGen{0};
    std::atomic<uint64_t> m_doneGen{0};
    uint64_t m_installedGen{0}; // audio thread only

    std::mutex m_requestMutex;
    Request m_request;

    std::mutex m_resultMutex;
    LoadResult m_result;

    // Background thread only.
    std::string m_loadedSampleDir;
    std::shared_ptr<const AbacDsp::SliceLibrary> m_currentLibrary;
    std::shared_ptr<const std::vector<std::string>> m_currentTrackNames;
    std::unordered_map<std::string, std::shared_ptr<const AbacDsp::GrooveProgram>> m_programCache;
    TagToTrack m_tagToTrack{};
    std::string m_currentGrooveName; // see currentGrooveName()'s own doc comment

    // Audio thread only.
    std::shared_ptr<const AbacDsp::SliceLibrary> m_installedLibrary;
    std::shared_ptr<const AbacDsp::GrooveProgram> m_installedProgram;
    std::string m_installedGrooveName;
    std::shared_ptr<const std::vector<std::string>> m_installedTrackNames;

    // Declared last so it is destroyed first: join() must complete (the
    // background thread fully stopped) before any member above it might still
    // be touching is destroyed. See ~GrooveKit()'s own doc comment.
    std::jthread m_thread;
};
