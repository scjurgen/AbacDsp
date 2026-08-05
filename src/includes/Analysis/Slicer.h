#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <span>
#include <utility>
#include <vector>

#include "Analysis/FftMisc.h"

namespace AbacDsp
{

/// @ingroup analysis
/// @brief Half-open slice range into a loop, in frames.
struct Slice
{
    size_t startFrame{0};
    size_t lengthFrames{0};
};

/**
 * @ingroup analysis
 * @brief Cuts a loop into slices, on a musical grid or at detected transients.
 *
 * Three detection strategies with different failure modes: a fixed grid ignores
 * the audio entirely, spectral flux against a global threshold misses quiet
 * notes next to loud ones, and the adaptive threshold tracks the local flux
 * level so a note is judged against its own neighbourhood.
 *
 * Detected onsets can be snapped to the grid and to zero crossings, since a cut
 * anywhere else starts on a step and clicks.
 *
 * Allocates its results, so it is not realtime-safe.
 * @see Bello et al., "A Tutorial on Onset Detection in Music Signals",
 *      IEEE Trans. Speech and Audio Processing 13(5), 2005.
 */
class Slicer
{
  public:
    struct TransientParams
    {
        float relativeThreshold{0.2f}; // onset level as a fraction of the loop's peak envelope
        size_t minGapFrames{1};        // reject onsets closer than this to the previous one
        size_t envelopeWindow{1};      // magnitude-envelope smoothing window in frames
        size_t snapMaxDistance{0};     // max distance to pull an onset onto the grid (0 = unlimited)
        size_t zeroCrossRadius{0};     // search radius for zero-crossing snap (0 = no snap)
    };

    struct SpectralParams
    {
        size_t fftSize{1024};          // STFT window length
        size_t hopSize{256};           // STFT hop in frames
        float relativeThreshold{0.3f}; // peak flux as a fraction of the max flux
        size_t minGapFrames{1};        // reject onsets closer than this to the previous one
    };

    // Spectral flux with a locally adaptive threshold instead of one global
    // fraction of the peak. The threshold tracks the local flux level, so a quiet
    // note that stands out from its own neighbourhood is detected even when it is
    // tiny next to the loud hits, while loud-passage double-triggers stay
    // suppressed. Recovers ghost notes that a single global threshold misses.
    struct AdaptiveParams
    {
        size_t fftSize{1024};   // STFT window length
        size_t hopSize{256};    // STFT hop in frames
        size_t localWindow{16}; // local-average half-window in STFT frames
        float lambda{2.5f};     // multiplier on the local average flux
        float delta{0.05f};     // absolute floor as a fraction of the peak flux
        size_t minGapFrames{1}; // reject onsets closer than this to the previous one
    };

    // Slice-start refinement: nudge each (coarse, often window-centred) onset to
    // the nearby sample that best balances staying close to the detected onset,
    // sitting near the local noise floor (a clean, quiet cut edge), and landing on
    // the local energy increase (the attack). Improves timing accuracy and yields
    // click-free edges. Sample counts are at the loop's own rate; the defaults were
    // tuned at 48 kHz. Cannot help legato transitions, which have no attack edge.
    struct RefineParams
    {
        size_t prerollSamples{4800};        // search this far before the onset
        size_t postrollSamples{480};        // and this far after
        float onsetDeviationSamples{960.f}; // Gaussian width of the proximity prior
        size_t stepWidthSamples{480};       // half-width of the energy-increase edge detector
        size_t attackHalfLifeSamples{5};    // envelope follower fast attack
        size_t decayHalfLifeSamples{240};   // envelope follower slower decay
        float wOnset{1.0f};                 // weight: proximity to the detected onset
        float wNoise{0.5f};                 // weight: closeness to the noise floor
        float wIncrease{0.5f};              // weight: local energy increase
    };

