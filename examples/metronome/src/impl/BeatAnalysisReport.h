#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <span>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include "Generators/BeatSequencer.h"

namespace MetronomeAnalysis
{

// Hysteresis (Schmitt-trigger) onset detector: one envelope, two thresholds. An onset fires
// when the envelope rises past the upper threshold; it only re-arms once the envelope has
// since dropped past the lower one. A held/strummed chord's rippling, beating sustain rarely
// dips far enough to re-arm, so it fires once rather than re-triggering on every ripple -
// unlike comparing against a single fixed level. The tradeoff: a genuinely new attack played
// while the previous one is still loud (within the hysteresis band) will not register as a
// separate hit.
class OnsetDetector
{
  public:
    explicit OnsetDetector(const float sampleRate) noexcept
        : m_attackCoeff(timeConstant(kAttackTimeMs, sampleRate))
        , m_releaseCoeff(timeConstant(kReleaseTimeMs, sampleRate))
        , m_minGapSamples(static_cast<size_t>(kMinGapMs * 0.001f * sampleRate))
    {
    }

    void reset() noexcept
    {
        m_envelope = 0.f;
        m_armed = true;
        m_gapCounter = 0;
    }

    // Feed one input sample; returns true on the sample an onset is detected on.
    [[nodiscard]] bool step(const float sample) noexcept
    {
        const float x = std::abs(sample);
        const float coeff = (x > m_envelope) ? m_attackCoeff : m_releaseCoeff;
        m_envelope = coeff * m_envelope + (1.f - coeff) * x;

        if (m_gapCounter > 0)
        {
            --m_gapCounter;
        }
        if (m_armed && m_envelope >= kUpperThreshold && m_gapCounter == 0)
        {
            m_armed = false;
            m_gapCounter = m_minGapSamples;
            return true;
        }
        if (!m_armed && m_envelope < kLowerThreshold)
        {
            m_armed = true;
        }
        return false;
    }

  private:
    static constexpr float kUpperThreshold{0.08f};  // linear, roughly -22 dBFS: fires an onset
    static constexpr float kLowerThreshold{0.048f}; // must drop below this to re-arm
    static constexpr float kAttackTimeMs{1.f};
    static constexpr float kReleaseTimeMs{60.f};
    static constexpr float kMinGapMs{20.f}; // debounces one physical attack edge, not a musical rest

    [[nodiscard]] static float timeConstant(const float timeMs, const float sampleRate) noexcept
    {
        return std::exp(-1.f / (timeMs * 0.001f * sampleRate));
    }

    float m_attackCoeff;
    float m_releaseCoeff;
    size_t m_minGapSamples;
    float m_envelope{0.f};
    bool m_armed{true};
    size_t m_gapCounter{0};
};

// Raw clock state captured at the instant an onset fires - nothing analysis-mode-dependent.
// Grid membership (which beat/subdivision, deviation in ms) is only ever computed later, at
// report-build time, against whatever grid is selected then (see evaluateHits()).
struct RawOnsetHit
{
    size_t beatIndexInBar{0};
    size_t beatSamplePos{0};
    size_t samplesPerBeat{0};
};

// Fixed-capacity, allocation-free collector for raw onset hits. Once full, a long take simply
// stops recording new hits rather than growing.
class RawOnsetCollector
{
  public:
    static constexpr size_t kCapacity{4096};

    void reset() noexcept
    {
        m_count = 0;
    }

    void push(const RawOnsetHit& hit) noexcept
    {
        if (m_count < kCapacity)
        {
            m_hits[m_count++] = hit;
        }
    }

    [[nodiscard]] std::span<const RawOnsetHit> hits() const noexcept
    {
        return std::span<const RawOnsetHit>(m_hits.data(), m_count);
    }

    [[nodiscard]] size_t count() const noexcept
    {
        return m_count;
    }

