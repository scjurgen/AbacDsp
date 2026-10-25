#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

#include "Analysis/YinPitchDetector.h"
#include "Numbers/Convert.h"

namespace AbacDsp
{

/// @ingroup analysis
/// @brief A note that should be heard: the time it should start and its pitch as a MIDI note number.
struct ExpectedNote
{
    float onsetSeconds{0.f};
    float midiNote{60.f};
};

/// @ingroup analysis
enum class PitchVerdict : uint8_t
{
    Right,
    Sharp,
    Flat,
    WrongNote,
    NoPitch,
    Missing,
};

/// @ingroup analysis
/// @brief What was found for one expected note. Times are in the recording; midiNote is fractional, 0 if unreadable.
struct VerifiedNote
{
    bool found{false};
    float expectedSeconds{0.f};
    float onsetSeconds{0.f};
    float offsetSeconds{0.f};
    float midiNote{0.f};
    float pitchErrorCents{0.f};
    float pitchConfidence{0.f};
    float peakDb{-120.f};
    float attackRmsDb{-120.f};
    PitchVerdict verdict{PitchVerdict::Missing};

    [[nodiscard]] float onsetErrorSeconds() const noexcept
    {
        return onsetSeconds - expectedSeconds;
    }
};

/// @ingroup analysis
/// @brief A sound above the threshold where no note was expected; midiNote is 0 when it has no readable pitch.
struct UnexpectedSound
{
    float fromSeconds{0.f};
    float toSeconds{0.f};
    float peakDb{-120.f};
    float midiNote{0.f};
    float pitchConfidence{0.f};
};

/// @ingroup analysis
struct VerificationReport
{
    std::vector<VerifiedNote> notes;
    std::vector<UnexpectedSound> unexpected;
    float alignmentSeconds{0.f};
    float medianNoteDb{-120.f};
    float thresholdDb{-120.f};
    float loudestIgnoredDb{-120.f};
    size_t shortBursts{0};

    // How far the loudest ignored sound lies under the threshold.
    [[nodiscard]] float marginDb() const noexcept
    {
        return thresholdDb - loudestIgnoredDb;
    }
};

/// @ingroup analysis
/// @brief Tuning of NoteVerifier; the defaults suit a plucked string instrument at moderate tempo.
struct NoteVerifierConfig
{
    float referenceA4{440.f};
    float windowSeconds{0.225f};
    float alignSearchSeconds{0.4f};
    float gateDb{-66.f};
    float riseDb{6.f};
    float noteBandDb{12.f};
    float unexpectedBelowMedianDb{15.f};
    float unexpectedMinMs{50.f};
    float pitchSkipMs{40.f};
    float pitchSpanMs{300.f};
    float pitchRangeFactor{2.2f};
    float minPitchConfidence{0.5f};
    float sharpFlatCents{25.f};
    float wrongNoteCents{50.f};
    float unexpectedMinHz{60.f};
    float unexpectedMaxHz{1500.f};
};

/**
 * @ingroup analysis
 * @brief Checks a monophonic recording against the notes that should have been played.
 *
 * Each expected note gets a time window; in it the strongest level rise is the note and its onset is where
 * the level first comes within noteBandDb of the peak, so quieter sounds in front of it do not count. Pitch
 * is measured within one octave of the expected one, so a wrong note is reported, not forced. A common
 * timing offset is fitted first. Sound unexpectedBelowMedianDb under the median note level, outside the
 * notes and unexpectedMinMs long, is reported; weaker sound only sets the margin to the threshold.
 * Limits: a note not riseDb above the ringing before it is missed, a louder sound inside a window competes
 * with the note, and one note must be found to have a reference level.
 * @see de Cheveigne and Kawahara, "YIN, a fundamental frequency estimator for speech and music",
 *      JASA 111(4), 2002.
 */
class NoteVerifier
{
  public:
    explicit NoteVerifier(const float sampleRate, const NoteVerifierConfig& config = {})
        : m_sampleRate(sampleRate)
        , m_config(config)
        , m_hop(std::max<size_t>(1, msToSamples(kFrameHopMs)))
    {
    }

