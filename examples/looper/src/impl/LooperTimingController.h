#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>

#include "EffectBase.h"
#include "Generators/BeatSequencer.h"
#include "Generators/MeterTimeline.h"

// BPM application, host sync, time-signature installation, meter-timeline
// finalization, and count-in/timekeeper reset: the parts of the transport
// clock that don't need to know whether the looper is recording, playing, or
// overdubbing.
//
// Deciding *when* a pending time-signature request actually applies does
// depend on that transport state (queued mid-recording, replayed from the
// take's own meter timeline during playback), so that stays in LooperImpl --
// see applyTimeSignatureRequest()/applyMeterAtBarBoundary().
class LooperTimingController
{
  public:
    // Standard meters selectable via the Time Sig control, index-matched to its
    // dropdown. eighthUnit means the beat is an eighth note, not a quarter note:
    // the metronome's per-beat tempo doubles the entered BPM to keep an eighth
    // note at half a quarter note's duration.
    struct TimeSignatureSpec
    {
        size_t beatsPerBar;
        bool eighthUnit;
    };
    static constexpr auto kTimeSignatures = std::to_array<TimeSignatureSpec>({
        {2, false},
        {3, false},
        {4, false},
        {5, false},
        {6, false},
        {7, false},
        {5, true},
        {6, true},
        {7, true},
        {9, true},
        {11, true},
        {13, true},
        {15, true},
    });
    static constexpr int kDefaultTimeSignature = 2; // index of 4/4

    LooperTimingController(AbacDsp::BeatSequencer& seq, AbacDsp::MeterTimeline& meterTimeline, float& appliedBpm,
                           bool& eighthNoteUnit, int& appliedTimeSignature, size_t& finalizedBarCount, bool& countingIn,
                           int& countInBarsOffset, uint64_t& countInEndTickAbs, bool& suppressNextClick,
                           const float sampleRate)
        : m_seq(seq)
        , m_meterTimeline(meterTimeline)
        , m_appliedBpm(appliedBpm)
        , m_eighthNoteUnit(eighthNoteUnit)
        , m_appliedTimeSignature(appliedTimeSignature)
        , m_finalizedBarCount(finalizedBarCount)
        , m_countingIn(countingIn)
        , m_countInBarsOffset(countInBarsOffset)
        , m_countInEndTickAbs(countInEndTickAbs)
        , m_suppressNextClick(suppressNextClick)
        , m_sampleRate(sampleRate)
    {
    }

    void syncToHostTransport(const EffectBase::HostTransport& transport)
    {
        if (!transport.isPlaying || transport.updateCount == m_lastSyncedUpdateCount)
        {
            return;
        }
        m_lastSyncedUpdateCount = transport.updateCount;
        const float bpm = std::clamp(static_cast<float>(transport.bpm), 20.f, 999.f);
        applyTimeSignatureAwareBpm(bpm);
        m_seq.syncToPpq(transport.ppqPosition);
    }

    // bpm is always the quarter-note tempo (standard convention, matching host
    // transport bpm too); an eighth-note meter doubles what actually reaches the
    // clock so an eighth note stays half a quarter note's duration.
    void applyTimeSignatureAwareBpm(const float bpm) noexcept
    {
        m_appliedBpm = bpm;
        m_seq.setBpm(m_eighthNoteUnit ? bpm * 2.f : bpm);
    }

    void installTimeSignature(const int index) noexcept
    {
        m_appliedTimeSignature = index;
        const auto& sig = kTimeSignatures[static_cast<size_t>(index)];
        m_seq.setBeatsPerBar(sig.beatsPerBar);
        m_eighthNoteUnit = sig.eighthUnit;
        applyTimeSignatureAwareBpm(m_appliedBpm);
    }

    // Derives the take's final bar count from its known frame length by walking
    // its recorded meter timeline (robust regardless of bar-lock stop/catch-up
    // slop), then builds the frame map used to size the loop's outer ring.
    void finalizeMeterTimeline(const size_t loopLengthFrames)
    {
        const float samplesPerQuarterBeat = m_sampleRate * 60.f / m_appliedBpm;
        m_finalizedBarCount = m_meterTimeline.barCountForFrames(loopLengthFrames, samplesPerQuarterBeat);
        m_meterTimeline.buildFrameMap(m_finalizedBarCount, samplesPerQuarterBeat);
    }

    // Every fresh take starts at bar 1 beat 1. suppressFirstClick distinguishes
    // a pure phase resync (Play-resume, freeze completion: the next beatStart
    // is an artifact of the jump, not a real beat) from a fresh Record start,
    // where beat 1 is a real downbeat the performer needs to hear.
    void resetTimekeeper(const bool suppressFirstClick) noexcept
    {
        m_seq.reset();
        m_suppressNextClick = suppressFirstClick;
        m_countInBarsOffset = 0;
    }

    // Takes priority over threshold-arming: count-in always auto-starts once
    // countInBars bar ticks elapse, no input crossing needed.
    void beginCountIn(const int countInBars, const uint64_t absPos)
    {
        m_countingIn = true;
        m_countInBarsOffset = countInBars;
        const size_t spb = m_seq.samplesPerBeat();
        const size_t samplesPerBar = spb * m_seq.beatsPerBar();
        long off = (spb > 0) ? m_seq.samplesToNearestBar() : 0;
        if (off <= 0)
        {
            // Nearest tick is behind us, or we're sitting right on one: either
            // way, a full bar of count-in must still elapse before the next one.
            off += static_cast<long>(samplesPerBar);
        }
        m_countInEndTickAbs =
            absPos + static_cast<uint64_t>(off) + static_cast<uint64_t>(countInBars - 1) * samplesPerBar;
    }

    // Resyncs the timekeeper to bar 1 beat 1 of the loop's own recorded meter
    // timeline (force-installing bar 0's meter regardless of whatever m_seq was
    // left at). Used both when Play explicitly (re)starts a stopped loop and
    // when a bar-locked recording auto-transitions straight into playback.
    void resyncTimekeeperToLoopStart()
    {
        resetTimekeeper(true);
        if (!m_meterTimeline.empty())
        {
            const auto& seg0 = m_meterTimeline.segmentForBar(0);
            m_seq.setBeatsPerBar(seg0.beatsPerBar);
            m_eighthNoteUnit = seg0.eighthUnit;
            applyTimeSignatureAwareBpm(m_appliedBpm);
        }
    }

  private:
    AbacDsp::BeatSequencer& m_seq;
    AbacDsp::MeterTimeline& m_meterTimeline;
    float& m_appliedBpm;
    bool& m_eighthNoteUnit;
    int& m_appliedTimeSignature;
    size_t& m_finalizedBarCount;
    bool& m_countingIn;
    int& m_countInBarsOffset;
    uint64_t& m_countInEndTickAbs;
    bool& m_suppressNextClick;
    float m_sampleRate;
    uint64_t m_lastSyncedUpdateCount{0};
};
