#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>

namespace AbacDsp
{
class ResonanceCompensation
{
    static constexpr std::array<std::array<float, 17>, 12> m_lut{{
        {0.00082447f, 0.00082447f, 0.00157852f, 0.00514445f, 0.01599692f, 0.04642448f, 0.12222195f, 0.28321755f,
         0.56679815f, 0.98275208f, 1.50821161f, 2.10774183f, 2.75123787f, 3.41878533f, 4.09901667f, 4.78556824f,
         5.47540426f}, // 0	8.17579842
        {0.00081720f, 0.00157221f, 0.00514090f, 0.01599481f, 0.04642084f, 0.12222649f, 0.28320792f, 0.56683367f,
         0.98265177f, 1.50813496f, 2.10775757f, 2.75124955f, 3.41879630f, 4.09902763f, 4.78557777f, 5.47541523f,
         6.16690159f}, // 12	16.35159683
        {0.00156507f, 0.00513425f, 0.01598578f, 0.04641390f, 0.12220296f, 0.28320658f, 0.56684893f, 0.98269802f,
         1.50810623f, 2.10774827f, 2.75128937f, 3.41883469f, 4.09897804f, 4.78557396f, 5.47544909f, 6.16693974f,
         6.85925674f}, // 24	32.70319366
        {0.00511409f, 0.01596842f, 0.04640013f, 0.12221098f, 0.28321692f, 0.56685388f, 0.98269576f, 1.50813758f,
         2.10775304f, 2.75126433f, 3.41884375f, 4.09898710f, 4.78561020f, 5.47546196f, 6.16694975f, 6.85926580f,
         7.55201912f}, // 36	65.40638733
        {0.01589110f, 0.04636280f, 0.12219864f, 0.28323621f, 0.56684977f, 0.98272258f, 1.50815403f, 2.10779834f,
         2.75131392f, 3.41887236f, 4.09902525f, 4.78562593f, 5.47548532f, 6.16698694f, 6.85930490f, 7.55204916f,
         8.24498844f}, // 48	130.81277466
        {0.04621814f, 0.12205157f, 0.28315645f, 0.56689948f, 0.98278350f, 1.50825012f, 2.10799718f, 2.75149965f,
         3.41897607f, 4.09913635f, 4.78576136f, 5.47564507f, 6.16715574f, 6.85948372f, 7.55222416f, 8.24516773f,
         8.93821144f}, // 60	261.62554932
        {0.12166922f, 0.28312826f, 0.56696659f, 0.98308176f, 1.50882614f, 2.10881519f, 2.75181341f, 3.41948199f,
         4.09978867f, 4.78649044f, 5.47640800f, 6.16793871f, 6.86027622f, 7.55301905f, 8.24596691f, 8.93899059f,
         9.63193989f}, // 72	523.25109863
        {0.28350991f, 0.56756663f, 0.98508686f, 1.50992775f, 2.11206269f, 2.75413585f, 3.42131639f, 4.10137224f,
         4.78794765f, 5.47780275f, 6.16930008f, 6.86162424f, 7.55435848f, 8.24729824f, 8.94034386f, 9.63343811f,
         10.32656097f}, // 84	1046.50219727
        {0.56885630f, 0.99513018f, 1.51657021f, 2.11588049f, 2.76054931f, 3.42906237f, 4.10979271f, 4.79670715f,
         5.48673105f, 6.17831612f, 6.87068129f, 7.56343699f, 8.25620651f, 8.94858837f, 9.64114761f, 10.33396435f,
         11.02694416f}, // 96	2093.00439453
        {1.00277770f, 1.54091680f, 2.15131259f, 2.80166912f, 3.47305274f, 4.15522432f, 4.83712912f, 5.52097130f,
         6.20946407f, 6.90028286f, 7.59226608f, 8.28483200f, 8.97713757f, 9.66974735f, 10.36262703f, 11.05563831f,
         11.74871826f}, // 108	4186.00878906
        {1.63956404f, 2.25277448f, 2.90394831f, 3.57557130f, 4.25781679f, 4.94547749f, 5.63587284f, 6.32764149f,
         7.02009916f, 7.71290112f, 8.40554333f, 9.09743500f, 9.78995514f, 10.48278904f, 11.17578030f, 11.86884785f,
         12.56195641f}, // 120	8372.01757812
        {2.95945144f, 3.59891605f, 4.24217892f, 4.91027832f, 5.58937120f, 6.27159595f, 6.95928192f, 7.64865446f,
         8.33828259f, 9.02967167f, 9.72194099f, 10.41464710f, 11.10757351f, 11.80061340f, 12.49370289f, 13.18682289f,
         13.87995625f}, // 132	16744.03515625

    }};

