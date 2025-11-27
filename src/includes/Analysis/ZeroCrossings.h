#pragma once

#include <functional>
#include <vector>
#include <algorithm>
#include <cmath>
#include <numeric>
#include <concepts>

namespace AbacDsp
{
/*
 * Usage:
 * std::vector<float> signal;
 * const auto stats = AbacDsp::calculateZeroCrossingStatistics(signal.data(), signal.size(), true);
 */
struct ZeroCrossingStatistics
{
    float meanPeriodLen{0.0f};
    float minPeriodLength{0.0f};
    float maxPeriodLength{0.0f};
    float standardDeviation{0.0f};
    size_t periodCount{0};
};

template <std::floating_point T>
inline T calculateDC(const T* data, const size_t size)
{
    if (size == 0)
    {
        return T{0};
    }
    return std::accumulate(data, data + size, T{0}) / static_cast<T>(size);
}

/*
 * the passed lambda is for analysing data series where the baseline is not 0, e.g.
 * modulation that wiggles around 0.5 -> [](const float x) {return x-0.5f;}
 */
template <std::floating_point T, typename PreprocessFunc = std::function<T(T)>>
inline size_t findFirstZeroCrossingNP(
    const T* data, const size_t maxSize, PreprocessFunc preprocess = [](T x) { return x; })
{
    if (maxSize < 2)
    {
        return maxSize;
    }

    auto previousValue = preprocess(data[0]);
    for (size_t index = 1; index < maxSize; ++index)
    {
        auto currentValue = preprocess(data[index]);
        if (previousValue < T{0} && currentValue >= T{0})
        {
            return index;
        }
        previousValue = currentValue;
    }
    return maxSize;
}

template <std::floating_point T>
inline size_t findFirstZeroCrossingNP(const T* data, const size_t maxSize, bool removeDC)
{
    if (!removeDC)
    {
        return findFirstZeroCrossingNP(data, maxSize, [](T x) { return x; });
    }

    const T dc = calculateDC(data, maxSize);
    return findFirstZeroCrossingNP(data, maxSize, [dc](T x) { return x - dc; });
}

template <std::floating_point T, typename PreprocessFunc = std::function<T(T)>>
inline float periodLengthByZeroCrossingAverage(
    const T* data, const size_t size, PreprocessFunc preprocess = [](T x) { return x; })
{
    const size_t firstIndex = findFirstZeroCrossingNP(data, size, preprocess);
    if (firstIndex == size)
    {
        return 0.0f;
    }

    auto previousValue = preprocess(data[firstIndex]);
    size_t cnt = 0;
    size_t lastIndex = firstIndex;

    for (size_t i = firstIndex + 1; i < size; ++i)
    {
        auto value = preprocess(data[i]);
        if (previousValue < T{0} && value >= T{0})
        {
            ++cnt;
            lastIndex = i;
        }
        previousValue = value;
    }

    if (cnt == 0)
    {
        return 0.0f;
    }

    return static_cast<float>(lastIndex - firstIndex) / static_cast<float>(cnt);
}

template <std::floating_point T>
inline float periodLengthByZeroCrossingAverage(const T* data, const size_t size, bool removeDC)
{
    if (!removeDC)
    {
        return periodLengthByZeroCrossingAverage(data, size, [](T x) { return x; });
    }

    const T dc = calculateDC(data, size);
    return periodLengthByZeroCrossingAverage(data, size, [dc](T x) { return x - dc; });
}

template <std::floating_point T, typename PreprocessFunc = std::function<T(T)>>
inline ZeroCrossingStatistics calculateZeroCrossingStatistics(
    const T* data, const size_t size, PreprocessFunc preprocess = [](T x) { return x; })
{
    ZeroCrossingStatistics stats;
    const size_t firstIndex = findFirstZeroCrossingNP(data, size, preprocess);

    if (firstIndex == size)
    {
        return stats;
    }

    std::vector<float> periodLengths;
    auto previousValue = preprocess(data[firstIndex]);
    size_t lastZeroCrossing = firstIndex;

    for (size_t i = firstIndex + 1; i < size; ++i)
    {
        auto value = preprocess(data[i]);
        if (previousValue < T{0} && value >= T{0})
        {
            float periodLength = static_cast<float>(i - lastZeroCrossing);
            periodLengths.push_back(periodLength);
            lastZeroCrossing = i;
        }
        previousValue = value;
    }

    if (periodLengths.empty())
    {
        return stats;
    }

    stats.periodCount = periodLengths.size();

    const float sum = std::accumulate(periodLengths.begin(), periodLengths.end(), 0.0f);
    stats.meanPeriodLen = sum / static_cast<float>(periodLengths.size());

    stats.minPeriodLength = *std::min_element(periodLengths.begin(), periodLengths.end());
    stats.maxPeriodLength = *std::max_element(periodLengths.begin(), periodLengths.end());

    float sumSquaredDifferences = 0.0f;
    for (const auto& length : periodLengths)
    {
        const float diff = length - stats.meanPeriodLen;
        sumSquaredDifferences += diff * diff;
    }

    stats.standardDeviation = std::sqrt(sumSquaredDifferences / static_cast<float>(periodLengths.size()));
    return stats;
}

template <std::floating_point T>
inline ZeroCrossingStatistics calculateZeroCrossingStatistics(const T* data, const size_t size, const bool removeDC)
{
    if (!removeDC)
    {
        return calculateZeroCrossingStatistics(data, size, [](T x) { return x; });
    }

    const T dc = calculateDC(data, size);
    return calculateZeroCrossingStatistics(data, size, [dc](T x) { return x - dc; });
}

template <std::floating_point T>
inline ZeroCrossingStatistics calculateZeroCrossingStatistics(std::vector<T> signal, const bool removeDC)
{
    if (!removeDC)
    {
        return calculateZeroCrossingStatistics(signal.data(), signal.size(), [](T x) { return x; });
    }

    const T dc = calculateDC(signal.data(), signal.size());
    return calculateZeroCrossingStatistics(signal.data(), signal.size(), [dc](T x) { return x - dc; });
}

template <std::floating_point T>
inline ZeroCrossingStatistics calculateZeroCrossingStatistics(std::contiguous_iterator auto first,
                                                              std::contiguous_iterator auto last, const bool removeDC)
{
    if (!removeDC)
    {
        return calculateZeroCrossingStatistics(std::to_address(first), std::ranges::distance(first, last),
                                               [](T x) { return x; });
    }

    const T dc = calculateDC(std::to_address(first), std::ranges::distance(first, last));
    return calculateZeroCrossingStatistics(std::to_address(first), std::ranges::distance(first, last),
                                           [dc](T x) { return x - dc; });
}

}
