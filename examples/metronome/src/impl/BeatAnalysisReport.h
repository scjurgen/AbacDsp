#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <span>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

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

// A single captured onset: its signed deviation from the grid, and which grid slot it was
// nearest to (see roleForGridPoint()), so a report can break deviations down per beat/subdivision.
struct DeviationHit
{
    float deviationMs{0.f};
    uint8_t role{0};
};

// role 0..beatsPerBar-1 identifies a specific beat; beatsPerBar+k identifies subdivision k.
[[nodiscard]] constexpr uint8_t roleForBeat(const size_t beatIndexInBar) noexcept
{
    return static_cast<uint8_t>(beatIndexInBar);
}

[[nodiscard]] constexpr uint8_t roleForSubdivision(const size_t beatsPerBar, const size_t subdivisionIndex) noexcept
{
    return static_cast<uint8_t>(beatsPerBar + subdivisionIndex);
}

// Fixed-capacity, allocation-free collector for onset hits. Once full, a long take simply
// stops recording new hits rather than growing.
class DeviationCollector
{
  public:
    static constexpr size_t kCapacity{4096};

    void reset() noexcept
    {
        m_count = 0;
    }

    void push(const float deviationMs, const uint8_t role) noexcept
    {
        if (m_count < kCapacity)
        {
            m_hits[m_count++] = {deviationMs, role};
        }
    }

    [[nodiscard]] std::span<const DeviationHit> hits() const noexcept
    {
        return std::span<const DeviationHit>(m_hits.data(), m_count);
    }

    [[nodiscard]] size_t count() const noexcept
    {
        return m_count;
    }

  private:
    std::array<DeviationHit, kCapacity> m_hits{};
    size_t m_count{0};
};

[[nodiscard]] inline std::vector<float> deviationsForRole(const std::span<const DeviationHit> hits, const uint8_t role)
{
    std::vector<float> result;
    for (const auto& hit : hits)
    {
        if (hit.role == role)
        {
            result.push_back(hit.deviationMs);
        }
    }
    return result;
}

[[nodiscard]] inline std::vector<float> allDeviations(const std::span<const DeviationHit> hits)
{
    std::vector<float> result;
    result.reserve(hits.size());
    for (const auto& hit : hits)
    {
        result.push_back(hit.deviationMs);
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

// Every inline chart is sized by its viewBox only (no pixel width/height attributes) and
// stretched to its container via CSS, so it scales to whatever column Bootstrap gives it
// instead of overflowing a narrower one.
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

        for (size_t i = 0; i < histogram.bins.size(); ++i)
        {
            const float barHeight =
                peak == 0 ? 0.f : kPlotHeight * static_cast<float>(histogram.bins[i]) / static_cast<float>(peak);
            const float x = static_cast<float>(kMargin) + static_cast<float>(i) * barWidth;
            const float y = static_cast<float>(kHeight - kMargin) - barHeight;
            svg << "<rect x=\"" << x << "\" y=\"" << y << "\" width=\"" << std::max(0.5f, barWidth - 0.5f)
                << "\" height=\"" << barHeight << "\" fill=\"#0d6efd\"/>";
        }

        if (histogram.minMs <= 0.f && 0.f <= maxMs)
        {
            const float zeroX =
                static_cast<float>(kMargin) + (0.f - histogram.minMs) / (maxMs - histogram.minMs) * kPlotWidth;
            svg << "<line x1=\"" << zeroX << "\" y1=\"" << kMargin << "\" x2=\"" << zeroX << "\" y2=\""
                << (kHeight - kMargin) << "\" stroke=\"#dc3545\" stroke-dasharray=\"4 3\"/>";
        }
    }
    svg << "</svg>";
    return svg.str();
}

