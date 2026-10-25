#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <optional>
#include <span>
#include <vector>

#include "Analysis/YinPitchDetector.h"
#include "Numbers/Convert.h"

namespace AbacDsp
{

/// @ingroup analysis
/// @brief One note found in a monophonic recording. midiNote is fractional; 0 means no pitch was readable.
struct PlayedNote
{
    float onsetSeconds{0.f};
    float offsetSeconds{0.f};
    float midiNote{0.f};
    float pitchConfidence{0.f};
    float peakDb{-120.f};
    float attackRmsDb{-120.f};

    [[nodiscard]] bool pitched() const noexcept
    {
        return pitchConfidence > 0.f;
    }
};

/// @ingroup analysis
/// @brief Tuning of NoteSegmenter; the defaults suit a guitar-like instrument or a voice.
struct NoteSegmenterConfig
{
    float minHz{70.f};
    float maxHz{1500.f};
    float referenceA4{440.f};
    std::optional<float> noiseFloorDb;
    float noiseMarginDb{10.f};
    float absoluteGateDb{-66.f};
    float onsetRiseDb{6.f};
    float minNoteMs{40.f};
    float releaseDropDb{20.f};
    float pitchHopHz{50.f};
    float minPitchConfidence{0.5f};
    float pitchSkipMs{40.f};
    float pitchSplitMs{60.f};
    float pitchSplitSemitones{0.75f};
};

/**
 * @ingroup analysis
 * @brief Offline note finder for a monophonic recording: onsets, pitch, length and level per note.
 *
 * An onset is a rise of the short-time level over its own recent minimum; the coarse frame is then
 * refined on the waveform to the sample where the attack passes a tenth of its peak. A note ends
 * when its level has fallen releaseDropDb below the peak, drops under the gate or the next note
 * starts. Pitch is the median of YIN estimates after the attack. A pitch that moves to another
 * semitone and stays there splits a note, which covers legato playing where the level does not dip.
 * Limits: a new attack that is not at least onsetRiseDb above the ringing of the previous note is
 * missed, and a pitch split is only as exact as the YIN window (tens of milliseconds).
 * @see de Cheveigne and Kawahara, "YIN, a fundamental frequency estimator for speech and music",
 *      JASA 111(4), 2002.
 */
class NoteSegmenter
{
  public:
    explicit NoteSegmenter(const float sampleRate, const NoteSegmenterConfig& config = {})
        : m_sampleRate(sampleRate)
        , m_config(config)
        , m_hop(std::max<size_t>(1, msToSamples(kFrameHopMs)))
    {
    }

    [[nodiscard]] std::vector<PlayedNote> analyze(const std::span<const float> audio) const
    {
        const auto db = frameLevelsDb(audio);
        if (db.size() < 3)
        {
            return {};
        }
        const float gate = gateDb(db);
        const auto onsets = findOnsets(audio, db, gate);
        const auto pitchTrack = trackPitch(audio);
        std::vector<PlayedNote> notes;
        for (size_t i = 0; i < onsets.size(); ++i)
        {
            const size_t nextOnset = i + 1 < onsets.size() ? onsets[i + 1].sample : audio.size();
            const size_t end = noteEnd(db, onsets[i], nextOnset, gate, audio.size());
            appendNotes(audio, pitchTrack, onsets[i].sample, end, notes);
        }
        return notes;
    }

  private:
    static constexpr float kFrameHopMs{2.5f};
    static constexpr float kLookbackMs{32.f};
    static constexpr float kSearchBeforeMs{40.f};
    static constexpr float kSearchAfterMs{15.f};
    static constexpr float kPeakWindowMs{80.f};
    static constexpr float kAttackLeadMs{6.f};
    static constexpr float kAttackWindowMs{100.f};
    static constexpr float kOnsetFraction{0.1f};
    static constexpr float kFloorPercentile{0.02f};
    static constexpr float kMinFloorDb{-90.f};
    static constexpr float kMaxFloorDb{-40.f};
    static constexpr float kSilenceDb{-120.f};

