#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <span>
#include <tuple>

#include "Analysis/BeatTempoTracker.h"
#include "Analysis/LevelMeter.h"
#include "Analysis/OnsetDetector.h"
#include "Analysis/PitchReading.h"
#include "Analysis/SpectrumAnalyzer.h"
#include "Analysis/YinPitchDetector.h"
#include "SonalysisSnapshots.h"

namespace Sonalysis
{

enum class PitchSource
{
    Mix,
    Left,
    Right
};

struct Settings
{
    float peakFallDbPerSecond{11.8f};
    float rmsWindowSeconds{0.3f};
    float peakHoldSeconds{3.f};
    size_t fftSize{8192};
    float fftFallDbPerSecond{24.f};
    bool spectrumPeakHold{false};
    float referenceA4{440.f};
    PitchSource pitchSource{PitchSource::Mix};
    bool pitchTracking{false};
};

/**
 * All analysers of one stereo stream, single-threaded: whoever calls analyse() owns it.
 * Input is interleaved L/R. Levels and onsets run per channel / per frame; the spectrum
 * sees the L+R mix; pitch tracking runs only while enabled, as it is the expensive part.
 */
class AnalysisEngine
{
  public:
    static constexpr size_t kMaxChunkFrames{256};
    static constexpr float kMinPitchHz{38.f};
    static constexpr float kMaxPitchHz{2000.f};
    static constexpr float kPitchUpdatesPerSecond{25.f};
    static constexpr float kMaxRmsWindowSeconds{1.f};

    explicit AnalysisEngine(const float sampleRate)
        : m_sampleRate(sampleRate)
        , m_levels{AbacDsp::LevelMeter{sampleRate, kMaxRmsWindowSeconds},
                   AbacDsp::LevelMeter{sampleRate, kMaxRmsWindowSeconds}}
        , m_spectrum(sampleRate, kSpectrumBands)
        , m_yin(sampleRate, kMinPitchHz, kMaxPitchHz, kPitchUpdatesPerSecond)
        , m_onsets(sampleRate)
        , m_tempo(sampleRate)
    {
        apply(m_settings);
    }

    void resetHold() noexcept
    {
        for (auto& meter : m_levels)
        {
            meter.reset();
        }
        m_spectrum.resetPeakHold();
    }

    void resetBeat() noexcept
    {
        m_tempo.reset();
        m_onsets.reset();
    }

    void analyse(const std::span<const float> interleaved)
    {
        for (size_t offset = 0; offset + 1 < interleaved.size(); offset += 2 * kMaxChunkFrames)
        {
            const size_t frames = std::min(kMaxChunkFrames, (interleaved.size() - offset) / 2);
            analyseChunk(interleaved.subspan(offset, 2 * frames));
        }
    }

    [[nodiscard]] LevelSnapshot levels() const noexcept
    {
        LevelSnapshot snapshot{};
        for (size_t c = 0; c < m_levels.size(); ++c)
        {
            snapshot.channels[c] = {m_levels[c].peakDb(), m_levels[c].rmsDb(), m_levels[c].holdDb(),
                                    m_levels[c].isClipped()};
        }
        return snapshot;
    }

    [[nodiscard]] SpectrumSnapshot spectrum() const noexcept
    {
        SpectrumSnapshot snapshot{};
        std::ranges::copy(m_spectrum.bandCenters(), snapshot.bandCenters.begin());
        std::ranges::copy(m_spectrum.bandsDb(), snapshot.bandsDb.begin());
        std::ranges::copy(m_spectrum.peakHoldDb(), snapshot.peakHoldDb.begin());
        snapshot.fftSize = m_spectrum.fftSize();
        snapshot.resolutionHz = m_sampleRate / static_cast<float>(snapshot.fftSize);
        snapshot.showPeakHold = m_settings.spectrumPeakHold;
        return snapshot;
    }

    [[nodiscard]] PitchSnapshot pitch() const noexcept
    {
        return m_pitch;
    }

    [[nodiscard]] XySnapshot xy() const noexcept
    {
        XySnapshot snapshot{};
        const auto split = static_cast<std::ptrdiff_t>(m_xyWrite);
        std::rotate_copy(m_xyRing.begin(), m_xyRing.begin() + split, m_xyRing.end(), snapshot.points.begin());
        return snapshot;
    }

