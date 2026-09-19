// Verification plot for MultiTapDelay: whole-sample-only, no-interpolation tap spacing.
// See README.md.

#include <array>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "Delays/MultiTapDelay.h"

namespace
{
constexpr size_t kMultiTapMaxSize = 4096;
constexpr size_t kNumTaps = 5;
constexpr std::array<size_t, kNumTaps> kTapDelays{200, 500, 900, 1400, 2000};

void writeMultiTapImpulse(std::ofstream& out)
{
    out << "@New plot: title=\"MultiTapDelay: whole-sample-only impulse response, 5 taps\"\n";
    AbacDsp::MultiTapDelay<kMultiTapMaxSize, kNumTaps> delay;
    for (size_t i = 0; i < kNumTaps; ++i)
    {
        delay.setTapDelay(i, kTapDelays[i]);
    }

    constexpr size_t kTotalSamples = kMultiTapMaxSize;
    std::array<std::vector<float>, kNumTaps> tapOut;
    for (auto& v : tapOut)
    {
        v.resize(kTotalSamples);
    }
    for (size_t n = 0; n < kTotalSamples; ++n)
    {
        delay.write(n == 0 ? 1.f : 0.f);
        for (size_t i = 0; i < kNumTaps; ++i)
        {
            // A simple external per-tap decay gain, the way resonik/spectraltap apply their
            // own gain after readTap() - MultiTapDelay itself has no built-in decay.
            const float gain = 1.f / (1.f + static_cast<float>(i));
            tapOut[i][n] = delay.readTap(i) * gain;
        }
    }

    for (size_t i = 0; i < kNumTaps; ++i)
    {
        out << "#tap " << i << " (delay=" << kTapDelays[i] << ")\n";
        for (size_t n = 0; n < kTotalSamples; ++n)
        {
            out << n << " " << tapOut[i][n] << "\n";
        }
    }
}
}

int main(int argc, char* argv[])
{
    const std::string outDir = argc > 1 ? argv[1] : ".";
    std::ofstream multiTapOut(outDir + "/td_multitap.txt");
    if (!multiTapOut)
    {
        std::cerr << "MultiTapDelayExplore: ERROR - failed to open output file in " << outDir << std::endl;
        return 1;
    }

    writeMultiTapImpulse(multiTapOut);

    std::cout << "MultiTapDelayExplore: wrote plot data into " << outDir << std::endl;
}
