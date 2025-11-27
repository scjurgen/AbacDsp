#include <chrono>
#include <iostream>
#include <vector>
#include <array>
#include <iomanip>
#include <algorithm>
#include <string>

#include "Filters/BiquadResoBandPassParallel.h"
#include "Filters/BiquadResoBPParallelSIMD.h"
#include "Filters/BiquadResoBP.h"

namespace AbacDsp::Test
{

struct PerformanceResult
{
    std::string m_name;
    double m_averageTimeNs;
    double m_totalTimeMs;
    size_t m_runs;
    size_t m_numElements;

    [[nodiscard]] double samplesPerSecond() const
    {
        constexpr size_t BlockSize = 16;
        constexpr double SampleRate = 48000.0;
        return (BlockSize * m_runs * 1e9) / (m_totalTimeMs * 1e6);
    }

    [[nodiscard]] double realtimeRatio() const
    {
        return samplesPerSecond() / 48000.0;
    }
};

template <size_t NumElements, size_t BlockSize>
class BPReferenceImplementation
{
  public:
    explicit BPReferenceImplementation(const float sampleRate)
        : m_sampleRate(sampleRate)
    {
    }
    void setByDecay(const size_t mainIndex, const size_t index, const float frequency, const float t)
    {
        m_bp[mainIndex].setByDecay(index, frequency, t);
    }

    void process(const float* in, float* outBuffer) noexcept
    {
        for (size_t s = 0; s < BlockSize; ++s)
        {
            outBuffer[s] = 0;
            for (size_t i = 0; i < NumElements; ++i)
            {
                outBuffer[s] += m_bp[i].step(in[s]);
            }
        }
    }

    std::array<BiquadResoBP, NumElements> m_bp;
    float m_sampleRate;
};

template <typename IMPL>
PerformanceResult perf(const std::string& name, size_t NumElements)
{
    const size_t samples = 500'000'000;
    std::cout << "Testing " << name << "\t" << samples << "x " << NumElements << " ... " << std::flush;

    IMPL sut{48000.f};
    for (size_t i = 0; i < NumElements; ++i)
    {
        sut.setByDecay(i, 0, 1000.f + static_cast<float>(i) * 4.2f, 10.f);
    }


    // Prepare test data
    alignas(16) std::array<float, 16> in{1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    alignas(16) std::array<float, 16> out{};
    double chksum = 0; // force compiler to get some result
    // Actual measurement
    auto start = std::chrono::high_resolution_clock::now();
    for (size_t j = 0; j < samples / NumElements; ++j)
    {
        sut.process(in.data(), out.data());
    }
    std::cout << out[1] << "\t";
    auto end = std::chrono::high_resolution_clock::now();

    auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);

    PerformanceResult result;
    result.m_name = name;
    result.m_totalTimeMs = duration.count() / 1e6;
    result.m_averageTimeNs = static_cast<double>(duration.count()) / samples;
    result.m_runs = samples;
    result.m_numElements = 1;

    std::cout << result.m_totalTimeMs << "ms\n";

    return result;
}


void printResults(const std::vector<PerformanceResult>& results, size_t numElements)
{
    std::cout << "\n===  Elements " << numElements << " Performance Results ===\n";
    std::cout << std::left << std::setw(30) << "Implementation" << std::right << std::setw(12) << "Avg/Block (ns)"
              << std::setw(15) << "Samples/sec" << std::setw(12) << "Realtime x" << "\n";
    std::cout << std::string(70, '-') << "\n";

    // Filter and sort results for this order
    std::vector<PerformanceResult> orderResults;
    std::copy_if(results.begin(), results.end(), std::back_inserter(orderResults),
                 [numElements](const auto& result) { return result.m_numElements == numElements; });

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
    std::vector<size_t> numElements;
    for (const auto& result : results)
    {
        if (std::find(numElements.begin(), numElements.end(), result.m_numElements) == numElements.end())
        {
            numElements.push_back(result.m_numElements);
        }
    }
    std::sort(numElements.begin(), numElements.end());

    std::cout << "\n=== Performance Test Results ===\n";

    // Print results for each order
    for (size_t n : numElements)
    {
        printResults(results, n);
    }
}
template <size_t... Ns>
void runPerfTestsImpl(auto& results, std::index_sequence<Ns...>)
{
    constexpr size_t BlockSize{16};
    ((
         [&]<size_t N = Ns>()
         {
             // {
             //     std::stringstream ss;
             //     ss << "Reference " << N;
             //     results.push_back(perf<BPReferenceImplementation<N, BlockSize>>(ss.str(), N));
             // }
             {
                 std::stringstream ss;
                 ss << "Par Simd  " << N;
                 results.push_back(perf<BiquadResoBpParallelSIMD<N, BlockSize>>(ss.str(), N));
             }
         }()),
     ...);
}

template <size_t... Ns>
void runPerfTests(auto& results, std::integer_sequence<size_t, Ns...>)
{
    runPerfTestsImpl(results, std::index_sequence<Ns...>{});
}

}


int main(int argc, char* argv[])
{
    using namespace AbacDsp::Test;

    std::cout << "Starting Biquad Bandpass Performance Test...\n";
    std::vector<PerformanceResult> results;
    runPerfTests(results, std::integer_sequence<size_t, 4, 8, 12, 16, 20, 24, 32, 40, 48, 64, 100, 128, 256, 500, 1000,
                                                1024, 2000, 2048, 3000, 4000, 5000, 6000, 10000>{});
    printAllResults(results);
    return 0;
}