    [[nodiscard]] VerificationReport verify(const std::span<const float> audio,
                                            const std::span<const ExpectedNote> expected) const
    {
        VerificationReport report;
        report.notes.resize(expected.size());
        const Signal signal = prepare(audio);
        for (size_t i = 0; i < expected.size(); ++i)
        {
            report.notes[i].expectedSeconds = expected[i].onsetSeconds;
        }
        if (expected.empty() || signal.db.size() < kMinFrames)
        {
            return report;
        }
        report.alignmentSeconds = static_cast<float>(locateAll(signal, expected, report.notes));
        const auto peaks = foundPeaks(report.notes);
        if (peaks.empty())
        {
            return report;
        }
        report.medianNoteDb = median(peaks);
        report.thresholdDb = report.medianNoteDb - m_config.unexpectedBelowMedianDb;
        const auto regions = describeNotes(signal, expected, report);
        scanUnexpected(signal, regions, report);
        return report;
    }

  private:
    static constexpr float kFrameHopMs{2.5f};
    static constexpr float kLookbackMs{32.f};
    static constexpr float kLevelWindowMs{30.f};
    static constexpr float kRefineRmsMs{1.f};
    static constexpr float kRefineSearchMs{100.f};
    static constexpr float kRefinePeakMs{50.f};
    static constexpr float kOnsetFraction{0.1f};
    static constexpr float kRegionLeadMs{40.f};
    static constexpr float kEdgeGuardMs{20.f};
    static constexpr float kEventGapMs{20.f};
    static constexpr float kAttackWindowMs{100.f};
    static constexpr float kPeakSearchMs{150.f};
    static constexpr float kAlignStepSeconds{0.005f};
    static constexpr float kAlignSlackSeconds{0.03f};
    static constexpr float kAlignRiseCapDb{25.f};
    static constexpr float kMinYinHz{20.f};
    static constexpr float kSilenceDb{-120.f};
    static constexpr size_t kAlignPasses{2};
    static constexpr size_t kMinFrames{3};

    struct Signal
    {
        std::span<const float> audio;
        std::vector<float> db;
        std::vector<float> rise;
        std::vector<double> squares;
    };

    struct Located
    {
        size_t frame{0};
        double onsetSeconds{0.0};
        float peakDb{kSilenceDb};
    };

    struct Region
    {
        double from{0.0};
        double to{0.0};
    };

    struct PitchReading
    {
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

    [[nodiscard]] double frameTime(const size_t frame) const noexcept
    {
        return static_cast<double>(frame * m_hop + m_hop) / static_cast<double>(m_sampleRate);
    }

    [[nodiscard]] size_t frameOf(const Signal& signal, const double seconds) const noexcept
    {
        const double frame =
            (seconds * static_cast<double>(m_sampleRate) - static_cast<double>(m_hop)) / static_cast<double>(m_hop);
        return static_cast<size_t>(std::clamp(frame, 0.0, static_cast<double>(signal.db.size() - 1)));
    }

    [[nodiscard]] static float toDb(const double amplitude) noexcept
    {
        return std::max(kSilenceDb, static_cast<float>(20.0 * std::log10(amplitude + 1e-9)));
    }

    [[nodiscard]] static float median(std::vector<float> values)
    {
        std::ranges::sort(values);
        return values[values.size() / 2];
    }

    // Frame k covers samples [k * hop, k * hop + 2 * hop).
    [[nodiscard]] Signal prepare(const std::span<const float> audio) const
    {
        Signal signal{audio, {}, {}, std::vector<double>(audio.size() + 1, 0.0)};
        for (size_t i = 0; i < audio.size(); ++i)
        {
            signal.squares[i + 1] = signal.squares[i] + static_cast<double>(audio[i]) * static_cast<double>(audio[i]);
        }
        const size_t window = 2 * m_hop;
        for (size_t start = 0; start + window <= audio.size(); start += m_hop)
        {
            signal.db.push_back(toDb(
                std::sqrt((signal.squares[start + window] - signal.squares[start]) / static_cast<double>(window))));
        }
        const size_t lookback = msToFrames(kLookbackMs);
        signal.rise.assign(signal.db.size(), 0.f);
        for (size_t k = 1; k < signal.db.size(); ++k)
        {
            const auto from = signal.db.begin() + static_cast<std::ptrdiff_t>(k > lookback ? k - lookback : 0);
            signal.rise[k] = signal.db[k] - *std::min_element(from, signal.db.begin() + static_cast<std::ptrdiff_t>(k));
        }
        return signal;
    }