    struct Onset
    {
        size_t frame{0};
        size_t sample{0};
    };

    struct PitchEstimate
    {
        size_t centre{0};
        float midi{0.f};
        float confidence{0.f};
    };

    [[nodiscard]] size_t msToSamples(const float ms) const noexcept
    {
        return static_cast<size_t>(std::max(0.f, ms) * 0.001f * m_sampleRate);
    }

    [[nodiscard]] size_t msToFrames(const float ms) const noexcept
    {
        return std::max<size_t>(1, msToSamples(ms) / m_hop);
    }

    [[nodiscard]] size_t frameCentre(const size_t frame) const noexcept
    {
        return frame * m_hop + m_hop;
    }

    [[nodiscard]] static float toDb(const double amplitude) noexcept
    {
        return std::max(kSilenceDb, static_cast<float>(20.0 * std::log10(amplitude + 1e-9)));
    }

    // Frame k covers samples [k * hop, k * hop + 2 * hop).
    [[nodiscard]] std::vector<float> frameLevelsDb(const std::span<const float> audio) const
    {
        const size_t window = 2 * m_hop;
        std::vector<float> db;
        for (size_t start = 0; start + window <= audio.size(); start += m_hop)
        {
            double sum = 0.0;
            for (const float v : audio.subspan(start, window))
            {
                sum += static_cast<double>(v) * static_cast<double>(v);
            }
            db.push_back(toDb(std::sqrt(sum / static_cast<double>(window))));
        }
        return db;
    }

    [[nodiscard]] float gateDb(const std::vector<float>& db) const
    {
        auto sorted = db;
        const auto rank = static_cast<size_t>(kFloorPercentile * static_cast<float>(sorted.size() - 1));
        std::ranges::nth_element(sorted, sorted.begin() + static_cast<std::ptrdiff_t>(rank));
        const float floorDb = std::clamp(m_config.noiseFloorDb.value_or(sorted[rank]), kMinFloorDb, kMaxFloorDb);
        return std::max(floorDb + m_config.noiseMarginDb, m_config.absoluteGateDb);
    }

    // The recording is taken to start from silence, so a note sounding at its very first frame is an onset too.
    [[nodiscard]] std::vector<size_t> findOnsetFrames(const std::vector<float>& frames, const float gate) const
    {
        const size_t lookback = msToFrames(kLookbackMs);
        std::vector<float> db(lookback, kSilenceDb);
        db.insert(db.end(), frames.begin(), frames.end());
        const size_t minGap = msToFrames(m_config.minNoteMs);
        std::vector<size_t> onsets;
        bool waitingForPeak = false;
        for (size_t k = lookback; k < db.size(); ++k)
        {
            waitingForPeak = waitingForPeak && db[k] >= db[k - 1];
            const bool tooSoon = !onsets.empty() && k - lookback - onsets.back() < minGap;
            if (waitingForPeak || tooSoon || db[k] < gate || db[k] <= db[k - 1])
            {
                continue;
            }
            const auto from = db.begin() + static_cast<std::ptrdiff_t>(k - lookback);
            const float localMin = *std::min_element(from, db.begin() + static_cast<std::ptrdiff_t>(k));
            if (db[k] - localMin >= m_config.onsetRiseDb)
            {
                onsets.push_back(k - lookback);
                waitingForPeak = true;
            }
        }
        return onsets;
    }

    [[nodiscard]] std::vector<Onset> findOnsets(const std::span<const float> audio, const std::vector<float>& db,
                                                const float gate) const
    {
        const auto frames = findOnsetFrames(db, gate);
        std::vector<Onset> onsets;
        size_t lowerBound = 0;
        for (size_t i = 0; i < frames.size(); ++i)
        {
            const size_t upperBound = i + 1 < frames.size() ? frameCentre(frames[i + 1]) : audio.size();
            const size_t sample = refineOnset(audio, db, frames[i], lowerBound, upperBound);
            onsets.push_back({frames[i], sample});
            lowerBound = sample + msToSamples(m_config.minNoteMs);
        }
        return onsets;
    }

