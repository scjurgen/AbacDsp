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
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

#include "AudioFile/SaveWav.h"

#include "Generators/BeatSequencer.h"
#include "Generators/MeterTimeline.h"
#include "Sampler/LoopFile.h"
#include "Sampler/LoopPartBank.h"
#include "Sampler/MidiFile.h"
#include "Sampler/SequencePattern.h"
#include "Sampler/SliceLibrary.h"

// Named loop save/load ("Loops" menu): background-threaded WAV+JSON(+MIDI)
// encode/decode of <loopsDirectory>/<name>.{wav,json,mid}(+_trackN.wav), and
// the generation-counter handshake for both directions. Requires the
// AbacDsp::LoopMetadata / AbacDsp::SequencePattern nlohmann ADL hooks (see
// LooperImpl.h) to already be declared in the translation unit.
//
// This service only decodes/encodes; installing a completed load's payload
// into the live audio-thread state (recorder, timekeeper, spectrogram,
// sequencer pattern) is the caller's job -- see pollLoadCompletion() and
// LooperImpl::checkLoopLoadCompletion().
template <size_t BlockSize>
class LoopStorageService
{
  public:
    struct LoopLoadOutcome
    {
        bool attempted{false};
        bool success{false};
        bool hasConflict{false};
        float wavBpm{0.f};
        float jsonBpm{0.f};
        std::string patchParamsJson; // empty if the loop predates this field or had none
    };

    // One frozen track's audio (interleaved) plus its slices, already laid
    // out contiguously so extractTrack() reconstructs it with no re-slicing.
    struct LoopLoadTrackData
    {
        std::vector<float> interleaved;
        std::vector<AbacDsp::Slice> slices;
    };

    // One part's decoded audio; hasContent() false means this part wasn't in
    // the saved loop (or wasn't listed in its "parts" manifest) at all.
    struct LoopLoadPartData
    {
        std::vector<float> left;
        std::vector<float> right;
        float bpm{120.f}; // this part's own tempo, not the session-wide resolvedBpm below
        AbacDsp::MeterTimeline meterTimeline;
        bool hasOverdub{false};
        std::vector<float> overdubLeft;
        std::vector<float> overdubRight;

        [[nodiscard]] bool hasContent() const noexcept
        {
            return !left.empty();
        }
    };

    // Decoded, ready-to-install payload handed back once pollLoadCompletion()
    // confirms a load. parts[0] is always Part A; parts[1..3] are populated
    // only for the suffixes the save's "parts" manifest actually listed.
    struct LoopLoadResult
    {
        std::array<LoopLoadPartData, AbacDsp::kMaxLoopParts> parts;
        float resolvedBpm{120.f};
        bool hasSequencerData{false};
        std::vector<LoopLoadTrackData> tracks;
        std::optional<AbacDsp::SequencePattern> pattern;
    };

    LoopStorageService(const AbacDsp::LoopPartBank<BlockSize>& bank, const AbacDsp::BeatSequencer& seq,
                       const AbacDsp::SliceLibrary& sliceLibrary, const AbacDsp::SequencePattern& pattern,
                       const std::array<AbacDsp::MeterTimeline, AbacDsp::kMaxLoopParts>& meterTimelines,
                       const std::array<float, AbacDsp::kMaxLoopParts>& appliedBpm, const bool& eighthNoteUnit,
                       const float sampleRate)
        : m_bank(bank)
        , m_seq(seq)
        , m_sliceLibrary(sliceLibrary)
        , m_pattern(pattern)
        , m_meterTimelines(meterTimelines)
        , m_appliedBpm(appliedBpm)
        , m_eighthNoteUnit(eighthNoteUnit)
        , m_sampleRate(sampleRate)
    {
        m_loopSaveThread = std::jthread(
            [this](const std::stop_token& stopToken)
            {
                uint64_t lastHandled = 0;
                while (!stopToken.stop_requested())
                {
                    std::unique_lock lock(m_loopSaveWaitMutex);
                    m_loopSaveCv.wait(lock, stopToken, [this, lastHandled]
                                      { return m_loopSaveRequestGen.load(std::memory_order_acquire) != lastHandled; });
                    if (stopToken.stop_requested())
                    {
                        return;
                    }
                    lastHandled = m_loopSaveRequestGen.load(std::memory_order_acquire);
                    runSaveLoopAs(lastHandled);
                }
            });

        m_loopLoadThread = std::jthread(
            [this](const std::stop_token& stopToken)
            {
                uint64_t lastHandled = 0;
                while (!stopToken.stop_requested())
                {
                    std::unique_lock lock(m_loopLoadWaitMutex);
                    m_loopLoadCv.wait(lock, stopToken, [this, lastHandled]
                                      { return m_loopLoadRequestGen.load(std::memory_order_acquire) != lastHandled; });
                    if (stopToken.stop_requested())
                    {
                        return;
                    }
                    lastHandled = m_loopLoadRequestGen.load(std::memory_order_acquire);
                    runLoadLoop(lastHandled);
                }
            });
    }

