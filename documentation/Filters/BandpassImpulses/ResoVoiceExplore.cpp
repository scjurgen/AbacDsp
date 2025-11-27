
#include "Generators/ResoVoice.h"


#include "Numbers/Convert.h"
#include "Analysis/SimpleStats.h"

#include <iostream>

void resoVoiceVolume()
{
    AbacDsp::Reso::ResoVoice<16, 1> sut{48000};
    std::vector<float> f;
    std::vector<float> decays;
    std::vector<std::vector<float>> results;

    for (size_t n = 12; n <= 120; n += 12)
    {
        const float frequency = Convert::noteToFrequency(static_cast<float>(n));
        f.push_back(frequency);
        std::vector<float> row;

        for (float dc = 0.001f; dc < 20.f; dc *= 2.f)
        {
            if (n == 12)
            {
                decays.push_back(dc);
            }
            float maxSum{0.f};
            for (size_t k = 0; k < 100; ++k)
            {
                sut.setImpulseMix(.0f);
                sut.resetVoices();
                sut.setSustainFeedLevel(0.f);

                sut.addGenerator(AbacDsp::Reso::VoiceSetting{frequency, 1.f, dc, 0.0f, 0.f, 0});
                float locMax = 0;
                for (size_t j = 0; j < 48000 / 16; ++j)
                {
                    std::array<std::array<float, 16>, 2> out{};
                    sut.processBlock(out);
                    for (int i = 0; i < 16; ++i)
                    {
                        locMax = std::max(locMax, std::abs(out[0][i]));
                        locMax = std::max(locMax, std::abs(out[1][i]));
                    }
                }
                maxSum += locMax;
            }
            row.push_back(maxSum / 100.f);
        }
        results.push_back(row);
    }

    std::cout << "f";
    for (const auto& dc : decays)
    {
        std::cout << "\t" << dc;
    }
    std::cout << "\n";

    for (size_t i = 0; i < results.size(); ++i)
    {
        std::cout << f[i];
        for (const auto& val : results[i])
        {
            std::cout << "\t" << val;
        }
        std::cout << "\n";
    }
}


void resoVoice()
{
    AbacDsp::Reso::ResoVoice<16, 4> sut{48000};
    float power = 0.1f;
    sut.setImpulseMix(0.5f);
    sut.resetVoices();
    sut.addGenerator(AbacDsp::Reso::VoiceSetting{440.f, power, 0.1f, 0.1f, 0.f, 0});
    sut.addGenerator(AbacDsp::Reso::VoiceSetting{440.f * 2, power, 0.1f, 0.1f, 0.5f, 0});
    sut.addGenerator(AbacDsp::Reso::VoiceSetting{440.f * 3.5, power, 0.1f, 0.1f, -0.5f, 0});
    sut.addGenerator(AbacDsp::Reso::VoiceSetting{440.f * 5, power, 0.1f, 0.1f, 0.f, 0});

    sut.setSustainFeedLevel(1.f);
    for (size_t j = 0; j < 500; ++j)
    {
        if (j == 250)
        {
            sut.setSustainFeedLevel(.0f);
        }
        std::array<std::array<float, 16>, 2> result{};
        sut.processBlock(result);
        for (int i = 0; i < 16; ++i)
        {
            std::cout << j * 16 + i << "\t" << result[0][i] << "\t" << result[1][i] << "\n";
        }
    }
}

int main(int /*ac*/, char* /*av*/[])
{
    resoVoiceVolume();
}