    static constexpr std::array m_times{0.0009765625f, 0.001953125f, 0.00390625f, 0.0078125f, 0.015625f, 0.03125f,
                                        0.0625f,       0.125f,       0.25f,       0.5f,       1.f,       2.f,
                                        4.f,           8.f,          16.f,        32.f,       64.f};

  public:
    [[nodiscard]] static float compensate(const float index, const float time) noexcept
    {
        constexpr float logFirst = 10.f;
        const auto col =
            static_cast<size_t>(std::clamp(std::log2(time) + logFirst, 0.f, static_cast<float>(m_times.size() - 2)));
        const auto col_frac = std::clamp((time - m_times[col]) / (m_times[col + 1] - m_times[col]), 0.0f, 1.0f);

        const auto row =
            static_cast<int>(std::clamp(std::floor(index / 12), 0.f, static_cast<float>(m_lut.size() - 2)));
        const auto row_frac = std::clamp(index / 12.f - static_cast<float>(row), 0.0f, 1.0f);

        // Bilinear interpolation in log-space
        const auto v00 = m_lut[static_cast<size_t>(row)][col];
        const auto v10 = m_lut[static_cast<size_t>(row + 1)][col];
        const auto v01 = m_lut[static_cast<size_t>(row)][col + 1];
        const auto v11 = m_lut[static_cast<size_t>(row + 1)][col + 1];

        const auto v0 = std::lerp(v00, v10, row_frac);
        const auto v1 = std::lerp(v01, v11, row_frac);
        return std::exp(std::lerp(v0, v1, col_frac));
    }
};

class SvfResoBP
{
    struct BandPassCoefficients
    {
        float g{};
        float k{};
        float a1{};
        float a2{};
        float a3{};
    };

  public:
    explicit SvfResoBP(const float sampleRate)
        : m_sampleRate(sampleRate)
        , m_piDivSampleRate(std::numbers::pi_v<float> / m_sampleRate)
    {
        computeCoefficients(0, 1000.f);
        computeCoefficients(1, 1000.f);
    }

    SvfResoBP() = default;

    void setSampleRate(const float sampleRate)
    {
        m_sampleRate = sampleRate;
        m_piDivSampleRate = std::numbers::pi_v<float> / m_sampleRate;
    }

    void setByDecay(const size_t index, const float frequency, const float t)
    {
        m_frequency = frequency;
        m_decayT = t;
        m_decayMax = static_cast<int>(m_sampleRate * t);
        constexpr auto k = 0.1447648273f;
        const float Q = std::numbers::pi_v<float> * frequency * t * k;
        computeCoefficients(index, frequency, Q);
    }

    void setDecay(const size_t index, const float t)
    {
        m_decayT = t;
        m_decayMax = static_cast<int>(m_sampleRate * t * 0.001f);
        constexpr auto k = 0.1447648273f;
        const float Q = std::numbers::pi_v<float> * m_frequency * t * k;
        updateK(index, Q);
    }

    void pitchBendCents(const float cents) noexcept
    {
        m_pitchBend = cents;
        recomputeCoefficientsWithBend(m_currentSet);
    }

    void computeCoefficients(const size_t index, const float frequency,
                             const float Q = 1.f / std::numbers::sqrt2_v<float>) noexcept
    {
        m_frequency = frequency;
        m_pitchBend = 0.f;
        const float k = 1.f / std::max(Q, 0.01f);
        m_cf[index].k = k;
        recomputeCoefficientsWithBend(index);
    }

