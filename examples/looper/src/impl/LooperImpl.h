#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <functional>
#include <vector>

#include "Analysis/Slicer.h"
#include "Audio/AudioBuffer.h"
#include "EffectBase.h"
#include "Generators/BeatSequencer.h"
#include "Generators/ClickGenerator.h"
#include "Sampler/LoopRecorder.h"
#include "Sampler/SlicePlayer.h"

// Slicing looper: captures audio, quantizes the loop to whole bars, slices it
// (grid or transient), and replays the slices locked to the loop position with a
// metronome click. The four transport controls are momentary pulses that toggle
// the real state; the editor reads the state back for labels, so there is no
// toggle-vs-state desync.
template <size_t BlockSize>
class LooperImpl final : public EffectBase
{
  public:
    static constexpr size_t kBeatsPerBar = 4;
    static constexpr size_t kWaveformPoints = 512;

    explicit LooperImpl(const float sampleRate)
        : EffectBase(sampleRate)
        , m_recorder(sampleRate)
        , m_slicePlayer(sampleRate)
        , m_seq(sampleRate)
        , m_click(sampleRate)
    {
        m_seq.setBeatsPerBar(kBeatsPerBar);
        m_seq.setBpm(m_appliedBpm);
        m_click.setVolumeDb(m_clickVol.load(std::memory_order_relaxed));
        m_slicePlayer.setFadeMs(3.f);
        m_slices.reserve(64);
        m_mono.reserve(static_cast<size_t>(sampleRate) * 4);
    }

    // Parameter setters (message thread): store into atomics, apply on the audio thread.
    void setBpm(const float value) noexcept
    {
        m_bpm.store(value, std::memory_order_relaxed);
    }
    void setSwing(const float value) noexcept
    {
        m_swing.store(value, std::memory_order_relaxed);
    }
    void setClickVolume(const float value) noexcept
    {
        m_clickVol.store(value, std::memory_order_relaxed);
    }
    void setSliceMode(const int value) noexcept
    {
        m_sliceMode.store(value, std::memory_order_relaxed);
    }
    void setSliceDivision(const int value) noexcept
    {
        m_sliceDivision.store(value, std::memory_order_relaxed);
    }
    void setHostSync(const bool value) noexcept
    {
        m_hostSyncReq.store(value, std::memory_order_relaxed);
    }
    void setRecord(const bool value) noexcept
    {
        if (value)
        {
            m_recordPulse.store(true, std::memory_order_relaxed);
        }
    }
    void setPlay(const bool value) noexcept
    {
        if (value)
        {
            m_playPulse.store(true, std::memory_order_relaxed);
        }
    }
    void setOverdub(const bool value) noexcept
    {
        if (value)
        {
            m_overdubPulse.store(true, std::memory_order_relaxed);
        }
    }
    void setClear(const bool value) noexcept
    {
        if (value)
        {
            m_clearPulse.store(true, std::memory_order_relaxed);
        }
    }

