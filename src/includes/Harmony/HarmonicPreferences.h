#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <optional>

namespace AbacDsp
{

/// @ingroup harmony
/// @brief The handful of hard-constraint kinds a patch can vow to keep - see HarmonicOrganism
/// for how each is actually evaluated against a candidate transition.
enum class VowKind
{
    KeepHomeAudible,
    SparseLowEnd,
    NoLargeVoiceJumps,
    PreserveOpenIntervals,
    LimitBrightness,
    LimitDensity
};

/// @ingroup harmony
/// @brief How strongly a vow holds: Never rejects a violating candidate outright;
/// Usually/Sometimes only penalize a violating candidate's score.
enum class VowStrength
{
    Never,
    Usually,
    Sometimes
};

struct Vow
{
    VowKind kind{VowKind::KeepHomeAudible};
    VowStrength strength{VowStrength::Usually};
};

inline constexpr float kUsuallyVowPenalty{0.6f};
inline constexpr float kSometimesVowPenalty{0.25f};

/// @brief Judges one vow against a candidate: nullopt if a Never vow is violated (the
/// candidate is rejected outright), otherwise `score` minus the strength's penalty (0 if the
/// vow isn't violated at all).
[[nodiscard]] constexpr std::optional<float> applyVow(const Vow& vow, const bool violated, const float score) noexcept
{
    if (!violated)
    {
        return score;
    }
    if (vow.strength == VowStrength::Never)
    {
        return std::nullopt;
    }
    const auto penalty = vow.strength == VowStrength::Usually ? kUsuallyVowPenalty : kSometimesVowPenalty;
    return score - penalty;
}

/// @ingroup harmony
/// @brief The moving centres of gravity a candidate harmonic state is scored against - each a
/// 0..1 axis (0.5 neutral) toward a "more" pole (brighter, closer to home, denser, ...).
enum class WishKind
{
    Luminosity,
    Closeness,
    Ambiguity,
    MinorColor,
    Density,
    Mobility,
    Tension
};

inline constexpr size_t kNumWishKinds{7};

/// @brief One weight per WishKind, indexed by `static_cast<size_t>(kind)`.
using WishWeights = std::array<float, kNumWishKinds>;

[[nodiscard]] constexpr WishWeights neutralWishWeights() noexcept
{
    WishWeights weights{};
    weights.fill(0.5f);
    return weights;
}

[[nodiscard]] constexpr float wishWeight(const WishWeights& weights, const WishKind kind) noexcept
{
    return weights[static_cast<size_t>(kind)];
}

constexpr void setWishWeight(WishWeights& weights, const WishKind kind, const float value) noexcept
{
    weights[static_cast<size_t>(kind)] = std::clamp(value, 0.f, 1.f);
}

/// @ingroup harmony
/// @brief A player performance gesture. Release is special: it carries no wish bias of its
/// own, it instead fast-decays every other active impulse (see ActiveImpulse::releaseEarly).
enum class ImpulseKind
{
    Stay,
    Lean,
    Open,
    Gather,
    Darken,
    Brighten,
    Disturb,
    Arrive,
    Release
};

/// @brief One wish this impulse nudges, and by how much (added to the wish's current weight
/// at the impulse's full strength).
struct WishBias
{
    WishKind kind{WishKind::Luminosity};
    float delta{0.f};
};

inline constexpr size_t kMaxBiasesPerImpulse{3};

struct ImpulseBiasSet
{
    std::array<WishBias, kMaxBiasesPerImpulse> biases{};
    size_t count{0};
};

/// @brief The fixed, curated wish biases for one impulse kind (see the spec's own impulse
/// table for the musical intent each maps onto).
[[nodiscard]] constexpr ImpulseBiasSet impulseBiases(const ImpulseKind kind) noexcept
{
    switch (kind)
    {
        case ImpulseKind::Stay:
            return {.biases = {WishBias{WishKind::Mobility, -0.4f}, WishBias{WishKind::Closeness, 0.3f}}, .count = 2};
        case ImpulseKind::Lean:
            return {.biases = {WishBias{WishKind::Mobility, 0.3f}}, .count = 1};
        case ImpulseKind::Open:
            return {.biases = {WishBias{WishKind::Ambiguity, 0.3f}, WishBias{WishKind::Density, -0.3f}}, .count = 2};
        case ImpulseKind::Gather:
            return {.biases = {WishBias{WishKind::Density, -0.3f}, WishBias{WishKind::Closeness, 0.3f},
                               WishBias{WishKind::Ambiguity, -0.3f}},
                    .count = 3};
        case ImpulseKind::Darken:
            return {.biases = {WishBias{WishKind::Luminosity, -0.4f}, WishBias{WishKind::MinorColor, 0.3f}},
                    .count = 2};
        case ImpulseKind::Brighten:
            return {.biases = {WishBias{WishKind::Luminosity, 0.4f}, WishBias{WishKind::MinorColor, -0.3f}},
                    .count = 2};
        case ImpulseKind::Disturb:
            return {.biases = {WishBias{WishKind::Tension, 0.4f}, WishBias{WishKind::Ambiguity, 0.3f}}, .count = 2};
        case ImpulseKind::Arrive:
            return {.biases = {WishBias{WishKind::Closeness, 0.4f}, WishBias{WishKind::Tension, -0.3f},
                               WishBias{WishKind::Mobility, -0.3f}},
                    .count = 3};
        case ImpulseKind::Release:
        default:
            return {};
    }
}

/**
 * @ingroup harmony
 * @brief One triggered impulse's envelope: rises over kAttackSeconds, holds at full strength
 * for kHoldSeconds, then fades back to 0 over kDecaySeconds - a bias with a life cycle, per the
 * spec's "enters, influences future choices for a while, and fades", not an instant switch.
 */
class ActiveImpulse
{
  public:
    static constexpr float kAttackSeconds{1.f};
    static constexpr float kHoldSeconds{8.f};
    static constexpr float kDecaySeconds{12.f};

