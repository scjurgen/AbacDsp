#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <format>
#include <iterator>
#include <string>
#include <vector>

#include "Analysis/Spectrogram.h"
#include "Generators/BeatSequencer.h"
#include "Generators/MeterTimeline.h"
#include "Sampler/LoopPartBank.h"
#include "Sampler/LoopRecorder.h"
#include "Sampler/SequencePattern.h"
#include "Sampler/SequencerEngine.h"
#include "Sampler/SliceLibrary.h"

// Read-only/UI-facing derived values: labels, waveform and slice display
// getters, spectrogram getters. No transport logic lives here; state stays
// owned by LooperImpl and is referenced the same way as the other extracted
// controllers/services.
template <size_t BlockSize>
class LooperViewModel
{
  public:
    static constexpr size_t kWaveformPoints = 512;

    struct Deps
    {
        const AbacDsp::LoopPartBank<BlockSize>& bank;
        const AbacDsp::BeatSequencer& seq;
        const std::array<AbacDsp::MeterTimeline, AbacDsp::kMaxLoopParts>& meterTimelines;
        const size_t& activePartIndex;
        const std::array<float, AbacDsp::kMaxLoopParts>& appliedBpm;
        const AbacDsp::SliceLibrary& sliceLibrary;
        const AbacDsp::SequencePattern& pattern;
        const AbacDsp::SequencerEngine<>& sequencer;
        const AbacDsp::SimpleSpectrogram& recordSpectrogram;
        const std::vector<float>& visualWave;
        std::vector<float>& preparedWave;
        const size_t& visualWindowSize;
        const std::atomic<uint64_t>& spectrogramRegenRequestGen;
        const std::atomic<uint64_t>& spectrogramRegenDoneGen;
        const bool& autoStopEnabled;
        const int& recordBars;
        const std::array<size_t, AbacDsp::kMaxLoopParts>& finalizedBarCounts;
        const int& countInBarsOffset;
        const bool& sequencerPlaying;
        float sampleRate;
    };

    explicit LooperViewModel(Deps deps)
        : m_deps(deps)
    {
    }

    [[nodiscard]] const AbacDsp::MeterTimeline& activeMeterTimeline() const noexcept
    {
        return m_deps.meterTimelines[m_deps.activePartIndex];
    }

    [[nodiscard]] size_t activeFinalizedBarCount() const noexcept
    {
        return m_deps.finalizedBarCounts[m_deps.activePartIndex];
    }

    // "Part A: 8 bars" for a bar-locked take, "Part B: 3.2s" for a free-recorded
    // one (no finalized bar count to show), "Part C: free" when empty.
    [[nodiscard]] std::string partStatusLabel(const size_t index) const
    {
        static constexpr std::array<const char*, AbacDsp::kMaxLoopParts> kNames{"Part A", "Part B", "Part C", "Part D"};
        const char* name = kNames[index];
        if (!m_deps.bank.hasContent(index))
        {
            return std::format("{}: free", name);
        }
        if (m_deps.finalizedBarCounts[index] > 0)
        {
            return std::format("{}: {} bars", name, m_deps.finalizedBarCounts[index]);
        }
        const float seconds = static_cast<float>(m_deps.bank.loopLengthFrames(index)) / m_deps.sampleRate;
        return std::format("{}: {:.1f}s", name, seconds);
    }

    [[nodiscard]] const AbacDsp::LoopRecorder<BlockSize>& activeRecorder() const noexcept
    {
        return m_deps.bank.active();
    }

    [[nodiscard]] const char* getStateLabel(const bool armed, const bool countingIn) const noexcept
    {
        if (armed)
        {
            return "Armed";
        }
        if (countingIn)
        {
            return "Counting in";
        }
        switch (activeRecorder().state())
        {
            case AbacDsp::LooperState::Empty:
                return "Empty";
            case AbacDsp::LooperState::Recording:
                return "Recording";
            case AbacDsp::LooperState::Playing:
                return "Playing";
            case AbacDsp::LooperState::Overdubbing:
                return "Overdub";
            case AbacDsp::LooperState::Stopped:
                return "Stopped";
        }
        return "";
    }

