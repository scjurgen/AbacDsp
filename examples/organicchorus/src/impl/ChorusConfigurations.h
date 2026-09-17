#pragma once

#include <array>
#include <cstddef>

// How the Tone/Speed/Depth/Feedback/Mix macros (0..1 fractions) map onto
// OrganicChorusEngine's plain DSP setters, per Configuration. First-pass numbers, not
// measured against a real BBD pedal - expect a tuning-by-ear pass later.

namespace OrganicChorus
{

enum class Configuration : size_t
{
    Classic = 0,
    Wide = 1,
    TriEnsemble = 2,
    Flanger = 3,
};

constexpr size_t kConfigurationCount{4};
constexpr size_t kMaxVoices{4};

// A value at macro fraction 0 and at fraction 1; at() linearly interpolates between them,
// so atZero need not be numerically smaller than atOne (Tone's high-pass, for one, runs
// from a higher cutoff at 0 down to a lower one at 1).
struct MacroRange
{
    float atZero;
    float atOne;

    [[nodiscard]] constexpr float at(const float fraction) const noexcept
    {
        return atZero + (atOne - atZero) * fraction;
    }
};

struct VoiceOffset
{
    float delayOffsetMs{0.f};
    float rateMultiplier{1.f};
    float pan{0.f};
};

struct ChorusConfigurationSpec
{
    size_t voiceCount;
    std::array<VoiceOffset, kMaxVoices> voices;
    MacroRange baseDelayMs;
    MacroRange wowRateHz;
    MacroRange wowDepth;
    // Flutter is physically the fast end of tape speed error - its rate follows Speed but
    // never drops below this floor, however slow Speed itself goes.
    float flutterRateFloorHz;
    MacroRange flutterDepth;
    MacroRange toneHighPassHz;
    MacroRange tonePreLowPassHz;
    MacroRange tonePostLowPassHz;
    float saturation;
    MacroRange feedback;
    float feedbackDampHz;
    float readHeadSafetyMarginSamples;
    float readHeadCorrectionThresholdSamples;
    // Ornstein-Uhlenbeck sigma at Drift=1: real mechanical speed wander on the tape
    // transport's own clock, independent of Depth - see Organicchorus_tests.cpp.
    float speedDriftMaxSigma;
};

// clang-format off
inline constexpr std::array<ChorusConfigurationSpec, kConfigurationCount> kConfigurations{{
    // Classic: one voice, short/tight, classic "watery" triangle-adjacent chorus.
    ChorusConfigurationSpec{
        1, {{{0.f, 1.f, 0.f}, {}, {}, {}}},
        {6.f, 9.f}, {0.05f, 6.0f}, {0.15f, 0.45f},
        0.6f, {0.1f, 0.5f},
        {400.f, 60.f}, {1200.f, 10000.f}, {1200.f, 10000.f},
        0.15f, {-0.80f, 0.80f}, 3500.f,
        250.f, 190.f, 0.05f
    },
    // Wide: two voices, panned hard, slightly detuned rate/offset for stereo spread.
    ChorusConfigurationSpec{
        2, {{{0.f, 1.f, -1.f}, {2.f, 1.08f, 1.f}, {}, {}}},
        {6.f, 11.f}, {0.05f, 6.0f}, {0.2f, 0.50f},
        0.6f, {0.15f, 0.55f},
        {350.f, 50.f}, {1500.f, 12000.f}, {1500.f, 12000.f},
        0.15f, {-0.80f, 0.80f}, 4000.f,
        250.f, 190.f, 0.05f
    },
    // Tri Ensemble: three voices spread wider/slower, cleaner (less BBD colour).
    ChorusConfigurationSpec{
        3,
        {{{0.f, 1.f, -1.f}, {5.f, 1.05f, 0.f}, {10.f, 0.93f, 1.f}, {}}},
        {8.f, 20.f}, {0.05f, 6.0f}, {0.3f, 0.60f},
        0.5f, {0.1f, 0.35f},
        {300.f, 40.f}, {2000.f, 16000.f}, {2000.f, 16000.f},
        0.05f, {-0.75f, 0.75f}, 6000.f,
        280.f, 220.f, 0.06f
    },
    // Flanger: very short delay, bipolar feedback, wide/fast sweep, brighter tone.
    ChorusConfigurationSpec{
        1, {{{0.f, 1.f, 0.f}, {}, {}, {}}},
        {7.f, 10.f}, {0.05f, 6.0f}, {0.2f, 0.55f},
        0.8f, {0.2f, 0.6f},
        {200.f, 20.f}, {1000.f, 14000.f}, {1000.f, 14000.f},
        0.1f, {-0.95f, 0.95f}, 9000.f,
        270.f, 210.f, 0.05f
    },
}};
// clang-format on

}
