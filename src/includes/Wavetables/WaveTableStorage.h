#pragma once

#include <algorithm>
#include <array>
#include <cassert>
#include <mutex>
#include <numbers>
#include <numeric>
#include <random>
#include <vector>

#include "Analysis/FftMisc.h"

namespace AbacDsp
{

/// @ingroup wavetables
/// @brief Built-in waveform shapes. Last is a count marker and must stay at the end.
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

inline const std::vector<std::string> waveTablesAsString{
    "Sine",   "Triangle",   "Saw",   "SharkFin",      "Square",           "Pulse",
    "Pulse1", "NoiseFloor", "White", "RectifiedSine", "RectifiedTriangle"};

constexpr size_t WaveTableSize{2048};

/// @ingroup wavetables
/// @brief One mipmap level: a band-limited table and the highest phase increment it may be played at.
struct WaveTable
{
    float topFreq{};
    std::array<float, WaveTableSize + 1> data{}; ///< One sample past the end, so interpolation needs no wrap test.
};

/**
 * @ingroup wavetables
 * @brief All mipmap levels of one waveform, ordered by increasing top frequency.
 *
 * A single table cannot serve the whole range: it holds a fixed set of
 * harmonics, and playing it fast pushes those harmonics past Nyquist. Each
 * level therefore drops the partials that would alias at its own pitch, and
 * lookup picks the highest level still safe for the requested increment.
 * @see https://www.earlevel.com/main/2012/05/04/a-wavetable-oscillator-part-1/
 */
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

/**
 * @ingroup wavetables
 * @brief Shared store of the built-in wavetable sets, generated once on first use.
 *
 * Building the mipmaps means an FFT and a synthesis pass per level per
 * waveform, far too much to repeat per voice, and the tables are immutable
 * once built. They are therefore generated lazily and shared by every
 * oscillator.
 *
 * Consequence worth knowing: the first call pays the whole construction cost,
 * so it must not be the one made from the audio thread.
 */
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
                std::ranges::generate(waveData,
                                      [n = 0]() mutable
                                      {
                                          const float phase = static_cast<float>(n) / static_cast<float>(tableSize);
                                          ++n;
                                          return std::sin(2.0f * std::numbers::pi_v<float> * phase);
                                      });
                break;
            case BasicWave::RectifiedSine:
                std::ranges::generate(waveData,
                                      [n = 0]() mutable
                                      {
                                          const float phase = static_cast<float>(n) / static_cast<float>(tableSize);
                                          ++n;
                                          return std::abs(std::sin(2.0f * std::numbers::pi_v<float> * phase));
                                      });
                break;
            case BasicWave::Triangle:
                std::ranges::generate(waveData,
                                      [n = 0]() mutable
                                      {
                                          const float phase = static_cast<float>(n) / static_cast<float>(tableSize);
                                          n++;
                                          return 2.0f * std::abs(2.0f * phase - 1.0f) - 1.0f;
                                      });
                break;
            case BasicWave::RectifiedTriangle:
                std::ranges::generate(waveData,
                                      [n = 0]() mutable
                                      {
                                          const float phase = static_cast<float>(n) / static_cast<float>(tableSize);
                                          n++;
                                          return std::abs(2.0f * std::abs(2.0f * phase - 1.0f) - 1.0f);
                                      });
                break;
            case BasicWave::Saw:
                std::ranges::generate(waveData,
                                      [n = 0]() mutable
                                      {
                                          const float phase = static_cast<float>(n) / static_cast<float>(tableSize);
                                          ++n;
                                          return -1.0f + 2.0f * phase;
                                      });
                break;
            case BasicWave::SharkFin:
                std::ranges::generate(waveData,
                                      [n = 0]() mutable
                                      {
                                          const float phase = static_cast<float>(n) / static_cast<float>(tableSize);
                                          ++n;
                                          return phase < 0.04f ? -1.0f + 2.0f * (phase / 0.04f)
                                                               : 1.0f - 2.0f * ((phase - 0.04f) / 0.96f);
                                      });
                break;
            case BasicWave::Square:
                std::ranges::generate(waveData, [n = 0]() mutable
                                      { return (n++ < static_cast<int>(tableSize / 2)) ? 1.0f : -1.0f; });
                break;
            case BasicWave::Pulse:
                std::ranges::generate(waveData, [n = 0]() mutable
                                      { return (n++ < static_cast<int>(0.2f * tableSize)) ? 1.0f : -1.0f; });
                break;
            case BasicWave::Pulse1:
                std::ranges::generate(waveData, [n = 0]() mutable
                                      { return (n++ < static_cast<int>(0.05f * tableSize)) ? 1.0f : -1.0f; });
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

    static WaveTableSet addSet(std::vector<float>& freqWaveRe, std::vector<float>& freqWaveIm);

    static float makeWaveTable(std::vector<float>& ar, std::vector<float>& ai, float scale, const float topFreq,
                               std::vector<WaveTable>& tables);

    static void smallFft(std::vector<float>& ar, std::vector<float>& ai);

    static std::vector<WaveTableSet> s_wtbls;
};
}