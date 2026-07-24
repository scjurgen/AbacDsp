#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <span>
#include <utility>
#include <vector>

#include "Analysis/FftMisc.h"

namespace AbacDsp
{

struct Slice
{
    size_t startFrame{0};
    size_t lengthFrames{0};
};

// Cuts a recorded loop into slices, either on a musical grid or at detected
// transients (optionally snapped to the grid and to zero crossings for
// click-free edges). Runs once when recording stops, not on the audio hot path,
// so it is allowed to allocate its result vectors.
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
        if (fftSize == 0 || n < fftSize)
        {
            return onsets;
        }

        HannWindowMagnitudesFft fft(fftSize);
        std::vector<float> frame(fftSize, 0.f);
        std::vector<float> mag(fftSize / 2, 0.f);
        std::vector<float> prevMag(fftSize / 2, 0.f);
        std::vector<float> flux;
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