    // One bar of the musical signal, downbeat at index 0, for the CircularBarDisplay
    // (fed through the generated default "signal" gauge call). SliceWaveDisplay
    // ignores this and uses getLoopWaveform() instead.
    [[nodiscard]] const std::vector<float>& visualizeWaveData()
    {
        m_deps.preparedWave.assign(
            m_deps.visualWave.begin(),
            std::next(m_deps.visualWave.begin(), static_cast<std::ptrdiff_t>(m_deps.visualWindowSize)));
        return m_deps.preparedWave;
    }

    [[nodiscard]] size_t getSamplesPerBar() const noexcept
    {
        return m_deps.seq.samplesPerBeat() * m_deps.seq.beatsPerBar();
    }

    [[nodiscard]] int getBarBeats() const noexcept
    {
        return static_cast<int>(m_deps.seq.beatsPerBar());
    }

    // Angular span of the outer loop ring, in whole bars. Empty shows one bar so
    // the clock still reads. While recording with Auto Stop on, the ring is already
    // sized to the fixed Record Bars target; with Auto Stop off there's no known
    // length yet, so the ring extends one bar ahead of the playhead instead, growing
    // as the take does.
    [[nodiscard]] int getOuterRingBars() const noexcept
    {
        const size_t spb = getSamplesPerBar();
        if (spb == 0)
        {
            return 1;
        }
        if (isRecording())
        {
            const size_t fixedBars = m_deps.autoStopEnabled ? static_cast<size_t>(m_deps.recordBars) : 0;
            if (fixedBars > 0)
            {
                return static_cast<int>(fixedBars);
            }
            return static_cast<int>(activeRecorder().recordedFrames() / spb) + 1;
        }
        // Prefer the finalized bar count over a live recompute: a mixed-meter
        // loop's live beatsPerBar changes bar-to-bar during playback replay
        // (LooperImpl::applyMeterAtBarBoundary()), which would otherwise make
        // len/spb drift as playback crosses each meter change. Free-record takes
        // never populate finalizedBarCount, so they keep the len/spb fallback
        // below unchanged.
        if (activeFinalizedBarCount() > 0)
        {
            return static_cast<int>(activeFinalizedBarCount());
        }
        const size_t len = activeRecorder().loopLengthFrames();
        return (len > 0) ? static_cast<int>(std::max<size_t>(1, len / spb)) : 1;
    }

    // One frame-length entry per bar of the outer ring (see getOuterRingBars()),
    // honoring the take's own meter timeline so a mixed-meter loop's bars are
    // sized proportionally, not uniformly. Falls back to a constant getSamplesPerBar()
    // per entry when there is no timeline (free-record takes clear it), matching
    // today's uniform-bar behavior exactly in that case.
    [[nodiscard]] std::vector<float> getBarFrameLengths() const
    {
        const auto n = static_cast<size_t>(std::max(0, getOuterRingBars()));
        std::vector<float> lengths(n);
        if (activeMeterTimeline().empty())
        {
            std::ranges::fill(lengths, static_cast<float>(getSamplesPerBar()));
            return lengths;
        }
        const float activeBpm = m_deps.appliedBpm[m_deps.activePartIndex];
        const float samplesPerQuarterBeat = (activeBpm > 0.f) ? m_deps.sampleRate * 60.f / activeBpm : 0.f;
        for (size_t bar = 0; bar < n; ++bar)
        {
            const auto& seg = activeMeterTimeline().segmentForBar(bar);
            const float samplesPerBeat = seg.eighthUnit ? samplesPerQuarterBeat * 0.5f : samplesPerQuarterBeat;
            lengths[bar] = samplesPerBeat * static_cast<float>(seg.beatsPerBar);
        }
        return lengths;
    }