  private:
    std::array<RawOnsetHit, kCapacity> m_hits{};
    size_t m_count{0};
};

// A raw hit, evaluated against a chosen analysis grid: its deviation from the grid point it
// landed nearest to, which exact beat-and-subdivision slot that was, and its true position
// within the bar (grid-independent, used to place it on the full-bar histogram).
struct EvaluatedHit
{
    float deviationMs{0.f};
    size_t slot{0};
    float barPosition{0.f};
};

// Fractional (0..1) offsets, within one beat, of a subdivision grid's hits - independent of any
// particular beat's sample length, usable directly as histogram-axis positions.
[[nodiscard]] inline std::vector<float> subdivisionFractionsFor(const AbacDsp::SubdivType type, const float swingRatio)
{
    constexpr size_t kReferenceSamplesPerBeat = 1'000'000;
    const auto positions = AbacDsp::computeSubPositions(type, kReferenceSamplesPerBeat, swingRatio);
    std::vector<float> fractions;
    fractions.reserve(positions.size());
    for (const size_t p : positions)
    {
        fractions.push_back(static_cast<float>(p) / static_cast<float>(kReferenceSamplesPerBeat));
    }
    return fractions;
}

// Which exact (beat, subdivision-within-that-beat) slot a grid point belongs to, as a flat
// index: slot = beat * (subdivisionCount + 1) + (0 for the beat itself, else 1 + subdivision).
[[nodiscard]] inline size_t slotForGridPoint(const AbacDsp::GridPoint& gridPoint, const size_t hitBeatIndexInBar,
                                             const size_t subdivisionCount) noexcept
{
    const size_t owningBeat = gridPoint.isBeat ? gridPoint.beatIndexInBar : hitBeatIndexInBar;
    const size_t withinBeat = gridPoint.isBeat ? 0 : 1 + gridPoint.subdivisionIndex;
    return owningBeat * (subdivisionCount + 1) + withinBeat;
}

// Evaluates every raw hit against the given analysis grid - the only place grid-dependent
// math (nearest-point search, ms conversion, slot assignment) happens in the whole report.
[[nodiscard]] inline std::vector<EvaluatedHit> evaluateHits(const std::span<const RawOnsetHit> rawHits,
                                                            const AbacDsp::SubdivType analysisMode,
                                                            const size_t beatsPerBar, const float swingRatio,
                                                            const float sampleRate)
{
    std::vector<EvaluatedHit> result;
    result.reserve(rawHits.size());
    for (const auto& hit : rawHits)
    {
        const auto subPositions = AbacDsp::computeSubPositions(analysisMode, hit.samplesPerBeat, swingRatio);
        const auto gridPoint = AbacDsp::nearestGridPointOn(hit.beatSamplePos, hit.beatIndexInBar, beatsPerBar,
                                                           hit.samplesPerBeat, subPositions);
        const float deviationMs = static_cast<float>(gridPoint.distanceSamples) / sampleRate * 1000.f;
        const size_t slot = slotForGridPoint(gridPoint, hit.beatIndexInBar, subPositions.size());
        const float barPosition = hit.samplesPerBeat == 0 ? static_cast<float>(hit.beatIndexInBar)
                                                          : static_cast<float>(hit.beatIndexInBar) +
                                                                static_cast<float>(hit.beatSamplePos) /
                                                                    static_cast<float>(hit.samplesPerBeat);
        result.push_back({deviationMs, slot, barPosition});
    }
    return result;
}

struct DeviationStats
{
    float meanMs{0.f};
    float stdDevMs{0.f};
    size_t count{0};
};

[[nodiscard]] inline DeviationStats computeStats(const std::span<const float> deviationsMs)
{
    DeviationStats stats{};
    stats.count = deviationsMs.size();
    if (stats.count == 0)
    {
        return stats;
    }
    float sum = 0.f;
    for (const float v : deviationsMs)
    {
        sum += v;
    }
    stats.meanMs = sum / static_cast<float>(stats.count);

    float variance = 0.f;
    for (const float v : deviationsMs)
    {
        const float d = v - stats.meanMs;
        variance += d * d;
    }
    stats.stdDevMs = std::sqrt(variance / static_cast<float>(stats.count));
    return stats;
}

struct Histogram
{
    float minMs{0.f};
    float binWidthMs{1.f};
    std::vector<size_t> bins;
};

// Bins over a fixed, symmetric [-rangeMs, +rangeMs] window instead of the data's own min/max,
// so every chart in a report shares one comparable, tempo-scaled axis.
[[nodiscard]] inline Histogram computeHistogram(const std::span<const float> deviationsMs, const float binWidthMs,
                                                const float rangeMs)
{
    Histogram histogram{};
    histogram.binWidthMs = binWidthMs;
    if (binWidthMs <= 0.f || rangeMs <= 0.f)
    {
        return histogram;
    }
    const float minMs = -rangeMs;
    const float maxMs = rangeMs;
    histogram.minMs = minMs;

    const auto binCount = std::max<size_t>(1, static_cast<size_t>(std::lround((maxMs - minMs) / binWidthMs)));
    histogram.bins.assign(binCount, 0);
    for (const float v : deviationsMs)
    {
        const float clamped = std::clamp(v, minMs, std::nextafter(maxMs, minMs));
        auto idx = static_cast<size_t>((clamped - minMs) / binWidthMs);
        idx = std::min(idx, binCount - 1);
        ++histogram.bins[idx];
    }
    return histogram;
}

// Sized by its viewBox only, stretched to its container via CSS. Mirrored left-to-right
// against deviationMs's own sign (positive = early, negative = late) so early-to-late reads
// left-to-right despite that sign - the "Early"/"Late" labels make the direction explicit.
[[nodiscard]] inline std::string renderHistogramSvg(const Histogram& histogram)
{
    constexpr int kWidth{600};
    constexpr int kHeight{180};
    constexpr int kMargin{20};
    constexpr float kPlotWidth{kWidth - 2.f * kMargin};
    constexpr float kPlotHeight{kHeight - 2.f * kMargin};

    std::ostringstream svg;
    svg << "<svg viewBox=\"0 0 " << kWidth << " " << kHeight
        << "\" style=\"width:100%;height:auto;display:block\" xmlns=\"http://www.w3.org/2000/svg\">";
    svg << "<line x1=\"" << kMargin << "\" y1=\"" << (kHeight - kMargin) << "\" x2=\"" << (kWidth - kMargin)
        << "\" y2=\"" << (kHeight - kMargin) << "\" stroke=\"#adb5bd\"/>";

    if (!histogram.bins.empty())
    {
        const size_t peak = *std::max_element(histogram.bins.begin(), histogram.bins.end());
        const float maxMs = histogram.minMs + static_cast<float>(histogram.bins.size()) * histogram.binWidthMs;
        const float barWidth = kPlotWidth / static_cast<float>(histogram.bins.size());
        const size_t binCount = histogram.bins.size();

        for (size_t i = 0; i < binCount; ++i)
        {
            const float barHeight =
                peak == 0 ? 0.f : kPlotHeight * static_cast<float>(histogram.bins[i]) / static_cast<float>(peak);
            const float x = static_cast<float>(kMargin) + static_cast<float>(binCount - 1 - i) * barWidth;
            const float y = static_cast<float>(kHeight - kMargin) - barHeight;
            svg << "<rect x=\"" << x << "\" y=\"" << y << "\" width=\"" << std::max(0.5f, barWidth - 0.5f)
                << "\" height=\"" << barHeight << "\" fill=\"#0d6efd\"/>";
        }

        if (histogram.minMs <= 0.f && 0.f <= maxMs)
        {
            const float naturalZeroX =
                static_cast<float>(kMargin) + (0.f - histogram.minMs) / (maxMs - histogram.minMs) * kPlotWidth;
            const float zeroX = static_cast<float>(kWidth) - naturalZeroX;
            svg << "<line x1=\"" << zeroX << "\" y1=\"" << kMargin << "\" x2=\"" << zeroX << "\" y2=\""
                << (kHeight - kMargin) << "\" stroke=\"#dc3545\" stroke-dasharray=\"4 3\"/>";
        }
    }
    svg << "<text x=\"" << kMargin << "\" y=\"" << (kMargin - 6) << "\" font-size=\"11\" fill=\"#6c757d\">Early</text>";
    svg << "<text x=\"" << (kWidth - kMargin) << "\" y=\"" << (kMargin - 6)
        << "\" font-size=\"11\" fill=\"#6c757d\" text-anchor=\"end\">Late</text>";
    svg << "</svg>";
    return svg.str();
}

[[nodiscard]] inline std::string msLabel(const float valueMs)
{
    std::ostringstream out;
    out.precision(1);
    out << std::fixed << valueMs << " ms";
    return out.str();
}

// A signed ms value read as a direction word instead of a +/- sign: deviationMs's own
// convention (negative = late, positive = early) - see GridPoint::distanceSamples.
[[nodiscard]] inline std::string signedMsLabel(const float valueMs)
{
    constexpr float kOnBeatToleranceMs = 0.05f;
    if (std::abs(valueMs) < kOnBeatToleranceMs)
    {
        return "on beat";
    }
    std::ostringstream out;
    out.precision(1);
    out << std::fixed << std::abs(valueMs) << " ms " << (valueMs < 0.f ? "late" : "early");
    return out.str();
}

[[nodiscard]] inline std::string statCard(const std::string_view label, const std::string_view value)
{
    std::ostringstream out;
    out << "<div class=\"col-md-3\"><div class=\"card\"><div class=\"card-body\">"
        << "<h6 class=\"card-subtitle text-muted\">" << label << "</h6>"
        << "<p class=\"fs-3 mb-0\">" << value << "</p></div></div></div>";
    return out.str();
}

[[nodiscard]] inline std::string chartCard(const std::string_view columnClass, const std::string_view title,
                                           const std::string_view svg)
{
    std::ostringstream out;
    out << "<div class=\"" << columnClass << "\"><div class=\"card\"><div class=\"card-header\">" << title
        << "</div><div class=\"card-body\">" << svg << "</div></div></div>";
    return out.str();
}

// One label per flat slot index (see slotForGridPoint()), in musical order: for each beat,
// "Beat N" followed by its own subdivision slots ("Off-beat" for a single one, e.g.
// straight/swung 8ths; "Sub 1", "Sub 2", ... for more, e.g. 16ths).
[[nodiscard]] inline std::vector<std::string> slotLabels(const size_t beatsPerBar, const size_t subdivisionCount)
{
    std::vector<std::string> labels;
    labels.reserve(beatsPerBar * (subdivisionCount + 1));
    for (size_t beat = 0; beat < beatsPerBar; ++beat)
    {
        const std::string beatLabel = "Beat " + std::to_string(beat + 1);
        labels.push_back(beatLabel);
        if (subdivisionCount == 1)
        {
            labels.push_back(beatLabel + " - Off-beat");
        }
        else
        {
            for (size_t sub = 0; sub < subdivisionCount; ++sub)
            {
                labels.push_back(beatLabel + " - Sub " + std::to_string(sub + 1));
            }
        }
    }
    return labels;
}

// Perceived timing quality by |deviation|, from a drummer's-ear perspective rather than a
// raw number: tight zones read as green, a "maybe intentional" laid-back zone as orange, a
// dragging/rushing zone as red. zoneColor is used (semi-transparent) for full-bar chart
// zones; badgeBg/badgeText are a light/dark pair for table-cell badges at full opacity.
struct ToleranceBand
{
    float upperMs{0.f};
    std::string_view zoneColor;
    std::string_view badgeBg;
    std::string_view badgeText;
    std::string_view shortLabel;
    std::string_view legendLabel;
};

inline constexpr std::array<ToleranceBand, 5> kToleranceBands{{
    {10.f, "#2e7d32", "#d7f5db", "#1b5e20", "Locked", "Locked (0-10 ms)"},
    {20.f, "#7cb342", "#eaf6d0", "#33691e", "Very tight", "Very tight (10-20 ms)"},
    {30.f, "#fdd835", "#fff6cf", "#7c6300", "Tight", "Tight (20-30 ms)"},
    {50.f, "#fb8c00", "#ffe6c7", "#8a4b00", "Loose", "Loose (30-50 ms)"},
    {80.f, "#e53935", "#fbdada", "#7a1212", "Off", "Off (>50 ms)"},
}};

[[nodiscard]] inline const ToleranceBand& bandForDeviation(const float absMs) noexcept
{
    for (const auto& band : kToleranceBands)
    {
        if (absMs <= band.upperMs)
        {
            return band;
        }
    }
    return kToleranceBands.back();
}

[[nodiscard]] inline std::string coloredBadge(const std::string_view text, const std::string_view bg,
                                              const std::string_view textColor)
{
    std::ostringstream out;
    out << "<span style=\"display:inline-block;padding:.1rem .5rem;border-radius:.25rem;background:" << bg
        << ";color:" << textColor << ";\">" << text << "</span>";
    return out.str();
}

[[nodiscard]] inline std::string deviationBadge(const float valueMs)
{
    const ToleranceBand& band = bandForDeviation(std::abs(valueMs));
    return coloredBadge(signedMsLabel(valueMs), band.badgeBg, band.badgeText);
}

// Tightness reads off spread (std deviation), not average bias: a slot can average near zero
// deviation yet still be loosely/inconsistently played, which the mean alone would hide.
[[nodiscard]] inline std::string tightnessBadge(const float stdDevMs)
{
    const ToleranceBand& band = bandForDeviation(std::abs(stdDevMs));
    return coloredBadge(band.shortLabel, band.badgeBg, band.badgeText);
}

// Playing feel by signed mean deviation - deliberately a different palette (blue/gray/purple)
// than ToleranceBand's green-to-red: direction is a style read, not a quality judgement.
// deviationMs sign: negative = the grid point already passed (late), positive = still ahead
// (early) - see GridPoint::distanceSamples.
struct DirectionBand
{
    float atMostMs{0.f};
    std::string_view badgeBg;
    std::string_view badgeText;
    std::string_view label;
};

inline constexpr std::array<DirectionBand, 5> kDirectionBands{{
    {-30.f, "#cfe2ff", "#084298", "Dragging"},
    {-10.f, "#e7f1ff", "#0a58ca", "Laid-back"},
    {10.f, "#e9ecef", "#495057", "On-beat"},
    {30.f, "#f3e8ff", "#6f42c1", "Pushing"},
    {std::numeric_limits<float>::max(), "#e0cffc", "#59359a", "Rushing"},
}};

[[nodiscard]] inline const DirectionBand& bandForMeanDeviation(const float meanMs) noexcept
{
    for (const auto& band : kDirectionBands)
    {
        if (meanMs <= band.atMostMs)
        {
            return band;
        }
    }
    return kDirectionBands.back();
}

[[nodiscard]] inline std::string directionBadge(const float meanMs)
{
    const DirectionBand& band = bandForMeanDeviation(meanMs);
    return coloredBadge(band.label, band.badgeBg, band.badgeText);
}

[[nodiscard]] inline std::string toleranceLegend()
{
    std::ostringstream out;
    out << "<div class=\"d-flex flex-wrap gap-3 mb-2\">";
    for (const auto& band : kToleranceBands)
    {
        out << "<span style=\"display:inline-flex;align-items:center;gap:.35rem;font-size:.85rem;\">"
            << "<span style=\"width:.9rem;height:.9rem;border-radius:.2rem;background:" << band.zoneColor
            << ";display:inline-block;\"></span>" << band.legendLabel << "</span>";
    }
    out << "</div>";
    return out.str();
}

// One row per exact slot: position label, hit count, a color-coded mean deviation badge, and a
// comment combining tightness (spread) and direction (bias) - dashes when a slot saw no hits.
[[nodiscard]] inline std::string resultsTable(const std::span<const std::string> labels,
                                              const std::span<const EvaluatedHit> hits)
{
    std::vector<std::vector<float>> perSlot(labels.size());
    for (const auto& hit : hits)
    {
        if (hit.slot < perSlot.size())
        {
            perSlot[hit.slot].push_back(hit.deviationMs);
        }
    }

    std::ostringstream out;
    out << "<div class=\"table-responsive\"><table class=\"table table-sm table-striped\">"
        << "<thead><tr><th>Position</th><th>Hits</th><th>Mean deviation</th><th>Std deviation</th>"
        << "<th>Comment</th></tr></thead><tbody>";
    for (size_t slot = 0; slot < labels.size(); ++slot)
    {
        const DeviationStats stats = computeStats(perSlot[slot]);
        const std::string comment =
            stats.count == 0 ? "-" : tightnessBadge(stats.stdDevMs) + " " + directionBadge(stats.meanMs);
        out << "<tr><td>" << labels[slot] << "</td><td>" << stats.count << "</td><td>"
            << (stats.count == 0 ? "-" : deviationBadge(stats.meanMs)) << "</td><td>"
            << (stats.count == 0 ? "-" : msLabel(stats.stdDevMs)) << "</td><td>" << comment << "</td></tr>";
    }
    out << "</tbody></table></div>";
    return out.str();
}

// Every exact beat and subdivision marker across the whole bar, as a continuous [0,
// beatsPerBar) axis position - the full-bar histogram's x-axis is this axis, not ms deviation.
[[nodiscard]] inline std::vector<float> fullBarMarkerPositions(const size_t beatsPerBar,
                                                               const std::span<const float> subdivisionFractions)
{
    std::vector<float> markers;
    markers.reserve(beatsPerBar * (subdivisionFractions.size() + 1));
    for (size_t beat = 0; beat < beatsPerBar; ++beat)
    {
        markers.push_back(static_cast<float>(beat));
        for (const float fraction : subdivisionFractions)
        {
            markers.push_back(static_cast<float>(beat) + fraction);
        }
    }
    return markers;
}

// A 16th note's worth of beats: the display axis starts this far before beat 1 rather than
// exactly at it, so hits just before beat 1 (wrapping from the end of the bar) plot next to
// it instead of splitting across the two edges of the graph.
inline constexpr float kAxisLeadInBeats = 0.25f;

// Wraps a raw [0, axisSpan) bar position into the lead-in-shifted display axis [axisMin,
// axisMin + axisSpan): the last kAxisLeadInBeats of the bar move to just before position 0.
[[nodiscard]] inline float wrapToDisplayAxis(const float barPosition, const float axisSpan,
                                             const float axisMin) noexcept
{
    return barPosition >= axisSpan + axisMin ? barPosition - axisSpan : barPosition;
}

// Hit density across the display axis, binned at a fixed 10 ms (tempo-converted) resolution.
[[nodiscard]] inline std::vector<size_t> fullBarHistogramBins(const std::span<const EvaluatedHit> hits,
                                                              const float axisSpan, const float axisMin,
                                                              const float binWidthBeats)
{
    if (binWidthBeats <= 0.f)
    {
        return {};
    }
    const float axisMax = axisMin + axisSpan;
    const auto binCount = std::max<size_t>(1, static_cast<size_t>(std::lround(axisSpan / binWidthBeats)));
    std::vector<size_t> bins(binCount, 0);
    for (const auto& hit : hits)
    {
        const float display = wrapToDisplayAxis(hit.barPosition, axisSpan, axisMin);
        const float clamped = std::clamp(display, axisMin, std::nextafter(axisMax, axisMin));
        auto idx = static_cast<size_t>((clamped - axisMin) / binWidthBeats);
        idx = std::min(idx, binCount - 1);
        ++bins[idx];
    }
    return bins;
}

// Concentric, color-coded timing-quality zones around one marker (see ToleranceBand): drawn
// widest-and-reddest first so each narrower, greener band paints over its center - a bullseye.
inline void renderToleranceZones(std::ostringstream& svg, const float marker, const float axisMin, const float axisMax,
                                 const float msToBeats, const int margin, const float plotWidth, const float plotHeight)
{
    const auto toX = [&](const float beatPosition) noexcept
    { return static_cast<float>(margin) + (beatPosition - axisMin) / (axisMax - axisMin) * plotWidth; };
    for (auto it = kToleranceBands.rbegin(); it != kToleranceBands.rend(); ++it)
    {
        const float halfWidthBeats = it->upperMs * msToBeats;
        const float x0 = toX(std::max(axisMin, marker - halfWidthBeats));
        const float x1 = toX(std::min(axisMax, marker + halfWidthBeats));
        svg << "<rect x=\"" << x0 << "\" y=\"" << margin << "\" width=\"" << (x1 - x0) << "\" height=\"" << plotHeight
            << "\" fill=\"" << it->zoneColor << "\" fill-opacity=\"0.16\"/>";
    }
}

// The report's visual centerpiece: hit density across the whole bar, with a solid line at
// every beat, a dashed line at every subdivision of the analysis mode, and color-coded
// timing-quality zones (see ToleranceBand) around each - on the lead-in-shifted axis above.
[[nodiscard]] inline std::string renderFullBarHistogramSvg(const std::span<const EvaluatedHit> hits,
                                                           const size_t beatsPerBar,
                                                           const std::span<const float> subdivisionFractions,
                                                           const float bpm)
{
    constexpr int kWidth{1200};
    constexpr int kHeight{240};
    constexpr int kMargin{24};
    constexpr float kPlotWidth{kWidth - 2.f * kMargin};
    constexpr float kPlotHeight{kHeight - 2.f * kMargin};
    constexpr float kBinWidthMs{10.f};

    const float axisSpan = static_cast<float>(beatsPerBar);
    const float axisMin = -kAxisLeadInBeats;
    const float axisMax = axisMin + axisSpan;
    const float msToBeats = bpm > 0.f ? bpm / 60000.f : 0.f;
    const float binWidthBeats = kBinWidthMs * msToBeats;
    const auto toX = [&](const float beatPosition) noexcept
    { return static_cast<float>(kMargin) + (beatPosition - axisMin) / axisSpan * kPlotWidth; };

    std::ostringstream svg;
    svg << "<svg viewBox=\"0 0 " << kWidth << " " << kHeight
        << "\" style=\"width:100%;height:auto;display:block\" xmlns=\"http://www.w3.org/2000/svg\">";

    for (const float marker : fullBarMarkerPositions(beatsPerBar, subdivisionFractions))
    {
        renderToleranceZones(svg, marker, axisMin, axisMax, msToBeats, kMargin, kPlotWidth, kPlotHeight);
    }

    svg << "<line x1=\"" << kMargin << "\" y1=\"" << (kHeight - kMargin) << "\" x2=\"" << (kWidth - kMargin)
        << "\" y2=\"" << (kHeight - kMargin) << "\" stroke=\"#adb5bd\"/>";

    const auto bins = fullBarHistogramBins(hits, axisSpan, axisMin, binWidthBeats);
    if (!bins.empty())
    {
        const size_t peak = *std::max_element(bins.begin(), bins.end());
        const float barWidth = kPlotWidth / static_cast<float>(bins.size());
        for (size_t i = 0; i < bins.size(); ++i)
        {
            const float barHeight =
                peak == 0 ? 0.f : kPlotHeight * static_cast<float>(bins[i]) / static_cast<float>(peak);
            const float x = static_cast<float>(kMargin) + static_cast<float>(i) * barWidth;
            const float y = static_cast<float>(kHeight - kMargin) - barHeight;
            svg << "<rect x=\"" << x << "\" y=\"" << y << "\" width=\"" << std::max(0.5f, barWidth - 0.5f)
                << "\" height=\"" << barHeight << "\" fill=\"#0d6efd\"/>";
        }
    }

    for (size_t beat = 0; beat < beatsPerBar; ++beat)
    {
        const float x = toX(static_cast<float>(beat));
        svg << "<line x1=\"" << x << "\" y1=\"" << kMargin << "\" x2=\"" << x << "\" y2=\"" << (kHeight - kMargin)
            << "\" stroke=\"#212529\" stroke-width=\"1.5\"/>";
        for (const float fraction : subdivisionFractions)
        {
            const float sx = toX(static_cast<float>(beat) + fraction);
            svg << "<line x1=\"" << sx << "\" y1=\"" << kMargin << "\" x2=\"" << sx << "\" y2=\"" << (kHeight - kMargin)
                << "\" stroke=\"#6c757d\" stroke-dasharray=\"4 3\"/>";
        }
    }

    svg << "</svg>";
    return svg.str();
}

// Self-contained HTML report (Bootstrap via CDN for layout, plain inline SVG for the charts).
// The analysis grid (which subdivisions raw hits are measured against) is whatever
// analysisMode is at the moment this is called, not whatever it was during capture.
[[nodiscard]] inline std::string buildReportHtml(const std::span<const RawOnsetHit> rawHits, const size_t beatsPerBar,
                                                 const float swingRatio, const AbacDsp::SubdivType analysisMode,
                                                 const std::string_view analysisModeName, const float bpm,
                                                 const float sampleRate, const std::string_view presetName)
{
    const std::vector<float> subdivisionFractions = subdivisionFractionsFor(analysisMode, swingRatio);
    const std::vector<EvaluatedHit> hits = evaluateHits(rawHits, analysisMode, beatsPerBar, swingRatio, sampleRate);

    std::vector<float> allDeviations;
    allDeviations.reserve(hits.size());
    for (const auto& hit : hits)
    {
        allDeviations.push_back(hit.deviationMs);
    }
    const DeviationStats stats = computeStats(allDeviations);
    const float rangeMs = bpm > 0.f ? 30000.f / bpm : 200.f;
    const std::string overallHistogram = renderHistogramSvg(computeHistogram(allDeviations, 10.f, rangeMs));
    const std::string fullBarHistogram = renderFullBarHistogramSvg(hits, beatsPerBar, subdivisionFractions, bpm);
    const std::vector<std::string> labels = slotLabels(beatsPerBar, subdivisionFractions.size());
    const std::string table = resultsTable(labels, hits);

    std::ostringstream html;
    html << "<!doctype html><html lang=\"en\"><head><meta charset=\"utf-8\">"
         << "<title>Metronome Timing Analysis</title>"
         << "<link href=\"https://cdn.jsdelivr.net/npm/bootstrap@5.3.3/dist/css/bootstrap.min.css\" "
            "rel=\"stylesheet\"></head>"
         << "<body class=\"bg-light\"><div class=\"container py-4\">"
         << "<h1 class=\"mb-4\">Metronome Timing Analysis</h1>"
         << "<div class=\"row g-3 mb-4\">" << statCard("Hits", std::to_string(stats.count))
         << statCard("Mean deviation", signedMsLabel(stats.meanMs))
         << statCard("Std deviation", msLabel(stats.stdDevMs))
         << statCard("Tempo / rhythm",
                     std::to_string(static_cast<int>(std::lround(bpm))) + " BPM, " + std::string(presetName))
         << "</div>"
         << "<div class=\"row g-3 mb-2\">" << statCard("Analysis grid", std::string(analysisModeName)) << "</div>"
         << toleranceLegend() << "<div class=\"row g-3 mb-4\">"
         << chartCard("col-12", "Full-bar hit distribution (10 ms bins, color-coded timing quality)", fullBarHistogram)
         << "</div>"
         << "<div class=\"row g-3 mb-4\">"
         << chartCard("col-12",
                      "Overall deviation histogram (10 ms bins, +/- " +
                          std::to_string(static_cast<int>(std::lround(rangeMs))) + " ms)",
                      overallHistogram)
         << "</div>"
         << "<h2 class=\"h5 mb-3\">By beat / subdivision</h2>" << table << "</div></body></html>";
    return html.str();
}

}
