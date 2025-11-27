#pragma once

#include <vector>
#include <span>
#include <numbers>
#include <cmath>
#include <random>
#include <algorithm>

class WindowFunctions
{
  public:
    template <typename T>
    static std::vector<T> hannWindow(size_t N)
    {
        std::vector<T> window(N);
        for (size_t i = 0; i < N; ++i)
        {
            T w = static_cast<T>(i) / static_cast<T>(N - 1);
            window[i] = T(0.5) * (T(1) - std::cos(T(2.0) * std::numbers::pi_v<T> * w));
        }
        return window;
    }

    template <typename T>
    static void blackmanHarrisWindow(std::span<T> x)
    {
        const T a0 = 0.35875;
        const T a1 = 0.48829;
        const T a2 = 0.14128;
        const T a3 = 0.01168;
        for (size_t i = 0; i < x.size(); i++)
        {
            T w = static_cast<T>(i) / static_cast<T>(x.size() - 1);
            x[i] *= a0 - a1 * std::cos(T(2.0) * std::numbers::pi_v<T> * w) +
                    a2 * std::cos(T(4.0) * std::numbers::pi_v<T> * w) +
                    a3 * std::cos(T(6.0) * std::numbers::pi_v<T> * w);
        }
    }
};

class Excitation
{
  public:
    static constexpr size_t NumNoise{65535};
    explicit Excitation(size_t patternLength = 1024)
        : m_sineLength(patternLength)
        , m_sine(patternLength + 1, 0.0f)
        , m_noise(NumNoise, 0.0f)
        , m_noiseFactor(0.0f)
    {
        generateSineWave();
        generateNoise();
    }

    float getInterpolatedValue(const float position) noexcept
    {
        if (position < 0.0f || m_sine.empty())
        {
            return 0.0f;
        }

        const size_t index0 = static_cast<size_t>(position);
        const size_t index1 = index0 + 1;
        const float fraction = position - static_cast<float>(index0);

        const float sineValue = m_sine[index0] * (1.0f - fraction) + m_sine[index1] * fraction;
        const float noiseValue = m_noise[m_noiseIndex] * (1.0f - fraction) + m_noise[m_noiseIndex + 1] * fraction;

        const float sineWeight = 1.0f - m_noiseFactor;
        const float noiseWeight = m_noiseFactor;
        m_noiseIndex = (m_noiseIndex + 1) % (NumNoise - 1);
        return sineWeight * sineValue + noiseWeight * noiseValue;
    }

    const std::vector<float>& getPattern() const noexcept
    {
        return m_sine;
    }

    const std::vector<float>& getSinePattern() const noexcept
    {
        return m_sine;
    }

    const std::vector<float>& getNoisePattern() const noexcept
    {
        return m_noise;
    }

    size_t getPatternLength() const noexcept
    {
        return m_sineLength;
    }

    void setNoise(const float noiseFactor)
    {
        m_noiseFactor = std::clamp(noiseFactor, 0.0f, 1.0f);
    }

    float getNoiseFactor() const noexcept
    {
        return m_noiseFactor;
    }

    void regenerateNoise()
    {
        generateNoise();
    }

  private:
    void generateSineWave()
    {
        const float periodsInPattern = 2.0f;
        const float phaseIncrement =
            periodsInPattern * 2.0f * std::numbers::pi_v<float> / static_cast<float>(m_sineLength);
        for (size_t i = 0; i < m_sineLength; ++i)
        {
            const float phase = static_cast<float>(i) * phaseIncrement;
            m_sine[i] = std::sin(phase);
        }

        auto window = WindowFunctions::hannWindow<float>(m_sineLength);
        for (size_t i = 0; i < m_sineLength; ++i)
        {
            m_sine[i] *= window[i];
        }

        m_sine[m_sineLength] = 0.0f;
    }

    void generateNoise()
    {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution distribution(.0f, 4.0f);
        for (size_t i = 0; i < m_noise.size(); ++i)
        {
            m_noise[i] = distribution(gen);
        }
    }

    size_t m_sineLength;
    std::vector<float> m_sine;
    std::vector<float> m_noise;
    size_t m_noiseIndex{0};
    float m_noiseFactor;
};