
#include "Filters/SvfResoBP.h"

#include "Generators/Excitation.h"
#include "Generators/ResoGenerator.h"

#include "Numbers/Convert.h"
#include "Analysis/SimpleStats.h"

#include <iostream>

static constexpr float sampleRate{48000.f};

std::size_t findLastAboveEpsilon(const std::vector<float>& v, float epsilon) noexcept
{
    const auto it = std::find_if(v.rbegin(), v.rend(), [epsilon](float x) noexcept { return std::fabs(x) > epsilon; });

    if (it == v.rend())
        return v.size(); // "not found" indicator

    return static_cast<std::size_t>(std::distance(v.begin(), it.base() - 1));
}
void checkCompensationModelForWaveExcitation()
{
    AbacDsp::SimpleStats<float> statsAll;
    const auto T60 = Convert::dbToGain(-60.f);
    constexpr float cutoffFreq = 2000.f;
    std::cout << "{";
    float decayStart = 0.0078125f / 8.f;
    float decayEnd = 64.f;
    for (float decay = decayStart; decay <= decayEnd; decay *= 2.f)
    {
        std::cout << static_cast<size_t>(decay * 48000) << "f,";
    }
    std::cout << "};\n";
    for (int n = 0; n <= 132; n += 1)
    {
        const auto freq = Convert::noteToFrequency(static_cast<float>(n));
        std::cout << "{";
        for (float decay = decayStart; decay <= decayEnd; decay *= 2.f)
        {
            AbacDsp::SvfResoBP sut{sampleRate};
            sut.setByDecay(0, freq, decay);
            sut.reset(0, AbacDsp::ResonanceCompensation::compensate(n, decay));
            // sut.reset(0, 1.f);

            const int periodLength = 1 + static_cast<int>(ceil(sampleRate / freq));
            float maxValue = 0;
            int decayTime = 0;
            std::vector<float> result;
            for (size_t j = 0; decayTime < 48000 * decay * 2; ++j)
            {
                float localMax = 0;
                for (int i = 0; i < periodLength * 2; ++i)
                {
                    decayTime++;
                    std::array<float, 1> out{};
                    sut.process0(out.data(), 1);
                    const auto v = out[0];
                    result.push_back(v);
                    maxValue = std::max(std::abs(v), maxValue);
                    localMax = std::max(std::abs(v), localMax);
                }
                if (decayTime > 1000 && localMax < T60)
                {
                    break;
                }
            }
            // std::cout << findLastAboveEpsilon(result, T60) / (decay * 48000.f) << "\t";
            std::cout << " " << std::setprecision(8) << maxValue << "f";
            // std::cout << " " << std::setprecision(8) << std::log(1 / maxValue) << "f";
            if (decay * 2 <= decayEnd)
            {
                std::cout << ", ";
            }
            statsAll.addDataPoint(maxValue);
        }
        std::cout << "}, //" << n << "\t" << freq << "\n";
    }
    statsAll.setPrecision(4);
    statsAll.printHorizontalSummaryHeader(std::cout, "Compensation Analysis");
    statsAll.printHorizontalSummary(std::cout, "All Frequencies");
}

void computeCompensationModelForWaveExcitation(int start, int step)
{
    AbacDsp::SimpleStats<float> statsAll;
    const auto T60 = Convert::dbToGain(-60.f);
    constexpr float cutoffFreq = 2000.f;
    std::cout << "{";
    float decayStart = 0.0078125f / 8.f;
    float decayEnd = 64.f;
    for (float decay = decayStart; decay <= decayEnd; decay *= 2.f)
    {
        std::cout << static_cast<size_t>(decay * 48000) << "f,";
    }
    std::cout << "};\n";
    for (int n = start; n <= 132; n += step)
    {
        const auto freq = Convert::noteToFrequency(static_cast<float>(n));
        std::cout << "{";
        for (float decay = decayStart; decay <= decayEnd; decay *= 2.f)
        {
            AbacDsp::SvfResoBP sut{sampleRate};
            sut.setByDecay(0, freq, decay);
            sut.reset(0, AbacDsp::ResonanceCompensation::compensate(n, decay));
            sut.reset(0, 1.f);

            const int periodLength = 1 + static_cast<int>(ceil(sampleRate / freq));
            float maxValue = 0;
            int decayTime = 0;
            std::vector<float> result;
            for (size_t j = 0; decayTime < 48000 * decay * 2; ++j)
            {
                float localMax = 0;
                for (int i = 0; i < periodLength * 4; ++i)
                {
                    decayTime++;
                    std::array<float, 1> out{};
                    sut.process0(out.data(), 1);
                    const auto v = out[0];
                    result.push_back(v);
                    maxValue = std::max(std::abs(v), maxValue);
                    localMax = std::max(std::abs(v), localMax);
                }
                if (decayTime > 1000 && localMax < T60)
                {
                    break;
                }
            }
            // std::cout << findLastAboveEpsilon(result, T60) / (decay * 48000.f) << "\t";
            // std::cout << " " << std::setprecision(8) << maxValue << "f";
            std::cout << " " << std::setprecision(8) << std::log(1 / maxValue) << "f";
            if (decay * 2 <= decayEnd)
            {
                std::cout << ", ";
            }
            statsAll.addDataPoint(maxValue);
        }
        std::cout << "}, //" << n << "\t" << freq << "\n";
    }
    statsAll.setPrecision(4);
    statsAll.printHorizontalSummaryHeader(std::cout, "Compensation Analysis");
    statsAll.printHorizontalSummary(std::cout, "All Frequencies");
}

int main(int /*ac*/, char* /*av*/[])
{
    for (size_t i = 0; i < 128; ++i)
    {
        std::cout << i << "\t" << AbacDsp::ResonanceCompensation::compensate(i, 0.5f) << "\n";
    }
    checkCompensationModelForWaveExcitation();
    computeCompensationModelForWaveExcitation(0, 12);
}