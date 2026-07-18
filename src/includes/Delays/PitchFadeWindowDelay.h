#pragma once

#include <algorithm>
#include <cmath>
#include <optional>
#include <random>
#include <span>
#include <vector>

#include "Analysis/YinPitchDetector.h"
#include "Numbers/Interpolation.h"

namespace AbacDsp
{

/*
 * Granular time domain pitch shifter.
 *
 * One ring buffer. The write head advances one sample per input sample; a read head
 * advances at `advance` (the pitch ratio v), so it drifts against the write head and has
 * to be picked up and put back periodically. Two heads overlap and crossfade so that the
 * reset is not heard.
 *
 *
 * GEOMETRY
 *
 * drift is how fast a head moves against the write head:
 *
 *   forward, v > 1    drift = v - 1    the head catches up
 *   forward, v < 1    drift = 1 - v    the head falls back
 *   reverse           drift = 1 + v    the head runs away
 *
 * A head may drift `window` before it has to be reset, so its life is window/drift: the
 * smaller the shift, the longer a grain stays valid, and at v == 1 it never expires at
 * all. MaxGrainLife caps that; see the delay note further down.
 *
 * span = drift * life is how far a head actually travels, and is what the placement uses,
 * NOT window. That distinction matters. Placing a head `v * window` back, as this once
 * did, costs a 53 ms delay for a half semitone detune when 3 ms of runway is all it
 * needs. window is the ceiling on the excursion, not the excursion itself.
 *
 * A head is placed at age MinAge + jitter + (catching up ? span : 0): a catching up head
 * starts a span back and closes on the write head, every other head starts close and
 * falls back by a span. The jitter always pushes away from the write head, never toward
 * it, since a catching up head that started jitter closer would end up jitter past it,
 * interpolating across samples that have not been written yet.
 *
 * A grain's life is fadeIn + at least one plain sample + fadeOut, so fadeTime is clamped
 * to (life - 1) / 2. Do not instead absorb an over long fade by shortening the plain part
 * to a token value: a grain is placed with runway for exactly one life, and stretching
 * its life beyond that walks the head into material it was never given room for. That was
 * a real defect here, and a loud one: the head lapped the write head mid grain, at full
 * gain, and the material jumped a whole buffer length.
 *
 *
 * THE HANDOFF
 *
 * triggerFade takes over the fade in head's position, so that position has to advance
 * BEFORE the handoff. Otherwise the outgoing head re-reads its own last sample and the
 * output holds for two samples at every grain boundary. One held sample is nothing on its
 * own, but it repeats at the grain rate, and it was the source of a long standing crackle
 * that was audible at every pitch setting including unity.
 *
 *
 * MEASURING THIS THING
 *
 * All of these were learned the hard way while chasing that crackle:
 *
 * - Never probe at a frequency commensurate with MAXSIZE. 220 Hz fits exactly 22 times
 *   into 4800 samples, so a buffer length jump lands back on the same phase and cancels,
 *   hiding precisely the defects worth finding. 237 Hz or similar.
 *
 * - Yin needs a lot of settled audio. A one second median wanders +-3 Hz on grain
 *   modulated output and will happily read 446 Hz for a 440 Hz tone; by 30 s it settles
 *   inside 1.2 Hz. Conclude nothing about pitch from a short run.
 *
 * - A click is concentrated in time, so an FFT smears it over thousands of bins and no
 *   single bin stands out. Sum the energy above a cutoff rather than taking the loudest
 *   bin: a stalled handoff measures about -60 dB above 5 kHz against about -89 dB for a
 *   clean one, while the per bin maximum does not separate them at all.
 *
 * - Separate clicks from warble by bandwidth. The jitter comb and the crossfade modulate
 *   near the carrier; a step discontinuity reaches Nyquist.
 *
 *
 * KNOWN AND ACCEPTED
 *
 * The jitter combs the crossfade. At 200 Hz the period is 240 samples while the jitter
 * spans 0..200, so grains land at near arbitrary phase and only about 10 dB of spurious
 * free range survives at unity. This is wanted for GrainMode::DriftJitter; the phase
 * alignment mentioned below as the fix is GrainMode::PitchSynchronous, see
 * phaseLockedPosition.
 *
 * MaxGrainLife bounds how far the delay wanders, but it also makes unity retrigger at
 * about 18 Hz where drift is zero and there is nothing to reset. Capping span instead of
 * life (life = span / drift, uncapped in time) would bound the delay just as well at a
 * lower artifact rate, and would make unity genuinely free. Deliberately not done, since
 * phase alignment would make most of it moot.
 *
 * Output pitch carries a residual bias of +0.3 to +1.2 Hz (2 to 4 cents) that depends on
 * the probe frequency and on the retrigger period R. Each reset jumps the material by R
 * samples, a phase step of 2*pi*f*R/sr, which accumulates as roughly
 * (phase step)/(2*pi) * sr/R. The bias moved when R changed from 1149 to 1201, which fits
 * that model. Not chased further; it is inaudible and phase alignment would change it.
 */
template <size_t MAXSIZE>
class PitchFadeWindowDelay
{
  public:
    static constexpr size_t MaxInterpolationWidth{4};

