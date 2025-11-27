#include "Filters/BiquadResoBP.h"

#include "Generators/Excitation.h"
#include "Generators/ResoGenerator.h"
#include "Generators/ResoVoice.h"
#include "Generators/ResoParallelSIMD.h"

#include "Numbers/Convert.h"
#include "Analysis/SimpleStats.h"

#include <iostream>

void checkCompensationModelForMaxValues()
{
    AbacDsp::BiquadResoBP bp{48000};
    auto compensateFreq = [](const float x) { return 7361.705f / (x + 0.0437f); };

    AbacDsp::SimpleStats<float> statsAll;
    AbacDsp::SimpleStats<float> statsBelow2kHz;
    const auto T40 = Convert::dbToGain(-40.f);
    constexpr float cutoffFreq = 2000.f;
    std::cout << "freq" << "\t" << "maxValue" << "\t" << "decayTime samples" << "\n";
    for (int n = 12; n <= 120; n += 1)
    {
        const auto freq = Convert::noteToFrequency(static_cast<float>(n));
        bp.setByDecay(0, freq, 1.0f);
        bp.reset(0.f, 1.f / compensateFreq(freq));

        const int periodLength = 1 + static_cast<int>(ceil(48000.f / freq));

        float maxValue = 0;
        int decayTime = 0;
        for (size_t j = 0; decayTime < 100000; ++j)
        {
            float localMax = 0;
            for (int i = 0; i < periodLength; ++i)
            {
                decayTime++;
                const auto v = bp.step(0.f);
                maxValue = std::max(std::abs(v), maxValue);
                localMax = std::max(std::abs(v), localMax);
            }
            if (localMax < T40)
            {
                break;
            }
        }
        std::cout << freq << "\t" << maxValue << "\t" << decayTime << "\n";

        statsAll.addDataPoint(maxValue);
        if (freq <= cutoffFreq)
        {
            statsBelow2kHz.addDataPoint(maxValue);
        }
    }
    statsAll.setPrecision(4);
    statsAll.printHorizontalSummaryHeader(std::cout, "Compensation Analysis");
    statsAll.printHorizontalSummary(std::cout, "All Frequencies");
    statsBelow2kHz.setPrecision(4);
    statsBelow2kHz.printHorizontalSummary(std::cout, "Frequencies <= 2 kHz");
}

float compensationFactor(const float freq)
{
    constexpr float a = -7.82268245f;
    constexpr float b = 0.68167842f;
    constexpr float c = 0.02509403f;

    const float log_freq = std::log(freq);
    const float log_f = a + b * log_freq + c * log_freq * log_freq;

    return 1000 * std::exp(log_f);
}
/*
void checkCompensationModelForWaveExcitation()
{
    // auto compensateFreq = [](const float x) { return 7361.705f / (x + 0.0437f); };

    AbacDsp::SimpleStats<float> statsAll;
    AbacDsp::SimpleStats<float> statsBelow2kHz;
    const auto T40 = Convert::dbToGain(-40.f);
    constexpr float cutoffFreq = 2000.f;
    std::cout << "freq" << "\t" << "maxValue" << "\t" << "decayTime samples" << "\n";
    for (int n = 0; n <= 127; n += 1)
    {
        ResoGenerator<1, 1> sut{48000.f, n, 1};
        sut.setExcitationNoise(0.f);
        const auto freq = Convert::noteToFrequency(static_cast<float>(n));
        const auto cFactor = compensationFactor(freq);
        sut.triggerNew(0, cFactor, 0);
        const int periodLength = 1 + static_cast<int>(ceil(48000.f / freq));
        float maxValue = 0;
        int decayTime = 0;
        for (size_t j = 0; decayTime < 100000; ++j)
        {
            float localMax = 0;
            for (int i = 0; i < periodLength; ++i)
            {
                decayTime++;
                std::array<float, 1> out{};
                sut.processBlock(out);
                const auto v = out[0];
                maxValue = std::max(std::abs(v), maxValue);
                localMax = std::max(std::abs(v), localMax);
            }
            if (decayTime > 10000 && localMax < T40)
            {
                break;
            }
        }

        std::cout << freq << "\t" << maxValue << "\t" << decayTime << "\n";

        statsAll.addDataPoint(maxValue);
        if (freq <= cutoffFreq)
        {
            statsBelow2kHz.addDataPoint(maxValue);
        }
    }
    statsAll.setPrecision(4);
    statsAll.printHorizontalSummaryHeader(std::cout, "Compensation Analysis");
    statsAll.printHorizontalSummary(std::cout, "All Frequencies");
    statsBelow2kHz.setPrecision(4);
    statsBelow2kHz.printHorizontalSummary(std::cout, "Frequencies <= 2 kHz");
}*/

void modelSimD()
{
    AbacDsp::SimpleStats<float> statsAll;
    const auto T40 = Convert::dbToGain(-40.f);
    constexpr float cutoffFreq = 2000.f;
    std::cout << "freq" << "\t" << "maxValue" << "\t" << "decayTime samples" << "\n";
    for (int n = 0; n <= 127; n += 1)
    {
        AbacDsp::ResoBpParallelSIMD<4, 1> sut{48000.f};
        const auto freq = Convert::noteToFrequency(static_cast<float>(n));
        sut.setByDecay(0, 0, freq, 1);
        const auto cFactor = compensationFactor(freq);
        sut.reset(0, 0, 1);
        const int periodLength = 1 + static_cast<int>(ceil(48000.f / freq));
        float maxValue = 0;
        int decayTime = 0;
        for (size_t j = 0; decayTime < 100000; ++j)
        {
            float localMax = 0;
            for (int i = 0; i < periodLength; ++i)
            {
                decayTime++;
                std::array<float, 1> out{};
                sut.process(out);
                const auto v = out[0];
                maxValue = std::max(std::abs(v), maxValue);
                localMax = std::max(std::abs(v), localMax);
            }
            if (decayTime > 10000 && localMax < T40)
            {
                break;
            }
        }

        std::cout << freq << "\t" << maxValue << "\t" << decayTime << "\n";

        statsAll.addDataPoint(maxValue);
    }
    statsAll.setPrecision(4);
    statsAll.printHorizontalSummaryHeader(std::cout, "Compensation Analysis");
    statsAll.printHorizontalSummary(std::cout, "All Frequencies");
}

int main(int /*ac*/, char* /*av*/[])
{
    // checkCompensationModelForMaxValues();
    // checkCompensationModelForWaveExcitation();
    modelSimD();
}