    [[nodiscard]] BeatSnapshot beat() const noexcept
    {
        const auto reading = m_tempo.reading();
        if (!reading)
        {
            return {false, 0.f, 0.f, 0, m_onsetCount};
        }
        return {true, reading->bpm, reading->jitterMs, reading->beats, m_onsetCount};
    }

    // Cheap enough to call on every worker pass; only an FFT size change does real work.
    void apply(const Settings& settings)
    {
        for (auto& meter : m_levels)
        {
            meter.setPeakFallDbPerSecond(settings.peakFallDbPerSecond);
            meter.setRmsWindowSeconds(settings.rmsWindowSeconds);
            meter.setPeakHoldSeconds(settings.peakHoldSeconds);
        }
        if (settings.fftSize != m_spectrum.fftSize())
        {
            m_spectrum.setFftSize(settings.fftSize);
        }
        m_spectrum.setFallDbPerSecond(settings.fftFallDbPerSecond);
        if (settings.spectrumPeakHold && !m_settings.spectrumPeakHold)
        {
            m_spectrum.resetPeakHold();
        }
        m_settings = settings;
        m_pitch.referenceA4 = settings.referenceA4;
        if (!settings.pitchTracking)
        {
            m_pitch.valid = false;
        }
    }

  private:
    void analyseChunk(const std::span<const float> interleaved)
    {
        const size_t frames = interleaved.size() / 2;
        for (size_t i = 0; i < frames; ++i)
        {
            m_left[i] = interleaved[2 * i];
            m_right[i] = interleaved[2 * i + 1];
            m_mix[i] = 0.5f * (m_left[i] + m_right[i]);
        }
        const std::span<const float> left(m_left.data(), frames);
        const std::span<const float> right(m_right.data(), frames);
        m_levels[0].process(left);
        m_levels[1].process(right);
        std::ignore = m_spectrum.process(std::span<const float>(m_mix.data(), frames));
        if (m_settings.pitchTracking)
        {
            trackPitch(frames);
        }
        trackBeat(left, right);
        captureXy(left, right);
        m_position += frames;
    }

    void trackPitch(const size_t frames)
    {
        const auto& source = m_settings.pitchSource == PitchSource::Left    ? m_left
                             : m_settings.pitchSource == PitchSource::Right ? m_right
                                                                            : m_mix;
        for (size_t i = 0; i < frames; ++i)
        {
            const float frequency = m_yin.step(source[i]);
            if (!m_yin.hasNewPitch())
            {
                continue;
            }
            m_pitch.confidence = m_yin.getLastConfidence();
            m_pitch.valid =
                frequency > 0.f && m_pitch.confidence >= AbacDsp::YinPitchDetector::kMinimumQualityThreshold;
            if (m_pitch.valid)
            {
                m_pitch.reading = AbacDsp::pitchReadingOf(frequency, m_settings.referenceA4);
            }
        }
    }

    // The louder channel drives the detector, so a hard-panned or out-of-phase pulse still counts.
    void trackBeat(const std::span<const float> left, const std::span<const float> right)
    {
        for (size_t i = 0; i < left.size(); ++i)
        {
            const float sample = std::abs(left[i]) >= std::abs(right[i]) ? left[i] : right[i];
            if (m_onsets.step(sample))
            {
                m_tempo.addOnset(m_position + i);
                ++m_onsetCount;
            }
        }
        m_tempo.advanceTo(m_position + left.size());
    }

    void captureXy(const std::span<const float> left, const std::span<const float> right) noexcept
    {
        for (size_t i = 0; i < left.size(); ++i)
        {
            m_xyRing[m_xyWrite] = {left[i], right[i]};
            m_xyWrite = (m_xyWrite + 1) % m_xyRing.size();
        }
    }

    float m_sampleRate;
    Settings m_settings{};
    std::array<AbacDsp::LevelMeter, 2> m_levels;
    AbacDsp::SpectrumAnalyzer m_spectrum;
    AbacDsp::YinPitchDetector m_yin;
    AbacDsp::OnsetDetector m_onsets;
    AbacDsp::BeatTempoTracker m_tempo;
    PitchSnapshot m_pitch{};
    std::array<XyPoint, kXyPoints> m_xyRing{};
    size_t m_xyWrite{0};
    std::array<float, kMaxChunkFrames> m_left{};
    std::array<float, kMaxChunkFrames> m_right{};
    std::array<float, kMaxChunkFrames> m_mix{};
    uint64_t m_position{0};
    uint64_t m_onsetCount{0};
};

}
