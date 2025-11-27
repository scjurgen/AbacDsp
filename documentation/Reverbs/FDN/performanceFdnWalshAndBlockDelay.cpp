#include <chrono>
#include <iostream>
#include <vector>
#include <array>
#include <iomanip>
#include <algorithm>
#include <string>

#include "Reverbs/FdnTankRef.h"
#include "Reverbs/FdnReverb.h"

#include "Reverbs/FdnTankBlockDelay.h"
#include "Reverbs/FdnTankBlockDelaySIMD.h"
#include "Reverbs/FdnTankBlockDelayWalsh.h"
#include "Reverbs/FdnTankBlockDelayWalshSIMD.h"

namespace AbacDsp::Test
{

const std::array<size_t, 32> primeSizes{113,  127,  149,  173,  199,  227,  257,  293,  331,  373,  419,
                                        467,  521,  577,  641,  709,  787,  877,  971,  1069, 1171, 1277,
                                        1399, 1523, 1657, 1801, 1949, 2099, 2251, 2411, 2579, 2749};

// as a reference:
const std::array<float, 32> primeDistances = {0.777f,  0.873f,  1.024f,  1.189f,  1.368f,  1.561f,  1.767f,  2.014f,
                                              2.276f,  2.564f,  2.881f,  3.211f,  3.582f,  3.967f,  4.407f,  4.874f,
                                              5.411f,  6.029f,  6.676f,  7.349f,  8.051f,  8.779f,  9.618f,  10.471f,
                                              11.392f, 12.382f, 13.399f, 14.431f, 15.476f, 16.576f, 17.731f, 18.899f};

struct PerformanceResult
{
    std::string m_name;
    double m_averageTimeNs;
    double m_totalTimeMs;
    size_t m_runs;
    size_t m_order;

    double samplesPerSecond() const
    {
        constexpr size_t BlockSize = 16;
        constexpr double SampleRate = 48000.0;
        return (BlockSize * m_runs * 1e9) / (m_totalTimeMs * 1e6);
    }

    double realtimeRatio() const
    {
        return samplesPerSecond() / 48000.0;
    }
};

template <typename IMPL>
PerformanceResult measurePerformance(const std::string& name, size_t order)
{
    std::cout << "Testing " << name << "..." << std::flush;

    const size_t runs = 400'000'000 / order / order;
    constexpr size_t BlockSize = 16;

    IMPL sut{48000.f};
    sut.setDecay(1000.f);

    // Set delay sizes
    for (size_t i = 0; i < order; ++i)
    {
        sut.setDirectSize(i, primeSizes[i]);
    }

    // Prepare test data
    constexpr std::array<float, 16> in{1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    std::array<float, 16> out{};

    // Warmup run
    for (size_t j = 0; j < 100; ++j)
    {
        sut.processBlock(in.data(), out.data());
    }

    // Actual measurement
    auto start = std::chrono::high_resolution_clock::now();
    for (size_t j = 0; j < runs; ++j)
    {
        sut.processBlock(in.data(), out.data());
    }
    auto end = std::chrono::high_resolution_clock::now();

    auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);

    PerformanceResult result;
    result.m_name = name;
    result.m_totalTimeMs = duration.count() / 1e6;
    result.m_averageTimeNs = static_cast<double>(duration.count()) / runs;
    result.m_runs = runs;
    result.m_order = order;

    std::cout << " Done\n";

    return result;
}

void printOrderResults(const std::vector<PerformanceResult>& results, size_t order)
{
    std::cout << "\n=== FDN Order " << order << " Performance Results ===\n";
    std::cout << std::left << std::setw(30) << "Implementation" << std::right << std::setw(12) << "Avg/Block (ns)"
              << std::setw(15) << "Samples/sec" << std::setw(12) << "Realtime x" << "\n";
    std::cout << std::string(70, '-') << "\n";

    // Filter and sort results for this order
    std::vector<PerformanceResult> orderResults;
    std::copy_if(results.begin(), results.end(), std::back_inserter(orderResults),
                 [order](const auto& result) { return result.m_order == order; });

    std::sort(orderResults.begin(), orderResults.end(),
              [](const auto& a, const auto& b) { return a.m_averageTimeNs < b.m_averageTimeNs; });

    for (const auto& result : orderResults)
    {
        std::cout << std::left << std::setw(30) << result.m_name << std::right << std::setw(12) << std::fixed
                  << std::setprecision(0) << result.m_averageTimeNs << std::setw(15) << std::fixed
                  << std::setprecision(0) << result.samplesPerSecond() << std::setw(12) << std::fixed
                  << std::setprecision(1) << result.realtimeRatio() << "\n";
    }

    // Performance comparison within order
    if (orderResults.size() >= 2)
    {
        std::cout << "\nComparison (fastest = 1.0x):\n";
        const auto& fastest = orderResults[0];
        for (const auto& result : orderResults)
        {
            double speedup = result.m_averageTimeNs / fastest.m_averageTimeNs;
            std::cout << "  " << std::left << std::setw(30) << result.m_name << std::right << std::fixed
                      << std::setprecision(2) << speedup << "x\n";
        }
    }
}

void printAllResults(const std::vector<PerformanceResult>& results)
{
    // Get unique orders
    std::vector<size_t> orders;
    for (const auto& result : results)
    {
        if (std::find(orders.begin(), orders.end(), result.m_order) == orders.end())
        {
            orders.push_back(result.m_order);
        }
    }
    std::sort(orders.begin(), orders.end());

    std::cout << "\n=== FDN Reverb Performance Test Results ===\n";
    std::cout << "Block size: 16 samples, " << results[0].m_runs << " runs each\n";

    // Print results for each order
    for (size_t order : orders)
    {
        printOrderResults(results, order);
    }
}

}