    // State queries (message thread, for editor labels and the display).
    [[nodiscard]] bool isHostSynced() const noexcept
    {
        return m_hostSync;
    }
    [[nodiscard]] bool isRecording() const noexcept
    {
        return m_recorder.state() == AbacDsp::LooperState::Recording;
    }
    [[nodiscard]] bool isPlaying() const noexcept
    {
        const auto s = m_recorder.state();
        return s == AbacDsp::LooperState::Playing || s == AbacDsp::LooperState::Overdubbing;
    }
    [[nodiscard]] bool isOverdubbing() const noexcept
    {
        return m_recorder.state() == AbacDsp::LooperState::Overdubbing;
    }
    [[nodiscard]] const char* getStateLabel() const noexcept
    {
        switch (m_recorder.state())
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

    // Backs the generated default "signal" gauge call; the display's real data is
    // pushed via getLoopWaveform()/getSliceBoundaries() from the editor timer.
    [[nodiscard]] const std::vector<float>& visualizeWaveData() const noexcept
    {
        return m_emptyWave;
    }

    [[nodiscard]] float getPlayheadNormalized() const noexcept
    {
        const size_t len = m_recorder.loopLengthFrames();
        return (len == 0) ? 0.f : static_cast<float>(m_recorder.playPositionFrames()) / static_cast<float>(len);
    }

    [[nodiscard]] std::vector<float> getSliceBoundaries() const
    {
        std::vector<float> normalized;
        const size_t len = m_recorder.loopLengthFrames();
        if (len == 0)
        {
            return normalized;
        }
        normalized.reserve(m_slices.size());
        for (const AbacDsp::Slice& slice : m_slices)
        {
            normalized.push_back(static_cast<float>(slice.startFrame) / static_cast<float>(len));
        }
        return normalized;
    }

    [[nodiscard]] std::vector<float> getLoopWaveform() const
    {
        std::vector<float> peaks;
        const size_t len = m_recorder.loopLengthFrames();
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
                const float mono = 0.5f * (m_recorder.sample(f, 0) + m_recorder.sample(f, 1));
                peak = std::max(peak, std::abs(mono));
            }
            peaks[p] = peak;
        }
        return peaks;
    }

    void processBlock(const AbacDsp::AudioBuffer<2, BlockSize>& in, AbacDsp::AudioBuffer<2, BlockSize>& out)
    {
        applyParameters();
        if (m_hostSync)
        {
            syncToHostTransport();
        }
        handleTransportPulses();

        m_recorder.setSamplesPerBar(m_seq.samplesPerBeat() * m_seq.beatsPerBar());

        AbacDsp::AudioBuffer<2, BlockSize> recorderOut{};
        m_recorder.processBlock(in, recorderOut);

        AbacDsp::AudioBuffer<2, BlockSize> sliceOut{};
        renderSlices(sliceOut);

        std::array<float, BlockSize> click{};
        renderClick(click);

        const bool monitorInput = isRecording() || isOverdubbing();
        for (size_t i = 0; i < BlockSize; ++i)
        {
            const float dryL = monitorInput ? in(i, 0) : 0.f;
            const float dryR = monitorInput ? in(i, 1) : 0.f;
            out(i, 0) = dryL + sliceOut(i, 0) + click[i];
            out(i, 1) = dryR + sliceOut(i, 1) + click[i];
        }
    }

  private:
    void applyParameters()
    {
        if (!m_hostSync)
        {
            const float bpm = m_bpm.load(std::memory_order_relaxed);
            if (std::not_equal_to<float>{}(bpm, m_appliedBpm))
            {
                m_seq.setBpm(bpm);
                m_appliedBpm = bpm;
            }
        }
        const float swing = m_swing.load(std::memory_order_relaxed);
        if (std::not_equal_to<float>{}(swing, m_appliedSwing))
        {
            m_seq.setSwingRatio(swingPercentToRatio(swing));
            m_appliedSwing = swing;
        }
        const int division = m_sliceDivision.load(std::memory_order_relaxed);
        if (division != m_appliedDivision)
        {
            m_seq.setSubdivType(divisionToSubdiv(division));
            m_appliedDivision = division;
        }
        m_click.setVolumeDb(m_clickVol.load(std::memory_order_relaxed));
        m_hostSync = m_hostSyncReq.load(std::memory_order_relaxed);
    }

    void syncToHostTransport()
    {
        const auto& transport = hostTransport();
        if (!transport.isPlaying || transport.updateCount == m_lastSyncedUpdateCount)
        {
            return;
        }
        m_lastSyncedUpdateCount = transport.updateCount;
        const float bpm = std::clamp(static_cast<float>(transport.bpm), 20.f, 999.f);
        m_seq.setBpm(bpm);
        m_appliedBpm = bpm;
        m_seq.syncToPpq(transport.ppqPosition);
    }

    void handleTransportPulses()
    {
        if (m_clearPulse.exchange(false, std::memory_order_relaxed))
        {
            m_recorder.clear();
            m_slices.clear();
            m_slicePlayer.setSlices(m_slices);
            m_lastSliceIndex = kNoSlice;
        }
        if (m_recordPulse.exchange(false, std::memory_order_relaxed))
        {
            toggleRecord();
        }
        if (m_playPulse.exchange(false, std::memory_order_relaxed))
        {
            togglePlay();
        }
        if (m_overdubPulse.exchange(false, std::memory_order_relaxed))
        {
            toggleOverdub();
        }
    }

    void toggleRecord()
    {
        if (isRecording())
        {
            m_recorder.setSamplesPerBar(m_seq.samplesPerBeat() * m_seq.beatsPerBar());
            m_recorder.stopRecord();
            computeSlices();
            m_seq.reset();
            m_lastSliceIndex = kNoSlice;
        }
        else
        {
            m_recorder.beginRecord();
            m_seq.reset();
        }
    }

    void togglePlay()
    {
        if (isPlaying())
        {
            m_recorder.stop();
            m_slicePlayer.reset();
            m_lastSliceIndex = kNoSlice;
        }
        else if (m_recorder.hasLoop())
        {
            m_recorder.play();
            m_seq.reset();
            m_lastSliceIndex = kNoSlice;
        }
    }

    void toggleOverdub()
    {
        if (isOverdubbing())
        {
            m_recorder.endOverdub();
            computeSlices();
        }
        else if (isPlaying())
        {
            m_recorder.beginOverdub();
        }
    }

    // Slice triggering is position-driven: whenever the loop playhead enters a new
    // slice, that slice is launched. Identity order for now; shuffle/reverse later.
    void renderSlices(AbacDsp::AudioBuffer<2, BlockSize>& out)
    {
        if (isPlaying() && m_recorder.hasLoop() && !m_slices.empty())
        {
            const size_t sliceIndex = sliceIndexAt(m_recorder.playPositionFrames());
            if (sliceIndex != m_lastSliceIndex && sliceIndex != kNoSlice)
            {
                m_slicePlayer.triggerSlice(sliceIndex, m_slices[sliceIndex].lengthFrames);
                m_lastSliceIndex = sliceIndex;
            }
        }
        m_slicePlayer.processBlock(out);
    }

    void renderClick(std::array<float, BlockSize>& click)
    {
        const bool active = isRecording() || isPlaying();
        for (size_t i = 0; i < BlockSize; ++i)
        {
            const auto event = m_seq.advance();
            if (active)
            {
                if (event.beatStart)
                {
                    m_click.trigger(event.beatIndexInBar == 0 ? AbacDsp::ClickAccent::Downbeat
                                                              : AbacDsp::ClickAccent::Beat);
                }
                else if (event.subdivision)
                {
                    m_click.triggerSub();
                }
            }
            click[i] = active ? m_click.step0() : 0.f;
        }
    }

    void computeSlices()
    {
        m_slices.clear();
        const size_t len = m_recorder.loopLengthFrames();
        if (len == 0)
        {
            m_slicePlayer.setLoop(m_recorder.loopView(), 0);
            m_slicePlayer.setSlices(m_slices);
            return;
        }
        const size_t divisionsPerBeat = divisionsPerBeatFor(m_appliedDivision);
        const size_t spb = std::max<size_t>(1, m_seq.samplesPerBeat());
        const size_t beats = std::max<size_t>(1, len / spb);
        const size_t sliceCount = std::max<size_t>(1, beats * divisionsPerBeat);

        if (m_sliceMode.load(std::memory_order_relaxed) == 0)
        {
            m_slices = AbacDsp::Slicer::gridSlices(len, sliceCount);
        }
        else
        {
            AbacDsp::Slicer::downmixToMono(m_recorder.loopView(), len, m_mono);
            AbacDsp::Slicer::TransientParams params{};
            params.relativeThreshold = 0.25f;
            params.minGapFrames = spb / (divisionsPerBeat * 2 + 1);
            params.envelopeWindow = std::max<size_t>(1, spb / 100);
            params.snapMaxDistance = spb / divisionsPerBeat / 2;
            params.zeroCrossRadius = 64;
            const std::vector<size_t> grid = AbacDsp::Slicer::gridBoundaries(len, len / sliceCount);
            m_slices = AbacDsp::Slicer::transientSlices(m_mono, len, params, grid);
        }
        m_slicePlayer.setLoop(m_recorder.loopView(), len);
        m_slicePlayer.setSlices(m_slices);
    }

    [[nodiscard]] size_t sliceIndexAt(const size_t position) const noexcept
    {
        for (size_t i = 0; i < m_slices.size(); ++i)
        {
            const AbacDsp::Slice& slice = m_slices[i];
            if (position >= slice.startFrame && position < slice.startFrame + slice.lengthFrames)
            {
                return i;
            }
        }
        return kNoSlice;
    }

    [[nodiscard]] static float swingPercentToRatio(const float percent) noexcept
    {
        return std::clamp(1.f + (percent - 50.f) / 50.f, 0.5f, 2.5f);
    }

    [[nodiscard]] static AbacDsp::SubdivType divisionToSubdiv(const int division) noexcept
    {
        switch (division)
        {
            case 0:
                return AbacDsp::SubdivType::None; // 1/4
            case 1:
                return AbacDsp::SubdivType::Eighth;
            case 2:
                return AbacDsp::SubdivType::Sixteenth;
            default:
                return AbacDsp::SubdivType::Sixteenth; // 1/32 approximated
        }
    }

    [[nodiscard]] static size_t divisionsPerBeatFor(const int division) noexcept
    {
        switch (division)
        {
            case 0:
                return 1; // 1/4
            case 1:
                return 2; // 1/8
            case 2:
                return 4; // 1/16
            default:
                return 8; // 1/32
        }
    }

    static constexpr size_t kNoSlice = static_cast<size_t>(-1);

    AbacDsp::LoopRecorder<BlockSize> m_recorder;
    AbacDsp::SlicePlayer<BlockSize> m_slicePlayer;
    AbacDsp::BeatSequencer m_seq;
    AbacDsp::ClickGenerator m_click;

    std::vector<AbacDsp::Slice> m_slices;
    std::vector<float> m_mono;
    std::vector<float> m_emptyWave;

    std::atomic<float> m_bpm{120.f};
    std::atomic<float> m_swing{50.f};
    std::atomic<float> m_clickVol{-12.f};
    std::atomic<int> m_sliceMode{0};
    std::atomic<int> m_sliceDivision{1};
    std::atomic<bool> m_hostSyncReq{false};

    std::atomic<bool> m_recordPulse{false};
    std::atomic<bool> m_playPulse{false};
    std::atomic<bool> m_overdubPulse{false};
    std::atomic<bool> m_clearPulse{false};

    float m_appliedBpm{120.f};
    float m_appliedSwing{50.f};
    int m_appliedDivision{1};
    bool m_hostSync{false};
    uint64_t m_lastSyncedUpdateCount{0};
    size_t m_lastSliceIndex{kNoSlice};
};