    // Split [0, loopLength) into sliceCount slices, distributing any remainder.
    [[nodiscard]] static std::vector<Slice> gridSlices(const size_t loopLength, const size_t sliceCount)
    {
        std::vector<Slice> slices;
        if (loopLength == 0 || sliceCount == 0)
        {
            return slices;
        }
        slices.reserve(sliceCount);
        for (size_t i = 0; i < sliceCount; ++i)
        {
            const size_t start = loopLength * i / sliceCount;
            const size_t end = loopLength * (i + 1) / sliceCount;
            slices.push_back({start, end - start});
        }
        return slices;
    }

    // Grid boundary positions at 0, step, 2*step, ... below loopLength.
    [[nodiscard]] static std::vector<size_t> gridBoundaries(const size_t loopLength, const size_t stepFrames)
    {
        std::vector<size_t> boundaries;
        if (loopLength == 0 || stepFrames == 0)
        {
            return boundaries;
        }
        for (size_t pos = 0; pos < loopLength; pos += stepFrames)
        {
            boundaries.push_back(pos);
        }
        return boundaries;
    }

    // Build slices from sorted boundary positions; a leading 0 is implied and the
    // final slice runs to loopLength. Positions at or past loopLength are ignored.
    [[nodiscard]] static std::vector<Slice> slicesFromBoundaries(std::span<const size_t> boundaries,
                                                                 const size_t loopLength)
    {
        std::vector<Slice> slices;
        if (loopLength == 0)
        {
            return slices;
        }
        std::vector<size_t> starts;
        starts.reserve(boundaries.size() + 1);
        starts.push_back(0);
        for (const size_t b : boundaries)
        {
            if (b > 0 && b < loopLength)
            {
                starts.push_back(b);
            }
        }
        std::sort(starts.begin(), starts.end());
        starts.erase(std::unique(starts.begin(), starts.end()), starts.end());

        slices.reserve(starts.size());
        for (size_t i = 0; i < starts.size(); ++i)
        {
            const size_t start = starts[i];
            const size_t end = (i + 1 < starts.size()) ? starts[i + 1] : loopLength;
            slices.push_back({start, end - start});
        }
        return slices;
    }

    [[nodiscard]] static std::vector<Slice> transientSlices(std::span<const float> mono, const size_t loopLength,
                                                            const TransientParams& params,
                                                            std::span<const size_t> snapGrid = {})
    {
        std::vector<size_t> onsets = detectOnsets(mono, loopLength, params);
        if (!snapGrid.empty())
        {
            for (size_t& onset : onsets)
            {
                onset = snapToNearest(onset, snapGrid, params.snapMaxDistance);
            }
        }
        if (params.zeroCrossRadius > 0)
        {
            for (size_t& onset : onsets)
            {
                onset = snapToZeroCrossing(mono, onset, params.zeroCrossRadius, loopLength);
            }
        }
        return slicesFromBoundaries(onsets, loopLength);
    }

    // Transient slices from spectral-flux onsets (STFT of the loop). Better on
    // tonal/percussive material than the time-domain envelope; snapping to the grid
    // and zero crossings is reused from TransientParams.
    [[nodiscard]] static std::vector<Slice> spectralTransientSlices(std::span<const float> mono,
                                                                    const size_t loopLength, const SpectralParams& sp,
                                                                    const TransientParams& snap,
                                                                    std::span<const size_t> snapGrid = {})
    {
        std::vector<size_t> onsets = spectralFluxOnsets(mono, loopLength, sp);
        if (!snapGrid.empty())
        {
            for (size_t& onset : onsets)
            {
                onset = snapToNearest(onset, snapGrid, snap.snapMaxDistance);
            }
        }
        if (snap.zeroCrossRadius > 0)
        {
            for (size_t& onset : onsets)
            {
                onset = snapToZeroCrossing(mono, onset, snap.zeroCrossRadius, loopLength);
            }
        }
        return slicesFromBoundaries(onsets, loopLength);
    }

