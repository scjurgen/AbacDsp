#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <numbers>
#include <random>
#include <span>
#include <vector>

#include "Modulation/Flutter.h"
#include "Modulation/Wow.h"
#include "Numbers/Interpolation.h"
#include "Parameters/SmoothingParameter.h"

namespace AbacDsp
{

/**
 * @ingroup delays
 * @brief Mono single-rate delay whose read head wobbles like a tape transport.
 *
 * The read position is the write head minus a delay, so it can never drift from it. That
 * delay is a smoothed base distance plus a Wow delay and a Flutter offset, updated every
 * TileSize samples and interpolated linearly in between. The result is clamped to the safety
 * margin, so the read head can not reach the write head however deep the modulation is.
 * setDelay() glides at a bounded read speed instead of jumping. The read is Catmull-Rom.
 *
 * There is no resampling here: run it at another rate by wrapping it in UpDownSampler.
 * Flutter is a speed error, used here directly as a position offset scaled to the physical
 * excursion at kFlutterReferenceHz.
 * @see https://en.wikipedia.org/wiki/Wow_and_flutter
 */
template <size_t BufferSize, size_t TileSize>
class WobbleDelay
{
    static_assert(BufferSize > 16);
    static_assert(TileSize > 0);

  public:
    /// Fastest retune in samples of delay change per sample, i.e. reads at 0.5x or 1.5x speed.
    static constexpr float kRetuneSlope{0.5f};
    /// Flutter frequency at which its position offset equals the physical delay excursion.
    static constexpr float kFlutterReferenceHz{10.f};
    /// Fewest samples between write and read that the four-point read can support.
    static constexpr float kStructuralMinDelay{3.f};
    static constexpr float kMaxDelay{static_cast<float>(BufferSize) - 4.f};
    static constexpr float kDefaultDelay{static_cast<float>(BufferSize) / 8.f};

    explicit WobbleDelay(const float sampleRate)
        : m_sampleRate(sampleRate)
        , m_controlRate(sampleRate / static_cast<float>(TileSize))
        , m_msToSamples(sampleRate / 1000.f)
        , m_flutterScale(sampleRate / (2.f * std::numbers::pi_v<float> * kFlutterReferenceHz))
        , m_buffer(BufferSize, 0.f)
        , m_flutter(m_controlRate)
        , m_wow(m_controlRate)
    {
        m_wow.setDepth(0.f);
    }

    /// @brief Moves the base delay to the given number of samples, gliding unless forced.
    void setDelay(const float samples, const bool force = false) noexcept
    {
        const auto target = std::clamp(samples, m_safetyMargin, kMaxDelay);
        const auto seconds = std::abs(target - m_distance.getLastValue()) / (kRetuneSlope * m_sampleRate);
        m_distance.newTransition(target, seconds, m_controlRate, force);
        if (force)
        {
            m_delay = target;
            m_delaySlope = 0.f;
        }
    }

    /// @brief Smallest allowed read distance, at least kStructuralMinDelay.
    void setSafetyMargin(const float samples) noexcept
    {
        m_safetyMargin = std::clamp(samples, kStructuralMinDelay, kMaxDelay / 2.f);
    }

    void setFlutterDepth(const float value) noexcept
    {
        m_flutter.setDepth(value);
    }

    void setFlutterRate(const float value) noexcept
    {
        m_flutter.setRate(value);
    }

    void setWowDepth(const float value) noexcept
    {
        m_wow.setPerceptualDepth(value);
    }

    void setWowRate(const float value) noexcept
    {
        m_wow.setRate(value);
    }

    void setWowVariance(const float value) noexcept
    {
        m_wow.setVariance(value);
    }

    void setWowDrift(const float value) noexcept
    {
        m_wow.setDrift(value);
    }

    void seed(const std::mt19937::result_type value) noexcept
    {
        m_wow.seed(value);
    }

    /// @brief Current distance between write and read head in samples, modulation included.
    [[nodiscard]] float currentDelay() const noexcept
    {
        return m_delay;
    }

    void reset() noexcept
    {
        std::ranges::fill(m_buffer, 0.f);
        m_flutter.reset();
    }

    [[nodiscard]] float step(const float in) noexcept
    {
        if (m_controlCountdown == 0)
        {
            updateControl();
            m_controlCountdown = TileSize;
        }
        --m_controlCountdown;
        m_delay += m_delaySlope;

        m_buffer[m_head] = in;
        m_head = m_head + 1 == BufferSize ? 0 : m_head + 1;
        return readDelayed();
    }

    void processBlock(std::span<const float> source, std::span<float> target) noexcept
    {
        std::ranges::transform(source, target.begin(), [this](const float in) { return step(in); });
    }

  private:
    void updateControl() noexcept
    {
        const auto base = m_distance.getValue();
        static_cast<void>(m_wow.step());
        const auto wow = m_wow.lastDelay() * m_msToSamples;
        const auto flutter = (m_flutter.step() - 1.f) * m_flutterScale;
        const auto target = std::clamp(base + wow + flutter, m_safetyMargin, kMaxDelay);
        m_delaySlope = (target - m_delay) / static_cast<float>(TileSize);
    }

    [[nodiscard]] static size_t wrap(const size_t index) noexcept
    {
        return index >= BufferSize ? index - BufferSize : index;
    }

    [[nodiscard]] float readDelayed() const noexcept
    {
        const auto whole = static_cast<size_t>(m_delay);
        const auto fraction = m_delay - static_cast<float>(whole);
        const auto first = m_head + BufferSize - 3 - whole;
        const std::array<float, 4> taps{m_buffer[wrap(first)], m_buffer[wrap(first + 1)], m_buffer[wrap(first + 2)],
                                        m_buffer[wrap(first + 3)]};
        return Interpolation::hermite43x(taps.data(), 1.f - fraction);
    }

    const float m_sampleRate;
    const float m_controlRate;
    const float m_msToSamples;
    const float m_flutterScale;

    std::vector<float> m_buffer;
    size_t m_head{0};

    LinearSmoothing m_distance{kDefaultDelay};
    Flutter m_flutter;
    Wow m_wow;
    float m_safetyMargin{kStructuralMinDelay};
    float m_delay{kDefaultDelay};
    float m_delaySlope{0.f};
    size_t m_controlCountdown{0};
};

}
