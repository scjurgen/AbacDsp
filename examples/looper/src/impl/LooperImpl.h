#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <span>
#include <vector>

#include "Analysis/Spectrogram.h"
#include "Audio/AudioBuffer.h"
#include "EffectBase.h"
#include "Generators/BeatSequencer.h"
#include "Generators/ClickGenerator.h"
#include "Sampler/LoopRecorder.h"

// Traditional-style slicing looper: captures audio, quantizes the loop to whole
// bars, and plays it back locked to a metronome click. The four transport
// controls are momentary pulses that toggle the real state; the editor reads the
// state back for labels, so there is no toggle-vs-state desync. Slicing is not
// performed here; it will be reintroduced as an on-demand sample sequencer layer
// on top of this plain looper.
template <size_t BlockSize>
class LooperImpl final : public EffectBase
{
  public:
    static constexpr size_t kBeatsPerBar = 4;
    static constexpr size_t kWaveformPoints = 512;

    explicit LooperImpl(const float sampleRate)
        : EffectBase(sampleRate)
        , m_recorder(sampleRate)
        , m_seq(sampleRate)
        , m_click(sampleRate)
    {
        m_seq.setBeatsPerBar(kBeatsPerBar);
        m_seq.setBpm(m_appliedBpm);
        m_click.setVolumeDb(m_clickVol.load(std::memory_order_relaxed));
        // One bar (4 beats) at the lowest tempo (50 BPM) is ~4.8 s; size generously.
        m_visualWave.assign(static_cast<size_t>(sampleRate * 5.f) + 16, 0.f);
        m_preparedWave.reserve(m_visualWave.size());
        m_recordSpectrogram.setSampleRate(sampleRate);
        // Cover the whole recordable span (~60 s) so a long loop's ring is fully
        // painted, not just its tail. hop = fftLength * windowForwardRatio (1024/3).
        m_recordSpectrogram.setSlices(static_cast<size_t>(60.f * sampleRate / (1024.f / 3.f)) + 64);
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
    void setLoopVolume(const float value) noexcept
    {
        m_loopVol.store(value, std::memory_order_relaxed);
    }
    void setThreshRec(const bool value) noexcept
    {
        m_threshRecReq.store(value, std::memory_order_relaxed);
    }
    void setRecThreshold(const float value) noexcept
    {
        m_recThreshold.store(value, std::memory_order_relaxed);
    }
    // Not consumed yet: reserved for the future on-demand slice sequencer.
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
    [[nodiscard]] bool isArmed() const noexcept
    {
        return m_armed;
    }
    [[nodiscard]] const char* getStateLabel() const noexcept
    {
        if (m_armed)
        {
            return "Armed";
        }
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

    // One bar of the musical signal, downbeat at index 0, for the CircularBarDisplay
    // (fed through the generated default "signal" gauge call). SliceWaveDisplay
    // ignores this and uses getLoopWaveform() instead.
    [[nodiscard]] const std::vector<float>& visualizeWaveData()
    {
        m_preparedWave.assign(m_visualWave.begin(),
                              std::next(m_visualWave.begin(), static_cast<std::ptrdiff_t>(m_visualWindowSize)));
        return m_preparedWave;
    }

    [[nodiscard]] size_t getSamplesPerBar() const noexcept
    {
        return m_seq.samplesPerBeat() * m_seq.beatsPerBar();
    }

    [[nodiscard]] int getBarBeats() const noexcept
    {
        return static_cast<int>(m_seq.beatsPerBar());
    }

    // Angular span of the outer loop ring, in whole bars. Empty shows one bar so
    // the clock still reads; while recording the ring extends one bar ahead of the
    // playhead so the bar in progress is already drawn full.
    [[nodiscard]] int getOuterRingBars() const noexcept
    {
        const size_t spb = getSamplesPerBar();
        if (spb == 0)
        {
            return 1;
        }
        if (isRecording())
        {
            return static_cast<int>(m_recorder.recordedFrames() / spb) + 1;
        }
        const size_t len = m_recorder.loopLengthFrames();
        return (len > 0) ? static_cast<int>(std::max<size_t>(1, len / spb)) : 1;
    }

    [[nodiscard]] float getBarPhase() const noexcept
    {
        return m_seq.barPhase();
    }

    [[nodiscard]] AbacDsp::SpectrumImageSet getSpectrogramData() const
    {
        return m_recordSpectrogram.getImageSet();
    }

    // Frame position of the spectrogram write head within the ring: the live record
    // position while capturing, the finalized loop length once stopped.
    [[nodiscard]] size_t getSpectrogramHeadFrames() const noexcept
    {
        return isRecording() ? m_recorder.recordedFrames() : m_recorder.loopLengthFrames();
    }

    [[nodiscard]] const std::vector<size_t>& getSubdivisionPositions() const noexcept
    {
        return m_seq.subPositions();
    }

    [[nodiscard]] float getPlayheadNormalized() const noexcept
    {
        const size_t len = m_recorder.loopLengthFrames();
        return (len == 0) ? 0.f : static_cast<float>(m_recorder.playPositionFrames()) / static_cast<float>(len);
    }

    // No slice table while this looper is plain; kept as a stable no-op for the
    // display widgets until the on-demand slice sequencer is designed.
    [[nodiscard]] std::vector<float> getSliceBoundaries() const
    {
        return {};
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

        // Feed the record spectrogram with the dry input while capturing; it freezes
        // (stops advancing) once recording stops, so the last image persists.
        if (isRecording())
        {
            std::array<float, BlockSize> inMono{};
            for (size_t i = 0; i < BlockSize; ++i)
            {
                inMono[i] = 0.5f * (in(i, 0) + in(i, 1));
            }
            m_recordSpectrogram.processBlock(std::span<const float>{inMono});
        }

        m_recorder.setSamplesPerBar(m_seq.samplesPerBeat() * m_seq.beatsPerBar());

        // Threshold recording: while armed, wait for the input to cross the level
        // before capture actually begins (this block is then recorded too).
        if (m_armed && blockPeak(in) >= m_recThresholdLinear)
        {
            m_armed = false;
            m_recorder.beginRecord();
            m_seq.reset();
        }

        AbacDsp::AudioBuffer<2, BlockSize> recorderOut{};
        m_recorder.processBlock(in, recorderOut);

        m_visualWindowSize = std::min(getSamplesPerBar(), m_visualWave.size());

        std::array<float, BlockSize> click{};
        renderClick(click);

        // Dry input is always monitored; the loop (record/play/overdub, traditional
        // single-buffer looper) plays at its own volume on top.
        const float loopGain = m_loopGain;
        for (size_t i = 0; i < BlockSize; ++i)
        {
            const float loopL = recorderOut(i, 0) * loopGain;
            const float loopR = recorderOut(i, 1) * loopGain;
            out(i, 0) = in(i, 0) + loopL + click[i];
            out(i, 1) = in(i, 1) + loopR + click[i];

            // Feed the bar display with the dry input only (no loop, no click).
            // A noise gate keeps the ring flat on quiet sections: the signed
            // sample passes only while the envelope stays above the threshold.
            const float visSignal = in(i, 0) + in(i, 1);
            m_visEnv = std::max(std::abs(visSignal), m_visEnv * kVisualGateRelease);
            if (m_barPos[i] < m_visualWindowSize)
            {
                m_visualWave[m_barPos[i]] = (m_visEnv >= kVisualGate) ? visSignal : 0.f;
            }
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
        m_loopGain = std::pow(10.f, m_loopVol.load(std::memory_order_relaxed) / 20.f);
        m_recThresholdLinear = std::pow(10.f, m_recThreshold.load(std::memory_order_relaxed) / 20.f);
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
        const bool clearReq = m_clearPulse.exchange(false, std::memory_order_relaxed);
        const bool recordReq = m_recordPulse.exchange(false, std::memory_order_relaxed);
        const bool playReq = m_playPulse.exchange(false, std::memory_order_relaxed);
        const bool overdubReq = m_overdubPulse.exchange(false, std::memory_order_relaxed);

        if (clearReq)
        {
            clearAll();
        }
        if (recordReq)
        {
            toggleRecord();
        }
        if (playReq)
        {
            togglePlay();
        }
        if (overdubReq)
        {
            toggleOverdub();
        }
    }

    void clearAll()
    {
        m_armed = false;
        m_recorder.clear();
    }

    void finishRecording()
    {
        m_recorder.setSamplesPerBar(m_seq.samplesPerBeat() * m_seq.beatsPerBar());
        m_recorder.stopRecord();
        m_seq.reset();
    }

    void toggleRecord()
    {
        if (isRecording())
        {
            finishRecording();
        }
        else if (m_armed)
        {
            m_armed = false; // pressing Record again while armed disarms
        }
        else if (m_threshRecReq.load(std::memory_order_relaxed))
        {
            m_armed = true; // wait for the input to cross the threshold
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
        }
        else if (m_recorder.hasLoop())
        {
            m_recorder.play();
            m_seq.reset();
        }
    }

    void toggleOverdub()
    {
        if (isRecording())
        {
            // Punch straight from recording into overdub: finalize the base take
            // (it keeps playing) and start summing input into it immediately.
            finishRecording();
            m_recorder.beginOverdub();
        }
        else if (isOverdubbing())
        {
            m_recorder.endOverdub();
        }
        else if (isPlaying())
        {
            m_recorder.beginOverdub();
        }
    }

    [[nodiscard]] static float blockPeak(const AbacDsp::AudioBuffer<2, BlockSize>& in) noexcept
    {
        float peak = 0.f;
        for (size_t i = 0; i < BlockSize; ++i)
        {
            peak = std::max(peak, std::max(std::abs(in(i, 0)), std::abs(in(i, 1))));
        }
        return peak;
    }

    void renderClick(std::array<float, BlockSize>& click)
    {
        const bool active = isRecording() || isPlaying() || m_armed;
        for (size_t i = 0; i < BlockSize; ++i)
        {
            const auto event = m_seq.advance();
            m_barPos[i] = event.beatIndexInBar * m_seq.samplesPerBeat() + event.beatSamplePos;
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

    AbacDsp::LoopRecorder<BlockSize> m_recorder;
    AbacDsp::BeatSequencer m_seq;
    AbacDsp::ClickGenerator m_click;
    AbacDsp::SimpleSpectrogram m_recordSpectrogram;

    std::vector<float> m_visualWave;
    std::vector<float> m_preparedWave;
    std::array<size_t, BlockSize> m_barPos{};
    size_t m_visualWindowSize{0};

    std::atomic<float> m_bpm{120.f};
    std::atomic<float> m_swing{50.f};
    std::atomic<float> m_clickVol{-12.f};
    std::atomic<float> m_loopVol{0.f};
    std::atomic<float> m_recThreshold{-36.f};
    std::atomic<bool> m_threshRecReq{false};
    float m_loopGain{1.f};
    float m_recThresholdLinear{0.0158f};
    bool m_armed{false};

    // Noise gate for the bar display feed: gate opens above ~-40 dBFS, releases slowly.
    static constexpr float kVisualGate{0.01f};
    static constexpr float kVisualGateRelease{0.9997f};
    float m_visEnv{0.f};
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
};