    // Rms over a millisecond starting at the sample.
    [[nodiscard]] double shortRms(const Signal& signal, const size_t sample) const noexcept
    {
        const size_t length = std::max<size_t>(1, msToSamples(kRefineRmsMs));
        const size_t from = std::min(sample, signal.audio.size());
        const size_t to = std::min(sample + length, signal.audio.size());
        return std::sqrt((signal.squares[to] - signal.squares[from]) / static_cast<double>(length));
    }

    // The first sample where the attack has covered a tenth of the way from the level in front of it to its peak.
    [[nodiscard]] double refineOnset(const Signal& signal, const size_t frame) const noexcept
    {
        const size_t start = (frame > 2 ? frame - 2 : 0) * m_hop;
        const size_t peakEnd = std::min(signal.audio.size(), frame * m_hop + msToSamples(kRefinePeakMs));
        double peak = 0.0;
        for (size_t i = frame * m_hop; i < peakEnd; ++i)
        {
            peak = std::max(peak, shortRms(signal, i));
        }
        const double base = shortRms(signal, start);
        const double threshold = base + static_cast<double>(kOnsetFraction) * (peak - base);
        const size_t searchEnd = std::min(signal.audio.size(), start + msToSamples(kRefineSearchMs));
        for (size_t i = start; i < searchEnd; ++i)
        {
            if (shortRms(signal, i) >= threshold)
            {
                return static_cast<double>(i) / static_cast<double>(m_sampleRate);
            }
        }
        return static_cast<double>(frame * m_hop) / static_cast<double>(m_sampleRate);
    }

    // The strongest rise in the window is the note; its onset is where the level first came within the band of its
    // peak.
    [[nodiscard]] std::optional<Located> locate(const Signal& signal, const double from, const double to) const
    {
        const size_t first = frameOf(signal, from);
        const size_t last = frameOf(signal, to);
        size_t strongest = 0;
        float strongestRise = 0.f;
        for (size_t k = std::max<size_t>(first, 1); k <= last; ++k)
        {
            if (signal.db[k] >= m_config.gateDb && signal.rise[k] > strongestRise)
            {
                strongestRise = signal.rise[k];
                strongest = k;
            }
        }
        if (strongestRise < m_config.riseDb)
        {
            return std::nullopt;
        }
        Located located;
        const size_t levelEnd = std::min(strongest + msToFrames(kLevelWindowMs), signal.db.size() - 1);
        for (size_t k = strongest; k <= levelEnd; ++k)
        {
            located.peakDb = std::max(located.peakDb, signal.db[k]);
        }
        located.frame = strongest;
        while (located.frame > first && signal.db[located.frame - 1] >= located.peakDb - m_config.noteBandDb)
        {
            --located.frame;
        }
        located.onsetSeconds = refineOnset(signal, located.frame);
        return located;
    }

    // Half the distance to the neighbouring expected notes, at most windowSeconds either side.
    [[nodiscard]] std::pair<double, double> windowOf(const std::span<const ExpectedNote> expected, const size_t index,
                                                     const double offset) const noexcept
    {
        const double centre = static_cast<double>(expected[index].onsetSeconds) + offset;
        const auto limit = static_cast<double>(m_config.windowSeconds);
        const double before = index > 0 ? std::min(limit, 0.5 * static_cast<double>(expected[index].onsetSeconds -
                                                                                    expected[index - 1].onsetSeconds))
                                        : limit;
        const double after = index + 1 < expected.size()
                                 ? std::min(limit, 0.5 * static_cast<double>(expected[index + 1].onsetSeconds -
                                                                             expected[index].onsetSeconds))
                                 : limit;
        return {std::max(0.0, centre - before), centre + after};
    }