    enum class GrainMode
    {
        DriftJitter,
        PitchSynchronous
    };

    PitchFadeWindowDelay()
        : m_buffer(MAXSIZE + MaxInterpolationWidth, 0.f)
    {
        updateGeometry();
    }

    void setSize(const size_t newSize) noexcept
    {
        m_requestedWindow = newSize;
        updateGeometry();
    }

    void setFadeTime(const size_t t) noexcept
    {
        m_requestedFadeTime = t;
        updateGeometry();
    }

    // Constructs the pitch tracker and switches to GrainMode::PitchSynchronous. Call
    // setGrainMode(GrainMode::DriftJitter) to switch back without tearing it down.
    void enablePitchSynchronousMode(const float sampleRate, const float minFreq = 80.f, const float maxFreq = 1000.f)
    {
        m_pitchTracker.emplace(sampleRate, minFreq, maxFreq);
        m_sampleRate = sampleRate;
        m_minPeriodSamples = sampleRate / maxFreq;
        m_maxPeriodSamples = sampleRate / minFreq;
        m_grainMode = GrainMode::PitchSynchronous;
    }

    void setGrainMode(const GrainMode mode) noexcept
    {
        m_grainMode = mode;
    }

    [[nodiscard]] float step(const float in)
    {
        if (m_grainMode == GrainMode::PitchSynchronous)
        {
            updatePeriodEstimate(in);
        }

        float returnValue = 0.f;

        auto& rdHd = m_readHeads;
        if (!rdHd.fade)
        {
            returnValue = getFractional(rdHd.fadeInPos);
            // the handoff in triggerFade takes over this position, so it has to advance
            // first: otherwise the outgoing head repeats its last sample and clicks
            advanceFade(rdHd.fadeInPos);
            if (--rdHd.plainSteps == 0)
            {
                triggerFade();
            }
        }
        else
        {
            const auto fadeInGain = static_cast<float>(rdHd.fadeTime - rdHd.fadeCount) * rdHd.fadeStep;
            returnValue =
                getFractional(rdHd.fadeInPos) * fadeInGain + getFractional(rdHd.fadeOutPos) * (1.f - fadeInGain);
            advanceFade(rdHd.fadeInPos);
            advanceFade(rdHd.fadeOutPos);
            if (--rdHd.fadeCount == 0)
            {
                rdHd.fade = false;
                rdHd.plainSteps = rdHd.plainAfterFade;
            }
        }

        if (m_head < MaxInterpolationWidth)
        {
            m_buffer[m_head + m_maxSize] = in;
        }

        m_buffer[m_head++] = in;
        m_head %= m_maxSize;
        return returnValue;
    }

    void setReverse(const bool reverse) noexcept
    {
        m_reverse = reverse;
        updateGeometry();
    }

    void setPitch(const float semitones) noexcept
    {
        setPitchRatio(std::pow(2.f, semitones / 12.f));
    }

    void setPitchRatio(const float ratio) noexcept
    {
        if (std::fpclassify(ratio) == FP_ZERO)
        {
            return;
        }
        m_readHeads.advance = ratio;
        updateGeometry();
    }

    void processBlock(std::span<const float> source, std::span<float> target)
    {
        for (size_t i = 0; i < source.size(); ++i)
        {
            target[i] = step(source[i]);
        }
    }

  private:
    // Feeds the tracker and stashes the resulting period in samples; a pitch of 0 Hz
    // (silence/unvoiced/out of range) clears it, which is how planGrain and
    // calcMaterialInPosition fall back to the drift/jitter geometry.
    void updatePeriodEstimate(const float in) noexcept
    {
        const auto pitchHz = m_pitchTracker->step(in);
        if (pitchHz > 0.f)
        {
            const auto period = m_sampleRate / pitchHz;
            m_currentPeriod = std::clamp(period, m_minPeriodSamples, m_maxPeriodSamples);
        }
        else
        {
            m_currentPeriod.reset();
        }
    }

