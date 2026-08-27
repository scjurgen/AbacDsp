#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <cctype>
#include <concepts>
#include <condition_variable>
#include <cstdint>
#include <filesystem>
#include <format>
#include <fstream>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <vector>

#include "AudioFile/LoadWav.h"

#include "Sampler/GrooveDrumPlayer.h"
#include "Sampler/GrooveHumanize.h"
#include "Sampler/GrooveMidiFile.h"
#include "Sampler/GrooveNoteMap.h"
#include "Sampler/SliceLibrary.h"

namespace AbacDsp
{

/// @ingroup sampler
/// @brief Groove info derived from a groove's sidecar `.json` (bar count computed
/// from its beat count + time signature; the rest is read as-is). Default-empty
/// when the sidecar is missing or unparseable.
struct GrooveMetadata
{
    unsigned bars{0};
    std::string feel;
    std::string timeSignature;
    float idealBpm{0.f};
    std::vector<std::string> dominantSounds;
};

/// @ingroup sampler
/// @brief Opt-in request to also pre-render a short playback-ready burst
/// (see GrooveKit::runLoad()). sampleRate == 0 (the default) means "no burst."
struct BurstConfig
{
    float sampleRate{0.f};
    float bpm{120.f};
};

/// @ingroup sampler
/// @brief The two fields GrooveKit reads from a groove's sidecar `.json`, nested
/// to mirror the file's own "rhythm" object (idealBpm/variation/etc. are unused).
struct GrooveSidecarRhythm
{
    std::string feel;
    std::string timeSignature;
};

/// @ingroup sampler
/// @brief The shape of a groove's sidecar `.json` that GrooveKit actually reads.
struct GrooveSidecar
{
    float idealBpm{0.f};
    GrooveSidecarRhythm rhythm;
    std::vector<std::string> dominantSounds;
};

/// @ingroup sampler
/// @brief The shape nlohmann::json satisfies, taken as a template parameter so this
/// header needs no JSON dependency of its own (mirrors Sampler/LoopFile.h).
template <typename Json>
concept GrooveJsonLike = requires(const std::string& text, Json j) {
    { Json::parse(text) } -> std::same_as<Json>;
    { j.template get<GrooveSidecar>() } -> std::same_as<GrooveSidecar>;
};

/**
 * @ingroup sampler
 * @brief Loads a round-robin drum sample kit + one groove MIDI file off the audio
 * thread, resolving each note to a loaded piece via GrooveNoteMap's tag fallback
 * (closest available articulation, not the exact one, when the kit lacks it).
 *
 * requestLoad() is safe to call repeatedly at runtime (switching grooves): the
 * sample kit itself only reloads when sampleDir actually changes, and a completed
 * load is installed by the audio thread's own pollAndInstall(), never by the
 * background thread reaching into state the audio thread might be reading mid-block.
 *
 * Json is the caller-supplied JSON type (see GrooveJsonLike), used only to read a
 * groove's sidecar metadata file.
 */
template <GrooveJsonLike Json>
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
    // own recorded name). A later call while one is in flight simply supersedes it.
    // burst is opt-in (see BurstConfig) - default omitted, no burst rendered.
    void requestLoad(std::string sampleDir, std::string midiDrumsRootDir, std::string relativeGrooveName,
                     BurstConfig burst = {})
    {
        Request request;
        request.sampleDir = std::move(sampleDir);
        request.midiDrumsRootDir = std::move(midiDrumsRootDir);
        request.relativeGrooveName = std::move(relativeGrooveName);
        request.burst = burst;
        submitRequest(std::move(request));
    }

    // Queues a load by style + 0-based variation index (a Groove-menu click, or the
    // Variation dial - which may fire from the audio thread, so the filename
    // resolution happens on the background thread, never here or in the caller).
    void requestLoadStyle(std::string sampleDir, std::string midiDrumsRootDir, std::string styleName,
                          const unsigned variationIndex, BurstConfig burst = {})
    {
        Request request;
        request.sampleDir = std::move(sampleDir);
        request.midiDrumsRootDir = std::move(midiDrumsRootDir);
        request.styleName = std::move(styleName);
        request.variationIndex = variationIndex;
        request.burst = burst;
        submitRequest(std::move(request));
    }

