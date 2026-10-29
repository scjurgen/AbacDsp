#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>

#include "Generators/KarplusStrongVoice.h"
#include "Numbers/Convert.h"

namespace KsInstrument
{

enum class Id : uint8_t
{
    Guitar,
    Bass,
    Harp,
    Nylon,
    Ukulele,
    MusicBox,
};

inline constexpr size_t kMaxStringLength{10000};
using Voice = AbacDsp::KarplusStrongVoice<kMaxStringLength>;

// Level of the first 100 ms (RMS, full scale) of a note at velocity 127 after the preset's trim.
inline constexpr float kReferenceLevelDb{-26.f};
// Relative dynamics: every velocity step is the same number of dB.
inline constexpr float kDbPerVelocityStep{0.25f};
inline constexpr int kTrimStepSemitones{4};

// dB to add so every pitch lands on kReferenceLevelDb; entry i is lowestNote + i * kTrimStepSemitones.
// The excitation must stay deterministic: a random burst varies a note's level by several dB.
inline constexpr auto kGuitarTrimDb =
    std::to_array<float>({-0.2f, -0.6f, -1.9f, -2.1f, -3.1f, -3.6f, -4.6f, -4.7f, -5.5f, -5.6f, -6.1f, -6.8f});
inline constexpr auto kBassTrimDb =
    std::to_array<float>({-12.8f, -13.0f, -13.2f, -13.3f, -13.3f, -13.4f, -13.4f, -13.4f, -13.4f, -13.3f});
inline constexpr auto kHarpTrimDb =
    std::to_array<float>({-2.3f, -2.7f, -3.8f, -4.4f, -5.3f, -5.6f, -6.4f, -6.5f, -6.9f, -7.5f});
inline constexpr auto kNylonTrimDb =
    std::to_array<float>({1.3f, 1.6f, 0.4f, 0.9f, -0.4f, -1.0f, -2.5f, -2.4f, -3.5f, -4.1f, -4.9f, -5.9f});
inline constexpr auto kUkuleleTrimDb = std::to_array<float>({-3.4f, -4.3f, -4.3f, -5.0f, -4.9f, -5.2f, -5.6f, -5.4f});
inline constexpr auto kMusicBoxTrimDb = std::to_array<float>({-12.4f, -12.0f, -11.6f, -11.1f, -10.4f, -9.6f, -8.6f});

struct Preset
{
    std::string_view name;
    AbacDsp::PluckType pluck;
    float decayMs;
    float decayOctaveFactor;
    float damper;
    float cutoffSemitones;
    int lowestNote;
    std::span<const float> trimDb;

    [[nodiscard]] constexpr int highestNote() const noexcept
    {
        return lowestNote + static_cast<int>(trimDb.size() - 1) * kTrimStepSemitones;
    }
};

// Indexed by Id.
inline constexpr auto kPresets = std::to_array<Preset>({
    {"Guitar", AbacDsp::PluckType::WhiteStatic, 1500.f, 0.5f, 0.3f, 24.f, 40, kGuitarTrimDb},
    {"Bass", AbacDsp::PluckType::Triangle, 2500.f, 0.3f, 0.35f, 20.f, 28, kBassTrimDb},
    {"Harp", AbacDsp::PluckType::WhiteStatic, 2500.f, 0.6f, 0.15f, 36.f, 48, kHarpTrimDb},
    {"Nylon", AbacDsp::PluckType::WhiteStatic, 1800.f, 0.5f, 0.55f, 20.f, 40, kNylonTrimDb},
    {"Ukulele", AbacDsp::PluckType::WhiteStatic, 900.f, 0.7f, 0.25f, 30.f, 60, kUkuleleTrimDb},
    {"Music box", AbacDsp::PluckType::Triangle, 1200.f, 0.9f, 0.1f, 40.f, 72, kMusicBoxTrimDb},
});
static_assert(kPresets.size() == static_cast<size_t>(Id::MusicBox) + 1);

[[nodiscard]] constexpr const Preset& presetOf(const Id id) noexcept
{
    return kPresets[static_cast<size_t>(id)];
}

[[nodiscard]] constexpr std::optional<Id> idFromIndex(const long long index) noexcept
{
    if (index < 0 || index >= static_cast<long long>(kPresets.size()))
    {
        return std::nullopt;
    }
    return static_cast<Id>(index);
}

// Linear between the measured points, held flat outside the preset's range.
[[nodiscard]] inline float trimDbAt(const Preset& preset, const float note) noexcept
{
    const float position = (note - static_cast<float>(preset.lowestNote)) / static_cast<float>(kTrimStepSemitones);
    const float clamped = std::clamp(position, 0.f, static_cast<float>(preset.trimDb.size() - 1));
    const auto below = static_cast<size_t>(clamped);
    const size_t above = std::min(below + 1, preset.trimDb.size() - 1);
    const float fraction = clamped - static_cast<float>(below);
    return preset.trimDb[below] + fraction * (preset.trimDb[above] - preset.trimDb[below]);
}

[[nodiscard]] inline float velocityDb(const uint8_t velocity) noexcept
{
    return (static_cast<float>(velocity) - 127.f) * kDbPerVelocityStep;
}

// Gain handed to the voice for a note: the velocity's level on top of the pitch trim.
[[nodiscard]] inline float noteGain(const Preset& preset, const uint8_t pitch, const uint8_t velocity) noexcept
{
    return Convert::dbToGain<float>(velocityDb(velocity) + trimDbAt(preset, static_cast<float>(pitch)));
}

// The filter envelope is held at sustain 1, which keeps the cutoff open for the whole note.
inline void apply(const Preset& preset, Voice& voice) noexcept
{
    voice.setPluckType(preset.pluck);
    voice.setDecayByTime(preset.decayMs);
    voice.setDecayOctaveFactor(preset.decayOctaveFactor);
    voice.setDamper(preset.damper);
    voice.setFilterCutoffSemitones(preset.cutoffSemitones);
    voice.setKeyTracking(1.f);
    voice.setFilterResonance(0.1f);
    voice.setFilterEnvelope(1.f, 1.f, 1.f);
}

}