    void locateNotes(const Signal& signal, const std::span<const ExpectedNote> expected, const double offset,
                     std::vector<VerifiedNote>& notes) const
    {
        for (size_t i = 0; i < expected.size(); ++i)
        {
            auto& note = notes[i];
            note = VerifiedNote{};
            note.expectedSeconds = static_cast<float>(static_cast<double>(expected[i].onsetSeconds) + offset);
            const auto [from, to] = windowOf(expected, i, offset);
            if (const auto located = locate(signal, from, to))
            {
                note.found = true;
                note.onsetSeconds = static_cast<float>(located->onsetSeconds);
                note.peakDb = located->peakDb;
                note.verdict = PitchVerdict::NoPitch;
            }
        }
    }

    // The offset that puts the strongest level rises on the expected onsets.
    [[nodiscard]] double combOffset(const Signal& signal, const std::span<const ExpectedNote> expected) const
    {
        const size_t slack = msToFrames(kAlignSlackSeconds * 1000.f);
        double bestScore = -1.0;
        double best = 0.0;
        const auto steps = static_cast<int>(std::lround(m_config.alignSearchSeconds / kAlignStepSeconds));
        for (int s = -steps; s <= steps; ++s)
        {
            const double offset = static_cast<double>(s) * static_cast<double>(kAlignStepSeconds);
            double score = 0.0;
            for (const auto& note : expected)
            {
                const size_t centre = frameOf(signal, std::max(0.0, static_cast<double>(note.onsetSeconds) + offset));
                float strongest = 0.f;
                for (size_t k = std::max<size_t>(1, centre > slack ? centre - slack : 0);
                     k <= std::min(centre + slack, signal.db.size() - 1); ++k)
                {
                    strongest = std::max(strongest, signal.rise[k]);
                }
                score += static_cast<double>(std::min(strongest, kAlignRiseCapDb));
            }
            if (score > bestScore + 1e-9 || (std::abs(score - bestScore) <= 1e-9 && std::abs(offset) < std::abs(best)))
            {
                bestScore = score;
                best = offset;
            }
        }
        return best;
    }

    [[nodiscard]] static std::optional<double> medianDelta(const std::span<const ExpectedNote> expected,
                                                           const std::vector<VerifiedNote>& notes)
    {
        std::vector<float> deltas;
        for (size_t i = 0; i < notes.size(); ++i)
        {
            if (notes[i].found)
            {
                deltas.push_back(notes[i].onsetSeconds -
                                 static_cast<float>(static_cast<double>(expected[i].onsetSeconds)));
            }
        }
        return deltas.empty() ? std::nullopt : std::optional<double>{static_cast<double>(median(deltas))};
    }

    [[nodiscard]] double locateAll(const Signal& signal, const std::span<const ExpectedNote> expected,
                                   std::vector<VerifiedNote>& notes) const
    {
        double offset = combOffset(signal, expected);
        locateNotes(signal, expected, offset, notes);
        for (size_t pass = 0; pass < kAlignPasses; ++pass)
        {
            if (const auto delta = medianDelta(expected, notes))
            {
                offset = *delta;
                locateNotes(signal, expected, offset, notes);
            }
        }
        return offset;
    }

    [[nodiscard]] static std::vector<float> foundPeaks(const std::vector<VerifiedNote>& notes)
    {
        std::vector<float> peaks;
        for (const auto& note : notes)
        {
            if (note.found)
            {
                peaks.push_back(note.peakDb);
            }
        }
        return peaks;
    }

    [[nodiscard]] PitchReading measurePitch(const Signal& signal, const size_t from, const size_t to, const float minHz,
                                            const float maxHz, const float skipMs) const
    {
        YinPitchDetector yin{m_sampleRate, std::max(minHz, kMinYinHz), std::min(maxHz, 0.25f * m_sampleRate)};
        const size_t skip = msToSamples(skipMs);
        std::vector<PitchReading> readings;
        for (size_t i = from; i < std::min(to, signal.audio.size()); ++i)
        {
            const float hz = yin.step(signal.audio[i]);
            const bool settled = i + 1 - from >= yin.getBufferSize() + skip;
            if (yin.hasNewPitch() && settled && hz > 0.f && yin.getLastConfidence() >= m_config.minPitchConfidence)
            {
                readings.push_back(
                    {Convert::frequencyToNote<float>(hz, m_config.referenceA4), yin.getLastConfidence()});
            }
        }
        if (readings.empty())
        {
            return {};
        }
        std::ranges::sort(readings, {}, &PitchReading::midi);
        return readings[readings.size() / 2];
    }