    // Directory named loop saves/loads live in; set once during setup.
    void setLoopsDirectory(std::string dir)
    {
        m_loopsDirectory = std::move(dir);
    }

    // A "/" in a loop name (e.g. "drums/verse groove") denotes a subfolder,
    // mirroring FileIo's named-patch convention; scans recursively to find them.
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
            if (entry.path().extension() == ".wav")
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
        const bool removedWav = std::filesystem::remove(partWavPath(name, 0), ec);
        std::filesystem::remove(partJsonPath(name, 0), ec);
        for (size_t p = 1; p < AbacDsp::kMaxLoopParts; ++p)
        {
            std::filesystem::remove(partWavPath(name, p), ec);
            std::filesystem::remove(partJsonPath(name, p), ec);
            std::filesystem::remove(partMidPath(name, p), ec);
            std::filesystem::remove(partOverdubWavPath(name, p), ec);
        }
        if (removedWav)
        {
            std::lock_guard<std::mutex> lock(m_currentLoopMutex);
            if (name == m_currentLoopName)
            {
                m_currentLoopName.clear();
            }
        }
        return removedWav;
    }

    bool renameLoopNamed(const std::string& oldName, const std::string& newName)
    {
        if (sanitizeLoopName(oldName).empty() || sanitizeLoopName(newName).empty() || oldName == newName)
        {
            return false;
        }
        std::error_code ec;
        std::filesystem::create_directories(partWavPath(newName, 0).parent_path(), ec);
        std::filesystem::rename(partWavPath(oldName, 0), partWavPath(newName, 0), ec);
        if (ec)
        {
            return false;
        }
        std::filesystem::rename(partJsonPath(oldName, 0), partJsonPath(newName, 0), ec);
        for (size_t p = 1; p < AbacDsp::kMaxLoopParts; ++p)
        {
            std::error_code partEc;
            std::filesystem::rename(partWavPath(oldName, p), partWavPath(newName, p), partEc);
            std::filesystem::rename(partJsonPath(oldName, p), partJsonPath(newName, p), partEc);
        }
        {
            std::lock_guard<std::mutex> lock(m_currentLoopMutex);
            if (oldName == m_currentLoopName)
            {
                m_currentLoopName = newName;
            }
        }
        return true; // sidecar rename failing isn't fatal; the wav already moved
    }

    // Message thread only (Editor's Loops menu ticks the currently loaded entry).
    [[nodiscard]] std::string currentLoopName() const
    {
        std::lock_guard<std::mutex> lock(m_currentLoopMutex);
        return m_currentLoopName;
    }

    // Guarded by the caller (recording/overdub/empty-loop checks); writes
    // <loopsDirectory>/<name>.wav + .json on the save worker (file I/O isn't RT-safe).
    // patchParamsJson is an opaque snapshot (caller's patch/parameter state) embedded
    // in the sidecar so a loop reload can restore the settings it was captured with.
    void requestSave(const std::string& name, const std::string& patchParamsJson = {})
    {
        if (m_loopSavePending || m_loopsDirectory.empty() || sanitizeLoopName(name).empty())
        {
            return;
        }
        m_loopSaveName = name;
        m_loopSaveParamsJson = patchParamsJson;
        m_loopSaveRequestedGen = m_loopSaveRequestGen.load(std::memory_order_relaxed) + 1;
        m_loopSavePending = true;
        m_loopSaveRequestGen.store(m_loopSaveRequestedGen, std::memory_order_release);
        m_loopSaveCv.notify_one();
    }

    [[nodiscard]] bool isSavePending() const noexcept
    {
        return m_loopSavePending;
    }

    [[nodiscard]] std::string consumeLastSavedLoopName()
    {
        std::lock_guard<std::mutex> lock(m_lastSavedLoopMutex);
        return std::exchange(m_lastSavedLoopName, std::string{});
    }

    // Audio thread, polled every block: clears the pending flag once the
    // worker's done-generation catches up.
    void checkSaveCompletion() noexcept
    {
        if (!m_loopSavePending || m_loopSaveDoneGen.load(std::memory_order_acquire) != m_loopSaveRequestedGen)
        {
            return;
        }
        m_loopSavePending = false;
    }

    // Guarded by the caller; reads <loopsDirectory>/<name>.wav + .json on the load worker.
    void requestLoad(const std::string& name)
    {
        if (m_loopLoadPending || m_loopsDirectory.empty() || sanitizeLoopName(name).empty())
        {
            return;
        }
        m_loopLoadName = name;
        m_loopLoadRequestedGen = m_loopLoadRequestGen.load(std::memory_order_relaxed) + 1;
        m_loopLoadPending = true;
        m_loopLoadRequestGen.store(m_loopLoadRequestedGen, std::memory_order_release);
        m_loopLoadCv.notify_one();
    }

    [[nodiscard]] bool isLoadPending() const noexcept
    {
        return m_loopLoadPending;
    }

    [[nodiscard]] LoopLoadOutcome consumeLoadOutcome()
    {
        std::lock_guard<std::mutex> lock(m_loopLoadOutcomeMutex);
        return std::exchange(m_loopLoadOutcome, LoopLoadOutcome{});
    }

    // Picks which BPM becomes the installed loop's tempo after a conflict was
    // reported; a no-op if no load is currently awaiting resolution.
    void resolveLoadBpm(const float bpm)
    {
        if (!m_loopLoadPending || !m_loopLoadNeedsResolve.load(std::memory_order_acquire))
        {
            return;
        }
        m_loopLoadNeedsResolve.store(false, std::memory_order_relaxed);
        m_loopLoadResolvedBpm = bpm;
        m_loopLoadConfirmedGen.store(m_loopLoadRequestedGen, std::memory_order_release);
    }

    // Message thread only: feeds an externally-decoded payload (see
    // LooperImpl::restoreExtraState()) into the same gen-counter handshake
    // requestLoad() uses on success, so the same pollLoadCompletion() poll
    // installs it. A no-op while a named load is already pending.
    // Only ever installs into Part A; host-state restore stays single-part
    // (out of scope here, same as the frozen tracks/sequencer pattern above).
    void injectRestoredLoad(std::vector<float> left, std::vector<float> right, const float resolvedBpm,
                            AbacDsp::MeterTimeline meterTimeline, std::vector<float> overdubLeft = {},
                            std::vector<float> overdubRight = {})
    {
        if (m_loopLoadPending)
        {
            return;
        }
        for (auto& part : m_loopLoadParts)
        {
            part = LoopLoadPartData{};
        }
        m_loopLoadParts[0].left = std::move(left);
        m_loopLoadParts[0].right = std::move(right);
        m_loopLoadParts[0].meterTimeline = std::move(meterTimeline);
        m_loopLoadParts[0].hasOverdub = !overdubLeft.empty() && !overdubRight.empty();
        m_loopLoadParts[0].overdubLeft = std::move(overdubLeft);
        m_loopLoadParts[0].overdubRight = std::move(overdubRight);
        m_loopLoadResolvedBpm = resolvedBpm;
        m_loopLoadHasSequencerData = false;
        m_loopLoadTracks.clear();
        m_loopLoadPattern.reset();

        m_loopLoadRequestedGen = m_loopLoadRequestGen.load(std::memory_order_relaxed) + 1;
        m_loopLoadPending = true;
        m_loopLoadNeedsResolve.store(false, std::memory_order_relaxed);
        m_loopLoadRequestGen.store(m_loopLoadRequestedGen, std::memory_order_release);
        m_loopLoadDoneGen.store(m_loopLoadRequestedGen, std::memory_order_release);
        m_loopLoadConfirmedGen.store(m_loopLoadRequestedGen, std::memory_order_release);
    }

    // Audio thread, polled every block; returns the decoded payload once
    // confirmed (immediately when there was no BPM conflict, or once
    // resolveLoadBpm() was called), for the caller to install.
    [[nodiscard]] std::optional<LoopLoadResult> pollLoadCompletion()
    {
        if (!m_loopLoadPending || m_loopLoadConfirmedGen.load(std::memory_order_acquire) != m_loopLoadRequestedGen)
        {
            return std::nullopt;
        }
        m_loopLoadPending = false;
        LoopLoadResult result;
        result.parts = std::move(m_loopLoadParts);
        result.resolvedBpm = m_loopLoadResolvedBpm;
        result.hasSequencerData = m_loopLoadHasSequencerData;
        result.tracks = std::move(m_loopLoadTracks);
        result.pattern = std::move(m_loopLoadPattern);
        return result;
    }

  private:
    // Named patches sanitize with juce::String; this is JUCE-free (Impl stays
    // library-agnostic), so filesystem-unsafe characters are stripped by hand.
    // A "/" denotes a subfolder (each segment sanitized on its own); segments
    // that go empty after trimming (leading/trailing/doubled slashes) are dropped.
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

    // <loopsDirectory>/<folder segments>/<leaf>, without an extension; the
    // leaf's parent directories are created on demand for the writing paths.
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

    // "" for Part A (index 0, unsuffixed for backward compatibility with
    // loops saved before multi-part support), "_partB"/"_partC"/"_partD"
    // otherwise.
    [[nodiscard]] static std::string partSuffix(const size_t index)
    {
        return index == 0 ? std::string{} : std::string("_part") + static_cast<char>('A' + index);
    }

    [[nodiscard]] std::filesystem::path partWavPath(const std::string& name, const size_t index,
                                                    const bool createDirs = false) const
    {
        auto path = loopBasePath(name, createDirs);
        path += partSuffix(index);
        path += ".wav";
        return path;
    }

    [[nodiscard]] std::filesystem::path partJsonPath(const std::string& name, const size_t index) const
    {
        auto path = loopBasePath(name);
        path += partSuffix(index);
        path += ".json";
        return path;
    }

    [[nodiscard]] std::filesystem::path partMidPath(const std::string& name, const size_t index) const
    {
        auto path = loopBasePath(name);
        path += partSuffix(index);
        path += ".mid";
        return path;
    }

    [[nodiscard]] std::filesystem::path partOverdubWavPath(const std::string& name, const size_t index) const
    {
        auto path = loopBasePath(name);
        path += partSuffix(index);
        path += "_overdub.wav";
        return path;
    }

    [[nodiscard]] std::filesystem::path loopTrackWavPath(const std::string& name, const size_t track) const
    {
        auto path = loopBasePath(name);
        path += "_track" + std::to_string(track) + ".wav";
        return path;
    }

    // MIDI ticks spanned by one bar of the given meter (denominator convention:
    // eighthUnit halves the quarter-note tick length, matching MidiFile's own
    // numerator/denominatorPower time-signature event fields).
    [[nodiscard]] static constexpr uint32_t midiTicksPerBar(const size_t beatsPerBar, const bool eighthUnit) noexcept
    {
        const uint32_t ticksPerBeat =
            eighthUnit ? AbacDsp::MidiFile::kTicksPerQuarterNote / 2 : AbacDsp::MidiFile::kTicksPerQuarterNote;
        return ticksPerBeat * static_cast<uint32_t>(beatsPerBar);
    }

    // Worker thread only: replays a track's slices via the public per-slice
    // accessor, in order, rather than reaching into the pool directly.
    void extractTrackAudioAndLengths(const size_t track, std::vector<float>& left, std::vector<float>& right,
                                     std::vector<size_t>& sliceLengths) const
    {
        const size_t count = m_sliceLibrary.sliceCountInTrack(track);
        sliceLengths.reserve(count);
        for (size_t i = 0; i < count; ++i)
        {
            const auto& info = m_sliceLibrary.sliceInfo(track, i);
            sliceLengths.push_back(info.lengthFrames);
            for (size_t f = 0; f < info.lengthFrames; ++f)
            {
                left.push_back(m_sliceLibrary.sample(track, i, f, 0));
                right.push_back(m_sliceLibrary.sample(track, i, f, 1));
            }
        }
    }

    // Worker thread only: writes <name><suffix>.wav (+ iXML metadata) and
    // <name><suffix>.json for one populated part; index 0 (Part A) also
    // carries patchParams/pattern/tracks/the "parts" manifest.
    void saveOnePart(const size_t index, const std::vector<std::string>& partSuffixList)
    {
        const size_t loopLen = m_bank.part(index).loopLengthFrames();
        std::vector<float> left(loopLen);
        std::vector<float> right(loopLen);
        for (size_t f = 0; f < loopLen; ++f)
        {
            left[f] = m_bank.part(index).sample(f, 0);
            right[f] = m_bank.part(index).sample(f, 1);
        }
        // Descriptive metadata only (the pattern's own serialized beatsPerBar is
        // authoritative on load); approximate using the loop's current meter,
        // which may not be exact for a take whose meter changed mid-recording.
        const float partBpm = m_appliedBpm[index];
        const float samplesPerBeat = m_sampleRate * 60.f / partBpm;
        const float beats = (samplesPerBeat > 0.f) ? static_cast<float>(loopLen) / samplesPerBeat : 0.f;
        const float bars = beats / static_cast<float>(m_seq.beatsPerBar());
        const AbacDsp::LoopMetadata meta{1, partBpm, bars, beats};
        AbacDsp::LoopFile<nlohmann::json>::saveStereoWav(partWavPath(m_loopSaveName, index, true).string(), left, right,
                                                         m_sampleRate, meta);

        // Standard MIDI File sidecar carrying this part's own tempo + meter
        // timeline; written unconditionally (a constant-meter take just gets
        // a single time-signature event) so loading only ever needs one path.
        AbacDsp::MidiFile midi;
        midi.setTempoBpm(partBpm);
        const auto& timeline = m_meterTimelines[index];
        const std::vector<AbacDsp::MeterSegment> segmentsToWrite =
            timeline.empty() ? std::vector<AbacDsp::MeterSegment>{{0, m_seq.beatsPerBar(), m_eighthNoteUnit}}
                             : timeline.segments();
        uint32_t midiTick = 0;
        for (size_t i = 0; i < segmentsToWrite.size(); ++i)
        {
            const auto& seg = segmentsToWrite[i];
            if (i > 0)
            {
                const auto& prevSeg = segmentsToWrite[i - 1];
                midiTick += static_cast<uint32_t>(seg.startBar - prevSeg.startBar) *
                            midiTicksPerBar(prevSeg.beatsPerBar, prevSeg.eighthUnit);
            }
            midi.addTimeSignature(midiTick, static_cast<uint8_t>(seg.beatsPerBar), seg.eighthUnit ? 3 : 2);
        }
        if (!midi.writeToFile(partMidPath(m_loopSaveName, index).string()))
        {
            std::cerr << "LoopStorageService: failed to write " << partMidPath(m_loopSaveName, index) << std::endl;
        }

        nlohmann::json j = meta;
        if (index == 0)
        {
            if (!m_loopSaveParamsJson.empty())
            {
                try
                {
                    j["patchParams"] = nlohmann::json::parse(m_loopSaveParamsJson);
                }
                catch (const nlohmann::json::exception& e)
                {
                    std::cerr << "LoopStorageService: failed to embed patch params: " << e.what() << std::endl;
                }
            }
            if (m_sliceLibrary.trackCount() > 0)
            {
                j["pattern"] = m_pattern;
                auto tracksJson = nlohmann::json::array();
                for (size_t t = 0; t < m_sliceLibrary.trackCount(); ++t)
                {
                    std::vector<float> trackLeft;
                    std::vector<float> trackRight;
                    std::vector<size_t> sliceLengths;
                    extractTrackAudioAndLengths(t, trackLeft, trackRight, sliceLengths);
                    const auto trackPath = loopTrackWavPath(m_loopSaveName, t).string();
                    AudioUtility::SaveWav::saveStereoAs(trackPath, trackLeft, trackRight, m_sampleRate);
                    // Relative to m_loopsDirectory, not just the basename,
                    // so a subfoldered loop name keeps its subfolder here too.
                    tracksJson.push_back({{"file", std::filesystem::path(trackPath).filename().string()},
                                          {"sliceLengths", sliceLengths}});
                }
                j["tracks"] = tracksJson;
            }
            if (!partSuffixList.empty())
            {
                j["parts"] = partSuffixList;
            }
        }
        if (m_bank.part(index).hasOverdub())
        {
            std::vector<float> overdubLeft(loopLen);
            std::vector<float> overdubRight(loopLen);
            for (size_t f = 0; f < loopLen; ++f)
            {
                overdubLeft[f] = m_bank.part(index).overdubSample(f, 0);
                overdubRight[f] = m_bank.part(index).overdubSample(f, 1);
            }
            const auto overdubPath = partOverdubWavPath(m_loopSaveName, index).string();
            AudioUtility::SaveWav::saveStereoAs(overdubPath, overdubLeft, overdubRight, m_sampleRate);
            j["overdub"] = {{"file", std::filesystem::path(overdubPath).filename().string()}};
        }
        std::ofstream jsonOut(partJsonPath(m_loopSaveName, index));
        if (jsonOut)
        {
            jsonOut << j.dump(2);
        }
    }

    // Worker thread only: saves every populated part (B/C/D only when they
    // actually have content), then a "parts" manifest in Part A's own json.
    void runSaveLoopAs(const uint64_t gen)
    {
        std::vector<std::string> partSuffixList;
        for (size_t p = 1; p < AbacDsp::kMaxLoopParts; ++p)
        {
            if (m_bank.part(p).loopLengthFrames() > 0)
            {
                partSuffixList.push_back(std::string(1, static_cast<char>('A' + p)));
            }
        }
        bool savedAny = false;
        for (size_t p = 0; p < AbacDsp::kMaxLoopParts; ++p)
        {
            if (m_bank.part(p).loopLengthFrames() == 0)
            {
                continue;
            }
            savedAny = true;
            saveOnePart(p, partSuffixList);
        }
        if (savedAny)
        {
            std::lock_guard<std::mutex> lock(m_lastSavedLoopMutex);
            m_lastSavedLoopName = m_loopSaveName;
        }
        {
            std::lock_guard<std::mutex> lock(m_currentLoopMutex);
            m_currentLoopName = m_loopSaveName;
        }
        m_loopSaveDoneGen.store(gen, std::memory_order_release);
    }

    // Worker thread only, resolves file references against loopDir. Builds
    // into locals first (throws on malformed data, caught by runLoadLoop())
    // so a partial failure leaves no scratch state.
    void loadSequencerData(const nlohmann::json& j, const std::filesystem::path& loopDir)
    {
        auto pattern = j.at("pattern").get<AbacDsp::SequencePattern>();
        std::vector<LoopLoadTrackData> tracks;
        for (const auto& trackJson : j.at("tracks"))
        {
            const auto trackPath = loopDir / trackJson.at("file").get<std::string>();
            const auto trackLoaded = AbacDsp::LoopFile<nlohmann::json>::loadStereoWav(trackPath.string());
            const auto sliceLengths = trackJson.at("sliceLengths").get<std::vector<size_t>>();

            LoopLoadTrackData data;
            data.interleaved.resize(trackLoaded.left.size() * 2);
            for (size_t f = 0; f < trackLoaded.left.size(); ++f)
            {
                data.interleaved[f * 2] = trackLoaded.left[f];
                data.interleaved[f * 2 + 1] = trackLoaded.right[f];
            }
            size_t offset = 0;
            for (const auto len : sliceLengths)
            {
                data.slices.push_back({offset, len});
                offset += len;
            }
            tracks.push_back(std::move(data));
        }
        m_loopLoadPattern = std::move(pattern);
        m_loopLoadTracks = std::move(tracks);
        m_loopLoadHasSequencerData = true;
    }

    // Worker thread only: reconstructs one part's meter timeline from its own
    // .mid sidecar, if present (absent just leaves an empty timeline).
    void loadPartMeter(const size_t index, AbacDsp::MeterTimeline& meterTimeline) const
    {
        AbacDsp::MidiFile midi;
        if (!midi.readFromFile(partMidPath(m_loopLoadName, index).string()))
        {
            return;
        }
        size_t bar = 0;
        uint32_t prevTick = 0;
        const auto& events = midi.timeSignatures();
        for (size_t i = 0; i < events.size(); ++i)
        {
            const auto& ev = events[i];
            if (i > 0)
            {
                const auto& prevEv = events[i - 1];
                const uint32_t ticksPerBarPrev = midiTicksPerBar(prevEv.numerator, prevEv.denominatorPower == 3);
                bar += (ticksPerBarPrev > 0) ? (ev.tick - prevTick) / ticksPerBarPrev : 0;
            }
            meterTimeline.addSegment(bar, ev.numerator, ev.denominatorPower == 3);
            prevTick = ev.tick;
        }
    }

    // Worker thread only: an "overdub" json entry resolves against loopDir,
    // shared by Part A and every other loaded part.
    void loadPartOverdub(const nlohmann::json& j, const std::filesystem::path& loopDir, LoopLoadPartData& part) const
    {
        const auto overdubFile = j.at("overdub").at("file").get<std::string>();
        const auto overdubLoaded = AbacDsp::LoopFile<nlohmann::json>::loadStereoWav((loopDir / overdubFile).string());
        if (!overdubLoaded.left.empty())
        {
            part.overdubLeft = overdubLoaded.left;
            part.overdubRight = overdubLoaded.right;
            part.hasOverdub = true;
        }
    }

    // Worker thread only: loads one of the "parts" manifest's extra parts
    // (B/C/D) - audio, its own overdub/meter, no session-level fields.
    void loadOneExtraPart(const size_t index, const std::filesystem::path& loopDir)
    {
        const auto loaded =
            AbacDsp::LoopFile<nlohmann::json>::loadStereoWav(partWavPath(m_loopLoadName, index).string());
        if (loaded.left.empty())
        {
            return;
        }
        m_loopLoadParts[index].left = loaded.left;
        m_loopLoadParts[index].right = loaded.right;
        m_loopLoadParts[index].bpm = loaded.embeddedMetadata ? loaded.embeddedMetadata->bpm : 120.f;
        std::ifstream jsonIn(partJsonPath(m_loopLoadName, index));
        if (jsonIn)
        {
            try
            {
                nlohmann::json j;
                jsonIn >> j;
                m_loopLoadParts[index].bpm = j.get<AbacDsp::LoopMetadata>().bpm;
                if (j.contains("overdub"))
                {
                    loadPartOverdub(j, loopDir, m_loopLoadParts[index]);
                }
            }
            catch (const std::exception& e)
            {
                std::cerr << "LoopStorageService: failed to parse " << partJsonPath(m_loopLoadName, index) << ": "
                          << e.what() << std::endl;
            }
        }
        loadPartMeter(index, m_loopLoadParts[index].meterTimeline);
    }

    // Worker thread only: decodes <name>.wav + .json (Part A) and compares
    // their BPM belief (iXML-embedded vs sidecar) rather than picking one
    // silently, then loads whichever B/C/D parts the "parts" manifest lists.
    void runLoadLoop(const uint64_t gen)
    {
        LoopLoadOutcome outcome;
        outcome.attempted = true;
        m_loopLoadHasSequencerData = false;
        m_loopLoadTracks.clear();
        m_loopLoadPattern.reset();
        for (auto& part : m_loopLoadParts)
        {
            part = LoopLoadPartData{};
        }
        const auto loaded = AbacDsp::LoopFile<nlohmann::json>::loadStereoWav(partWavPath(m_loopLoadName, 0).string());
        if (!loaded.left.empty())
        {
            std::optional<AbacDsp::LoopMetadata> sidecarMeta;
            std::vector<std::string> partSuffixList;
            const auto loopDir = loopBasePath(m_loopLoadName).parent_path();
            std::ifstream jsonIn(partJsonPath(m_loopLoadName, 0));
            if (jsonIn)
            {
                try
                {
                    nlohmann::json j;
                    jsonIn >> j;
                    sidecarMeta = j.get<AbacDsp::LoopMetadata>();
                    if (j.contains("patchParams"))
                    {
                        outcome.patchParamsJson = j.at("patchParams").dump();
                    }
                    if (j.contains("pattern") && j.contains("tracks"))
                    {
                        loadSequencerData(j, loopDir);
                    }
                    if (j.contains("overdub"))
                    {
                        loadPartOverdub(j, loopDir, m_loopLoadParts[0]);
                    }
                    if (j.contains("parts"))
                    {
                        partSuffixList = j.at("parts").get<std::vector<std::string>>();
                    }
                }
                catch (const std::exception& e)
                {
                    std::cerr << "LoopStorageService: failed to parse " << partJsonPath(m_loopLoadName, 0) << ": "
                              << e.what() << std::endl;
                }
            }

            loadPartMeter(0, m_loopLoadParts[0].meterTimeline);
            m_loopLoadParts[0].left = loaded.left;
            m_loopLoadParts[0].right = loaded.right;
            outcome.success = true;
            {
                std::lock_guard<std::mutex> lock(m_currentLoopMutex);
                m_currentLoopName = m_loopLoadName;
            }

            for (const auto& letter : partSuffixList)
            {
                if (letter.size() != 1 || letter[0] < 'B' ||
                    letter[0] >= static_cast<char>('A' + AbacDsp::kMaxLoopParts))
                {
                    continue;
                }
                loadOneExtraPart(static_cast<size_t>(letter[0] - 'A'), loopDir);
            }

            float resolvedBpm = 120.f;
            bool needsResolve = false;
            if (sidecarMeta && loaded.embeddedMetadata)
            {
                if (std::abs(sidecarMeta->bpm - loaded.embeddedMetadata->bpm) > 0.01f)
                {
                    outcome.hasConflict = true;
                    outcome.wavBpm = loaded.embeddedMetadata->bpm;
                    outcome.jsonBpm = sidecarMeta->bpm;
                    needsResolve = true;
                }
                else
                {
                    resolvedBpm = sidecarMeta->bpm;
                }
            }
            else if (sidecarMeta)
            {
                resolvedBpm = sidecarMeta->bpm;
            }
            else if (loaded.embeddedMetadata)
            {
                resolvedBpm = loaded.embeddedMetadata->bpm;
            }
            m_loopLoadResolvedBpm = resolvedBpm;
            m_loopLoadNeedsResolve.store(needsResolve, std::memory_order_release);
        }

        {
            std::lock_guard<std::mutex> lock(m_loopLoadOutcomeMutex);
            m_loopLoadOutcome = outcome;
        }
        m_loopLoadDoneGen.store(gen, std::memory_order_release);
        if (outcome.success && !outcome.hasConflict)
        {
            m_loopLoadConfirmedGen.store(gen, std::memory_order_release);
        }
    }

    const AbacDsp::LoopPartBank<BlockSize>& m_bank;
    const AbacDsp::BeatSequencer& m_seq;
    const AbacDsp::SliceLibrary& m_sliceLibrary;
    const AbacDsp::SequencePattern& m_pattern;
    const std::array<AbacDsp::MeterTimeline, AbacDsp::kMaxLoopParts>& m_meterTimelines;
    const std::array<float, AbacDsp::kMaxLoopParts>& m_appliedBpm;
    const bool& m_eighthNoteUnit;
    float m_sampleRate;

    std::string m_loopsDirectory; // set once via setLoopsDirectory(), before any save/load
    std::string m_loopSaveName;
    std::string m_loopSaveParamsJson;
    bool m_loopSavePending{false};
    uint64_t m_loopSaveRequestedGen{0};
    std::atomic<uint64_t> m_loopSaveRequestGen{0};
    std::atomic<uint64_t> m_loopSaveDoneGen{0};
    std::mutex m_loopSaveWaitMutex;
    std::condition_variable_any m_loopSaveCv;
    std::jthread m_loopSaveThread;
    std::mutex m_lastSavedLoopMutex;
    std::string m_lastSavedLoopName;
    mutable std::mutex m_currentLoopMutex;
    std::string m_currentLoopName;

    std::string m_loopLoadName;
    bool m_loopLoadPending{false};
    uint64_t m_loopLoadRequestedGen{0};
    std::atomic<uint64_t> m_loopLoadRequestGen{0};
    std::atomic<uint64_t> m_loopLoadDoneGen{0};      // worker finished decode + conflict check
    std::atomic<uint64_t> m_loopLoadConfirmedGen{0}; // safe for the audio thread to install
    std::mutex m_loopLoadWaitMutex;
    std::condition_variable_any m_loopLoadCv;
    std::jthread m_loopLoadThread;
    std::array<LoopLoadPartData, AbacDsp::kMaxLoopParts>
        m_loopLoadParts; // worker-owned, audio thread reads once confirmed
    float m_loopLoadResolvedBpm{120.f};
    std::atomic<bool> m_loopLoadNeedsResolve{false};
    std::mutex m_loopLoadOutcomeMutex;
    LoopLoadOutcome m_loopLoadOutcome;
    std::vector<LoopLoadTrackData> m_loopLoadTracks; // worker-owned scratch, same handoff as parts above
    std::optional<AbacDsp::SequencePattern> m_loopLoadPattern;
    bool m_loopLoadHasSequencerData{false};
};
