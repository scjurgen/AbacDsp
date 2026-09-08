#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <optional>
#include <random>

#include "Generators/OrnsteinUhlenbeckProcess.h"
#include "Harmony/HarmonicPalette.h"
#include "Parameters/SmoothingParameter.h"

namespace AbacDsp
{

inline constexpr float kMobilityRangeSemitones{24.f};
inline constexpr int kLargeJumpSemitones{7};
inline constexpr int kLowRegisterThreshold{-6};
inline constexpr float kBrightnessLimit{0.85f};
inline constexpr size_t kDensityLimit{6};
// Chosen empirically (see dev-explore.sh probes during Character-control tuning): the
// ChromaticWeather region's entries all violate KeepHomeAudible (Usually, -0.6 penalty) by
// design, so a bonus much below this barely moves them at all against candidates that don't.
inline constexpr float kPreferredRegionBonus{0.7f};

/// @brief A soft nudge toward one palette region - preferredRegion's own region gets
/// kPreferredRegionBonus added to its score; nullopt (no preference) or any other region
/// gets nothing. A bonus rather than a filter, so an unreachable region never dead-ends.
[[nodiscard]] constexpr float regionBonus(const HarmonicState& candidate,
                                          const std::optional<PaletteRegion>& preferredRegion) noexcept
{
    return preferredRegion && candidate.region == *preferredRegion ? kPreferredRegionBonus : 0.f;
}

/// @brief This candidate's affinity (0..1) for one wish axis: how close candidate's own
/// value on that axis sits to weights' current target. The five intrinsic axes read
/// candidate's tag directly; Closeness/Mobility read the move from current instead.
[[nodiscard]] constexpr float wishAffinity(const HarmonicState& current, const HarmonicState& candidate,
                                           const WishKind kind, const WishWeights& weights) noexcept
{
    if (const auto tag = stateTag(candidate, kind))
    {
        return 1.f - std::abs(wishWeight(weights, kind) - *tag);
    }
    if (kind == WishKind::Closeness)
    {
        const auto currentPc = current.voicing.pitchClasses();
        const auto candidatePc = candidate.voicing.pitchClasses();
        const auto biggest = std::max(currentPc.size(), candidatePc.size());
        const float closeness =
            biggest > 0 ? static_cast<float>(currentPc.commonToneCount(candidatePc)) / static_cast<float>(biggest)
                        : 1.f;
        return 1.f - std::abs(wishWeight(weights, WishKind::Closeness) - closeness);
    }
    const auto motion = voiceLeadingMotion(current.voicing, candidate.voicing);
    const float mobility =
        std::clamp(static_cast<float>(motion.totalMotionSemitones) / kMobilityRangeSemitones, 0.f, 1.f);
    return 1.f - std::abs(wishWeight(weights, WishKind::Mobility) - mobility);
}

/// @brief Whether candidate breaks one vow kind, judged against homeOffsetSemitones (the
/// current tonal home) and, for the motion-based kind, against current's own voicing.
[[nodiscard]] constexpr bool vowViolated(const VowKind kind, const int homeOffsetSemitones,
                                         const HarmonicState& current, const HarmonicState& candidate) noexcept
{
    switch (kind)
    {
        case VowKind::KeepHomeAudible:
            return !candidate.voicing.pitchClasses().contains(homeOffsetSemitones);
        case VowKind::SparseLowEnd:
        {
            size_t lowCount = 0;
            for (const auto note : candidate.voicing.notes())
            {
                lowCount += note <= kLowRegisterThreshold ? 1 : 0;
            }
            return lowCount > 1;
        }
        case VowKind::NoLargeVoiceJumps:
            return voiceLeadingMotion(current.voicing, candidate.voicing).maxMotionSemitones > kLargeJumpSemitones;
        case VowKind::PreserveOpenIntervals:
        {
            const auto notes = candidate.voicing.notes();
            for (size_t i = 0; i < notes.size(); ++i)
            {
                for (size_t j = i + 1; j < notes.size(); ++j)
                {
                    const auto diff = notes[i] - notes[j];
                    if ((diff < 0 ? -diff : diff) <= 1)
                    {
                        return true;
                    }
                }
            }
            return false;
        }
        case VowKind::LimitBrightness:
            return candidate.luminosity > kBrightnessLimit;
        case VowKind::LimitDensity:
        default:
            return candidate.voicing.size() > kDensityLimit;
    }
}

/// @brief The default vow set every patch ships with in v1 (per-patch overrides are future
/// work). Only NoLargeVoiceJumps is Never - a hard cap protecting the "voice-led, not
/// block-chord replacement" character structurally, not just by preference.
inline constexpr std::array<Vow, 6> kDefaultVows{{{VowKind::KeepHomeAudible, VowStrength::Usually},
                                                  {VowKind::SparseLowEnd, VowStrength::Sometimes},
                                                  {VowKind::NoLargeVoiceJumps, VowStrength::Never},
                                                  {VowKind::PreserveOpenIntervals, VowStrength::Sometimes},
                                                  {VowKind::LimitBrightness, VowStrength::Sometimes},
                                                  {VowKind::LimitDensity, VowStrength::Sometimes}}};

/**
 * @ingroup harmony
 * @brief The slow decision process that picks the instrument's harmonic home over time: a
 * climate of wish weights drifts on its own, player impulses bias it temporarily, and every
 * dwell period the palette is scored (vow-filtered, then wish-affinity-ranked) to either commit
 * a transition or stay. Seedable for repeatable patches; see the plan's "Timing and organic
 * choice" section for the musical intent behind dwell/cooldown.
 */
class HarmonicOrganism
{
  public:
    // OrnsteinUhlenbeckProcess's theta floors its own reversion at ~1s regardless of sigma, so
    // kClimateSmoothingSeconds slows its output down to a "tens of seconds" climate timescale.
    static constexpr float kClimateRateHz{10.f};
    static constexpr float kClimateSigma{0.15f};
    static constexpr float kClimateSmoothingSeconds{40.f};
    static constexpr float kDwellSeconds{30.f};
    static constexpr float kCooldownSeconds{20.f};
    static constexpr size_t kTopCandidateCount{3};
    static constexpr size_t kMaxActiveImpulses{4};
    static constexpr size_t kMaxClimateStepsPerCall{3600};