    [[nodiscard]] static float maxAbs(const std::span<const float> audio, const size_t from, const size_t to) noexcept
    {
        float peak = 0.f;
        for (size_t i = from; i < std::min(to, audio.size()); ++i)
        {
            peak = std::max(peak, std::abs(audio[i]));
        }
        return peak;
    }

    // The first sample where the attack has covered a tenth of the way from the level before it to its peak.
    // That level is taken at the quietest frame in front of the onset, so a note ringing on is not mistaken for it.
    [[nodiscard]] size_t refineOnset(const std::span<const float> audio, const std::vector<float>& db,
                                     const size_t coarseFrame, const size_t lowerBound,
                                     const size_t upperBound) const noexcept
    {
        const size_t coarse = frameCentre(coarseFrame);
        const size_t lookback = msToFrames(kSearchBeforeMs);
        size_t quietFrame = coarseFrame > lookback ? coarseFrame - lookback : 0;
        for (size_t f = quietFrame; f <= coarseFrame; ++f)
        {
            quietFrame = db[f] <= db[quietFrame] ? f : quietFrame;
        }
        const size_t searchStart = std::max(lowerBound, quietFrame * m_hop);
        const size_t searchEnd = std::min(audio.size(), coarse + msToSamples(kSearchAfterMs));
        const size_t peakEnd = std::min(upperBound, coarse + msToSamples(kPeakWindowMs));
        const size_t baseWindow =
            std::clamp(static_cast<size_t>(1.2f * m_sampleRate / m_config.minHz), msToSamples(2.f), msToSamples(30.f));
        const size_t quietEnd = coarse > msToSamples(kAttackLeadMs) ? coarse - msToSamples(kAttackLeadMs) : 0;
        const float base = maxAbs(audio, searchStart, std::min(searchStart + baseWindow, quietEnd));
        const float peak = maxAbs(audio, searchStart, peakEnd);
        const float threshold = base + kOnsetFraction * (peak - base);
        for (size_t s = searchStart; s < searchEnd; ++s)
        {
            if (std::abs(audio[s]) >= threshold)
            {
                return s;
            }
        }
        return std::min(coarse, audio.size());
    }

    [[nodiscard]] size_t noteEnd(const std::vector<float>& db, const Onset& onset, const size_t nextOnsetSample,
                                 const float gate, const size_t audioSize) const
    {
        const size_t attackFrames = msToFrames(kAttackWindowMs);
        const size_t lastFrame = std::min(db.size(), onset.frame + attackFrames);
        const auto peakIt = std::max_element(db.begin() + static_cast<std::ptrdiff_t>(onset.frame),
                                             db.begin() + static_cast<std::ptrdiff_t>(lastFrame));
        const float limit = std::max(gate, *peakIt - m_config.releaseDropDb);
        for (auto it = peakIt; it != db.end(); ++it)
        {
            const size_t frame = static_cast<size_t>(std::distance(db.begin(), it));
            if (*it < limit || frameCentre(frame) >= nextOnsetSample)
            {
                return std::min({frameCentre(frame), nextOnsetSample, audioSize});
            }
        }
        return std::min(nextOnsetSample, audioSize);
    }

    [[nodiscard]] std::vector<PitchEstimate> trackPitch(const std::span<const float> audio) const
    {
        YinPitchDetector yin{m_sampleRate, m_config.minHz, m_config.maxHz, m_config.pitchHopHz};
        const size_t halfWindow = yin.getBufferSize() / 2;
        std::vector<PitchEstimate> track;
        for (size_t i = 0; i < audio.size(); ++i)
        {
            const float hz = yin.step(audio[i]);
            if (yin.hasNewPitch() && hz > 0.f && yin.getLastConfidence() >= m_config.minPitchConfidence)
            {
                const float midi = Convert::frequencyToNote<float>(hz, m_config.referenceA4);
                track.push_back({i + 1 > halfWindow ? i + 1 - halfWindow : 0, midi, yin.getLastConfidence()});
            }
        }
        return track;
    }