    // Transient slices from adaptive spectral-flux onsets. Like
    // spectralTransientSlices but with a locally adaptive threshold (see
    // AdaptiveParams), which recovers quiet notes without over-triggering on the
    // loud passages. Grid and zero-crossing snapping are reused from TransientParams.
    [[nodiscard]] static std::vector<Slice> adaptiveTransientSlices(std::span<const float> mono,
                                                                    const size_t loopLength, const AdaptiveParams& ap,
                                                                    const TransientParams& snap,
                                                                    std::span<const size_t> snapGrid = {})
    {
        std::vector<size_t> onsets = adaptiveFluxOnsets(mono, loopLength, ap);
        if (!snapGrid.empty())
        {
            for (size_t& onset : onsets)
            {
                onset = snapToNearest(onset, snapGrid, snap.snapMaxDistance);
            }
        }
        if (snap.zeroCrossRadius > 0)
        {
            for (size_t& onset : onsets)
            {
                onset = snapToZeroCrossing(mono, onset, snap.zeroCrossRadius, loopLength);
            }
        }
        return slicesFromBoundaries(onsets, loopLength);
    }

    // Refine detected onsets to precise slice starts (see RefineParams). Runs once
    // off the audio thread, so it may allocate. Returns sorted, de-duplicated
    // positions; each onset stays within its own [previous, next] neighbours.
    [[nodiscard]] static std::vector<size_t> refineSliceStarts(std::span<const size_t> onsets,
                                                               std::span<const float> mono, const size_t loopLength,
                                                               const RefineParams& params)
    {
        std::vector<size_t> refined;
        const size_t n = std::min(loopLength, mono.size());
        if (onsets.empty() || n == 0)
        {
            return refined;
        }
        const std::vector<float> env =
            envelopeFollower(mono, n, params.attackHalfLifeSamples, params.decayHalfLifeSamples);
        refined.reserve(onsets.size());
        for (size_t i = 0; i < onsets.size(); ++i)
        {
            const size_t onset = onsets[i];
            const size_t prev = (i > 0) ? onsets[i - 1] : 0;
            const size_t next = (i + 1 < onsets.size()) ? onsets[i + 1] : n;
            const size_t lo = std::max(prev, (onset > params.prerollSamples) ? onset - params.prerollSamples : 0);
            const size_t hi = std::min({next, onset + params.postrollSamples, n});
            if (hi <= lo + 2 || onset >= n)
            {
                refined.push_back(onset);
                continue;
            }
            refined.push_back(lo + refineWindow(std::span<const float>(env).subspan(lo, hi - lo), onset - lo, params));
        }
        std::sort(refined.begin(), refined.end());
        refined.erase(std::unique(refined.begin(), refined.end()), refined.end());
        return refined;
    }

    // Onset positions (in frames) from half-wave-rectified spectral flux peaks. The
    // detected position is centred on the analysis window (+fftSize/2), since the
    // Hann window gives an entering transient its strongest weight at the centre.
    [[nodiscard]] static std::vector<size_t> spectralFluxOnsets(std::span<const float> mono, const size_t loopLength,
                                                                const SpectralParams& params)
    {
        std::vector<size_t> onsets;
        const size_t n = std::min(loopLength, mono.size());
        const size_t fftSize = params.fftSize;
        const size_t hop = std::max<size_t>(1, params.hopSize);
        const std::vector<float> flux = computeSpectralFlux(mono, n, fftSize, hop);
        if (flux.size() < 3)
        {
            return onsets;
        }
        const float maxFlux = *std::max_element(flux.begin(), flux.end());
        if (maxFlux <= 0.f)
        {
            return onsets;
        }
        const float threshold = maxFlux * std::clamp(params.relativeThreshold, 0.f, 1.f);
        const size_t minGap = std::max<size_t>(1, params.minGapFrames);
        const size_t centre = fftSize / 2;

        bool haveOnset = false;
        size_t lastOnset = 0;
        for (size_t k = 1; k + 1 < flux.size(); ++k)
        {
            const bool localPeak = flux[k] >= threshold && flux[k] >= flux[k - 1] && flux[k] > flux[k + 1];
            if (!localPeak)
            {
                continue;
            }
            const size_t pos = std::min(k * hop + centre, n - 1);
            if (!haveOnset || pos - lastOnset >= minGap)
            {
                onsets.push_back(pos);
                lastOnset = pos;
                haveOnset = true;
            }
        }
        return onsets;
    }

