#pragma once
#include "Analysis/FftSmall.h"

#include <array>
#include <algorithm>
#include <cassert>
#include <mutex>
#include <numbers>
#include <numeric>
#include <random>
#include <vector>

namespace AbacDsp
{

enum class BasicWave
{
    Sine = 0,
    Triangle,
    Saw,
    SharkFin,
    Square,
    Pulse,
    Pulse1,
    NoiseFloor,
    White,
    RectifiedSine,
    RectifiedTriangle,
    Last // N.B.: Always keep this as the last entry
};

const std::vector<std::string> waveTablesAsString{
    "Sine",   "Triangle",   "Saw",   "SharkFin",      "Square",           "Pulse",
    "Pulse1", "NoiseFloor", "White", "RectifiedSine", "RectifiedTriangle"};

constexpr size_t WaveTableSize{2048};

struct WaveTable
{
    float topFreq{};
    std::array<float, WaveTableSize + 1> data; // Extra sample for interpolation
};


struct WaveTableSet
{
    BasicWave wave;
    std::vector<WaveTable> tables;

    [[nodiscard]] size_t getIndexByFrequency(const float inc) const noexcept
    {
        const auto it = std::ranges::find_if(tables, [inc](const auto& wt) { return inc < wt.topFreq; });
        return it != tables.end() ? std::distance(tables.begin(), it) : tables.size() - 1;
    }
};

class WaveTableStore
{
  public:
    static WaveTableSet& getTableSet(const BasicWave index)
    {
        prefillTablesOnce();
        return s_wtbls[static_cast<size_t>(index)];
    }

  private:
    template <std::ranges::range Range>
    static float calculateRMS(const Range& wave)
    {
        const float sumSquares =
            std::accumulate(wave.begin(), wave.end(), 0.0f, [](const float acc, const float x) { return acc + x * x; });
        return std::sqrt(sumSquares / static_cast<float>(std::ranges::size(wave)));
    }

    template <std::ranges::range Range>
        requires std::floating_point<std::ranges::range_value_t<Range>>
    static void applyCompensation(Range& wave, const float rms, const float globalGain)
    {
        float gain = globalGain / rms;
        for (auto& sample : wave)
        {
            sample *= gain;
        }
    }

    static void prefillTablesOnce() noexcept
    {
        static std::once_flag flag;
        std::call_once(flag,
                       []
                       {
                           s_wtbls.clear();
                           s_wtbls.reserve(static_cast<size_t>(BasicWave::Last));
                           for (int wave = static_cast<int>(BasicWave::Sine); wave < static_cast<int>(BasicWave::Last);
                                ++wave)
                           {
                               s_wtbls.push_back(prefill(static_cast<BasicWave>(wave)));
                           }
                       });
    }

    static WaveTableSet prefill(const BasicWave wave)
    {
        constexpr size_t tableSize = WaveTableSize;
        std::vector<float> waveData(tableSize);

        switch (wave)
        {
            case BasicWave::Sine:
                std::ranges::generate(waveData, [n = 0]() mutable
                                      { return std::sin(2.0f * std::numbers::pi_v<float> * (n++) / tableSize); });
                break;
            case BasicWave::RectifiedSine:
                std::ranges::generate(
                    waveData, [n = 0]() mutable
                    { return std::abs(std::sin(2.0f * std::numbers::pi_v<float> * (n++) / tableSize)); });
                break;
            case BasicWave::Triangle:
                std::ranges::generate(waveData,
                                      [n = 0]() mutable
                                      {
                                          const float phase = n++ / static_cast<float>(tableSize);
                                          return 2.0f * std::abs(2.0f * phase - 1.0f) - 1.0f;
                                      });
                break;
            case BasicWave::RectifiedTriangle:
                std::ranges::generate(waveData,
                                      [n = 0]() mutable
                                      {
                                          const float phase = n++ / static_cast<float>(tableSize);
                                          return std::abs(2.0f * std::abs(2.0f * phase - 1.0f) - 1.0f);
                                      });
                break;
            case BasicWave::Saw:
                std::ranges::generate(waveData, [n = 0]() mutable
                                      { return -1.0f + 2.0f * (n++ / static_cast<float>(tableSize)); });
                break;
            case BasicWave::SharkFin:
                std::ranges::generate(waveData,
                                      [n = 0]() mutable
                                      {
                                          const float phase = n++ / static_cast<float>(tableSize);
                                          return phase < 0.04f ? -1.0f + 2.0f * (phase / 0.04f)
                                                               : 1.0f - 2.0f * ((phase - 0.04f) / 0.96f);
                                      });
                break;
            case BasicWave::Square:
                std::ranges::generate(waveData, [n = 0]() mutable { return (n++ < tableSize / 2) ? 1.0f : -1.0f; });
                break;
            case BasicWave::Pulse:
                std::ranges::generate(waveData, [n = 0]() mutable { return (n++ < 0.2f * tableSize) ? 1.0f : -1.0f; });
                break;
            case BasicWave::Pulse1:
                std::ranges::generate(waveData, [n = 0]() mutable { return (n++ < 0.05f * tableSize) ? 1.0f : -1.0f; });
                break;

            case BasicWave::NoiseFloor:
            case BasicWave::White: // we fake the white table
            {
                std::mt19937 gen{std::random_device{}()};
                std::uniform_real_distribution<float> dist(-1.f, 1.f);
                std::ranges::generate(waveData, [&dist, &gen]() mutable { return dist(gen); });
                break;
            }
            default:
                break;
        }

        WaveTableSet set = fftFromSlice(waveData);
        set.wave = wave;

        if (wave == BasicWave::NoiseFloor || wave == BasicWave::White)
        {
            scaleSet(set, 1E-5f);
        }
        else
        {
            const float rmsCompensation = calculateRMS(set.tables[0].data);
            for (auto& [_, data] : set.tables)
            {
                applyCompensation(data, rmsCompensation, 0.5f);
            }
        }
        return set;
    }

    static void scaleSet(WaveTableSet& set, const float factor)
    {
        for (auto& v : set.tables)
        {
            std::transform(v.data.begin(), v.data.end(), v.data.begin(),
                           [factor](const float in) { return factor * in; });
        }
    }
    static WaveTableSet fftFromSlice(const std::vector<float>& slice);

    /*
     * @brief Generates band-limited wavetables by progressively reducing harmonics for antialiasing.
     *
     * Starting from the full spectrum, each iteration creates a wavetable with fewer harmonics for lower frequency
     * bands. DC add Nyquist are zeroed, then significant harmonics are identified.
     * For each band:
     *   1. Finds highest non-negligible harmonic (magnitude > 1E-4)
     *   2. Copies harmonics up to current max into new spectrum
     *   3. Generates time-domain wavetable via makeWaveTable()
     *   4. Adjusts maxHarmonic for next band until no significant harmonics remain
     *
     * Resulting wavetable set is stored in s_wtbls for later playback at appropriate frequencies.
     */
    static WaveTableSet addSet(std::vector<float>& freqWaveRe, std::vector<float>& freqWaveIm);

    /**
     * @brief Convert spectral data to a normalized wavetable with wrap sample and add to currently generated
     * wavetable
     */
    static float makeWaveTable(std::vector<float>& ar, std::vector<float>& ai, float scale, const float topFreq,
                               std::vector<WaveTable>& tables);

    static void smallFft(std::vector<float>& ar, std::vector<float>& ai);

    static std::vector<WaveTableSet> s_wtbls;
};
}