#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <format>
#include <iterator>
#include <nlohmann/json.hpp>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>

namespace DrumInput
{

enum class Pad : uint8_t
{
    Kick,
    Snare,
    Hihat,
    HihatPedal,
    Crash,
    TomHigh,
    TomLow,
    TomFloor,
};

struct PadInfo
{
    std::string_view jsonKey;
    std::string_view label;
};

// Indexed by Pad.
// clang-format off
inline constexpr auto kPads = std::to_array<PadInfo>({
    {"kick",       "Kick"},
    {"snare",      "Snare"},
    {"hihat",      "Hihat"},
    {"hihatPedal", "Hihat pedal"},
    {"crash",      "Crash"},
    {"tomHigh",    "Tom high"},
    {"tomLow",     "Tom low"},
    {"tomFloor",   "Tom floor"},
});
// clang-format on
static_assert(kPads.size() == static_cast<size_t>(Pad::TomFloor) + 1);

inline constexpr uint8_t kNoNote{0xFF};
inline constexpr uint8_t kAnyChannel{0};
inline constexpr size_t kMaxNameBytes{64};
inline constexpr float kMaxCcGraceMs{20.f};

struct HihatSettings
{
    uint8_t pedalCc{4};
    uint8_t halfOpenAbove{120};
    uint8_t closedAbove{125};
    float ccGraceMs{3.f};
};

struct ClickSettings
{
    uint8_t outputChannel{10};
    uint8_t downbeatNote{58};
    uint8_t beatNote{59};
};

// Trivially copyable, so it can be handed to the audio thread through a TripleBuffer.
// Defaults are the built-in kit; note names in a file are read with middleCOctave (4: C4 = 60).
struct DrumMap
{
    uint8_t inputChannel{kAnyChannel};
    int middleCOctave{4};
    std::array<uint8_t, kPads.size()> notes{74, 60, 64, 76, 70, 63, 62, 72};
    HihatSettings hihat{};
    ClickSettings click{};

    [[nodiscard]] bool acceptsChannel(const uint8_t channel) const noexcept
    {
        return inputChannel == kAnyChannel || inputChannel == channel;
    }