    // Onset positions (in frames) from spectral flux peaks that clear a locally
    // adaptive threshold: floor + lambda * (local average flux), floor a fraction
    // of the peak (see AdaptiveParams). Same window-centred positions as
    // spectralFluxOnsets.
    [[nodiscard]] static std::vector<size_t> adaptiveFluxOnsets(std::span<const float> mono, const size_t loopLength,
                                                                const AdaptiveParams& params)
    {
        std::vector<size_t> onsets;
        const size_t n = std::min(loopLength, mono.size());
        const size_t hop = std::max<size_t>(1, params.hopSize);
        const std::vector<float> flux = computeSpectralFlux(mono, n, params.fftSize, hop);
        if (flux.size() < 3)
        {
            return onsets;
        }
        const float peak = *std::max_element(flux.begin(), flux.end());
        if (peak <= 0.f)
        {
            return onsets;
        }
        const float floor = std::clamp(params.delta, 0.f, 1.f) * peak;
        const size_t half = std::max<size_t>(1, params.localWindow);
        const size_t minGap = std::max<size_t>(1, params.minGapFrames);
        const size_t centre = params.fftSize / 2;

        bool haveOnset = false;
        size_t lastOnset = 0;
        for (size_t k = 1; k + 1 < flux.size(); ++k)
        {
            const float threshold = floor + params.lambda * localFluxMean(flux, k, half);
            const bool localPeak = flux[k] >= threshold && flux[k] >= flux[k - 1] && flux[k] > flux[k + 1];
            if (!localPeak)
            {
                continue;
            }
            const size_t pos = std::min(k * hop + centre, n - 1);
            if (!haveOnset || pos - lastOnset >= minGap)
            {
                onsets.push_back(pos);
                lastOnset = pos;
                haveOnset = true;
            }
        }
        return onsets;
    }

    [[nodiscard]] static std::vector<size_t> detectOnsets(std::span<const float> mono, const size_t loopLength,
                                                          const TransientParams& params)
    {
        std::vector<size_t> onsets;
        const size_t n = std::min(loopLength, mono.size());
        if (n == 0)
        {
            return onsets;
        }
        const std::vector<float> envelope = magnitudeEnvelope(mono, n, std::max<size_t>(1, params.envelopeWindow));
        const float peak = *std::max_element(envelope.begin(), envelope.end());
        if (peak <= 0.f)
        {
            return onsets;
        }
        const float threshold = peak * std::clamp(params.relativeThreshold, 0.f, 1.f);
        const size_t minGap = std::max<size_t>(1, params.minGapFrames);

        bool below = true;
        size_t lastOnset = 0;
        bool haveOnset = false;
        for (size_t i = 0; i < n; ++i)
        {
            const bool above = envelope[i] >= threshold;
            if (above && below)
            {
                if (!haveOnset || i - lastOnset >= minGap)
                {
                    onsets.push_back(i);
                    lastOnset = i;
                    haveOnset = true;
                }
            }
            below = !above;
        }
        return onsets;
    }

    // Nearest grid boundary; leaves pos unchanged if the closest is farther than
    // maxDistance (maxDistance == 0 means always snap to the nearest).
    [[nodiscard]] static size_t snapToNearest(const size_t pos, std::span<const size_t> boundaries,
                                              const size_t maxDistance)
    {
        if (boundaries.empty())
        {
            return pos;
        }
        size_t best = boundaries[0];
        size_t bestDist = absDiff(pos, best);
        for (const size_t b : boundaries)
        {
            const size_t d = absDiff(pos, b);
            if (d < bestDist)
            {
                bestDist = d;
                best = b;
            }
        }
        if (maxDistance != 0 && bestDist > maxDistance)
        {
            return pos;
        }
        return best;
    }

