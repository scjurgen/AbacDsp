#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <functional>
#include <string>
#include <vector>

#include "Numbers/Interpolation.h"

namespace AbacDsp
{

using InterpolationFn = float (*)(const float* controlPoints, float fraction);

/**
 * @ingroup nonlinear
 * @brief One distortion transfer-function preset: a sparse control-point
 * curve plus the interpolation kernel that reconstructs it between points.
 *
 * midPoint offsets sample positions by half a step, so a zero-order-hold
 * preset (a stair-step transfer curve) centers its flats on the input value
 * instead of starting a new step exactly at it. latency is the interpolator's
 * support offset (zeroOrderHold: -1, *Pt2: 0, *43x: 1, *65x: 2).
 */
struct WaveTableControlPoints
{
    bool midPoint;
    InterpolationFn interpolation;
    int latency;
    std::string name;
    float delta;
    std::vector<float> controlPoints;
};

inline const std::vector<WaveTableControlPoints> kDistortionWaveTableSet = {
    {false, Interpolation::linearPt2, 0, "classic silicon", 2.0f, {-1.f, 1.f}},
    {false, Interpolation::bspline43x, 1, "classic germanium", 0.5f, {-1.f, -0.5f, 0.f, 0.5f, 1.f}},
    {false, Interpolation::bspline43x, 1, "classic soft", 0.5f, {-1.f, -0.7f, 0.f, 0.7f, 1.f}},
    {false,
     Interpolation::hermite43x,
     1,
     "classic soft tanh",
     0.125f,
     {-0.995055f, -0.99186f,  -0.986614f, -0.978026f, -0.964028f, -0.941376f, -0.905148f, -0.848284f, -0.761594f,
      -0.635149f, -0.462117f, -0.244919f, 0.f,        0.244919f,  0.462117f,  0.635149f,  0.761594f,  0.848284f,
      0.905148f,  0.941376f,  0.964028f,  0.978026f,  0.986614f,  0.99186f,   0.995055f}},
    {false, Interpolation::bspline43x, 1, "classic medium", 0.2f, {-1.f, -0.5f, -1.430511475e-06f, 0.5f, 1.f}},
    {false, Interpolation::bspline43x, 1, "classic hard", 0.1f, {-1.f, -0.5f, 0.f, 0.5f, 1.f}},
    {true, Interpolation::zeroOrderHold, 0, "classic fuzz", 0.01f, {-1.f, -1.f, 0.f, 1.f, 1.f}},
    {false, Interpolation::linearPt2, 0, "classic rectify", 0.5f, {1.f, 0.5f, 0.f, 0.5f, 1.f}},
    {false, Interpolation::linearPt2, 0, "classic half wave", 0.5f, {0.0f, 0.0f, 0.f, 0.5f, 1.f}},

    {true, Interpolation::zeroOrderHold, 0, "bit crunch noisy", 0.1f, {-1.f, 1.f,  -1.f, 1.f,  -1.f, 1.f,  -1.f, 1.f,
                                                                       -1.f, 1.f,  -1.f, 1.f,  -1.f, 1.f,  -1.f, 0.f,
                                                                       1.f,  -1.f, 1.f,  -1.f, 1.f,  -1.f, 1.f,  -1.f,
                                                                       1.f,  -1.f, 1.f,  -1.f, 1.f,  -1.f, 1.f}},

    {true, Interpolation::zeroOrderHold, 0, "bit crunch 0 narrow", 0.1f, {0.f, -1.f, 0.f, 1.f, 0.f}},
    {true,
     Interpolation::zeroOrderHold,
     0,
     "bit crunch 0 wide",
     0.125f,
     {0.f, -1.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 1.f, 0.f}},
    {true, Interpolation::zeroOrderHold, 0, "bit crunch 1", 0.5f, {-1.f, -0.5f, 0.f, 0.5f, 1.f}},
    {true,
     Interpolation::zeroOrderHold,
     0,
     "bit crunch 2",
     0.25f,
     {-1.f, -0.75f, -0.5f, -0.25f, 0.f, 0.25f, 0.5f, 0.75f, 1.f}},
    {true, Interpolation::zeroOrderHold, 0, "bit crunch 3", 0.1f, {-1.f,  -0.9f, -0.8f, -0.7f, -0.6f, -0.5f, -0.4f,
                                                                   -0.3f, -0.2f, -0.1f, 0.f,   0.1f,  0.2f,  0.3f,
                                                                   0.4f,  0.5f,  0.6f,  0.7f,  0.8f,  0.9f,  1.f}},

    {false, Interpolation::linearPt2, 0, "octave noise", 0.1f, {-1.f, 1.f, -1.f, 0.f, 1.f, -1.f, 1.f}},
    {false,
     Interpolation::linearPt2,
     0,
     "octave-noise-more",
     0.1f,
     {-1.f, 1.f, -1.f, 1.f, -1.f, 1.f, -1.f, 1.f, -1.f, 0.f, 1.f, -1.f, 1.f, -1.f, 1.f, -1.f, 1.f, -1.f, 1.f}},
    {false,
     Interpolation::bspline43x,
     1,
     "octave-noisish",
     0.1f,
     {-1.f, 1.f, -1.f, 2.f, -1.f, 1.f, -1.f, 1.f, -1.f, 0.f, 1.f, -1.f, 1.f, -2.f, 1.f, -1.f, 1.f, -1.f, 1.f}},
    {false, Interpolation::hermite43x, 1, "octave a bit", 0.3f, {-1.f, -0.9f, 0.3f, -1.f, 0.f, 0.3f, 0.6f, 0.9f, 1.f}},
    {false,
     Interpolation::hermite43x,
     1,
     "octave soft",
     0.3f,
     {-1.f, -0.85f, 0.1f, -0.4f, 0.f, 0.6f, -0.1f, 0.85f, 1.f}},
    {false,
     Interpolation::hermite43x,
     1,
     "octave classic",
     0.3f,
     {-1.f, -0.96f, -0.8f, 0.9f, -0.95f, 0.f, 0.95f, -0.9f, 0.8f, 0.96f, 1.f}},
    {false,
     Interpolation::hermite43x,
     1,
     "octave x2",
     0.2f,
     {-1.f, -0.9f, 1.f, -1.f, 1.f, -1.f, -1.90735409e-06f, 1.f, -1.f, 1.f, -1.f, 0.9f, 1.f}},
    {false,
     Interpolation::hermite43x,
     1,
     "octave x2 half wave",
     0.2f,
     {-1.f, -0.9f, 1.f, -1.f, 1.f, -1.f, -1.621246383e-06f, 0.7f, 0.8f, 0.9f, -1.f, 0.9f, 1.f}},

    {false,
     Interpolation::bspline43x,
     1,
     "asymmetric medium",
     0.3f,
     {-0.9999f, -1.f, -0.86f, -0.55f, -0.075f, 0.85f, 1.f, 1.f, 0.9999f}},
    {false,
     Interpolation::hermite43x,
     1,
     "asymmetric harder",
     0.25f,
     {-1.f, -0.75f, -0.5f, -0.25f, 0.f, 0.9f, 1.f, 1.f, 1.f, 1.f}},
    {false, Interpolation::linearPt2, 0, "asymmetric linear and fuzz", 0.1f, {-1.f,  -0.9f, -0.8f, -0.7f, -0.6f, -0.5f,
                                                                              -0.4f, -0.3f, -0.2f, -0.1f, 0.f,   1.f,
                                                                              1.f,   1.f,   1.f,   1.f,   1.f,   1.f,
                                                                              1.f,   1.f,   1.f}},

    {false,
     Interpolation::bspline43x,
     1,
     "broken amp 1",
     0.25f,
     {-0.25f, -0.5f, -1.4f, -0.5f, 0.f, 0.5f, 1.4f, 0.5f, 0.25f}},
    {false, Interpolation::bspline43x, 1, "broken amp 2", 0.25f, {0.f, 0.f, 0.f, -0.5f, 0.f, 0.5f, 1.4f, 0.5f, 0.15f}},
    {false,
     Interpolation::bspline43x,
     1,
     "broken amp 3",
     0.1f,
     {0.f, 0.f, 0.f, 0.f, -1.5f, 0.f, 1.5f, 0.f, 0.f, 0.f, 0.f}},
    {false,
     Interpolation::bspline43x,
     1,
     "broken amp 4",
     0.2f,
     {-1.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 1.f}},
    {false,
     Interpolation::hermite43x,
     1,
     "broken amp 5",
     0.2f,
     {0.f, 0.f, -1.f, -1.f, -1.f, -1.907350452e-06f, 1.f, 1.f, 1.f, 0.f, 0.f}},
};

/**
 * @ingroup nonlinear
 * @brief Resamples a sparse, equidistant control-point curve at arbitrary x
 * through a chosen interpolation kernel, padding both ends by edge repetition.
 *
 * The control points are recentered into a dense, fixed-step buffer sized
 * from delta so getValue() never needs a bounds check; only the one-time
 * setup in setControlVector() is proportional to the (small) number of
 * points.
 */
class InterpolateWithEquidistantControlPoints
{
  public:
    void setInterpolationFunction(const InterpolationFn f, const int offset) noexcept
    {
        m_interpolationFunction = f;
        m_interpolationOffset = offset;
    }

