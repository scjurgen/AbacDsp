#pragma once

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <span>

#include "Filters/Chebyshev4Lowpass.h"
#include "Wavetables/MorphCarrierLevel.h"
#include "Wavetables/WaveTableOscillator.h"

namespace AbacDsp
{

/**
 * @ingroup modulation
 * @brief Ring modulator whose carrier is a morphing wavetable oscillator of constant RMS.
 *
 * The output is the input times the carrier. The tables of the store have no DC, so the input does not survive,
 * only sums and differences of its partials with the partials of the carrier. The carrier is WaveTableOscillator over
 * three slots, Sine, Square and SharkFin by default, morphed from 0 (slot 0) through 0.5 (slot 1) to 1 (slot 2), with
 * the polarity of the textbook waves (a sine rises first, a shark fin falls after its short rise). Neighbouring slots
 * should have their fundamentals in phase: a Saw next to a Square is in antiphase, the fundamental nulls near 0.75 and
 * the level compensation then lifts a carrier without its fundamental.
 *
 * The carrier level is held at carrierRms for every morph and pitch (MorphCarrierLevel): the tables lose harmonics
 * with pitch and a crossfade of correlated waves dips, which would make the product louder and quieter by up to a
 * dB while the controls move. A sine carrier of RMS 0.7071 makes this the multiplier of RingModulator.
 *
 * The carrier is band-limited by the mip levels and limited to a quarter of the sample rate; the product is not
 * band-limited, so a sideband that lands above Nyquist folds back. The setters are control-rate: call them between
 * blocks of the caller's control block; the phase is continuous across a change. Glide and mixing belong to the caller.
 * Real-time safe except the constructor and setWaveset(), which allocate and sweep the tables.
 * @see https://en.wikipedia.org/wiki/Ring_modulation
 */
class WaveTableRingModulator
{
  public:
    static constexpr float kDefaultCarrierRms{0.57f};
    static constexpr float kDefaultFrequency{440.f};
    static constexpr float kMinFrequency{0.1f};
    static constexpr float kMaxFrequencyRatio{0.25f};
    static constexpr size_t kControlBlock{16};

    explicit WaveTableRingModulator(const float sampleRate, const float carrierRms = kDefaultCarrierRms)
        : m_sampleRate(sampleRate)
        , m_carrierRms(carrierRms)
        , m_level({BasicWave::Sine, BasicWave::Square, BasicWave::SharkFin})
        , m_carrier(sampleRate)
        , m_prefilter(sampleRate)
    {
        m_carrier.setWaveset(2, BasicWave::SharkFin);
        m_prefilter.setRampSamples(kControlBlock);
        m_prefilter.set(Chebyshev4Lowpass::kOpenCutoff, kInputRippleDb, true);
        setFrequency(kDefaultFrequency);
        setMorph(0.f);
    }

    /// Puts a periodic wave in a slot (0 to 2); the noise waves have no cycle to morph through.
    void setWaveset(const size_t slot, const BasicWave wave)
    {
        assert(slot < MorphCarrierLevel::kSlots);
        assert(wave != BasicWave::White && wave != BasicWave::NoiseFloor);
        m_carrier.setWaveset(slot, wave);
        m_level.setWave(slot, wave);
        updateGain();
    }

    /// Sets the carrier frequency, limited to kMinFrequency up to kMaxFrequencyRatio of the sample rate; a value that
    /// is not finite is ignored.
    void setFrequency(const float hz) noexcept
    {
        if (!std::isfinite(hz))
        {
            return;
        }
        m_frequency = std::clamp(hz, kMinFrequency, kMaxFrequencyRatio * m_sampleRate);
        m_carrier.setFrequency(m_frequency);
        updateGain();
    }

    /// Sets the morph, 0 to 1 (clamped): slot 0 at 0, slot 1 at 0.5, slot 2 at 1.
    void setMorph(const float morph) noexcept
    {
        m_morph = std::clamp(morph, 0.f, 1.f);
        m_oscillatorMorph = 2.f * m_morph - 1.f;
        m_carrier.setMorph(m_oscillatorMorph);
        updateGain();
    }

    /// Sets the cutoff of the lowpass in front of the product in Hz (Chebyshev4Lowpass::kOpenCutoff: none); a value
    /// that is not finite is ignored. The coefficients move over kControlBlock samples, so call it once per control
    /// block.
    void setInputCutoff(const float hz) noexcept
    {
        if (std::isfinite(hz))
        {
            m_prefilter.set(hz, kInputRippleDb);
        }
    }

    /// Restarts the carrier at phase 0, so that the next sample of a sine carrier is 0.
    void resetPhase() noexcept
    {
        m_carrier.resetPhase();
    }

    /// Restarts the carrier and clears the input lowpass.
    void reset() noexcept
    {
        m_carrier.resetPhase();
        m_prefilter.reset();
    }

    [[nodiscard]] float frequency() const noexcept
    {
        return m_frequency;
    }

    [[nodiscard]] float morph() const noexcept
    {
        return m_morph;
    }

    [[nodiscard]] float step(const float in) noexcept
    {
        float carrier{0.f};
        m_carrier.processBlock(&carrier, 1);
        return m_prefilter.process(in) * carrier * m_gain;
    }

    /// Multiplies `in` by the carrier into `out` (the same size); `out` may be `in`.
    void processBlock(const std::span<const float> in, const std::span<float> out) noexcept
    {
        assert(in.size() == out.size());
        std::array<float, kChunk> carrier{};
        for (size_t offset = 0; offset < in.size(); offset += kChunk)
        {
            const size_t count = std::min(kChunk, in.size() - offset);
            m_carrier.processBlock(carrier.data(), count);
            for (size_t i = 0; i < count; ++i)
            {
                out[offset + i] = m_prefilter.process(in[offset + i]) * carrier[i] * m_gain;
            }
        }
    }

  private:
    static constexpr size_t kChunk{64};
    static constexpr float kInputRippleDb{1.f};

    void updateGain() noexcept
    {
        const float measured = m_level.rms(m_oscillatorMorph, m_frequency / m_sampleRate);
        // The store builds its tables inverted (a sine reads as -sin); the sign puts them the textbook way up.
        m_gain = measured > 0.f ? -m_carrierRms / measured : 0.f;
    }

    const float m_sampleRate;
    const float m_carrierRms;
    MorphCarrierLevel m_level;
    WaveTableOscillator m_carrier;
    Chebyshev4Lowpass m_prefilter;
    float m_frequency{kDefaultFrequency};
    float m_morph{0.f};
    float m_oscillatorMorph{-1.f};
    float m_gain{1.f};
};

} // namespace AbacDsp