    // Queues a push/life-only change (see GrooveHumanize.h): reuses the
    // already-analyzed notes of whichever groove is currently loaded rather
    // than re-parsing the MIDI file. Reload-triggered, like requestLoad().
    void requestHumanizeChange(const float push, const float life)
    {
        Request request;
        request.isHumanizeOnly = true;
        request.push = push;
        request.life = life;
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
        m_installedMetadata = result.metadata;
        m_installedDiagnosticsCsv = std::move(result.diagnosticsCsv);
        m_installedBurstAudio = std::move(result.burstAudio);
        m_installedBurstTickPos = result.burstTickPos;
        m_installedBurstNextTriggerIndex = result.burstNextTriggerIndex;
        m_installedTagToTrack = result.tagToTrack;
        m_installedGen = doneGen;
    }

    [[nodiscard]] bool isReady() const noexcept
    {
        return m_installedLibrary != nullptr;
    }

    [[nodiscard]] const SliceLibrary* library() const noexcept
    {
        return m_installedLibrary.get();
    }

    [[nodiscard]] const GrooveProgram* program() const noexcept
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

    // Resolves an instrument tag to its track in the installed kit; nullopt if
    // that tag isn't present.
    [[nodiscard]] std::optional<size_t> trackForTag(const GrooveTag tag) const noexcept
    {
        return m_installedTagToTrack[static_cast<size_t>(tag)];
    }

    // Bars/rhythm/instruments for the most recently installed groove - see
    // GrooveMetadata. Default-empty until a groove has actually loaded.
    [[nodiscard]] const GrooveMetadata& installedMetadata() const noexcept
    {
        return m_installedMetadata;
    }

    // Per-note push/life diagnostics for the installed groove (see
    // GrooveHumanize.h's toCsv()) - empty until a groove has loaded.
    [[nodiscard]] const std::string& installedDiagnosticsCsv() const noexcept
    {
        return m_installedDiagnosticsCsv;
    }

    // Pre-rendered playback-ready burst for the installed groove (see
    // BurstConfig); null if no burst was requested for that load.
    [[nodiscard]] std::shared_ptr<const std::vector<float>> installedBurstAudio() const noexcept
    {
        return m_installedBurstAudio;
    }

    [[nodiscard]] double installedBurstTickPos() const noexcept
    {
        return m_installedBurstTickPos;
    }

    [[nodiscard]] size_t installedBurstNextTriggerIndex() const noexcept
    {
        return m_installedBurstNextTriggerIndex;
    }