    // Nearest sign change to pos within radius; falls back to the sample closest
    // to zero. Reduces clicks at slice edges.
    [[nodiscard]] static size_t snapToZeroCrossing(std::span<const float> mono, const size_t pos, const size_t radius,
                                                   const size_t loopLength)
    {
        const size_t n = std::min(loopLength, mono.size());
        if (n == 0 || pos >= n)
        {
            return pos;
        }
        const size_t lo = (pos > radius) ? pos - radius : 0;
        const size_t hi = std::min(n - 1, pos + radius);

        size_t bestCross = pos;
        size_t bestCrossDist = radius + 1;
        bool foundCross = false;
        size_t minAbsIdx = pos;
        float minAbs = std::abs(mono[pos]);
        for (size_t i = lo + 1; i <= hi; ++i)
        {
            if ((mono[i - 1] < 0.f && mono[i] >= 0.f) || (mono[i - 1] > 0.f && mono[i] <= 0.f))
            {
                const size_t d = absDiff(pos, i);
                if (d < bestCrossDist)
                {
                    bestCrossDist = d;
                    bestCross = i;
                    foundCross = true;
                }
            }
            const float a = std::abs(mono[i]);
            if (a < minAbs)
            {
                minAbs = a;
                minAbsIdx = i;
            }
        }
        return foundCross ? bestCross : minAbsIdx;
    }

    // Downmix an interleaved stereo loop to mono (L+R) for analysis.
    static void downmixToMono(std::span<const float> interleaved, const size_t loopLength, std::vector<float>& mono)
    {
        mono.assign(loopLength, 0.f);
        const size_t available = interleaved.size() / 2;
        const size_t n = std::min(loopLength, available);
        for (size_t i = 0; i < n; ++i)
        {
            mono[i] = interleaved[i * 2] + interleaved[i * 2 + 1];
        }
    }

  private:
    // Causal two-stage (fast attack / slow decay) envelope follower on |x|, with
    // half-life expressed in samples: alpha = 2^(-1/halfLife).
    [[nodiscard]] static std::vector<float> envelopeFollower(std::span<const float> mono, const size_t n,
                                                             const size_t attackHalfLife, const size_t decayHalfLife)
    {
        const auto alpha = [](const size_t halfLife)
        { return halfLife == 0 ? 0.f : std::exp(std::log(0.5f) / static_cast<float>(halfLife)); };
        const float aAttack = alpha(attackHalfLife);
        const float aDecay = alpha(decayHalfLife);

        std::vector<float> env(n, 0.f);
        float y = 0.f;
        for (size_t i = 0; i < n; ++i)
        {
            const float x = std::abs(mono[i]);
            const float a = (x > y) ? aAttack : aDecay;
            y = (1.f - a) * x + a * y;
            env[i] = y;
        }
        return env;
    }

    [[nodiscard]] static float lowQuantile(std::vector<float> values, const float q)
    {
        if (values.empty())
        {
            return 0.f;
        }
        const size_t idx = std::min(values.size() - 1, static_cast<size_t>(q * static_cast<float>(values.size())));
        std::nth_element(values.begin(), values.begin() + static_cast<std::ptrdiff_t>(idx), values.end());
        return values[idx];
    }

