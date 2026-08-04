#pragma once

#include <algorithm>
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
#include "Sampler/LoopRecorder.h"
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

    // Decoded, ready-to-install payload handed back once pollLoadCompletion()
    // confirms a load.
    struct LoopLoadResult
    {
        std::vector<float> left;
        std::vector<float> right;
        float resolvedBpm{120.f};
        AbacDsp::MeterTimeline meterTimeline;
        bool hasSequencerData{false};
        std::vector<LoopLoadTrackData> tracks;
        std::optional<AbacDsp::SequencePattern> pattern;
        bool hasOverdub{false};
        std::vector<float> overdubLeft;
        std::vector<float> overdubRight;
    };

    LoopStorageService(const AbacDsp::LoopRecorder<BlockSize>& recorder, const AbacDsp::BeatSequencer& seq,
                       const AbacDsp::SliceLibrary& sliceLibrary, const AbacDsp::SequencePattern& pattern,
                       const AbacDsp::MeterTimeline& meterTimeline, const float& appliedBpm, const bool& eighthNoteUnit,
                       const float sampleRate)
        : m_recorder(recorder)
        , m_seq(seq)
        , m_sliceLibrary(sliceLibrary)
        , m_pattern(pattern)
        , m_meterTimeline(meterTimeline)
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

    [[nodiscard]] std::vector<std::string> listLoopNames() const
    {
        std::vector<std::string> names;
        if (m_loopsDirectory.empty() || !std::filesystem::exists(m_loopsDirectory))
        {
            return names;
        }
        for (const auto& entry : std::filesystem::directory_iterator(m_loopsDirectory))
        {
            if (entry.path().extension() == ".wav")
            {
                names.push_back(entry.path().stem().string());
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
        const bool removedWav = std::filesystem::remove(loopWavPath(name), ec);
        std::filesystem::remove(loopJsonPath(name), ec);
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
        std::filesystem::rename(loopWavPath(oldName), loopWavPath(newName), ec);
        if (ec)
        {
            return false;
        }
        std::filesystem::rename(loopJsonPath(oldName), loopJsonPath(newName), ec);
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
    void injectRestoredLoad(std::vector<float> left, std::vector<float> right, const float resolvedBpm,
                            AbacDsp::MeterTimeline meterTimeline, std::vector<float> overdubLeft = {},
                            std::vector<float> overdubRight = {})
    {
        if (m_loopLoadPending)
        {
            return;
        }
        m_loopLoadLeft = std::move(left);
        m_loopLoadRight = std::move(right);
        m_loopLoadResolvedBpm = resolvedBpm;
        m_loopLoadMeterTimeline = std::move(meterTimeline);
        m_loopLoadHasSequencerData = false;
        m_loopLoadTracks.clear();
        m_loopLoadPattern.reset();
        m_loopLoadHasOverdub = !overdubLeft.empty() && !overdubRight.empty();
        m_loopLoadOverdubLeft = std::move(overdubLeft);
        m_loopLoadOverdubRight = std::move(overdubRight);

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
        result.left = std::move(m_loopLoadLeft);
        result.right = std::move(m_loopLoadRight);
        result.resolvedBpm = m_loopLoadResolvedBpm;
        result.meterTimeline = std::move(m_loopLoadMeterTimeline);
        result.hasSequencerData = m_loopLoadHasSequencerData;
        result.tracks = std::move(m_loopLoadTracks);
        result.pattern = std::move(m_loopLoadPattern);
        result.hasOverdub = m_loopLoadHasOverdub;
        result.overdubLeft = std::move(m_loopLoadOverdubLeft);
        result.overdubRight = std::move(m_loopLoadOverdubRight);
        return result;
    }

  private:
    // Named patches sanitize with juce::String; this is JUCE-free (Impl stays
    // library-agnostic), so filesystem-unsafe characters are stripped by hand.
    [[nodiscard]] static std::string sanitizeLoopName(const std::string& name)
    {
        std::string result;
        for (const char c : name)
        {
            if (std::string_view("/\\:*?\"<>|").find(c) == std::string_view::npos)
            {
                result += c;
            }
        }
        const auto first = result.find_first_not_of(" \t");
        if (first == std::string::npos)
        {
            return {};
        }
        const auto last = result.find_last_not_of(" \t");
        return result.substr(first, last - first + 1);
    }

    [[nodiscard]] std::filesystem::path loopWavPath(const std::string& name) const
    {
        return std::filesystem::path(m_loopsDirectory) / (sanitizeLoopName(name) + ".wav");
    }

    [[nodiscard]] std::filesystem::path loopJsonPath(const std::string& name) const
    {
        return std::filesystem::path(m_loopsDirectory) / (sanitizeLoopName(name) + ".json");
    }

    [[nodiscard]] std::filesystem::path loopMidPath(const std::string& name) const
    {
        return std::filesystem::path(m_loopsDirectory) / (sanitizeLoopName(name) + ".mid");
    }

    [[nodiscard]] std::filesystem::path loopTrackWavPath(const std::string& name, const size_t track) const
    {
        return std::filesystem::path(m_loopsDirectory) /
               (sanitizeLoopName(name) + "_track" + std::to_string(track) + ".wav");
    }

    [[nodiscard]] std::filesystem::path loopOverdubWavPath(const std::string& name) const
    {
        return std::filesystem::path(m_loopsDirectory) / (sanitizeLoopName(name) + "_overdub.wav");
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

    // Worker thread only: writes <name>.wav (+ iXML metadata) and <name>.json;
    // if any tracks are frozen, also <name>_track<N>.wav per track.
    void runSaveLoopAs(const uint64_t gen)
    {
        const size_t loopLen = m_recorder.loopLengthFrames();
        if (loopLen > 0)
        {
            std::vector<float> left(loopLen);
            std::vector<float> right(loopLen);
            for (size_t f = 0; f < loopLen; ++f)
            {
                left[f] = m_recorder.sample(f, 0);
                right[f] = m_recorder.sample(f, 1);
            }
            // Descriptive metadata only (the pattern's own serialized beatsPerBar is
            // authoritative on load); approximate using the loop's current meter,
            // which may not be exact for a take whose meter changed mid-recording.
            const float samplesPerBeat = m_sampleRate * 60.f / m_appliedBpm;
            const float beats = (samplesPerBeat > 0.f) ? static_cast<float>(loopLen) / samplesPerBeat : 0.f;
            const float bars = beats / static_cast<float>(m_seq.beatsPerBar());
            const AbacDsp::LoopMetadata meta{1, m_appliedBpm, bars, beats};
            AbacDsp::LoopFile<nlohmann::json>::saveStereoWav(loopWavPath(m_loopSaveName).string(), left, right,
                                                             m_sampleRate, meta);

            // Standard MIDI File sidecar carrying the take's own tempo + meter
            // timeline; written unconditionally (a constant-meter take just gets
            // a single time-signature event) so loading only ever needs one path.
            AbacDsp::MidiFile midi;
            midi.setTempoBpm(m_appliedBpm);
            const std::vector<AbacDsp::MeterSegment> segmentsToWrite =
                m_meterTimeline.empty() ? std::vector<AbacDsp::MeterSegment>{{0, m_seq.beatsPerBar(), m_eighthNoteUnit}}
                                        : m_meterTimeline.segments();
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
            if (!midi.writeToFile(loopMidPath(m_loopSaveName).string()))
            {
                std::cerr << "LoopStorageService: failed to write " << loopMidPath(m_loopSaveName) << std::endl;
            }

            nlohmann::json j = meta;
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
                    tracksJson.push_back({{"file", std::filesystem::path(trackPath).filename().string()},
                                          {"sliceLengths", sliceLengths}});
                }
                j["tracks"] = tracksJson;
            }
            if (m_recorder.hasOverdub())
            {
                std::vector<float> overdubLeft(loopLen);
                std::vector<float> overdubRight(loopLen);
                for (size_t f = 0; f < loopLen; ++f)
                {
                    overdubLeft[f] = m_recorder.overdubSample(f, 0);
                    overdubRight[f] = m_recorder.overdubSample(f, 1);
                }
                const auto overdubPath = loopOverdubWavPath(m_loopSaveName).string();
                AudioUtility::SaveWav::saveStereoAs(overdubPath, overdubLeft, overdubRight, m_sampleRate);
                j["overdub"] = {{"file", std::filesystem::path(overdubPath).filename().string()}};
            }
            std::ofstream jsonOut(loopJsonPath(m_loopSaveName));
            if (jsonOut)
            {
                jsonOut << j.dump(2);
            }
            std::lock_guard<std::mutex> lock(m_lastSavedLoopMutex);
            m_lastSavedLoopName = m_loopSaveName;
        }
        {
            std::lock_guard<std::mutex> lock(m_currentLoopMutex);
            m_currentLoopName = m_loopSaveName;
        }
        m_loopSaveDoneGen.store(gen, std::memory_order_release);
    }

    // Worker thread only. Builds into locals first (throws on malformed data,
    // caught by runLoadLoop()) so a partial failure leaves no scratch state.
    void loadSequencerData(const nlohmann::json& j)
    {
        auto pattern = j.at("pattern").get<AbacDsp::SequencePattern>();
        std::vector<LoopLoadTrackData> tracks;
        for (const auto& trackJson : j.at("tracks"))
        {
            const auto trackPath = std::filesystem::path(m_loopsDirectory) / trackJson.at("file").get<std::string>();
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

    // Worker thread only: decodes <name>.wav + .json and compares their BPM
    // belief (iXML-embedded vs sidecar) rather than picking one silently.
    void runLoadLoop(const uint64_t gen)
    {
        LoopLoadOutcome outcome;
        outcome.attempted = true;
        m_loopLoadHasSequencerData = false;
        m_loopLoadTracks.clear();
        m_loopLoadPattern.reset();
        m_loopLoadMeterTimeline.clear();
        m_loopLoadHasOverdub = false;
        m_loopLoadOverdubLeft.clear();
        m_loopLoadOverdubRight.clear();
        const auto loaded = AbacDsp::LoopFile<nlohmann::json>::loadStereoWav(loopWavPath(m_loopLoadName).string());
        if (!loaded.left.empty())
        {
            std::optional<AbacDsp::LoopMetadata> sidecarMeta;
            std::ifstream jsonIn(loopJsonPath(m_loopLoadName));
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
                        loadSequencerData(j);
                    }
                    if (j.contains("overdub"))
                    {
                        const auto overdubFile = j.at("overdub").at("file").get<std::string>();
                        const auto overdubPath = std::filesystem::path(m_loopsDirectory) / overdubFile;
                        const auto overdubLoaded =
                            AbacDsp::LoopFile<nlohmann::json>::loadStereoWav(overdubPath.string());
                        if (!overdubLoaded.left.empty())
                        {
                            m_loopLoadOverdubLeft = overdubLoaded.left;
                            m_loopLoadOverdubRight = overdubLoaded.right;
                            m_loopLoadHasOverdub = true;
                        }
                    }
                }
                catch (const std::exception& e)
                {
                    std::cerr << "LoopStorageService: failed to parse " << loopJsonPath(m_loopLoadName) << ": "
                              << e.what() << std::endl;
                }
            }

            AbacDsp::MidiFile midi;
            if (midi.readFromFile(loopMidPath(m_loopLoadName).string()))
            {
                size_t bar = 0;
                uint32_t prevTick = 0;
                const auto& events = midi.timeSignatures();
                for (size_t i = 0; i < events.size(); ++i)
                {
                    const auto& ev = events[i];
                    if (i > 0)
                    {
                        const auto& prevEv = events[i - 1];
                        const uint32_t ticksPerBarPrev =
                            midiTicksPerBar(prevEv.numerator, prevEv.denominatorPower == 3);
                        bar += (ticksPerBarPrev > 0) ? (ev.tick - prevTick) / ticksPerBarPrev : 0;
                    }
                    m_loopLoadMeterTimeline.addSegment(bar, ev.numerator, ev.denominatorPower == 3);
                    prevTick = ev.tick;
                }
            }

            m_loopLoadLeft = loaded.left;
            m_loopLoadRight = loaded.right;
            outcome.success = true;
            {
                std::lock_guard<std::mutex> lock(m_currentLoopMutex);
                m_currentLoopName = m_loopLoadName;
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

    const AbacDsp::LoopRecorder<BlockSize>& m_recorder;
    const AbacDsp::BeatSequencer& m_seq;
    const AbacDsp::SliceLibrary& m_sliceLibrary;
    const AbacDsp::SequencePattern& m_pattern;
    const AbacDsp::MeterTimeline& m_meterTimeline;
    const float& m_appliedBpm;
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
    std::vector<float> m_loopLoadLeft; // worker-owned scratch, audio thread reads once confirmed
    std::vector<float> m_loopLoadRight;
    float m_loopLoadResolvedBpm{120.f};
    std::atomic<bool> m_loopLoadNeedsResolve{false};
    std::mutex m_loopLoadOutcomeMutex;
    LoopLoadOutcome m_loopLoadOutcome;
    std::vector<LoopLoadTrackData> m_loopLoadTracks; // worker-owned scratch, same handoff as left/right above
    std::optional<AbacDsp::SequencePattern> m_loopLoadPattern;
    bool m_loopLoadHasSequencerData{false};
    AbacDsp::MeterTimeline m_loopLoadMeterTimeline; // worker-owned scratch, reconstructed from the .mid sidecar
    bool m_loopLoadHasOverdub{false};
    std::vector<float> m_loopLoadOverdubLeft;
    std::vector<float> m_loopLoadOverdubRight;
};