    explicit HarmonicOrganism(const float sampleRate, const unsigned seed = 1) noexcept
        : m_sampleRate(sampleRate)
        , m_palette(defaultPalette())
        , m_rng(seed)
    {
        for (size_t i = 0; i < kNumWishKinds; ++i)
        {
            m_climate[i].seed(seed + static_cast<unsigned>(i) + 1u);
            // OrnsteinUhlenbeckProcess::setSigma() couples its reversion target to sigma
            // itself (mu = sigma), so it wanders near +sigma, not around 0 - advanceClimate()
            // subtracts kClimateSigma back out each step to recentre it.
            m_climate[i].setSigma(kClimateSigma);
        }
    }

    /// @brief Retunes the whole palette to a new tonal home; keeps the current place's index
    /// (same relative shape, now voiced around the new home) rather than resetting to home.
    void setHome(const int pitchClass) noexcept
    {
        m_homeOffsetSemitones = pitchClass;
        m_palette = transposedPalette(pitchClass);
    }

    /// @brief Soft-biases future transitions toward one palette region (see regionBonus()) -
    /// nullopt clears the preference. Persistent, like setHome(), not an impulse.
    void setPreferredRegion(const std::optional<PaletteRegion> region) noexcept
    {
        m_preferredRegion = region;
    }

    /// @brief Starts (or refreshes) one impulse's life cycle; Release instead fast-decays
    /// every other currently active impulse rather than carrying a bias of its own.
    void triggerImpulse(const ImpulseKind kind) noexcept
    {
        if (kind == ImpulseKind::Release)
        {
            for (auto& slot : m_activeImpulses)
            {
                if (slot.has_value())
                {
                    slot->releaseEarly();
                }
            }
            return;
        }
        for (auto& slot : m_activeImpulses)
        {
            if (!slot.has_value() || slot->isFinished())
            {
                slot.emplace(kind);
                return;
            }
        }
    }

    /// @brief Advances climate/impulses by the elapsed time this block represents and, once a
    /// dwell period has elapsed (and any post-transition cooldown has cleared), makes one
    /// stay-or-transition decision.
    void step(const size_t numSamples) noexcept
    {
        const auto dt = static_cast<float>(numSamples) / m_sampleRate;
        advanceClimate(dt);
        advanceImpulses(dt);

        m_dwellAccumSeconds += dt;
        m_cooldownRemainingSeconds = std::max(0.f, m_cooldownRemainingSeconds - dt);
        if (m_dwellAccumSeconds < kDwellSeconds || m_cooldownRemainingSeconds > 0.f)
        {
            return;
        }
        m_dwellAccumSeconds = 0.f;
        considerTransition();
    }

    [[nodiscard]] const HarmonicState& currentState() const noexcept
    {
        return m_palette[m_currentIndex];
    }

    /// @brief This step's effective wish weights (climate baseline plus active impulses) - the
    /// "moving wishes and their current weights" the spec's advanced view wants to show.
    [[nodiscard]] WishWeights currentWishWeights() const noexcept
    {
        return effectiveWishWeights();
    }

    /// @brief One-shot: nullopt unless step() just committed a transition, matching the
    /// existing drain*Command() idiom the realizer already uses for script commands.
    [[nodiscard]] std::optional<HarmonicState> takePendingTransition() noexcept
    {
        if (!m_pendingIndex)
        {
            return std::nullopt;
        }
        const auto result = m_palette[*m_pendingIndex];
        m_pendingIndex.reset();
        return result;
    }