[[nodiscard]] inline std::string renderTimelineSvg(const std::span<const float> deviationsMs)
{
    constexpr int kWidth{1200};
    constexpr int kHeight{220};
    constexpr int kMargin{24};
    constexpr float kPlotWidth{kWidth - 2.f * kMargin};
    constexpr float kPlotHeight{kHeight - 2.f * kMargin};
    constexpr float kAxisLimitMs{150.f}; // clamp the plotted range; extreme outliers still exist in the data

    std::ostringstream svg;
    svg << "<svg viewBox=\"0 0 " << kWidth << " " << kHeight
        << "\" style=\"width:100%;height:auto;display:block\" xmlns=\"http://www.w3.org/2000/svg\">";
    const float zeroY = static_cast<float>(kMargin) + kPlotHeight * 0.5f;
    svg << "<line x1=\"" << kMargin << "\" y1=\"" << zeroY << "\" x2=\"" << (kWidth - kMargin) << "\" y2=\"" << zeroY
        << "\" stroke=\"#adb5bd\"/>";

    if (!deviationsMs.empty())
    {
        const float stepX = deviationsMs.size() > 1 ? kPlotWidth / static_cast<float>(deviationsMs.size() - 1) : 0.f;
        for (size_t i = 0; i < deviationsMs.size(); ++i)
        {
            const float clamped = std::clamp(deviationsMs[i], -kAxisLimitMs, kAxisLimitMs);
            const float x = static_cast<float>(kMargin) + static_cast<float>(i) * stepX;
            const float y = zeroY - clamped / kAxisLimitMs * (kPlotHeight * 0.5f);
            svg << "<circle cx=\"" << x << "\" cy=\"" << y << "\" r=\"2.5\" fill=\"#0d6efd\"/>";
        }
    }
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

// One beat/subdivision label per role index, in musical order: "Beat 1".."Beat N", then the
// subdivision slots ("Off-beat" for a single one, e.g. straight/swung 8ths; "Sub 1", "Sub 2", ...
// when there is more than one, e.g. 16ths).
[[nodiscard]] inline std::vector<std::string> roleLabels(const size_t beatsPerBar, const size_t subdivisionCount)
{
    std::vector<std::string> labels;
    labels.reserve(beatsPerBar + subdivisionCount);
    for (size_t i = 0; i < beatsPerBar; ++i)
    {
        labels.push_back("Beat " + std::to_string(i + 1));
    }
    if (subdivisionCount == 1)
    {
        labels.emplace_back("Off-beat");
    }
    else
    {
        for (size_t i = 0; i < subdivisionCount; ++i)
        {
            labels.push_back("Sub " + std::to_string(i + 1));
        }
    }
    return labels;
}

[[nodiscard]] inline std::string roleCard(const std::string& label, const std::span<const float> deviationsMs,
                                          const float rangeMs)
{
    const DeviationStats stats = computeStats(deviationsMs);
    std::ostringstream out;
    out << "<div class=\"col-md-4 col-lg-3\"><div class=\"card h-100\"><div class=\"card-header\">" << label
        << "</div><div class=\"card-body\">";
    if (stats.count == 0)
    {
        out << "<p class=\"text-muted mb-0\">No hits</p>";
    }
    else
    {
        out << renderHistogramSvg(computeHistogram(deviationsMs, 10.f, rangeMs))
            << "<p class=\"small text-muted mt-2 mb-0\">" << stats.count << " hits, mean " << msLabel(stats.meanMs)
            << ", std " << msLabel(stats.stdDevMs) << "</p>";
    }
    out << "</div></div></div>";
    return out.str();
}

// Self-contained HTML report (Bootstrap via CDN for layout, plain inline SVG for the charts).
// Every histogram shares one fixed axis, half the beat duration at the given tempo either way -
// the farthest a "nearest grid point" search can ever place a beat-role hit.
[[nodiscard]] inline std::string buildReportHtml(const std::span<const DeviationHit> hits, const size_t beatsPerBar,
                                                 const size_t subdivisionCount, const float bpm,
                                                 const std::string_view presetName)
{
    const std::vector<float> all = allDeviations(hits);
    const DeviationStats stats = computeStats(all);
    const float rangeMs = bpm > 0.f ? 30000.f / bpm : 200.f;
    const std::string overallHistogram = renderHistogramSvg(computeHistogram(all, 10.f, rangeMs));
    const std::string timeline = renderTimelineSvg(all);
    const std::vector<std::string> labels = roleLabels(beatsPerBar, subdivisionCount);

    std::ostringstream html;
    html << "<!doctype html><html lang=\"en\"><head><meta charset=\"utf-8\">"
         << "<title>Metronome Timing Analysis</title>"
         << "<link href=\"https://cdn.jsdelivr.net/npm/bootstrap@5.3.3/dist/css/bootstrap.min.css\" "
            "rel=\"stylesheet\"></head>"
         << "<body class=\"bg-light\"><div class=\"container py-4\">"
         << "<h1 class=\"mb-4\">Metronome Timing Analysis</h1>"
         << "<div class=\"row g-3 mb-4\">" << statCard("Hits", std::to_string(stats.count))
         << statCard("Mean deviation", msLabel(stats.meanMs)) << statCard("Std deviation", msLabel(stats.stdDevMs))
         << statCard("Tempo / rhythm",
                     std::to_string(static_cast<int>(std::lround(bpm))) + " BPM, " + std::string(presetName))
         << "</div>"
         << "<div class=\"row g-3 mb-4\">"
         << chartCard("col-12",
                      "Deviation histogram (10 ms bins, +/- " + std::to_string(static_cast<int>(std::lround(rangeMs))) +
                          " ms)",
                      overallHistogram)
         << "</div>";

    if (!labels.empty())
    {
        html << "<h2 class=\"h5 mb-3\">By beat / subdivision</h2><div class=\"row g-3 mb-4\">";
        for (size_t role = 0; role < labels.size(); ++role)
        {
            html << roleCard(labels[role], deviationsForRole(hits, static_cast<uint8_t>(role)), rangeMs);
        }
        html << "</div>";
    }

    html << "<div class=\"row g-3\">"
         << chartCard("col-12", "Hit timeline (negative = early, positive = late)", timeline) << "</div>"
         << "</div></body></html>";
    return html.str();
}

}