    void setControlVector(const bool midPoint, const std::vector<float>& control, const float dx)
    {
        const auto maxControlPoints = static_cast<size_t>(std::ceil(10.0f / dx));
        m_control.assign(maxControlPoints + 4, 0.0f);
        for (size_t i = 0; i <= m_control.size() / 2; ++i)
        {
            m_control[i] = control.front();
            m_control[m_control.size() - i - 1] = control.back();
        }
        const auto offset = (m_control.size() - control.size()) / 2;
        for (size_t i = 0; i < control.size(); ++i)
        {
            m_control[i + offset] = control[i];
        }
        // Zero-order-hold presets stay in the middle of an interval (0..1 -> -0.5..0.5)
        // rather than starting a new step exactly at the input value.
        const auto edgeMargin = midPoint ? maxControlPoints + 3 : maxControlPoints + 2;
        m_minValue = -dx * static_cast<float>(edgeMargin) / 2.0f;
        m_dx = 1.0f / dx;
    }

    [[nodiscard]] float getValue(const float x) const noexcept
    {
        const auto index = (x - m_minValue) * m_dx;
        const auto intIndexPosition = std::floor(index);
        const auto fraction = index - intIndexPosition;
        const auto* cp = &m_control[static_cast<size_t>(intIndexPosition) - static_cast<size_t>(m_interpolationOffset)];
        return m_interpolationFunction(cp, fraction);
    }