    // "<groove name>: N bars, <feel> feel, <time signature> - <instruments>" for
    // the status bar. Pure/static so it's testable without a real load.
    [[nodiscard]] static std::string formatGrooveInfoText(const std::string& grooveName, const GrooveMetadata& metadata)
    {
        std::string text = grooveName + ": " + std::to_string(metadata.bars) + " bars";
        if (!metadata.feel.empty())
        {
            text += ", " + metadata.feel + " feel";
        }
        if (!metadata.timeSignature.empty())
        {
            text += ", " + metadata.timeSignature;
        }
        if (metadata.idealBpm > 0.f)
        {
            text += std::format(", {:.0f} BPM", metadata.idealBpm);
        }
        if (!metadata.dominantSounds.empty())
        {
            text += " - ";
            for (size_t i = 0; i < metadata.dominantSounds.size(); ++i)
            {
                text += (i == 0 ? "" : ", ") + metadata.dominantSounds[i];
            }
        }
        return text;
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
    static constexpr float kBurstSeconds = 1.f;

    // One drum piece's round-robin takes, concatenated into one interleaved buffer
    // ready for SliceLibrary::extractTrack() (each take becomes one slice in it).
    struct PieceAudio
    {
        std::vector<float> interleaved;
        std::vector<Slice> slices;
    };

    using TagToTrack = std::array<std::optional<size_t>, static_cast<size_t>(GrooveTag::Count)>;

    struct LoadResult
    {
        std::shared_ptr<const SliceLibrary> library;
        std::shared_ptr<const GrooveProgram> program;
        std::string grooveName;
        std::shared_ptr<const std::vector<std::string>> trackNames;
        GrooveMetadata metadata;
        std::string diagnosticsCsv;
        std::shared_ptr<const std::vector<float>> burstAudio;
        double burstTickPos{0.0};
        size_t burstNextTriggerIndex{0};
        TagToTrack tagToTrack{};
    };

    // A groove's analyzed notes, sidecar metadata and tick geometry, built
    // together on a cache miss and reused on a cache hit - kit-independent,
    // so it stays valid across both a sample-kit reload and a push/life-only
    // change, and only needs rebuilding when the source MIDI file changes.
    struct AnalyzedGroove
    {
        std::vector<GrooveAnalysisNote> notes;
        uint16_t ticksPerQuarterNote{480};
        uint32_t loopLengthTicks{480};
        GrooveMetadata metadata;
    };

    static constexpr GridResolution kGridResolution = GridResolution::Sixteenth;

    // Exactly one of relativeGrooveName or styleName is set, depending on which
    // requestXxx() was called; runLoad() branches on that to decide whether it
    // needs to resolve a style+index into a filename itself. isHumanizeOnly
    // requests (see requestHumanizeChange()) ignore every other field and
    // reuse whichever groove is currently loaded.
    struct Request
    {
        std::string sampleDir;
        std::string midiDrumsRootDir;
        std::string relativeGrooveName;
        std::string styleName;
        unsigned variationIndex{0};
        BurstConfig burst;
        bool isHumanizeOnly{false};
        float push{0.f};
        float life{1.f};
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

    // Background thread only. Reloads the sample kit only when sampleDir changed;
    // a push/life-only request skips both the kit reload and the MIDI re-parse,
    // reusing the currently loaded groove.
    void runLoad(const uint64_t gen)
    {
        Request request;
        {
            std::lock_guard lock(m_requestMutex);
            request = m_request;
        }
        std::string midiDrumsRootDir = m_loadedMidiDrumsRootDir;
        std::string grooveName = m_currentGrooveName;
        BurstConfig burst;

        if (request.isHumanizeOnly)
        {
            m_push = request.push;
            m_life = request.life;
        }
        else
        {
            const std::string& sampleDir = request.sampleDir;
            midiDrumsRootDir = request.midiDrumsRootDir;
            grooveName = request.styleName.empty()
                             ? request.relativeGrooveName
                             : resolveGrooveName(midiDrumsRootDir, request.styleName, request.variationIndex)
                                   .value_or(std::string());
            burst = request.burst;
            if (sampleDir != m_loadedSampleDir)
            {
                reloadKit(sampleDir);
                m_loadedSampleDir = sampleDir;
            }
            m_loadedMidiDrumsRootDir = midiDrumsRootDir;
        }

        AnalyzedGroove analyzed;
        if (const auto it = m_analyzedCache.find(grooveName); it != m_analyzedCache.end())
        {
            analyzed = it->second;
        }
        else
        {
            const auto midiPath = (std::filesystem::path(midiDrumsRootDir) / grooveName).string();
            analyzed = analyzeGrooveFile(midiPath);
            m_analyzedCache.emplace(grooveName, analyzed);
        }

        const auto humanized =
            applyHumanize(analyzed.notes, analyzed.ticksPerQuarterNote, kGridResolution, m_push, m_life);
        auto program = buildGrooveProgram(analyzed, humanized, m_tagToTrack);

        std::shared_ptr<const std::vector<float>> burstAudio;
        double burstTickPos = 0.0;
        size_t burstNextTriggerIndex = 0;
        if (burst.sampleRate > 0.f && m_currentLibrary && program)
        {
            const auto burstFrames = static_cast<size_t>(burst.sampleRate * kBurstSeconds);
            auto renderedBurst =
                GrooveDrumPlayer::renderBurst(burst.sampleRate, burst.bpm, *m_currentLibrary, *program, burstFrames);
            burstAudio = std::make_shared<const std::vector<float>>(std::move(renderedBurst.audio));
            burstTickPos = renderedBurst.tickPos;
            burstNextTriggerIndex = renderedBurst.nextTriggerIndex;
        }

        {
            std::lock_guard lock(m_resultMutex);
            m_result.library = m_currentLibrary;
            m_result.program = std::move(program);
            m_result.grooveName = grooveName;
            m_result.trackNames = m_currentTrackNames;
            m_result.metadata = analyzed.metadata;
            m_result.diagnosticsCsv = toCsv(analyzed.notes, humanized);
            m_result.burstAudio = std::move(burstAudio);
            m_result.burstTickPos = burstTickPos;
            m_result.burstNextTriggerIndex = burstNextTriggerIndex;
            m_result.tagToTrack = m_tagToTrack;
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
            totalFrames += audio.interleaved.size() / SliceLibrary::kChannels;
        }
        auto library = std::make_shared<SliceLibrary>(std::max<size_t>(1, totalFrames));
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
            const size_t startFrame = audio.interleaved.size() / SliceLibrary::kChannels;
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
        static constexpr std::array<std::pair<GrooveTag, std::string_view>, 42> kClassification{{
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
            {GrooveTag::ClickLow, "clicklow"},
            {GrooveTag::ClickHigh, "clickhigh"},
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

    // loopLengthTicks is the file's own declared End of Track tick, not a guess
    // from note positions - a downbeat-only trailing note gives no signal of
    // intended silence. Falls back to nearest beat past the last note if no EOT.
    [[nodiscard]] static AnalyzedGroove analyzeGrooveFile(const std::string& midiFile)
    {
        AnalyzedGroove analyzed;
        GrooveMidiFile midi;
        if (midiFile.empty() || !midi.readFromFile(midiFile) || midi.noteEvents().empty())
        {
            analyzed.ticksPerQuarterNote = midi.ticksPerQuarterNote();
            analyzed.loopLengthTicks = analyzed.ticksPerQuarterNote;
            return analyzed;
        }
        const uint16_t ticksPerQuarterNote = midi.ticksPerQuarterNote();
        analyzed.ticksPerQuarterNote = ticksPerQuarterNote;
        if (const uint32_t eot = midi.endOfTrackTick(); eot > 0)
        {
            analyzed.loopLengthTicks = eot;
        }
        else
        {
            const uint32_t maxTick = midi.noteEvents().back().tick;
            const uint32_t totalBeats = maxTick / ticksPerQuarterNote + 1;
            analyzed.loopLengthTicks = totalBeats * static_cast<uint32_t>(ticksPerQuarterNote);
        }
        analyzed.notes = analyzeNotes(midi.noteEvents(), ticksPerQuarterNote, midi.timeSignatures(), kGridResolution);
        analyzed.metadata = readGrooveMetadata(midiFile, analyzed.loopLengthTicks, ticksPerQuarterNote);
        return analyzed;
    }

    // Triggers land at each humanized note's output tick; kept sorted since
    // push can reorder notes that started out adjacent.
    [[nodiscard]] static std::shared_ptr<GrooveProgram> buildGrooveProgram(const AnalyzedGroove& analyzed,
                                                                           const std::vector<HumanizedNote>& humanized,
                                                                           const TagToTrack& tagToTrack)
    {
        auto program = std::make_shared<GrooveProgram>();
        program->ticksPerQuarterNote = analyzed.ticksPerQuarterNote;
        program->loopLengthTicks = analyzed.loopLengthTicks;
        for (size_t i = 0; i < analyzed.notes.size(); ++i)
        {
            const auto track = resolveTrack(analyzed.notes[i].note, tagToTrack);
            if (!track.has_value())
            {
                continue;
            }
            // Cubic velocity curve: low hits read as noticeably quieter than a linear
            // mapping would give, matching how velocity is generally perceived.
            const float velocityNorm = static_cast<float>(humanized[i].velocity) / 127.f;
            program->triggers.push_back({humanized[i].tick, *track, velocityNorm * velocityNorm * velocityNorm});
        }
        std::ranges::stable_sort(program->triggers, {}, &GrooveTrigger::tick);
        return program;
    }

    // Reads midiFile's sidecar "<stem>.json" (idealBpm/rhythm/dominantSounds,
    // see MidiDrums's own file layout) and derives bars from the beat count.
    // Missing/unparseable sidecar just leaves metadata default-empty.
    [[nodiscard]] static GrooveMetadata readGrooveMetadata(const std::string& midiFile, const uint32_t loopLengthTicks,
                                                           const uint16_t ticksPerQuarterNote)
    {
        GrooveMetadata metadata;
        std::ifstream file(std::filesystem::path(midiFile).replace_extension(".json"));
        if (!file.is_open())
        {
            return metadata;
        }
        std::ostringstream text;
        text << file.rdbuf();
        GrooveSidecar sidecar;
        try
        {
            sidecar = Json::parse(text.str()).template get<GrooveSidecar>();
        }
        catch (const std::exception&)
        {
            return metadata;
        }
        metadata.feel = sidecar.rhythm.feel;
        metadata.timeSignature = sidecar.rhythm.timeSignature;
        metadata.idealBpm = sidecar.idealBpm;
        metadata.dominantSounds = sidecar.dominantSounds;
        const uint32_t totalBeats = ticksPerQuarterNote > 0 ? loopLengthTicks / ticksPerQuarterNote : 0;
        metadata.bars = totalBeats / parseBeatsPerBar(metadata.timeSignature);
        return metadata;
    }

    // "N/M" -> N; 4 if timeSignature doesn't parse (most groove time signatures
    // in practice, and a safe divisor default either way).
    [[nodiscard]] static unsigned parseBeatsPerBar(const std::string& timeSignature)
    {
        const auto slash = timeSignature.find('/');
        if (slash == std::string::npos)
        {
            return 4;
        }
        try
        {
            return static_cast<unsigned>(std::stoul(timeSignature.substr(0, slash)));
        }
        catch (const std::exception&)
        {
            return 4;
        }
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
    std::string m_loadedMidiDrumsRootDir;
    std::shared_ptr<const SliceLibrary> m_currentLibrary;
    std::shared_ptr<const std::vector<std::string>> m_currentTrackNames;
    std::unordered_map<std::string, AnalyzedGroove> m_analyzedCache;
    TagToTrack m_tagToTrack{};
    std::string m_currentGrooveName; // see currentGrooveName()'s own doc comment
    float m_push{0.f};
    float m_life{1.f};

    // Audio thread only.
    std::shared_ptr<const SliceLibrary> m_installedLibrary;
    std::shared_ptr<const GrooveProgram> m_installedProgram;
    std::string m_installedGrooveName;
    std::shared_ptr<const std::vector<std::string>> m_installedTrackNames;
    GrooveMetadata m_installedMetadata;
    std::string m_installedDiagnosticsCsv;
    std::shared_ptr<const std::vector<float>> m_installedBurstAudio;
    double m_installedBurstTickPos{0.0};
    size_t m_installedBurstNextTriggerIndex{0};
    TagToTrack m_installedTagToTrack{};

    // Declared last so it is destroyed first: join() must complete (the
    // background thread fully stopped) before any member above it might still
    // be touching is destroyed. See ~GrooveKit()'s own doc comment.
    std::jthread m_thread;
};

}
