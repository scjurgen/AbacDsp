#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <functional>
#include <utility>

namespace AbacDsp
{

/**
 * @ingroup generators
 * @brief Linear ramp from the current value to a target over a fixed step count.
 *
 * forceTarget() jumps immediately (used on note trigger, where an audible
 * ramp from the previous voice's leftover value would be a glitch);
 * setTarget() ramps (used for anything that should move smoothly while a
 * voice sounds, such as MPE dimension changes).
 */
template <size_t Steps>
class FixedSmoothing
{
  public:
    void setTarget(const float value) noexcept
    {
        m_smoothingStep = Steps;
        m_targetValue = value;
        m_advance = (m_targetValue - m_currentValue) * kReciprocalSteps;
    }

    void forceTarget(const float value) noexcept
    {
        m_currentValue = value;
        m_smoothingStep = 0;
    }

    [[nodiscard]] float getSmoothed() noexcept
    {
        if (m_smoothingStep != 0)
        {
            --m_smoothingStep;
            if (m_smoothingStep == 0)
            {
                m_currentValue = m_targetValue;
            }
            else
            {
                m_currentValue += m_advance;
            }
        }
        return m_currentValue;
    }

    [[nodiscard]] bool isActive() const noexcept
    {
        return m_smoothingStep != 0;
    }

  private:
    static constexpr float kReciprocalSteps{1.0f / static_cast<float>(Steps)};

    float m_currentValue{0.0f};
    float m_targetValue{0.0f};
    size_t m_smoothingStep{0};
    float m_advance{0.0f};
};

/**
 * @ingroup generators
 * @brief Sums a set of named modulation sources through a caller-supplied
 * formula, smoothing the result through FixedSmoothing.
 *
 * Ids is a compile-time list of enumerators naming the sources this instance
 * accepts (e.g. Key, Velocity, DimensionX); set() is a no-op for any other
 * enumerator. The evaluator sees a fixed-size array indexed by declaration
 * order in Ids, not by the enumerator's own value, so the sources actually
 * present stay contiguous regardless of gaps in the enum.
 */
template <size_t NumSteps, typename IdType, IdType... Ids>
class ValueConnector : public FixedSmoothing<NumSteps>
{
    static_assert(sizeof...(Ids) > 0, "ValueConnector needs at least one source Id");

  public:
    using Values = std::array<float, sizeof...(Ids)>;
    using Eval = std::function<float(const Values&)>;

    explicit ValueConnector(Eval evalFunction) noexcept
        : m_evalFunction(std::move(evalFunction))
    {
        size_t position = 0;
        ((m_index[static_cast<size_t>(Ids)] = position++), ...);
    }

    void set(const IdType id, const float value) noexcept
    {
        const auto relativeIndex = static_cast<size_t>(id);
        if (relativeIndex >= m_index.size())
        {
            return;
        }
        m_values[m_index[relativeIndex]] = value;
        FixedSmoothing<NumSteps>::setTarget(m_evalFunction(m_values));
    }

    void setForced(const IdType id, const float value) noexcept
    {
        const auto relativeIndex = static_cast<size_t>(id);
        if (relativeIndex >= m_index.size())
        {
            return;
        }
        m_values[m_index[relativeIndex]] = value;
        FixedSmoothing<NumSteps>::forceTarget(m_evalFunction(m_values));
    }

    [[nodiscard]] float get() noexcept
    {
        return FixedSmoothing<NumSteps>::getSmoothed();
    }

    [[nodiscard]] bool hasChangedValue() const noexcept
    {
        return FixedSmoothing<NumSteps>::isActive();
    }

  private:
    static constexpr size_t kMaxId = std::max({static_cast<size_t>(Ids)...});

    const Eval m_evalFunction;
    Values m_values{};
    std::array<size_t, kMaxId + 1> m_index{};
};

/**
 * @ingroup generators
 * @brief ValueConnector with a two-stage evaluator: a fixed part recomputed
 * only by presetFast(), and a variable part recomputed on every set().
 *
 * Splits sources that only change on note trigger (base value, key, velocity)
 * from sources that change continuously while the voice sounds (MPE
 * dimensions, envelopes), so the continuous path avoids recomputing the
 * trigger-time part every call.
 */
template <size_t NumSteps, typename IdType, IdType... Ids>
class ValueConnectorWithPreset : public FixedSmoothing<NumSteps>
{
    static_assert(sizeof...(Ids) > 0, "ValueConnectorWithPreset needs at least one source Id");

  public:
    using Values = std::array<float, sizeof...(Ids)>;
    using EvalFixedPart = std::function<float(const Values&)>;
    using EvalVarPart = std::function<float(float fixedPart, const Values&)>;

    explicit ValueConnectorWithPreset(EvalFixedPart evalFixedPartFunction, EvalVarPart evalFunction) noexcept
        : m_evalFixedPartFunction(std::move(evalFixedPartFunction))
        , m_evalFunction(std::move(evalFunction))
    {
        size_t position = 0;
        ((m_index[static_cast<size_t>(Ids)] = position++), ...);
    }

    void presetFast() noexcept
    {
        m_fixedValue = m_evalFixedPartFunction(m_values);
        FixedSmoothing<NumSteps>::forceTarget(m_evalFunction(m_fixedValue, m_values));
    }

    void set(const IdType id, const float value) noexcept
    {
        const auto relativeIndex = static_cast<size_t>(id);
        if (relativeIndex >= m_index.size())
        {
            return;
        }
        m_values[m_index[relativeIndex]] = value;
        FixedSmoothing<NumSteps>::setTarget(m_evalFunction(m_fixedValue, m_values));
    }

    [[nodiscard]] float get() noexcept
    {
        return FixedSmoothing<NumSteps>::getSmoothed();
    }

    [[nodiscard]] bool hasChangedValue() const noexcept
    {
        return FixedSmoothing<NumSteps>::isActive();
    }

  private:
    static constexpr size_t kMaxId = std::max({static_cast<size_t>(Ids)...});

    const EvalFixedPart m_evalFixedPartFunction;
    const EvalVarPart m_evalFunction;
    float m_fixedValue{0.0f};
    Values m_values{};
    std::array<size_t, kMaxId + 1> m_index{};
};

}