    void updateK(const size_t index, const float Q) noexcept
    {
        const float g = m_cf[index].g;
        const float k = 1.f / std::max(Q, 0.01f);
        const float denom = 1.f / (1.f + g * (g + k));
        m_cf[index].k = k;
        m_cf[index].a1 = denom;
        m_cf[index].a2 = g * denom;
        m_cf[index].a3 = g * m_cf[index].a2;
    }

    [[nodiscard]] float step(const float in) noexcept
    {
        const auto& cf = m_cf[m_currentSet];
        const float v3 = in - m_z[1];
        const float v1 = cf.a1 * m_z[0] + cf.a2 * v3;
        const float v2 = m_z[1] + cf.a2 * m_z[0] + cf.a3 * v3;
        m_z[0] = 2.f * v1 - m_z[0];
        m_z[1] = 2.f * v2 - m_z[1];
        return cf.k * v1;
    }

    [[nodiscard]] float step0() noexcept
    {
        const auto& cf = m_cf[m_currentSet];
        const float v3 = -m_z[1];
        const float v1 = cf.a1 * m_z[0] + cf.a2 * v3;
        const float v2 = m_z[1] + cf.a2 * m_z[0] + cf.a3 * v3;
        m_z[0] = std::clamp(2.f * v1 - m_z[0], -1000.f, 1000.f);
        m_z[1] = std::clamp(2.f * v2 - m_z[1], -1000.f, 1000.f);
        return cf.k * v1;
    }

    void process(const float* in, float* outBuffer, const size_t numSamples) noexcept
    {
        std::transform(in, in + numSamples, outBuffer, [this](const float v) { return step(v); });
    }

    void process0(float* outBuffer, const size_t numSamples) noexcept
    {
        std::generate_n(outBuffer, numSamples, [this] { return step0(); });
    }

    void reset(const float v1 = 0.f, const float v2 = 0.f) noexcept
    {
        m_z[0] = v1;
        m_z[1] = v2;
    }

    void pump(const float f) noexcept
    {
        m_z[0] *= f;
        m_z[1] *= f;
    }

    [[nodiscard]] float currentMagnitudeSquared() const noexcept
    {
        return m_z[0] * m_z[0] + m_z[1] * m_z[1];
    }

    [[nodiscard]] float currentMagnitude() const noexcept
    {
        return std::sqrt(currentMagnitudeSquared());
    }

    void damp(const bool damp) noexcept
    {
        m_currentSet = damp ? 1 : 0;
    }

    [[nodiscard]] bool isActive() noexcept
    {
        if (m_decayCount > 0)
        {
            m_decayCount--;
            return true;
        }

        if (std::abs(m_z[0]) > 1E-6f || std::abs(m_z[1]) > 1E-6f)
        {
            m_inActiveCount = 0;
        }
        else
        {
            m_inActiveCount++;
        }

        return m_inActiveCount < 32;
    }

    void triggered() noexcept
    {
        m_decayCount = m_decayMax;
    }

  private:
    void recomputeCoefficientsWithBend(const size_t index) noexcept
    {
        constexpr float centsToOctave = 1.f / 1200.f;
        const float ratio = std::exp2(m_pitchBend * centsToOctave);
        const float bendFrequency = m_frequency * ratio;

        const float g = std::tan(m_piDivSampleRate * bendFrequency);
        m_cf[index].g = g;

        constexpr float decayConst = 0.1447648273f;
        const float Q = std::numbers::pi_v<float> * bendFrequency * m_decayT * decayConst;
        const float k = 1.f / std::max(Q, 0.01f);
        m_cf[index].k = k;

        const float denom = 1.f / (1.f + g * (g + k));
        m_cf[index].a1 = denom;
        m_cf[index].a2 = g * denom;
        m_cf[index].a3 = g * m_cf[index].a2;
    }

    float m_sampleRate{48000.f};
    float m_piDivSampleRate{0.00006544984695f};
    size_t m_currentSet{0};
    int m_decayCount{0};
    int m_decayMax{0};
    float m_decayT{0};
    int m_inActiveCount{0};
    std::array<BandPassCoefficients, 2> m_cf{};
    std::array<float, 2> m_z{};
    float m_frequency{};
    float m_pitchBend{0.f};
};

}