    [[nodiscard]] PitchVerdict verdictOf(const float errorCents) const noexcept
    {
        if (std::abs(errorCents) > m_config.wrongNoteCents)
        {
            return PitchVerdict::WrongNote;
        }
        if (std::abs(errorCents) > m_config.sharpFlatCents)
        {
            return errorCents > 0.f ? PitchVerdict::Sharp : PitchVerdict::Flat;
        }
        return PitchVerdict::Right;
    }

    [[nodiscard]] float attackRmsDb(const Signal& signal, const double onsetSeconds) const noexcept
    {
        const size_t from =
            std::min(static_cast<size_t>(onsetSeconds * static_cast<double>(m_sampleRate)), signal.audio.size());
        const size_t to = std::min(from + msToSamples(kAttackWindowMs), signal.audio.size());
        const double count = static_cast<double>(std::max<size_t>(1, to - from));
        return toDb(std::sqrt((signal.squares[to] - signal.squares[from]) / count));
    }

    // The note sounds until its level falls under the threshold, or until the next note starts.
    [[nodiscard]] double noteEndSeconds(const Signal& signal, const double onsetSeconds, const double limitSeconds,
                                        const float thresholdDb) const
    {
        const size_t onsetFrame = frameOf(signal, onsetSeconds);
        const size_t peakEnd = std::min(onsetFrame + msToFrames(kPeakSearchMs), signal.db.size());
        const auto peak = std::max_element(signal.db.begin() + static_cast<std::ptrdiff_t>(onsetFrame),
                                           signal.db.begin() + static_cast<std::ptrdiff_t>(peakEnd));
        size_t frame = static_cast<size_t>(std::distance(signal.db.begin(), peak));
        while (frame + 1 < signal.db.size() && frameTime(frame) < limitSeconds && signal.db[frame] >= thresholdDb)
        {
            ++frame;
        }
        return frameTime(frame);
    }

    void measureNote(const Signal& signal, const ExpectedNote& expected, const double limitSeconds, VerifiedNote& note,
                     const float thresholdDb) const
    {
        note.offsetSeconds = static_cast<float>(
            noteEndSeconds(signal, static_cast<double>(note.onsetSeconds), limitSeconds, thresholdDb));
        note.attackRmsDb = attackRmsDb(signal, static_cast<double>(note.onsetSeconds));
        const float hz = Convert::noteToFrequency<float>(expected.midiNote, m_config.referenceA4);
        const auto from =
            static_cast<size_t>(static_cast<double>(note.onsetSeconds) * static_cast<double>(m_sampleRate));
        const size_t to =
            std::min(static_cast<size_t>(static_cast<double>(note.offsetSeconds) * static_cast<double>(m_sampleRate)),
                     from + msToSamples(m_config.pitchSpanMs));
        const auto reading = measurePitch(signal, from, to, hz / m_config.pitchRangeFactor,
                                          hz * m_config.pitchRangeFactor, m_config.pitchSkipMs);
        if (reading.midi <= 0.f)
        {
            return;
        }
        note.midiNote = reading.midi;
        note.pitchConfidence = reading.confidence;
        note.pitchErrorCents = (reading.midi - expected.midiNote) * 100.f;
        note.verdict = verdictOf(note.pitchErrorCents);
    }