int main(int argc, char* argv[])
{
    using namespace AbacDsp::Test;

    std::cout << "Starting FDN Reverb Performance Test...\n";
    std::cout << "Block size: 16 samples\n";

    std::vector<PerformanceResult> results;
    constexpr size_t BlockSize{16};
    results.push_back(measurePerformance<AbacDsp::FdnTankRef<48000, 32, BlockSize>>("Tank Reference 32", 32));
    results.push_back(measurePerformance<AbacDsp::FdnTankBlockDelay<48000, 32, BlockSize>>("Tank blk delay 32", 32));
    results.push_back(
        measurePerformance<AbacDsp::FdnTankBlockDelaySIMD<48000, 32, BlockSize>>("Tank blk delaySIMD 32", 32));
    results.push_back(measurePerformance<AbacDsp::FdnTank<48000, 32, BlockSize>>("Tank modulation 32", 32));
    results.push_back(
        measurePerformance<AbacDsp::FdnTankBlockDelaySIMDWalsh<48000, 32, BlockSize>>("FdnTankWalshSIMD 32", 32));
    results.push_back(measurePerformance<AbacDsp::FdnTankBlockDelayWalsh<48000, 32, BlockSize>>("FdnTankWalsh 32", 32));

    results.push_back(measurePerformance<AbacDsp::FdnTankRef<48000, 16, BlockSize>>("Tank Reference 16", 16));
    results.push_back(measurePerformance<AbacDsp::FdnTankBlockDelay<48000, 16, BlockSize>>("Tank blk delay 16", 16));
    results.push_back(
        measurePerformance<AbacDsp::FdnTankBlockDelaySIMD<48000, 16, BlockSize>>("Tank blk delaySIMD 16", 16));
    results.push_back(measurePerformance<AbacDsp::FdnTank<48000, 16, BlockSize>>("Tank modulation 16", 16));
    results.push_back(measurePerformance<AbacDsp::FdnTankBlockDelayWalsh<48000, 16, BlockSize>>("FdnTankWalsh 16", 16));
    results.push_back(
        measurePerformance<AbacDsp::FdnTankBlockDelaySIMDWalsh<48000, 16, BlockSize>>("FdnTankWalsh Simd 16", 16));

    results.push_back(measurePerformance<AbacDsp::FdnTankRef<48000, 8, BlockSize>>("Tank Reference 8", 8));
    results.push_back(measurePerformance<AbacDsp::FdnTankBlockDelay<48000, 8, BlockSize>>("Tank blk delay 8", 8));
    results.push_back(
        measurePerformance<AbacDsp::FdnTankBlockDelaySIMD<48000, 8, BlockSize>>("Tank blk delaySIMD 8", 8));
    results.push_back(measurePerformance<AbacDsp::FdnTankBlockDelayWalsh<48000, 8, BlockSize>>("FdnTankWalsh 8", 8));
    results.push_back(
        measurePerformance<AbacDsp::FdnTankBlockDelaySIMDWalsh<48000, 8, BlockSize>>("FdnTankWalsh Simd 8", 8));
    results.push_back(measurePerformance<AbacDsp::FdnTank<48000, 8, BlockSize>>("Tank modulation 8", 8));

    results.push_back(measurePerformance<AbacDsp::FdnTankRef<48000, 4, BlockSize>>("Tank Reference 4", 4));
    results.push_back(measurePerformance<AbacDsp::FdnTankBlockDelay<48000, 4, BlockSize>>("Tank blk delay 4", 4));
    results.push_back(
        measurePerformance<AbacDsp::FdnTankBlockDelaySIMD<48000, 4, BlockSize>>("Tank blk delaySIMD 4", 4));
    results.push_back(measurePerformance<AbacDsp::FdnTank<48000, 4, BlockSize>>("Tank modulation 4", 4));
    results.push_back(measurePerformance<AbacDsp::FdnTankBlockDelayWalsh<48000, 4, BlockSize>>("FdnTankWalsh 4", 4));
    results.push_back(
        measurePerformance<AbacDsp::FdnTankBlockDelaySIMDWalsh<48000, 4, BlockSize>>("FdnTankWalsh Simd 4", 4));

    printAllResults(results);

    return 0;
}