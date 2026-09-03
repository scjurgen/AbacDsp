#pragma once

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <random>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "PerfConfig.h"

namespace AbacDsp::Perf
{

/// @brief Fixed white-noise blocks fed to every benchmarked module, so nothing ever sees
/// degenerate all-zero input. Reused every call rather than advanced sample by sample -
/// the benchmark loop itself is the noise "played out in loop".
struct NoiseBlocks
{
    std::array<float, kBlockSize> mono{};
    std::array<float, 2 * kBlockSize> interleavedStereo{};
    std::vector<float> longInterleavedStereo;
};

[[nodiscard]] inline NoiseBlocks makeNoiseBlocks(const unsigned seed = 1234u, const float longSeconds = 2.f)
{
    std::mt19937 rng(seed);
    std::uniform_real_distribution<float> dist(-1.f, 1.f);

    NoiseBlocks blocks;
    std::ranges::generate(blocks.mono, [&] { return dist(rng); });
    std::ranges::generate(blocks.interleavedStereo, [&] { return dist(rng); });

    blocks.longInterleavedStereo.resize(static_cast<size_t>(longSeconds * kSampleRate) * 2);
    std::ranges::generate(blocks.longInterleavedStereo, [&] { return dist(rng); });
    return blocks;
}

struct SutResult
{
    std::string category;
    std::string variant;
    double nsPerBlock{};
    double samplesPerSecond{};
    double realtimeMultiple{};
    size_t instanceBytes{};
    size_t maxInstances{};
    size_t totalBytesAtMax{};
    bool cappedAtLimit{false};
};

namespace detail
{
using Clock = std::chrono::steady_clock;

[[nodiscard]] inline double secondsSince(const Clock::time_point& start) noexcept
{
    return std::chrono::duration<double>(Clock::now() - start).count();
}

template <typename MakeInstance, typename ProcessOneBlock>
[[nodiscard]] double timeSingleInstance(MakeInstance& makeInstance, ProcessOneBlock& processOneBlock,
                                        const NoiseBlocks& noise, const double secondsPerProbe)
{
    auto sut = makeInstance();

    constexpr size_t calibrationIterations = 256;
    const auto calibStart = Clock::now();
    for (size_t i = 0; i < calibrationIterations; ++i)
    {
        processOneBlock(sut, noise);
    }
    const auto perIterSeconds = std::max(secondsSince(calibStart) / calibrationIterations, 1e-9);
    const auto probeIterations = std::max<size_t>(1, static_cast<size_t>(secondsPerProbe / perIterSeconds));

    std::array<double, 3> probeSeconds{};
    for (auto& seconds : probeSeconds)
    {
        const auto start = Clock::now();
        for (size_t i = 0; i < probeIterations; ++i)
        {
            processOneBlock(sut, noise);
        }
        seconds = secondsSince(start);
    }
    std::ranges::sort(probeSeconds);
    return (probeSeconds[1] / static_cast<double>(probeIterations)) * 1e9;
}

/// @brief Worst single pass over all K instances, in seconds: the real-time question is
/// whether every pass fits the block deadline, not just the average one.
template <typename Sut, typename MakeInstance, typename ProcessOneBlock>
[[nodiscard]] double worstCycleSeconds(const size_t k, MakeInstance& makeInstance, ProcessOneBlock& processOneBlock,
                                       const NoiseBlocks& noise, const double secondsPerProbe)
{
    std::vector<Sut> instances;
    instances.reserve(k);
    for (size_t i = 0; i < k; ++i)
    {
        instances.push_back(makeInstance());
    }

    constexpr int warmupCycles = 2;
    for (int w = 0; w < warmupCycles; ++w)
    {
        for (auto& inst : instances)
        {
            processOneBlock(inst, noise);
        }
    }

    const auto probeStart = Clock::now();
    double worst = 0.0;
    size_t cycles = 0;
    while (secondsSince(probeStart) < secondsPerProbe || cycles < 2)
    {
        const auto cycleStart = Clock::now();
        for (auto& inst : instances)
        {
            processOneBlock(inst, noise);
        }
        worst = std::max(worst, secondsSince(cycleStart));
        ++cycles;
    }
    return worst;
}

template <typename Sut, typename MakeInstance, typename ProcessOneBlock>
[[nodiscard]] std::pair<size_t, bool> maxInstancesInOneThread(MakeInstance& makeInstance,
                                                              ProcessOneBlock& processOneBlock,
                                                              const NoiseBlocks& noise, const double secondsPerProbe,
                                                              const double deadlineSeconds, const size_t cap)
{
    const auto sustainsRealtime = [&](const size_t k)
    { return worstCycleSeconds<Sut>(k, makeInstance, processOneBlock, noise, secondsPerProbe) <= deadlineSeconds; };

    size_t lastGood = 0;
    size_t firstBad = 0;
    size_t k = 1;
    while (true)
    {
        const size_t tested = std::min(k, cap);
        if (!sustainsRealtime(tested))
        {
            firstBad = tested;
            break;
        }
        lastGood = tested;
        if (tested == cap)
        {
            break;
        }
        k *= 2;
    }

    if (lastGood == 0)
    {
        return {0, false};
    }
    if (firstBad == 0)
    {
        return {cap, true};
    }

    size_t low = lastGood;
    size_t high = firstBad;
    while (low + 1 < high)
    {
        const size_t mid = low + (high - low) / 2;
        if (sustainsRealtime(mid))
        {
            low = mid;
        }
        else
        {
            high = mid;
        }
    }
    return {low, false};
}

}

/// @brief Times one instance and searches for the most instances that keep up with the
/// audio-callback deadline in a single thread. MakeInstance is `() -> Sut`, ProcessOneBlock
/// is `(Sut&, const NoiseBlocks&) -> void`.
template <typename MakeInstance, typename ProcessOneBlock>
[[nodiscard]] SutResult benchmark(std::string category, std::string variant, MakeInstance makeInstance,
                                  ProcessOneBlock processOneBlock, const NoiseBlocks& noise,
                                  const double secondsPerProbe, const size_t cap = kDefaultInstanceCap)
{
    using Sut = std::invoke_result_t<MakeInstance>;

    SutResult result;
    result.category = std::move(category);
    result.variant = std::move(variant);
    result.instanceBytes = sizeof(Sut);

    result.nsPerBlock = detail::timeSingleInstance(makeInstance, processOneBlock, noise, secondsPerProbe);
    result.samplesPerSecond = (static_cast<double>(kBlockSize) * 1e9) / result.nsPerBlock;
    result.realtimeMultiple = result.samplesPerSecond / static_cast<double>(kSampleRate);

    const auto deadlineSeconds = static_cast<double>(kBlockSize) / static_cast<double>(kSampleRate);
    const auto [maxInstances, capped] = detail::maxInstancesInOneThread<Sut>(makeInstance, processOneBlock, noise,
                                                                             secondsPerProbe, deadlineSeconds, cap);
    result.maxInstances = maxInstances;
    result.cappedAtLimit = capped;
    result.totalBytesAtMax = result.instanceBytes * result.maxInstances;
    return result;
}

}