    [[nodiscard]] std::optional<Pad> padForNote(const uint8_t note) const noexcept
    {
        const auto found = std::ranges::find(notes, note);
        if (note == kNoNote || found == notes.end())
        {
            return std::nullopt;
        }
        return static_cast<Pad>(std::distance(notes.begin(), found));
    }
};

struct DrumMapFile
{
    DrumMap map;
    std::string name;
};

struct DrumMapLoad
{
    std::optional<DrumMapFile> file;
    std::string error;
};

[[nodiscard]] inline DrumMapFile defaultDrumMapFile()
{
    return {DrumMap{}, "Built-in default"};
}

[[nodiscard]] constexpr std::optional<int> pitchClass(const char letter) noexcept
{
    constexpr auto kLetters = std::to_array<char>({'C', 'D', 'E', 'F', 'G', 'A', 'B'});
    constexpr auto kClasses = std::to_array<int>({0, 2, 4, 5, 7, 9, 11});
    const auto found = std::ranges::find(kLetters, letter);
    if (found == kLetters.end())
    {
        return std::nullopt;
    }
    return kClasses[static_cast<size_t>(std::distance(kLetters.begin(), found))];
}

// Accepts "C4", "D#4", "Bb3", "C-1": letter, optional '#' or 'b', whole octave number.
[[nodiscard]] inline std::optional<uint8_t> parseNoteName(const std::string_view text, const int middleCOctave)
{
    if (text.size() < 2)
    {
        return std::nullopt;
    }
    const auto base = pitchClass(text.front());
    if (!base)
    {
        return std::nullopt;
    }
    std::string_view rest = text.substr(1);
    int accidental = 0;
    if (rest.front() == '#' || rest.front() == 'b')
    {
        accidental = rest.front() == '#' ? 1 : -1;
        rest.remove_prefix(1);
    }
    const bool negative = !rest.empty() && rest.front() == '-';
    if (negative)
    {
        rest.remove_prefix(1);
    }
    if (rest.empty() || rest.size() > 2 ||
        !std::ranges::all_of(rest, [](const char c) { return c >= '0' && c <= '9'; }))
    {
        return std::nullopt;
    }
    const int magnitude = std::stoi(std::string{rest});
    const int octave = negative ? -magnitude : magnitude;
    const int note = 60 + 12 * (octave - middleCOctave) + *base + accidental;
    if (note < 0 || note > 127)
    {
        return std::nullopt;
    }
    return static_cast<uint8_t>(note);
}

[[nodiscard]] inline std::string noteName(const uint8_t note, const int middleCOctave)
{
    constexpr auto kNames =
        std::to_array<std::string_view>({"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"});
    const int octave = static_cast<int>(note) / 12 - 5 + middleCOctave;
    return std::format("{}{}", kNames[note % 12], octave);
}

namespace detail
{
using Json = nlohmann::json;

[[noreturn]] inline void fail(const std::string& message)
{
    throw std::runtime_error(message);
}

inline void requireObject(const Json& value, const std::string_view where)
{
    if (!value.is_object())
    {
        fail(std::format("{} must be an object", where));
    }
}

inline void rejectUnknownKeys(const Json& object, const std::span<const std::string_view> known,
                              const std::string_view where)
{
    for (const auto& item : object.items())
    {
        if (std::ranges::find(known, item.key()) == known.end())
        {
            fail(std::format("unknown key \"{}\" in {}", item.key(), where));
        }
    }
}

[[nodiscard]] inline int intInRange(const Json& value, const std::string_view what, const int low, const int high)
{
    if (!value.is_number_integer())
    {
        fail(std::format("{} must be a whole number", what));
    }
    const auto number = value.get<long long>();
    if (number < low || number > high)
    {
        fail(std::format("{} must be {} to {}", what, low, high));
    }
    return static_cast<int>(number);
}

[[nodiscard]] inline uint8_t byteInRange(const Json& value, const std::string_view what, const int low, const int high)
{
    return static_cast<uint8_t>(intInRange(value, what, low, high));
}

[[nodiscard]] inline uint8_t noteValue(const Json& value, const std::string_view what, const int middleCOctave)
{
    if (!value.is_string())
    {
        return byteInRange(value, what, 0, 127);
    }
    const auto text = value.get<std::string>();
    const auto note = parseNoteName(text, middleCOctave);
    if (!note)
    {
        fail(std::format("{}: \"{}\" is not a note name (like C4, D#4, Bb3) in 0 .. 127", what, text));
    }
    return *note;
}

[[nodiscard]] inline std::string parseName(const Json& value)
{
    if (!value.is_string())
    {
        fail("name must be a string");
    }
    auto name = value.get<std::string>();
    const auto isControl = [](const char c) { return static_cast<unsigned char>(c) < 0x20 || c == 0x7F; };
    if (name.empty() || name.size() > kMaxNameBytes || std::ranges::any_of(name, isControl))
    {
        fail(std::format("name must be 1 to {} bytes without control characters", kMaxNameBytes));
    }
    return name;
}

inline void parseNotes(const Json& notes, DrumMap& map)
{
    requireObject(notes, "notes");
    std::array<std::string_view, kPads.size()> keys{};
    std::ranges::transform(kPads, keys.begin(), &PadInfo::jsonKey);
    rejectUnknownKeys(notes, keys, "notes");
    map.notes.fill(kNoNote);
    for (size_t pad = 0; pad < kPads.size(); ++pad)
    {
        const std::string key{kPads[pad].jsonKey};
        if (!notes.contains(key))
        {
            continue;
        }
        const uint8_t note = noteValue(notes.at(key), "notes." + key, map.middleCOctave);
        if (const auto other = map.padForNote(note))
        {
            fail(std::format("notes.{} uses note {}, already used by notes.{}", key, note,
                             kPads[static_cast<size_t>(*other)].jsonKey));
        }
        map.notes[pad] = note;
    }
}

inline void parseHihat(const Json& hihat, HihatSettings& settings)
{
    requireObject(hihat, "hihat");
    constexpr auto kKeys = std::to_array<std::string_view>({"pedalCc", "halfOpenAbove", "closedAbove", "ccGraceMs"});
    rejectUnknownKeys(hihat, kKeys, "hihat");
    if (hihat.contains("pedalCc"))
    {
        settings.pedalCc = byteInRange(hihat.at("pedalCc"), "hihat.pedalCc", 0, 127);
    }
    if (hihat.contains("halfOpenAbove"))
    {
        settings.halfOpenAbove = byteInRange(hihat.at("halfOpenAbove"), "hihat.halfOpenAbove", 0, 127);
    }
    if (hihat.contains("closedAbove"))
    {
        settings.closedAbove = byteInRange(hihat.at("closedAbove"), "hihat.closedAbove", 0, 127);
    }
    if (settings.halfOpenAbove > settings.closedAbove)
    {
        fail("hihat.halfOpenAbove must not be above hihat.closedAbove");
    }
    if (hihat.contains("ccGraceMs"))
    {
        const auto& grace = hihat.at("ccGraceMs");
        if (!grace.is_number() || grace.get<double>() < 0.0 || grace.get<double>() > kMaxCcGraceMs)
        {
            fail(std::format("hihat.ccGraceMs must be a number from 0 to {}", kMaxCcGraceMs));
        }
        settings.ccGraceMs = grace.get<float>();
    }
}

inline void parseClick(const Json& click, ClickSettings& settings, const int middleCOctave)
{
    requireObject(click, "click");
    constexpr auto kKeys = std::to_array<std::string_view>({"outputChannel", "downbeat", "beat"});
    rejectUnknownKeys(click, kKeys, "click");
    if (click.contains("outputChannel"))
    {
        settings.outputChannel = byteInRange(click.at("outputChannel"), "click.outputChannel", 1, 16);
    }
    if (click.contains("downbeat"))
    {
        settings.downbeatNote = noteValue(click.at("downbeat"), "click.downbeat", middleCOctave);
    }
    if (click.contains("beat"))
    {
        settings.beatNote = noteValue(click.at("beat"), "click.beat", middleCOctave);
    }
}

[[nodiscard]] inline DrumMapFile parseRoot(const Json& root)
{
    requireObject(root, "the drum map");
    constexpr auto kKeys =
        std::to_array<std::string_view>({"name", "inputChannel", "middleCOctave", "notes", "hihat", "click"});
    rejectUnknownKeys(root, kKeys, "the drum map");
    if (!root.contains("notes"))
    {
        fail("the drum map needs a \"notes\" object");
    }
    DrumMapFile file{DrumMap{}, "Unnamed drum map"};
    if (root.contains("name"))
    {
        file.name = parseName(root.at("name"));
    }
    if (root.contains("inputChannel"))
    {
        file.map.inputChannel = byteInRange(root.at("inputChannel"), "inputChannel", 0, 16);
    }
    if (root.contains("middleCOctave"))
    {
        file.map.middleCOctave = intInRange(root.at("middleCOctave"), "middleCOctave", 2, 5);
    }
    parseNotes(root.at("notes"), file.map);
    if (root.contains("hihat"))
    {
        parseHihat(root.at("hihat"), file.map.hihat);
    }
    if (root.contains("click"))
    {
        parseClick(root.at("click"), file.map.click, file.map.middleCOctave);
    }
    return file;
}
}

// Unknown keys are errors so that a misspelt key is not silently ignored. Pads missing from
// "notes" stay unmapped; every other missing field keeps its built-in default.
[[nodiscard]] inline DrumMapLoad parseDrumMap(const std::string_view jsonText)
{
    try
    {
        return {detail::parseRoot(detail::Json::parse(jsonText)), {}};
    }
    catch (const detail::Json::exception& e)
    {
        return {std::nullopt, std::string{"invalid JSON: "} + e.what()};
    }
    catch (const std::runtime_error& e)
    {
        return {std::nullopt, e.what()};
    }
}

}