    // Score every sample in a linear-envelope window and return the argmax. The
    // window is peak-referenced to dB, then scored by proximity to the onset
    // (Gaussian), closeness to the noise floor, and local energy increase.
    [[nodiscard]] static size_t refineWindow(std::span<const float> envSeg, const size_t onsetLocal,
                                             const RefineParams& p)
    {
        const size_t len = envSeg.size();
        if (len < 3)
        {
            return onsetLocal;
        }
        float peakLin = 1e-9f;
        for (const float v : envSeg)
        {
            peakLin = std::max(peakLin, v);
        }
        std::vector<float> envDb(len, 0.f);
        for (size_t k = 0; k < len; ++k)
        {
            envDb[k] = std::max(-80.f, 20.f * std::log10(std::max(envSeg[k], 1e-9f) / peakLin));
        }

        const float noise = lowQuantile(envDb, 0.05f);
        const float denom = std::max(1e-3f, -noise); // peak is 0 dB after peak-referencing
        const float sigma = std::max(1.f, p.onsetDeviationSamples);
        const size_t w = std::max<size_t>(1, p.stepWidthSamples);

        float best = -std::numeric_limits<float>::infinity();
        size_t bestIdx = onsetLocal;
        for (size_t k = 0; k < len; ++k)
        {
            const float z = (static_cast<float>(k) - static_cast<float>(onsetLocal)) / sigma;
            const float distOnset = std::exp(-0.5f * z * z);
            const float distNoise = 1.f - std::clamp((envDb[k] - noise) / denom, 0.f, 1.f);

            const size_t aLo = (k > w) ? k - w : 0;
            const size_t aHi = std::min(len - 1, k + w);
            float before = 0.f;
            for (size_t i = aLo; i < k; ++i)
            {
                before += envDb[i];
            }
            float after = 0.f;
            for (size_t i = k; i <= aHi; ++i)
            {
                after += envDb[i];
            }
            const float meanBefore = (k > aLo) ? before / static_cast<float>(k - aLo) : envDb[k];
            const float meanAfter = after / static_cast<float>(aHi - k + 1);
            const float increase = std::clamp((meanAfter - meanBefore) / denom, 0.f, 1.f);

            const float score = p.wOnset * distOnset + p.wNoise * distNoise + p.wIncrease * increase;
            if (score > best)
            {
                best = score;
                bestIdx = k;
            }
        }
        return bestIdx;
    }

    // Half-wave-rectified spectral flux per STFT frame: sum of positive
    // magnitude increases between consecutive Hann-windowed frames.
    [[nodiscard]] static std::vector<float> computeSpectralFlux(std::span<const float> mono, const size_t n,
                                                                const size_t fftSize, const size_t hop)
    {
        std::vector<float> flux;
        if (fftSize == 0 || n < fftSize)
        {
            return flux;
        }
        HannWindowMagnitudesFft fft(fftSize);
        std::vector<float> frame(fftSize, 0.f);
        std::vector<float> mag(fftSize / 2, 0.f);
        std::vector<float> prevMag(fftSize / 2, 0.f);
        flux.reserve((n - fftSize) / hop + 1);

        bool havePrev = false;
        for (size_t start = 0; start + fftSize <= n; start += hop)
        {
            std::copy_n(mono.data() + start, fftSize, frame.data());
            fft.compute(frame, mag);
            float sf = 0.f;
            if (havePrev)
            {
                for (size_t b = 0; b < mag.size(); ++b)
                {
                    const float d = mag[b] - prevMag[b];
                    if (d > 0.f)
                    {
                        sf += d;
                    }
                }
            }
            flux.push_back(sf);
            std::swap(prevMag, mag);
            havePrev = true;
        }
        return flux;
    }

    [[nodiscard]] static float localFluxMean(const std::vector<float>& flux, const size_t k, const size_t halfWindow)
    {
        const size_t lo = (k > halfWindow) ? k - halfWindow : 0;
        const size_t hi = std::min(flux.size() - 1, k + halfWindow);
        float sum = 0.f;
        for (size_t i = lo; i <= hi; ++i)
        {
            sum += flux[i];
        }
        return sum / static_cast<float>(hi - lo + 1);
    }

    [[nodiscard]] static std::vector<float> magnitudeEnvelope(std::span<const float> mono, const size_t n,
                                                              const size_t window)
    {
        std::vector<float> envelope(n, 0.f);
        float runningSum = 0.f;
        for (size_t i = 0; i < n; ++i)
        {
            runningSum += std::abs(mono[i]);
            if (i >= window)
            {
                runningSum -= std::abs(mono[i - window]);
            }
            const size_t count = std::min(i + 1, window);
            envelope[i] = runningSum / static_cast<float>(count);
        }
        return envelope;
    }

    [[nodiscard]] static size_t absDiff(const size_t a, const size_t b) noexcept
    {
        return (a > b) ? a - b : b - a;
    }
};

}