    void triggerFade()
    {
        const auto grain = planGrain();
        m_readHeads.fade = true;
        m_readHeads.fadeOutPos = m_readHeads.fadeInPos;
        m_readHeads.fadeTime = std::clamp(m_requestedFadeTime, size_t{2}, (grain.life - 1) / 2);
        m_readHeads.fadeCount = m_readHeads.fadeTime;
        m_readHeads.fadeStep = 1.f / static_cast<float>(m_readHeads.fadeTime - 1);
        m_readHeads.plainAfterFade = grain.life - m_readHeads.fadeTime * 2;
        m_readHeads.fadeInPos = calcMaterialInPosition(grain.span);
    }

    void advanceFade(float& fadePos) noexcept
    {
        fadePos += m_reverse ? -m_readHeads.advance : m_readHeads.advance;
        while (fadePos < 0.f)
        {
            fadePos += m_maxSize;
        }
        while (fadePos >= m_maxSize)
        {
            fadePos -= m_maxSize;
        }
    }

    [[nodiscard]] float getFractional(const float position) const
    {
        const auto idx = static_cast<size_t>(std::floor(position));
        const float fractional = position - static_cast<float>(idx);
        // hermite43x interpolates between y[1] and y[2], so the window starts one sample early
        const auto base = (idx + m_maxSize - 1) % m_maxSize;
        return Interpolation::hermite43x(&m_buffer[base], fractional);
    }

    // How far a read head moves against the write head per sample.
    [[nodiscard]] float driftFactor() const noexcept
    {
        const float advance = m_readHeads.advance;
        if (m_reverse)
        {
            return 1.f + advance;
        }
        return advance > 1.f ? advance - 1.f : 1.f - advance;
    }

    struct Grain
    {
        size_t life{MinLife};
        float span{0.f};
    };

    /*
     * A grain only has to be reset once its head has drifted a window away from where it
     * started, which is what takes window/drift samples: the smaller the pitch shift, the
     * longer a grain stays valid, and at unity it never expires at all. MaxGrainLife caps
     * that, both to keep refreshing the material and to bound the excursion, since span is
     * how far the delay wanders before the head is put back.
     */
    [[nodiscard]] Grain planDriftJitterGrain() const noexcept
    {
        constexpr float MinDrift{1.f / 1024.f};
        const float drift = driftFactor();
        const float lifeLimit = static_cast<float>(m_window) / std::max(drift, MinDrift);
        const auto life = std::max(static_cast<size_t>(std::min(lifeLimit, static_cast<float>(MaxGrainLife))), MinLife);
        return {life, drift * static_cast<float>(life)};
    }

    /*
     * Classic PSOLA analysis windows span two periods with 50% overlap, so life targets
     * 2*period; triggerFade then clamps the requested fade time to (life-1)/2, which lands
     * fadeTime on roughly one period on its own. life is still bounded by window/drift and
     * MaxGrainLife exactly as the drift/jitter path is, so a very low detected pitch can
     * never push a grain past the same safety ceiling the existing geometry already has.
     */
    [[nodiscard]] Grain planPitchSynchronousGrain(const float period) const noexcept
    {
        constexpr float MinDrift{1.f / 1024.f};
        const float drift = std::max(driftFactor(), MinDrift);
        const auto periodLife = static_cast<size_t>(std::round(period * 2.f));
        const auto spanLimitedLife = static_cast<size_t>(static_cast<float>(m_window) / drift);
        const auto life = std::clamp(std::min({periodLife, spanLimitedLife, MaxGrainLife}), MinLife, MaxGrainLife);
        return {life, drift * static_cast<float>(life)};
    }

    [[nodiscard]] Grain planGrain() const noexcept
    {
        if (m_grainMode == GrainMode::PitchSynchronous && m_currentPeriod)
        {
            return planPitchSynchronousGrain(*m_currentPeriod);
        }
        return planDriftJitterGrain();
    }

    void updateGeometry() noexcept
    {
        constexpr size_t HeadRoom{m_randomVariation + MinAge + MaxInterpolationWidth};
        m_window = std::clamp(m_requestedWindow, MinWindow, m_maxSize - HeadRoom);
    }

    [[nodiscard]] float wrapPosition(float pos) const noexcept
    {
        while (pos >= static_cast<float>(m_maxSize))
        {
            pos -= static_cast<float>(m_maxSize);
        }
        while (pos < 0.f)
        {
            pos += static_cast<float>(m_maxSize);
        }
        return pos;
    }

    /*
     * Where to drop a new head, as an age: how far behind the write head it reads. A
     * catching-up head starts a span back and closes on the write head, arriving at
     * MinAge as the grain ends; every other head starts at MinAge and falls back by a
     * span over its life. Jitter always pushes away from the write head, never toward it,
     * or a catching-up head would end up reading samples not written yet.
     */
    [[nodiscard]] float jitterPosition(const bool catchingUp, const float span)
    {
        const auto jitter = static_cast<float>(m_randDistribution(m_randomGenerator));
        const float age = static_cast<float>(MinAge) + jitter + (catchingUp ? span : 0.f);
        return std::round(wrapPosition(static_cast<float>(m_head) - age));
    }

