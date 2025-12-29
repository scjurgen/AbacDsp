#include "Filters/SvfResoBP.h"

#include "Generators/Excitation.h"
#include "Generators/ResoGenerator.h"
#include "Generators/ResoVoice.h"

#include "Numbers/Convert.h"
#include "Analysis/SimpleStats.h"

#include <iostream>
static constexpr float sampleRate{48000.f};
void checkCompensationModelForMaxValues()
{
    AbacDsp::BiquadResoBP bp{sampleRate};
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

        const int periodLength = 1 + static_cast<int>(ceil(sampleRate / freq));

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

class ResonanceCompensation
{
    static constexpr std::array<std::array<float, 10>, 11> m_lut{
        {{1.00779f, 1.02387f, 1.06867f, 1.18268f, 1.44618f, 2.01459f, 3.18881f, 5.55530f, 10.3099f, 19.8261f},
         {1.02387f, 1.06867f, 1.18268f, 1.44618f, 2.01459f, 3.18881f, 5.55530f, 10.3099f, 19.8261f, 38.8744f},
         {1.06867f, 1.18268f, 1.44618f, 2.01459f, 3.18881f, 5.55530f, 10.3099f, 19.8261f, 38.8744f, 76.9889f},
         {1.18268f, 1.44618f, 2.01459f, 3.18881f, 5.55530f, 10.3099f, 19.8261f, 38.8744f, 76.9889f, 153.003f},
         {1.44618f, 2.01459f, 3.18881f, 5.55530f, 10.3099f, 19.8261f, 38.8744f, 76.9889f, 153.003f, 305.436f},
         {2.01459f, 3.18881f, 5.55530f, 10.3099f, 19.8261f, 38.8744f, 76.9889f, 153.003f, 305.436f, 610.263f},
         {3.16456f, 5.55942f, 10.3145f, 19.8437f, 38.8959f, 77.0371f, 153.222f, 305.883f, 611.385f, 1220.51f},
         {5.56863f, 10.3496f, 19.8804f, 38.9582f, 77.1497f, 153.506f, 306.384f, 612.370f, 1222.37f, 2443.10f},
         {10.3982f, 20.0212f, 39.2430f, 77.8307f, 155.072f, 309.161f, 617.284f, 1233.82f, 2465.75f, 4927.48f},
         {20.8906f, 41.0697f, 81.4963f, 160.767f, 319.010f, 636.166f, 1271.56f, 2539.62f, 5080.56f, 10158.1f},
         {45.4297f, 90.2340f, 179.589f, 358.659f, 715.851f, 1431.30f, 2862.77f, 5722.48f, 11451.4f, 22884.9f}}};

    static constexpr std::array<float, 11> m_indices{{0, 12, 24, 36, 48, 60, 72, 84, 96, 108, 120}};
    static constexpr std::array<float, 10> m_times{
        {0.01f, 0.02f, 0.04f, 0.08f, 0.16f, 0.32f, 0.64f, 1.28f, 2.56f, 5.12f}};

  public:
    static float compensate(float index, float time)
    {
        // Find column (time)
        int col = 0;
        for (int i = 0; i < 9; ++i)
        {
            if (time < m_times[i + 1])
            {
                col = i;
                break;
            }
            col = 9;
        }
        float col_frac = (time - m_times[col]) / (m_times[col + 1] - m_times[col]);
        col_frac = std::clamp(col_frac, 0.0f, 1.0f);
        // Find row (index)
        int row = 0;
        for (int i = 0; i < 10; ++i)
        {
            if (index < m_indices[i + 1])
            {
                row = i;
                break;
            }
            row = 10;
        }
        float row_frac = (index - m_indices[row]) / (m_indices[row + 1] - m_indices[row]);
        row_frac = std::clamp(row_frac, 0.0f, 1.0f);

        // Bilinear interpolation
        float v00 = m_lut[row][col];
        float v10 = m_lut[row + 1][col];
        float v01 = m_lut[row][col + 1];
        float v11 = m_lut[row + 1][col + 1];

        float v0 = std::lerp(v00, v10, row_frac);
        float v1 = std::lerp(v01, v11, row_frac);
        return std::lerp(v0, v1, col_frac);
    }
};

void checkCompensationModelForWaveExcitation()
{
    // auto compensateFreq = [](const float x) { return 7361.705f / (x + 0.0437f); };
    FILE* fp;
    fp = fopen("/tmp/result.raw", "wb");
    AbacDsp::SimpleStats<float> statsAll;
    AbacDsp::SimpleStats<float> statsBelow2kHz;
    const auto T40 = Convert::dbToGain(-40.f);
    constexpr float cutoffFreq = 2000.f;
    std::cout << "dec/f";
    for (float decay = 0.01f; decay <= 10.f; decay *= 2.f)
    {
        std::cout << "\t" << decay;
    }
    std::cout << "\n";
    for (int n = 0; n <= 127; n += 12)
    {
        const auto freq = Convert::noteToFrequency(static_cast<float>(n));
        std::cout << n;
        for (float decay = 0.01f; decay <= 10.f; decay *= 2.f)
        {
            AbacDsp::SvfResoBP sut{sampleRate};
            sut.setByDecay(0, freq, decay);
            sut.reset(0, ResonanceCompensation::compensate(n, decay));
            // sut.reset(0, getCompensationLUT(freq));

            const int periodLength = 1 + static_cast<int>(ceil(sampleRate / freq));
            float maxValue = 0;
            int decayTime = 0;
            for (size_t j = 0; decayTime < 1000000; ++j)
            {
                float localMax = 0;
                for (int i = 0; i < periodLength * 2; ++i)
                {
                    decayTime++;
                    std::array<float, 1> out{};
                    sut.process0(out.data(), 1);
                    fwrite(out.data(), 1, 4, fp);
                    const auto v = out[0];
                    maxValue = std::max(std::abs(v), maxValue);
                    localMax = std::max(std::abs(v), localMax);
                }
                if (decayTime > 1000 && localMax < T40)
                {
                    break;
                }
            }

            std::cout << "\t" << maxValue;

            statsAll.addDataPoint(maxValue);
            if (freq <= cutoffFreq)
            {
                statsBelow2kHz.addDataPoint(maxValue);
            }
        }
        std::cout << "\n";
    }
    statsAll.setPrecision(4);
    statsAll.printHorizontalSummaryHeader(std::cout, "Compensation Analysis");
    statsAll.printHorizontalSummary(std::cout, "All Frequencies");
    statsBelow2kHz.setPrecision(4);
    statsBelow2kHz.printHorizontalSummary(std::cout, "Frequencies <= 2 kHz");
    fclose(fp);
}


int main(int /*ac*/, char* /*av*/[])
{
    // checkCompensationModelForMaxValues();
    checkCompensationModelForWaveExcitation();
}