    [[nodiscard]] std::vector<PitchEstimate> estimatesIn(const std::vector<PitchEstimate>& track, const size_t from,
                                                         const size_t to) const
    {
        const size_t skipped = from + msToSamples(m_config.pitchSkipMs);
        std::vector<PitchEstimate> inRange;
        for (const auto& estimate : track)
        {
            if (estimate.centre >= skipped && estimate.centre < to)
            {
                inRange.push_back(estimate);
            }
        }
        return inRange;
    }

    // Sample positions where the pitch settled on another semitone, found with hysteresis so vibrato does not split.
    [[nodiscard]] std::vector<size_t> pitchChanges(const std::vector<PitchEstimate>& estimates) const
    {
        const auto runLength = static_cast<size_t>(std::ceil(m_config.pitchSplitMs * 0.001f * m_config.pitchHopHz));
        std::vector<size_t> changes;
        if (estimates.empty())
        {
            return changes;
        }
        int current = static_cast<int>(std::lround(estimates.front().midi));
        int candidate = current;
        size_t count = 0;
        for (size_t i = 0; i < estimates.size(); ++i)
        {
            const int semitone = static_cast<int>(std::lround(estimates[i].midi));
            const bool away = std::abs(estimates[i].midi - static_cast<float>(current)) > m_config.pitchSplitSemitones;
            count = away && semitone == candidate ? count + 1 : (away ? 1 : 0);
            candidate = away ? semitone : current;
            if (count >= runLength)
            {
                changes.push_back(estimates[i + 1 - count].centre);
                current = semitone;
                count = 0;
            }
        }
        return changes;
    }

    void appendNotes(const std::span<const float> audio, const std::vector<PitchEstimate>& track, const size_t onset,
                     const size_t end, std::vector<PlayedNote>& notes) const
    {
        if (end < onset + msToSamples(m_config.minNoteMs))
        {
            return;
        }
        const auto estimates = estimatesIn(track, onset, end);
        std::vector<size_t> starts{onset};
        for (const size_t change : pitchChanges(estimates))
        {
            starts.push_back(change);
        }
        for (size_t i = 0; i < starts.size(); ++i)
        {
            const size_t stop = i + 1 < starts.size() ? starts[i + 1] : end;
            notes.push_back(describeNote(audio, track, starts[i], stop));
        }
    }

    [[nodiscard]] PlayedNote describeNote(const std::span<const float> audio, const std::vector<PitchEstimate>& track,
                                          const size_t onset, const size_t end) const
    {
        PlayedNote note;
        note.onsetSeconds = static_cast<float>(onset) / m_sampleRate;
        note.offsetSeconds = static_cast<float>(end) / m_sampleRate;
        const size_t attackEnd = std::min(end, onset + msToSamples(kAttackWindowMs));
        double sum = 0.0;
        float peak = 0.f;
        for (const float v : audio.subspan(onset, attackEnd - onset))
        {
            sum += static_cast<double>(v) * static_cast<double>(v);
            peak = std::max(peak, std::abs(v));
        }
        const double count = static_cast<double>(std::max<size_t>(1, attackEnd - onset));
        note.attackRmsDb = toDb(std::sqrt(sum / count));
        note.peakDb = toDb(peak);
        auto estimates = estimatesIn(track, onset, end);
        if (!estimates.empty())
        {
            std::ranges::sort(estimates, {}, &PitchEstimate::midi);
            note.midiNote = estimates[estimates.size() / 2].midi;
            note.pitchConfidence = estimates[estimates.size() / 2].confidence;
        }
        return note;
    }

    float m_sampleRate;
    NoteSegmenterConfig m_config;
    size_t m_hop;
};

}