    // "bar.beat" position, 1-based, generic over beatsPerBar (no fixed 4/4 assumption).
    // A count-in runs up to bar 1 from a negative/zero number (e.g. a 2-bar count-in
    // is -1, 0, then 1 is the real start of the take). Once recording, the bar count
    // just runs up; once a fixed-length loop is playing back, it wraps to 1 every
    // time the loop repeats.
    [[nodiscard]] std::string getBarBeatLabel() const
    {
        long bar = static_cast<long>(m_deps.seq.barIndex()) - m_deps.countInBarsOffset;
        if (isPlaying() || isOverdubbing())
        {
            // Prefer the finalized bar count over a live recompute: see the same
            // reasoning in getOuterRingBars() (a mixed-meter loop's live spb
            // changes bar-to-bar during playback replay).
            size_t barsInLoop = activeFinalizedBarCount();
            if (barsInLoop == 0)
            {
                const size_t spb = getSamplesPerBar();
                barsInLoop = (spb > 0) ? activeRecorder().loopLengthFrames() / spb : 0;
            }
            if (barsInLoop > 0)
            {
                const long n = static_cast<long>(barsInLoop);
                bar = ((bar % n) + n) % n;
            }
        }
        return std::format("{}.{}", bar + 1, m_deps.seq.beatIndexInBar() + 1);
    }

    // Recording headroom left in the capture buffer, mm:ss, so a performer always
    // knows how much runway remains before a free take gets cut off mid-bar. Once a
    // loop is finalized the buffer isn't being consumed anymore, so this is moot.
    [[nodiscard]] std::string getRemainingRecordLabel() const
    {
        if (activeRecorder().hasLoop())
        {
            return {};
        }
        const size_t remainingFrames =
            activeRecorder().maxFrames() - std::min(activeRecorder().maxFrames(), activeRecorder().recordedFrames());
        const auto remainingSeconds = static_cast<int>(static_cast<float>(remainingFrames) / m_deps.sampleRate);
        return std::format("{}:{:02d}", remainingSeconds / 60, remainingSeconds % 60);
    }

    [[nodiscard]] float getBarPhase() const noexcept
    {
        return m_deps.seq.barPhase();
    }

    [[nodiscard]] AbacDsp::SpectrumImageSet getSpectrogramData() const
    {
        return m_deps.recordSpectrogram.getImageSet();
    }

    // Frame position of the spectrogram write head within the ring: the live record
    // position while capturing, the finalized loop length once stopped. Reports 0
    // (blank ring) while a post-load regen is still catching up, since activeSlice
    // wouldn't yet correspond to the full loop length CircularLoopDisplay expects.
    [[nodiscard]] size_t getSpectrogramHeadFrames() const noexcept
    {
        if (isRecording())
        {
            return activeRecorder().recordedFrames();
        }
        if (m_deps.spectrogramRegenRequestGen.load(std::memory_order_acquire) !=
            m_deps.spectrogramRegenDoneGen.load(std::memory_order_acquire))
        {
            return 0;
        }
        return activeRecorder().loopLengthFrames();
    }

    [[nodiscard]] const std::vector<size_t>& getSubdivisionPositions() const noexcept
    {
        return m_deps.seq.subPositions();
    }

    [[nodiscard]] float getPlayheadNormalized() const noexcept
    {
        const size_t len = activeRecorder().loopLengthFrames();
        return (len == 0) ? 0.f : static_cast<float>(activeRecorder().playPositionFrames()) / static_cast<float>(len);
    }

    // About the live loop's own display, not the frozen tracks below; no-op for now.
    [[nodiscard]] std::vector<float> getSliceBoundaries() const
    {
        return {};
    }

    [[nodiscard]] size_t getFrozenTrackCount() const noexcept
    {
        return m_deps.sliceLibrary.trackCount();
    }

    [[nodiscard]] size_t getFrozenSliceCount() const noexcept
    {
        return m_deps.sliceLibrary.sliceCount();
    }