    /*
     * Snaps to the nearest period-multiple of the OUTGOING head's own position
     * (fadeOutPos) that still has at least idealAge of margin from the write head, rather
     * than quantizing against the write head itself. Material is only locally periodic,
     * so phase only matches the head being crossfaded against if anchored to where THAT
     * head actually is; anchoring to the write head instead only coincides at unity pitch
     * and otherwise drifts, which measured as wrong output pitch, not just noise. Falling
     * short of idealAge for a catching-up head runs it into the write head before its
     * planned life is up (a real, measured mid-grain click), so idealAge is a hard floor
     * here, not just a preference.
     */
    [[nodiscard]] float phaseLockedPosition(const bool catchingUp, const float span, const float period)
    {
        const float idealAge = static_cast<float>(MinAge) + (catchingUp ? span : 0.f);
        const float idealPos = wrapPosition(static_cast<float>(m_head) - idealAge);

        const float delta = idealPos - m_readHeads.fadeOutPos;
        // shortest signed distance on the ring, so the period count is correct across a
        // buffer wraparound too
        const float wrappedDelta =
            delta - static_cast<float>(m_maxSize) * std::round(delta / static_cast<float>(m_maxSize));
        const float baseK = std::floor(wrappedDelta / period);

        auto candidate = [&](const float count) { return wrapPosition(m_readHeads.fadeOutPos + count * period); };
        auto ageOf = [&](const float pos) { return wrapPosition(static_cast<float>(m_head) - pos); };

        // try a small neighborhood of period counts and keep the one closest to idealAge
        // from above; a plain nearest-k round can land short of idealAge or even on the
        // wrong side of the write head entirely
        float bestPos = candidate(baseK);
        float bestScore = -1.f;
        for (float k = baseK - 1.f; k <= baseK + 3.f; k += 1.f)
        {
            const float pos = candidate(k);
            const float age = ageOf(pos);
            if (age < idealAge)
            {
                continue;
            }
            const float score = age - idealAge;
            if (bestScore < 0.f || score < bestScore)
            {
                bestScore = score;
                bestPos = pos;
            }
        }
        if (bestScore < 0.f)
        {
            // no candidate in the neighborhood respected the safety margin; fall back to
            // the jittered placement rather than risk an unsafe position
            return jitterPosition(catchingUp, span);
        }
        return std::round(bestPos);
    }

    [[nodiscard]] float calcMaterialInPosition(const float span)
    {
        const bool catchingUp = !m_reverse && m_readHeads.advance > 1.f;
        if (m_grainMode == GrainMode::PitchSynchronous && m_currentPeriod)
        {
            return phaseLockedPosition(catchingUp, span, *m_currentPeriod);
        }
        return jitterPosition(catchingUp, span);
    }

    struct ReadHead
    {
        float fadeInPos{MAXSIZE * 0.25f};
        float fadeOutPos{MAXSIZE * 0.75f};
        float advance{0.001f};
        size_t fadeTime{2};
        size_t fadeCount{0};
        size_t plainSteps{1};
        size_t plainAfterFade{1};
        float fadeStep{1.f};
        bool fade{false};
    };

    // hermite43x reads one sample either side, so a head any closer would interpolate
    // across samples the write head has not reached yet
    static constexpr size_t MinAge{4};
    static constexpr size_t MinWindow{5};
    // a grain must be fadeIn + at least one plain sample + fadeOut
    static constexpr size_t MinLife{5};
    // bounds how far the delay wanders before a head is put back near the write head
    static constexpr size_t MaxGrainLife{5000};

    ReadHead m_readHeads{};

    std::vector<float> m_buffer;
    static constexpr size_t m_maxSize{MAXSIZE};
    size_t m_requestedWindow{MAXSIZE};
    size_t m_requestedFadeTime{MAXSIZE / 4};
    size_t m_window{MAXSIZE};
    size_t m_head{0};
    bool m_reverse{false};
    static constexpr size_t m_randomVariation{200};
    std::minstd_rand m_randomGenerator;
    std::uniform_int_distribution<size_t> m_randDistribution{0, m_randomVariation};

    GrainMode m_grainMode{GrainMode::DriftJitter};
    std::optional<YinPitchDetector> m_pitchTracker;
    std::optional<float> m_currentPeriod;
    float m_sampleRate{0.f};
    float m_minPeriodSamples{0.f};
    float m_maxPeriodSamples{0.f};
};

}