    explicit constexpr ActiveImpulse(const ImpulseKind kind) noexcept
        : m_kind(kind)
    {
    }

    /// @brief Advances the envelope; call once per evaluation with the elapsed time since last.
    constexpr void advance(const float deltaSeconds) noexcept
    {
        m_elapsedSeconds += deltaSeconds;
    }

    /// @brief Forces the envelope into its decay phase early - e.g. when Release is triggered.
    constexpr void releaseEarly() noexcept
    {
        m_elapsedSeconds = std::max(m_elapsedSeconds, kAttackSeconds + kHoldSeconds);
    }

    [[nodiscard]] constexpr ImpulseKind kind() const noexcept
    {
        return m_kind;
    }

    /// @brief Current strength, 0..1: rising through the attack, held at 1, then fading to 0.
    [[nodiscard]] constexpr float envelopeValue() const noexcept
    {
        if (m_elapsedSeconds < kAttackSeconds)
        {
            return kAttackSeconds > 0.f ? m_elapsedSeconds / kAttackSeconds : 1.f;
        }
        const auto afterAttack = m_elapsedSeconds - kAttackSeconds;
        if (afterAttack < kHoldSeconds)
        {
            return 1.f;
        }
        const auto afterHold = afterAttack - kHoldSeconds;
        if (afterHold >= kDecaySeconds)
        {
            return 0.f;
        }
        return 1.f - afterHold / kDecaySeconds;
    }

    [[nodiscard]] constexpr bool isFinished() const noexcept
    {
        return m_elapsedSeconds >= kAttackSeconds + kHoldSeconds + kDecaySeconds;
    }

  private:
    ImpulseKind m_kind;
    float m_elapsedSeconds{0.f};
};

/// @brief Adds this impulse's current-strength bias onto weights, clamped to 0..1. Call once
/// per evaluation with weights already holding each wish's baseline (e.g. from the climate
/// process) - this does not itself accumulate across repeated calls.
constexpr void applyImpulse(const ActiveImpulse& impulse, WishWeights& weights) noexcept
{
    const auto strength = impulse.envelopeValue();
    const auto biasSet = impulseBiases(impulse.kind());
    for (size_t i = 0; i < biasSet.count; ++i)
    {
        const auto& bias = biasSet.biases[i];
        setWishWeight(weights, bias.kind, wishWeight(weights, bias.kind) + bias.delta * strength);
    }
}

}
