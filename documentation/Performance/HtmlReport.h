#pragma once

#include <algorithm>
#include <array>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

#include "PerfConfig.h"
#include "PerfHarness.h"
#include "SystemInfo.h"

namespace AbacDsp::Perf
{

struct ReportHeader
{
    std::string comment;
    GitInfo git;
    ProcessorInfo cpu;
    bool isReleaseBuild{};
    double totalRunSeconds{};
};

namespace detail
{

[[nodiscard]] inline std::string htmlEscape(const std::string& in)
{
    std::string out;
    out.reserve(in.size());
    for (const char c : in)
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
            default:
                out += c;
        }
    }
    return out;
}

[[nodiscard]] inline std::string formatNow()
{
    const auto t = std::time(nullptr);
    std::tm tmBuf{};
#ifdef _WIN32
    localtime_s(&tmBuf, &t);
#else
    localtime_r(&t, &tmBuf);
#endif
    std::ostringstream oss;
    oss << std::put_time(&tmBuf, "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

[[nodiscard]] inline std::string formatBytes(const size_t bytes)
{
    static constexpr std::array<const char*, 4> units{"B", "KB", "MB", "GB"};
    auto value = static_cast<double>(bytes);
    size_t unit = 0;
    while (value >= 1024.0 && unit + 1 < units.size())
    {
        value /= 1024.0;
        ++unit;
    }
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(unit == 0 ? 0 : 2) << value << " " << units[unit];
    return oss.str();
}

inline void writeResultRow(std::ostream& out, const SutResult& r)
{
    out << "<tr><td>" << htmlEscape(r.variant) << "</td>" << "<td>" << std::fixed << std::setprecision(1)
        << r.nsPerBlock << "</td>" << "<td>" << std::fixed << std::setprecision(0) << r.samplesPerSecond << "</td>"
        << "<td>" << std::fixed << std::setprecision(1) << r.realtimeMultiple << "x</td>" << "<td>"
        << formatBytes(r.instanceBytes) << "</td>" << "<td>" << r.maxInstances << (r.cappedAtLimit ? "+" : "")
        << "</td>" << "<td>" << formatBytes(r.totalBytesAtMax) << "</td></tr>\n";
}

}

[[nodiscard]] inline std::string renderHtmlReport(const ReportHeader& header, const std::vector<SutResult>& results)
{
    std::ostringstream html;
    html << "<!doctype html><html><head><meta charset=\"utf-8\"><title>AbacDsp Performance Report</title>\n";
    html << "<style>\n"
            "body{font-family:-apple-system,Helvetica,Arial,sans-serif;margin:2em;color:#1a1a1a;background:#fff}\n"
            "h1{font-size:1.4em}h2{font-size:1.1em;margin-top:2em;border-bottom:1px solid #ccc}\n"
            "table{border-collapse:collapse;width:100%;margin:0.5em 0 1.5em}\n"
            "th,td{border:1px solid #ddd;padding:6px 10px;text-align:right;font-variant-numeric:tabular-nums}\n"
            "th:first-child,td:first-child{text-align:left}\n"
            "th{background:#f2f2f2}\n"
            "dl{display:grid;grid-template-columns:max-content 1fr;gap:2px 1em;font-size:0.9em}\n"
            "dt{font-weight:600}\n"
            ".warn{color:#a33;font-weight:600}\n"
            "</style></head><body>\n";

    html << "<h1>AbacDsp Single-Thread Performance Report</h1>\n<dl>";
    html << "<dt>Generated</dt><dd>" << detail::formatNow() << "</dd>";
    html << "<dt>Comment</dt><dd>" << detail::htmlEscape(header.comment) << "</dd>";
    html << "<dt>Git</dt><dd>" << detail::htmlEscape(header.git.branch) << " @ "
         << detail::htmlEscape(header.git.commit) << "</dd>";
    html << "<dt>CPU</dt><dd>" << detail::htmlEscape(header.cpu.model) << ", ";
    if (header.cpu.frequencyGHz > 0.0)
    {
        html << std::fixed << std::setprecision(2) << header.cpu.frequencyGHz << " GHz, ";
    }
    html << header.cpu.memoryMB << " MB RAM</dd>";
    html << "<dt>Sample rate / block size</dt><dd>" << static_cast<int>(kSampleRate) << " Hz / " << kBlockSize
         << " samples (" << std::fixed << std::setprecision(2)
         << (1000.0 * static_cast<double>(kBlockSize) / kSampleRate) << " ms)</dd>";
    html << "<dt>Total run time</dt><dd>" << std::fixed << std::setprecision(1) << header.totalRunSeconds << " s</dd>";
    if (!header.isReleaseBuild)
    {
        html << "<dt class=\"warn\">Build</dt><dd class=\"warn\">Not a Release build - timings are not "
                "meaningful</dd>";
    }
    html << "</dl>\n";

    const std::string columns = "<tr><th>Variant</th><th>ns/block</th><th>samples/s</th><th>realtime</th>"
                                "<th>bytes/instance</th><th>max instances</th><th>bytes @ max</th></tr>\n";

    std::vector<std::string> categories;
    for (const auto& r : results)
    {
        if (std::ranges::find(categories, r.category) == categories.end())
        {
            categories.push_back(r.category);
        }
    }
    for (const auto& category : categories)
    {
        html << "<h2>" << detail::htmlEscape(category) << "</h2>\n<table>" << columns;
        for (const auto& r : results)
        {
            if (r.category == category)
            {
                detail::writeResultRow(html, r);
            }
        }
        html << "</table>\n";
    }

    html << "<p style=\"font-size:0.85em;color:#666\">max instances = highest instance count whose worst "
            "single-thread pass over all instances still finished inside one block period; a trailing '+' means "
            "the search was capped, not exhausted. bytes/instance is sizeof() of the benchmarked object only and "
            "excludes any internal heap buffers (relevant for the sample-rate converters).</p>\n";
    html << "</body></html>\n";
    return html.str();
}

inline void writeHtmlReport(const std::string& path, const ReportHeader& header, const std::vector<SutResult>& results)
{
    std::ofstream out(path);
    out << renderHtmlReport(header, results);
}

}
