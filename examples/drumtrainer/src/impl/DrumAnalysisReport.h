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

#include "DrumHitMatcher.h"

namespace DrumAnalysis
{

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
// left-to-right despite that sign; the "Early"/"Late" labels make the direction explicit.
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

// A signed ms value read as a direction word instead of a +/- sign (negative = late,
// positive = early).
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

[[nodiscard]] inline std::string escapeHtml(const std::string_view text)
{
    std::string out;
    out.reserve(text.size());
    for (const char c : text)
    {
        switch (c)
        {
            case '&':
                out += "&amp;";
                break;
            case '<':
                out += "&lt;";
                break;
            case '>':
                out += "&gt;";
                break;
            case '"':
                out += "&quot;";
                break;
            case '\'':
                out += "&#39;";
                break;
            default:
                out += c;
        }
    }
    return out;
}

// Lowercase letters, digits, '.' and '_' are kept; every other run of characters becomes one '-'.
[[nodiscard]] inline std::string fileNameStem(const std::string_view name)
{
    std::string stem;
    for (const char c : name)
    {
        if (c >= 'A' && c <= 'Z')
        {
            stem += static_cast<char>(c - 'A' + 'a');
        }
        else if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '.' || c == '_')
        {
            stem += c;
        }
        else if (!stem.empty() && stem.back() != '-')
        {
            stem += '-';
        }
    }
    while (!stem.empty() && stem.back() == '-')
    {
        stem.pop_back();
    }
    return stem;
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

// Perceived timing quality by |deviation|, from a drummer's-ear perspective rather than a
// raw number: tight zones read as green, a "maybe intentional" laid-back zone as orange, a
// dragging/rushing zone as red. zoneColor marks the legend; badgeBg/badgeText are a
// light/dark pair for table-cell badges.
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

// Tightness reads off spread (std deviation), not average bias: a hit can average near zero
// deviation yet still be loosely/inconsistently played, which the mean alone would hide.
[[nodiscard]] inline std::string tightnessBadge(const float stdDevMs)
{
    const ToleranceBand& band = bandForDeviation(std::abs(stdDevMs));
    return coloredBadge(band.shortLabel, band.badgeBg, band.badgeText);
}

// Playing feel by signed mean deviation (negative = late, positive = early), deliberately in a
// different palette (blue/gray/purple) than ToleranceBand's green-to-red: direction is a style
// read, not a quality judgement.
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

inline constexpr float kHistogramBinMs{5.f};

// Indexed by DrumPattern::Instrument.
inline constexpr auto kInstrumentLabels = std::to_array<std::string_view>(
    {"Kick", "Snare", "Hihat closed", "Hihat open", "Crash", "Tom high", "Tom low", "Tom floor"});
static_assert(kInstrumentLabels.size() == DrumPattern::kInstrumentNames.size());

[[nodiscard]] inline std::string_view instrumentLabel(const DrumPattern::Instrument instrument) noexcept
{
    return kInstrumentLabels[static_cast<size_t>(instrument)];
}

inline constexpr float kWholeBeatTolerance{0.0005f};

// "Beat 3 + 0.5", prefixed with "Bar 2, " when the pattern spans more than one bar.
[[nodiscard]] inline std::string positionLabel(const float positionBeats, const size_t beatsPerBar,
                                               const size_t patternBars)
{
    const auto wholeBeats = static_cast<size_t>(std::floor(positionBeats + kWholeBeatTolerance));
    const float fraction = positionBeats - static_cast<float>(wholeBeats);
    std::ostringstream label;
    if (patternBars > 1 && beatsPerBar > 0)
    {
        label << "Bar " << wholeBeats / beatsPerBar + 1 << ", ";
    }
    label << "Beat " << (beatsPerBar > 0 ? wholeBeats % beatsPerBar : wholeBeats) + 1;
    if (fraction >= kWholeBeatTolerance)
    {
        label.precision(3);
        label << " + " << fraction;
    }
    return label.str();
}

[[nodiscard]] inline std::string percentLabel(const size_t part, const size_t whole)
{
    if (whole == 0)
    {
        return "-";
    }
    return std::to_string(static_cast<int>(std::lround(100.0 * static_cast<double>(part) / static_cast<double>(whole)))) +
           " %";
}

[[nodiscard]] inline std::string statsCells(const std::span<const float> deviationsMs)
{
    const DeviationStats stats = computeStats(deviationsMs);
    if (stats.count == 0)
    {
        return "<td>-</td><td>-</td><td>-</td>";
    }
    return "<td>" + deviationBadge(stats.meanMs) + "</td><td>" + msLabel(stats.stdDevMs) + "</td><td>" +
           tightnessBadge(stats.stdDevMs) + " " + directionBadge(stats.meanMs) + "</td>";
}

struct InstrumentTally
{
    size_t expected{0};
    std::vector<float> deviationsMs;
    size_t missed{0};
    size_t extra{0};
    size_t wrongArticulation{0};
};

[[nodiscard]] inline std::array<InstrumentTally, kInstrumentLabels.size()> tallyByInstrument(const TakeSetup& setup,
                                                                                            const MatchResult& result)
{
    std::array<InstrumentTally, kInstrumentLabels.size()> tallies{};
    const auto expectedOf = [&](const size_t patternHit)
    { return static_cast<size_t>(setup.pattern.hits[patternHit].instrument); };
    for (const auto& hit : result.matched)
    {
        auto& tally = tallies[expectedOf(hit.patternHit)];
        ++tally.expected;
        tally.deviationsMs.push_back(hit.deviationMs);
        tally.wrongArticulation += hit.wrongArticulation ? 1 : 0;
    }
    for (const auto& miss : result.missed)
    {
        auto& tally = tallies[expectedOf(miss.patternHit)];
        ++tally.expected;
        ++tally.missed;
    }
    for (const auto& extra : result.extras)
    {
        ++tallies[static_cast<size_t>(extra.instrument)].extra;
    }
    return tallies;
}

// Wrong articulation only applies to the hihat; its hits still count for timing.
[[nodiscard]] inline std::string instrumentTable(const std::span<const InstrumentTally> tallies)
{
    std::ostringstream out;
    out << "<div class=\"table-responsive\"><table class=\"table table-sm table-striped\">"
        << "<thead><tr><th>Instrument</th><th>Expected</th><th>Hit</th><th>Missed</th><th>Extra</th>"
        << "<th>Wrong hihat</th><th>Mean deviation</th><th>Std deviation</th><th>Comment</th></tr></thead><tbody>";
    for (size_t i = 0; i < tallies.size(); ++i)
    {
        const auto& tally = tallies[i];
        if (tally.expected == 0 && tally.extra == 0)
        {
            continue;
        }
        const auto instrument = static_cast<DrumPattern::Instrument>(i);
        const bool isHihat = limbOf(instrument) == DrumPattern::Instrument::HihatClosed;
        out << "<tr><td>" << instrumentLabel(instrument) << "</td><td>" << tally.expected << "</td><td>"
            << tally.deviationsMs.size() << " (" << percentLabel(tally.deviationsMs.size(), tally.expected) << ")</td><td>"
            << tally.missed << "</td><td>" << tally.extra << "</td><td>"
            << (isHihat ? std::to_string(tally.wrongArticulation) : std::string{"-"}) << "</td>"
            << statsCells(tally.deviationsMs) << "</tr>";
    }
    out << "</tbody></table></div>";
    return out.str();
}

// One row per hit of the pattern, over all repetitions of the pattern in the take.
[[nodiscard]] inline std::string positionTable(const TakeSetup& setup, const MatchResult& result)
{
    const auto& pattern = setup.pattern;
    std::vector<std::vector<float>> deviations(pattern.hitCount);
    std::vector<size_t> misses(pattern.hitCount, 0);
    std::vector<size_t> wrong(pattern.hitCount, 0);
    for (const auto& hit : result.matched)
    {
        deviations[hit.patternHit].push_back(hit.deviationMs);
        wrong[hit.patternHit] += hit.wrongArticulation ? 1 : 0;
    }
    for (const auto& miss : result.missed)
    {
        ++misses[miss.patternHit];
    }
    std::ostringstream out;
    out << "<div class=\"table-responsive\"><table class=\"table table-sm table-striped\">"
        << "<thead><tr><th>Position</th><th>Instrument</th><th>Hit</th><th>Missed</th><th>Wrong hihat</th>"
        << "<th>Mean deviation</th><th>Std deviation</th><th>Comment</th></tr></thead><tbody>";
    for (size_t i = 0; i < pattern.hitCount; ++i)
    {
        const auto& hit = pattern.hits[i];
        const bool isHihat = limbOf(hit.instrument) == DrumPattern::Instrument::HihatClosed;
        out << "<tr><td>" << positionLabel(hit.positionBeats, setup.beatsPerBar, pattern.patternBars) << "</td><td>"
            << instrumentLabel(hit.instrument) << "</td><td>" << deviations[i].size() << "</td><td>" << misses[i]
            << "</td><td>" << (isHihat ? std::to_string(wrong[i]) : std::string{"-"}) << "</td>"
            << statsCells(deviations[i]) << "</tr>";
    }
    out << "</tbody></table></div>";
    return out.str();
}

[[nodiscard]] inline std::string instrumentHistograms(const std::span<const InstrumentTally> tallies)
{
    std::ostringstream out;
    for (size_t i = 0; i < tallies.size(); ++i)
    {
        if (tallies[i].deviationsMs.empty())
        {
            continue;
        }
        const auto histogram = computeHistogram(tallies[i].deviationsMs, kHistogramBinMs, kMaxMatchWindowMs);
        out << chartCard("col-md-6", instrumentLabel(static_cast<DrumPattern::Instrument>(i)),
                         renderHistogramSvg(histogram));
    }
    return out.str();
}

[[nodiscard]] inline std::string takeDescription(const TakeSetup& setup, const std::string_view takeName)
{
    std::ostringstream out;
    out << std::lround(setup.bpm) << " BPM, " << setup.pattern.patternBars << " x " << setup.beatsPerBar << " beats";
    if (!takeName.empty())
    {
        out << ", " << takeName;
    }
    return out.str();
}

[[nodiscard]] inline std::string reportHead(const std::string_view scriptName)
{
    std::ostringstream html;
    html << "<!doctype html><html lang=\"en\"><head><meta charset=\"utf-8\">"
         << "<title>DrumTrainer Timing Analysis" << (scriptName.empty() ? "" : " - ") << escapeHtml(scriptName)
         << "</title>"
         << "<link href=\"https://cdn.jsdelivr.net/npm/bootstrap@5.3.3/dist/css/bootstrap.min.css\" "
            "rel=\"stylesheet\"></head>"
         << "<body class=\"bg-light\"><div class=\"container py-4\">"
         << "<h1 class=\"" << (scriptName.empty() ? "mb-4" : "mb-1") << "\">DrumTrainer Timing Analysis</h1>"
         << (scriptName.empty() ? "" : "<p class=\"fs-4 text-muted mb-4\">" + escapeHtml(scriptName) + "</p>");
    return html.str();
}

// Self-contained HTML report (Bootstrap via CDN for layout, plain inline SVG for the charts).
[[nodiscard]] inline std::string buildReportHtml(const TakeSetup& setup, const std::span<const CapturedHit> played,
                                                 const std::string_view takeName,
                                                 const std::string_view scriptName = {})
{
    std::ostringstream html;
    html << reportHead(scriptName);
    if (!setup.pattern.active || setup.pattern.hitCount == 0)
    {
        html << "<div class=\"alert alert-warning\">No target pattern: load a pattern script that adds hits, "
                "then start a new take.</div></div></body></html>";
        return html.str();
    }
    const MatchResult result = matchTake(setup, played);
    const auto tallies = tallyByInstrument(setup, result);
    std::vector<float> allDeviations;
    allDeviations.reserve(result.matched.size());
    size_t wrongArticulation = 0;
    for (const auto& hit : result.matched)
    {
        allDeviations.push_back(hit.deviationMs);
        wrongArticulation += hit.wrongArticulation ? 1 : 0;
    }
    const DeviationStats stats = computeStats(allDeviations);

    html << "<div class=\"row g-3 mb-3\">"
         << statCard("Hit", std::to_string(result.matched.size()) + " / " + std::to_string(result.expectedCount))
         << statCard("Hit rate", percentLabel(result.matched.size(), result.expectedCount))
         << statCard("Missed", std::to_string(result.missed.size()))
         << statCard("Extra", std::to_string(result.extras.size())) << "</div>"
         << "<div class=\"row g-3 mb-3\">" << statCard("Mean deviation", signedMsLabel(stats.meanMs))
         << statCard("Std deviation", msLabel(stats.stdDevMs))
         << statCard("Wrong hihat", std::to_string(wrongArticulation))
         << statCard("Hihat pedal (not scored)", std::to_string(result.pedalHits)) << "</div>"
         << "<div class=\"row g-3 mb-4\">" << statCard("Take", takeDescription(setup, takeName))
         << statCard("Bars", std::to_string(setup.capturedBars)) << "</div>" << toleranceLegend()
         << "<div class=\"row g-3 mb-4\">"
         << chartCard("col-12", "Overall deviation (5 ms bins, +/- 70 ms)",
                      renderHistogramSvg(computeHistogram(allDeviations, kHistogramBinMs, kMaxMatchWindowMs)))
         << "</div>"
         << "<h2 class=\"h5 mb-3\">By instrument</h2>" << instrumentTable(tallies)
         << "<div class=\"row g-3 mb-4\">" << instrumentHistograms(tallies) << "</div>"
         << "<h2 class=\"h5 mb-3\">By pattern position</h2>" << positionTable(setup, result) << "</div></body></html>";
    return html.str();
}

}