    // totalSteps() is frame-accurate (stepsPerBeat == samplesPerBeat at freeze
    // time), so it doubles directly as each thumbnail's placement denominator.
    [[nodiscard]] std::vector<AbacDsp::SequencerSliceThumbnail> getSequencerSliceThumbnails() const
    {
        const size_t totalSteps = m_deps.pattern.totalSteps();
        std::vector<AbacDsp::SequencerSliceThumbnail> thumbnails;
        if (totalSteps == 0)
        {
            return thumbnails;
        }
        thumbnails.reserve(m_deps.pattern.eventCount());
        for (const auto& event : m_deps.pattern.events())
        {
            if (event.track >= m_deps.sliceLibrary.trackCount() ||
                event.sliceIndex >= m_deps.sliceLibrary.sliceCountInTrack(event.track))
            {
                continue;
            }
            const auto& info = m_deps.sliceLibrary.sliceInfo(event.track, event.sliceIndex);
            const auto image = m_deps.sliceLibrary.thumbnail(event.track, event.sliceIndex);
            thumbnails.push_back({static_cast<float>(event.stepPosition) / static_cast<float>(totalSteps),
                                  static_cast<float>(info.lengthFrames) / static_cast<float>(totalSteps),
                                  AbacDsp::SliceLibrary::kThumbWidth, AbacDsp::SliceLibrary::kThumbHeight, image.data(),
                                  m_deps.sampleRate});
        }
        return thumbnails;
    }

    [[nodiscard]] std::vector<float> getSequencerSliceBoundaries() const
    {
        const size_t totalSteps = m_deps.pattern.totalSteps();
        if (totalSteps == 0)
        {
            return {};
        }
        std::vector<float> boundaries;
        boundaries.reserve(m_deps.pattern.eventCount());
        for (const auto& event : m_deps.pattern.events())
        {
            boundaries.push_back(static_cast<float>(event.stepPosition) / static_cast<float>(totalSteps));
        }
        return boundaries;
    }

    // Tracks the shared BeatSequencer clock regardless of isSequencerPlaying(),
    // so the marker previews trigger timing even before Seq Play is pressed.
    [[nodiscard]] float getSequencerPlayheadNormalized() const noexcept
    {
        const size_t lengthBars = m_deps.pattern.lengthBars();
        if (lengthBars == 0)
        {
            return 0.f;
        }
        const float pos = static_cast<float>(m_deps.sequencer.barIndex()) + m_deps.seq.barPhase();
        return std::clamp(pos / static_cast<float>(lengthBars), 0.f, 1.f);
    }

    [[nodiscard]] const char* getSequencerStateLabel() const noexcept
    {
        if (!m_deps.sequencerPlaying)
        {
            return m_deps.pattern.eventCount() == 0 ? "Seq Empty" : "Seq Stopped";
        }
        return "Seq Playing";
    }

    // Exact per-sample loop content and length (mirror LoopRecorder's own
    // accessors). Not used by the UI (which only needs the coarse peak
    // waveform below); exposed for precise verification of the Phase 8b
    // beat-lock placement.
    [[nodiscard]] float rawLoopSample(const size_t frame, const size_t channel) const noexcept
    {
        return activeRecorder().sample(frame, channel);
    }

    [[nodiscard]] size_t rawLoopLengthFrames() const noexcept
    {
        return activeRecorder().loopLengthFrames();
    }

    [[nodiscard]] std::vector<float> getLoopWaveform() const
    {
        std::vector<float> peaks;
        const size_t len = activeRecorder().loopLengthFrames();
        if (len == 0)
        {
            return peaks;
        }
        peaks.assign(kWaveformPoints, 0.f);
        const size_t step = std::max<size_t>(1, len / kWaveformPoints);
        for (size_t p = 0; p < kWaveformPoints; ++p)
        {
            const size_t begin = p * len / kWaveformPoints;
            const size_t end = std::min(len, begin + step);
            float peak = 0.f;
            for (size_t f = begin; f < end; ++f)
            {
                const float mono = 0.5f * (activeRecorder().sample(f, 0) + activeRecorder().sample(f, 1));
                peak = std::max(peak, std::abs(mono));
            }
            peaks[p] = peak;
        }
        return peaks;
    }

  private:
    [[nodiscard]] bool isRecording() const noexcept
    {
        return activeRecorder().state() == AbacDsp::LooperState::Recording;
    }

    [[nodiscard]] bool isPlaying() const noexcept
    {
        const auto s = activeRecorder().state();
        return s == AbacDsp::LooperState::Playing || s == AbacDsp::LooperState::Overdubbing;
    }

    [[nodiscard]] bool isOverdubbing() const noexcept
    {
        return activeRecorder().state() == AbacDsp::LooperState::Overdubbing;
    }

    Deps m_deps;
};