  private:
    // Steps the fast underlying OU wander, recentres it (see the constructor's own note on
    // setSigma()), and re-aims a long LinearSmoothing ramp at it each tick - repeatedly
    // re-aiming a slow ramp at a fast-moving target is what turns it into a slow-moving one.
    void advanceClimate(const float dt) noexcept
    {
        m_climateAccumSeconds += dt;
        constexpr float interval = 1.f / kClimateRateHz;
        for (size_t guard = 0; m_climateAccumSeconds >= interval && guard < kMaxClimateStepsPerCall; ++guard)
        {
            for (size_t i = 0; i < kNumWishKinds; ++i)
            {
                const auto wander = m_climate[i].step() - kClimateSigma;
                const auto target = std::clamp(0.5f + wander, 0.f, 1.f);
                m_climateSmoothed[i].newTransition(target, kClimateSmoothingSeconds, kClimateRateHz);
                m_climateBaseline[i] = m_climateSmoothed[i].getValue();
            }
            m_climateAccumSeconds -= interval;
        }
    }

    void advanceImpulses(const float dt) noexcept
    {
        for (auto& slot : m_activeImpulses)
        {
            if (!slot.has_value())
            {
                continue;
            }
            slot->advance(dt);
            if (slot->isFinished())
            {
                slot.reset();
            }
        }
    }

    [[nodiscard]] WishWeights effectiveWishWeights() const noexcept
    {
        auto weights = m_climateBaseline;
        for (const auto& slot : m_activeImpulses)
        {
            if (slot.has_value())
            {
                applyImpulse(*slot, weights);
            }
        }
        return weights;
    }

    [[nodiscard]] float scoreCandidate(const HarmonicState& current, const HarmonicState& candidate,
                                       const WishWeights& weights, bool& rejected) const noexcept
    {
        float score = 0.f;
        for (size_t k = 0; k < kNumWishKinds; ++k)
        {
            score += wishAffinity(current, candidate, static_cast<WishKind>(k), weights);
        }
        score /= static_cast<float>(kNumWishKinds);
        score += regionBonus(candidate, m_preferredRegion);

        for (const auto& vow : kDefaultVows)
        {
            const auto violated = vowViolated(vow.kind, m_homeOffsetSemitones, current, candidate);
            const auto judged = applyVow(vow, violated, score);
            if (!judged)
            {
                rejected = true;
                return 0.f;
            }
            score = *judged;
        }
        rejected = false;
        return score;
    }

    void considerTransition() noexcept
    {
        const auto weights = effectiveWishWeights();
        const auto current = m_palette[m_currentIndex];

        std::array<size_t, kDefaultPaletteSize> survivors{};
        std::array<float, kDefaultPaletteSize> survivorScores{};
        size_t survivorCount = 0;
        for (size_t i = 0; i < kDefaultPaletteSize; ++i)
        {
            bool rejected = false;
            const auto score = scoreCandidate(current, m_palette[i], weights, rejected);
            if (rejected)
            {
                continue;
            }
            survivors[survivorCount] = i;
            survivorScores[survivorCount] = score;
            ++survivorCount;
        }
        if (survivorCount == 0)
        {
            return;
        }

        const auto topCount = std::min(kTopCandidateCount, survivorCount);
        for (size_t a = 0; a < topCount; ++a)
        {
            auto best = a;
            for (size_t b = a + 1; b < survivorCount; ++b)
            {
                if (survivorScores[b] > survivorScores[best])
                {
                    best = b;
                }
            }
            std::swap(survivors[a], survivors[best]);
            std::swap(survivorScores[a], survivorScores[best]);
        }

        std::uniform_int_distribution<size_t> pick(0, topCount - 1);
        const auto chosenIndex = survivors[pick(m_rng)];
        if (chosenIndex != m_currentIndex)
        {
            m_currentIndex = chosenIndex;
            m_pendingIndex = chosenIndex;
            m_cooldownRemainingSeconds = kCooldownSeconds;
        }
    }

    float m_sampleRate;
    int m_homeOffsetSemitones{0};
    std::array<HarmonicState, kDefaultPaletteSize> m_palette;
    size_t m_currentIndex{0};
    std::optional<size_t> m_pendingIndex;
    std::optional<PaletteRegion> m_preferredRegion;

    std::array<OrnsteinUhlenbeckProcess, kNumWishKinds> m_climate{
        OrnsteinUhlenbeckProcess(kClimateRateHz), OrnsteinUhlenbeckProcess(kClimateRateHz),
        OrnsteinUhlenbeckProcess(kClimateRateHz), OrnsteinUhlenbeckProcess(kClimateRateHz),
        OrnsteinUhlenbeckProcess(kClimateRateHz), OrnsteinUhlenbeckProcess(kClimateRateHz),
        OrnsteinUhlenbeckProcess(kClimateRateHz)};
    std::array<LinearSmoothing, kNumWishKinds> m_climateSmoothed{
        LinearSmoothing(0.5f), LinearSmoothing(0.5f), LinearSmoothing(0.5f), LinearSmoothing(0.5f),
        LinearSmoothing(0.5f), LinearSmoothing(0.5f), LinearSmoothing(0.5f)};
    WishWeights m_climateBaseline{neutralWishWeights()};
    float m_climateAccumSeconds{0.f};

    std::array<std::optional<ActiveImpulse>, kMaxActiveImpulses> m_activeImpulses{};

    float m_dwellAccumSeconds{0.f};
    float m_cooldownRemainingSeconds{0.f};

    std::mt19937 m_rng;
};

}
