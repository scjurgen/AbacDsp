#include <iostream>

#include "AudioFile/SaveWav.h"
#include "NaiveGenerators/Generator.h"

#include <vector>

int main()
{
    constexpr auto sampleRate{48000.f};
    std::vector<float> data(10000, 0.f);
    AbacDsp::Generator<AbacDsp::Wave::Saw> sw(sampleRate, 200);
    sw.render(data.begin(), data.end());
    AbacDsp::OnePoleFilter<AbacDsp::OnePoleFilterCharacteristic::LowPass> lowpass(sampleRate);
    lowpass.setCutoff(500.f);
    lowpass.processBlock(data.data(), data.size());

    AudioUtility::FileOut::MonoToAudioFile(data, sampleRate).save("check.wav");
}