    [[nodiscard]] std::vector<Region> describeNotes(const Signal& signal, const std::span<const ExpectedNote> expected,
                                                    VerificationReport& report) const
    {
        const double duration = static_cast<double>(signal.audio.size()) / static_cast<double>(m_sampleRate);
        std::vector<Region> regions;
        for (size_t i = 0; i < report.notes.size(); ++i)
        {
            if (!report.notes[i].found)
            {
                continue;
            }
            const size_t next = nextFound(report.notes, i);
            const double limit = next < report.notes.size() ? static_cast<double>(report.notes[next].onsetSeconds) -
                                                                  0.001 * static_cast<double>(kRegionLeadMs)
                                                            : duration;
            measureNote(signal, expected[i], limit, report.notes[i], report.thresholdDb);
            regions.push_back({std::max(0.0, static_cast<double>(report.notes[i].onsetSeconds) -
                                                 0.001 * static_cast<double>(kRegionLeadMs)),
                               static_cast<double>(report.notes[i].offsetSeconds)});
        }
        return regions;
    }

    [[nodiscard]] static size_t nextFound(const std::vector<VerifiedNote>& notes, const size_t index) noexcept
    {
        size_t next = index + 1;
        while (next < notes.size() && !notes[next].found)
        {
            ++next;
        }
        return next;
    }

    [[nodiscard]] std::vector<bool> explainedFrames(const Signal& signal, const std::vector<Region>& regions) const
    {
        std::vector<bool> explained(signal.db.size(), false);
        for (size_t k = 0; k < explained.size(); ++k)
        {
            const double t = frameTime(k);
            explained[k] = std::ranges::any_of(regions, [t](const Region& r) { return t >= r.from && t <= r.to; });
        }
        return explained;
    }

    // Loudest frame that is not part of a note and not next to one.
    [[nodiscard]] float loudestIgnored(const Signal& signal, const std::vector<bool>& explained,
                                       const float thresholdDb) const
    {
        const size_t guard = msToFrames(kEdgeGuardMs);
        float loudest = kSilenceDb;
        for (size_t k = 0; k < explained.size(); ++k)
        {
            const size_t from = k > guard ? k - guard : 0;
            const size_t to = std::min(k + guard + 1, explained.size());
            const bool nearNote =
                std::any_of(explained.begin() + static_cast<std::ptrdiff_t>(from),
                            explained.begin() + static_cast<std::ptrdiff_t>(to), [](const bool e) { return e; });
            if (!nearNote && signal.db[k] < thresholdDb)
            {
                loudest = std::max(loudest, signal.db[k]);
            }
        }
        return loudest;
    }

    void addEvent(const Signal& signal, const size_t first, const size_t last, VerificationReport& report) const
    {
        UnexpectedSound event;
        event.fromSeconds = static_cast<float>(frameTime(first));
        event.toSeconds = static_cast<float>(frameTime(last));
        for (size_t k = first; k <= last; ++k)
        {
            event.peakDb = std::max(event.peakDb, signal.db[k]);
        }
        const double minSeconds = 0.001 * static_cast<double>(m_config.unexpectedMinMs);
        if (static_cast<double>(event.toSeconds - event.fromSeconds) < minSeconds)
        {
            ++report.shortBursts;
            return;
        }
        const auto reading = measurePitch(signal, first * m_hop, last * m_hop, m_config.unexpectedMinHz,
                                          m_config.unexpectedMaxHz, kEventGapMs);
        event.midiNote = reading.midi;
        event.pitchConfidence = reading.confidence;
        report.unexpected.push_back(event);
    }

    // A run of loud frames outside the notes; a dip shorter than the gap does not end it.
    void scanUnexpected(const Signal& signal, const std::vector<Region>& regions, VerificationReport& report) const
    {
        const auto explained = explainedFrames(signal, regions);
        report.loudestIgnoredDb = loudestIgnored(signal, explained, report.thresholdDb);
        const size_t gap = msToFrames(kEventGapMs);
        const auto loud = [&](const size_t k) { return !explained[k] && signal.db[k] >= report.thresholdDb; };
        size_t k = 0;
        while (k < signal.db.size())
        {
            if (!loud(k))
            {
                ++k;
                continue;
            }
            size_t last = k;
            for (size_t j = k; j < signal.db.size() && !explained[j] && j - last <= gap; ++j)
            {
                last = loud(j) ? j : last;
            }
            addEvent(signal, k, last, report);
            k = last + 1;
        }
    }

    float m_sampleRate;
    NoteVerifierConfig m_config;
    size_t m_hop;
};

}
