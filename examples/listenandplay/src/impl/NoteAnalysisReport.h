#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <format>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "Harmony/ChordName.h"
#include "NoteMatcher.h"
#include "TakeScore.h"

namespace NoteReport
{

inline constexpr float kBarWidth{26.f};
inline constexpr float kChartHeight{150.f};
inline constexpr float kChartTimingRangeMs{250.f};
inline constexpr float kChartLevelRangeDb{12.f};

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

[[nodiscard]] inline std::string pitchLabel(const float midi)
{
    const int note = static_cast<int>(std::lround(midi));
    return std::format("{}{}", AbacDsp::noteName(((note % 12) + 12) % 12), note / 12 - 1);
}

// Bar and beat of a position, both counted from 1; a pickup is shown as beats before the downbeat.
[[nodiscard]] inline std::string positionLabel(const float positionBeats, const size_t beatsPerBar)
{
    if (positionBeats < 0.f)
    {
        return std::format("pickup {:.2f}", positionBeats);
    }
    const auto bar = static_cast<size_t>(positionBeats / static_cast<float>(beatsPerBar));
    return std::format("{}.{:.2f}", bar + 1, positionBeats - static_cast<float>(bar * beatsPerBar) + 1.f);
}

[[nodiscard]] inline std::string badge(const std::string_view text, const std::string_view background)
{
    return std::format("<span style=\"display:inline-block;padding:.1rem .5rem;border-radius:.25rem;"
                       "background:{};color:#fff\">{}</span>",
                       background, text);
}

[[nodiscard]] inline std::string creditBadge(const std::string_view text, const float credit)
{
    return badge(text, credit >= 0.9f ? "#2e7d32" : (credit >= 0.4f ? "#f9a825" : "#c62828"));
}

[[nodiscard]] inline std::string statCard(const std::string_view label, const std::string_view value)
{
    return std::format("<div class=\"col-md-3\"><div class=\"card\"><div class=\"card-body\">"
                       "<h6 class=\"card-subtitle text-muted\">{}</h6><p class=\"fs-3 mb-0\">{}</p></div></div></div>",
                       label, value);
}

// One bar per matched note around a zero line, scaled to +/- rangeValue.
[[nodiscard]] inline std::string barChart(const std::span<const float> values, const float rangeValue,
                                          const std::string_view unit)
{
    const float width = kBarWidth * static_cast<float>(values.size() + 1);
    const float middle = kChartHeight * 0.5f;
    std::string svg = std::format("<svg viewBox=\"0 0 {} {}\" width=\"100%\" style=\"max-height:{}px\">"
                                  "<line x1=\"0\" y1=\"{}\" x2=\"{}\" y2=\"{}\" stroke=\"#888\"/>",
                                  width, kChartHeight, kChartHeight, middle, width, middle);
    for (size_t i = 0; i < values.size(); ++i)
    {
        const float clamped = std::clamp(values[i], -rangeValue, rangeValue);
        const float height = std::abs(clamped) / rangeValue * middle;
        const float x = kBarWidth * (static_cast<float>(i) + 0.5f);
        svg += std::format(
            "<rect x=\"{}\" y=\"{}\" width=\"{}\" height=\"{}\" fill=\"{}\"><title>{:.1f} {}</title></rect>", x,
            clamped >= 0.f ? middle - height : middle, kBarWidth * 0.7f, height,
            std::abs(values[i]) > rangeValue ? "#c62828" : "#1565c0", values[i], unit);
    }
    return svg + "</svg>";
}

[[nodiscard]] inline std::string noteRow(const NoteMatching::TakeSetup& setup, const NoteMatching::MatchResult& match,
                                         const NoteScoring::TakeScore& score, const size_t matchedIndex)
{
    const auto& note = match.matched[matchedIndex];
    const auto& target = setup.pattern.notes[note.expected];
    const std::string played = note.pitched ? pitchLabel(note.playedMidi) : "?";
    const std::string cents =
        note.pitched ? std::format("{:+.0f} cents", note.pitchError * 100.f - std::round(note.pitchError) * 100.f)
                     : "no pitch";
    return std::format(
        "<tr><td>{}</td><td>{}</td><td>{}</td><td>{}</td><td>{}</td><td>{}</td><td>{}</td></tr>",
        positionLabel(target.positionBeats, setup.pattern.beatsPerBar), pitchLabel(target.pitch),
        note.pitchRight ? creditBadge(played, 1.f) : creditBadge(played, 0.f), cents,
        creditBadge(std::format("{:+.0f} ms", note.timingMs), score.timingCredit[matchedIndex]),
        creditBadge(std::format("{:+.1f} dB", score.levelErrorDb[matchedIndex]), score.dynamicsCredit[matchedIndex]),
        note.pitchRight ? "ok" : "wrong note");
}

[[nodiscard]] inline std::string noteTable(const NoteMatching::TakeSetup& setup, const NoteMatching::MatchResult& match,
                                           const NoteScoring::TakeScore& score)
{
    std::string rows;
    for (size_t i = 0; i < match.matched.size(); ++i)
    {
        rows += noteRow(setup, match, score, i);
    }
    for (const size_t index : match.missed)
    {
        const auto& target = setup.pattern.notes[index];
        rows += std::format("<tr><td>{}</td><td>{}</td><td colspan=\"4\">{}</td><td>missed</td></tr>",
                            positionLabel(target.positionBeats, setup.pattern.beatsPerBar), pitchLabel(target.pitch),
                            badge("not played", "#c62828"));
    }
    return "<table class=\"table table-sm table-striped\"><thead><tr><th>Position</th><th>Expected</th>"
           "<th>Played</th><th>Intonation</th><th>Timing</th><th>Level</th><th>Result</th></tr></thead><tbody>" +
           rows + "</tbody></table>";
}

[[nodiscard]] inline std::string takeDescription(const NoteMatching::TakeSetup& setup,
                                                 const NoteScoring::TakeScore& score)
{
    std::string text =
        std::format("{:.0f} bpm, click {}", static_cast<double>(setup.bpm), setup.clickOn ? "on" : "off");
    if (!setup.clickOn)
    {
        text += std::format(", you played at {:.0f} % of the tempo", static_cast<double>(score.tempoRatio) * 100.0);
    }
    if (score.latencyKnown)
    {
        text += std::format(", suggested latency {:.0f} ms", static_cast<double>(score.suggestedLatencyMs));
    }
    if (score.octaveShift != 0)
    {
        text += std::format(", {} octave{} {}", std::abs(score.octaveShift) / 12,
                            std::abs(score.octaveShift) > 12 ? "s" : "", score.octaveShift > 0 ? "up" : "down");
    }
    return text;
}

[[nodiscard]] inline std::string buildReportHtml(const NoteMatching::TakeSetup& setup,
                                                 const NoteMatching::MatchResult& match,
                                                 const NoteScoring::TakeScore& score, const std::string_view lessonName)
{
    std::string html = std::format(
        "<!doctype html><html lang=\"en\"><head><meta charset=\"utf-8\"><title>ListenAndPlay - {0}</title>"
        "<link href=\"https://cdn.jsdelivr.net/npm/bootstrap@5.3.3/dist/css/bootstrap.min.css\" rel=\"stylesheet\">"
        "</head><body class=\"bg-light\"><div class=\"container py-4\"><h1 class=\"mb-1\">ListenAndPlay</h1>"
        "<p class=\"fs-4 text-muted mb-4\">{0}</p>",
        escapeHtml(lessonName));
    if (!setup.pattern.active || setup.pattern.noteCount == 0)
    {
        return html + "<div class=\"alert alert-warning\">No phrase to score.</div></div></body></html>";
    }
    html += std::format(
        "<div class=\"row g-3 mb-3\">{}{}{}{}</div><div class=\"row g-3 mb-3\">{}{}{}{}</div>"
        "<p class=\"text-muted\">{}</p>",
        statCard("Score", std::format("{:.0f} % {}", score.overall,
                                      score.passed ? badge("passed", "#2e7d32") : badge("not yet", "#c62828"))),
        statCard("Notes", std::format("{:.0f} %", score.notes)),
        statCard("Rhythm", std::format("{:.0f} %", score.rhythm)),
        statCard("Dynamics", std::format("{:.0f} %", score.dynamics)),
        statCard("Found", std::format("{} / {}", score.matchedCount, score.expectedCount)),
        statCard("Missed", std::to_string(score.missedCount)),
        statCard("Extra notes", std::to_string(score.extraCount)),
        statCard("Timing", std::format("{:+.0f} &plusmn; {:.0f} ms", score.meanTimingMs, score.timingSpreadMs)),
        takeDescription(setup, score));
    std::vector<float> timings;
    for (const auto& note : match.matched)
    {
        timings.push_back(note.timingMs);
    }
    html += "<div class=\"row g-3 mb-4\"><div class=\"col-md-6\"><h2 class=\"h6\">Timing per note (ms, early is "
            "negative, +/- 250)</h2>" +
            barChart(timings, kChartTimingRangeMs, "ms") +
            "</div><div class=\"col-md-6\"><h2 class=\"h6\">Level per note against the phrase (dB, +/- 12)</h2>" +
            barChart(score.levelErrorDb, kChartLevelRangeDb, "dB") + "</div></div>";
    html += noteTable(setup, match, score);
    if (!match.extras.empty())
    {
        html += std::format("<p class=\"text-muted\">{} note(s) were played that the phrase does not have.</p>",
                            match.extras.size());
    }
    return html + "</div></body></html>";
}

}