  private:
    std::vector<float> m_control{};
    InterpolationFn m_interpolationFunction{Interpolation::linearPt2};
    int m_interpolationOffset{0};
    float m_minValue{-1.0f};
    float m_dx{1.0f};
};

/**
 * @ingroup nonlinear
 * @brief Fixed-size lookup table built once from an arbitrary transfer
 * function, read back through bspline interpolation for a smooth curve.
 */
template <size_t BufferSize, bool ClampValues>
class WaveShaperInterpolated
{
  public:
    static constexpr int kPadForInterpolation{12};
    static constexpr int kHalfPad{kPadForInterpolation / 2};
    static_assert(kPadForInterpolation % 2 == 0);

    using ValueFunction = std::function<float(float x)>;

    void assignTable(const float minValue, const float maxValue, const ValueFunction& f)
    {
        m_minValue = minValue;
        m_maxValue = maxValue;
        const auto bufferSpan = static_cast<float>(BufferSize - 1);
        m_rangeDelta = bufferSpan / (maxValue - minValue);
        for (int i = 0; i < static_cast<int>(m_curve.size()); ++i)
        {
            const auto x = m_minValue + (m_maxValue - m_minValue) * static_cast<float>(i - kHalfPad) / bufferSpan;
            m_curve[static_cast<size_t>(i)] = f(x);
        }
    }

    [[nodiscard]] float step(const float in) const noexcept
    {
        auto value = in;
        if constexpr (ClampValues)
        {
            value = std::clamp(value, m_minValue, m_maxValue);
        }
        const auto index = (value - m_minValue) * m_rangeDelta + static_cast<float>(kHalfPad);
        const auto intIndexPosition = std::floor(index);
        const auto fraction = index - intIndexPosition;
        return Interpolation::bspline43x(&m_curve[static_cast<size_t>(intIndexPosition) - 1], fraction);
    }

    void processBlock(const float* source, float* target, const size_t numSamples) const noexcept
    {
        std::transform(source, source + numSamples, target, [this](const float x) { return step(x); });
    }

    void processBlock(float* inPlace, const size_t numSamples) const noexcept
    {
        std::transform(inPlace, inPlace + numSamples, inPlace, [this](const float x) { return step(x); });
    }

  private:
    std::array<float, BufferSize + kPadForInterpolation> m_curve{};
    float m_minValue{-1.0f};
    float m_maxValue{1.0f};
    float m_rangeDelta{0.1f};
};

/**
 * @ingroup nonlinear
 * @brief The full set of distortion presets from kDistortionWaveTableSet,
 * each pre-rendered into its own WaveShaperInterpolated table at construction.
 */
class WaveShaperTableStore
{
  public:
    static constexpr size_t kTableSize{1025};

    WaveShaperTableStore()
    {
        m_waveShapes.reserve(kDistortionWaveTableSet.size());
        for (const auto& setting : kDistortionWaveTableSet)
        {
            InterpolateWithEquidistantControlPoints control;
            control.setInterpolationFunction(setting.interpolation, setting.latency);
            control.setControlVector(setting.midPoint, setting.controlPoints, setting.delta);

            WaveShaperInterpolated<kTableSize, true> shaper;
            shaper.assignTable(-4.0f, 4.0f, [&control](const float x) { return control.getValue(x); });
            m_waveShapes.push_back(std::move(shaper));
        }
    }

    [[nodiscard]] float getValue(const size_t index, const float x) const noexcept
    {
        return m_waveShapes[index].step(x);
    }

    void processBlock(const size_t index, float* inPlace, const size_t numSamples) const noexcept
    {
        m_waveShapes[index].processBlock(inPlace, numSamples);
    }

  private:
    std::vector<WaveShaperInterpolated<kTableSize, true>> m_waveShapes;
};

}
