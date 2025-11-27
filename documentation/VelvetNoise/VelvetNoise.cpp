#include "Generators/RandomStyle/VelvetCrackle.h"
#include "AudioFile.h"

void modelVelvetCrackling()
{
    constexpr size_t numSamples = 500000;

    AudioFile<float> audioFile;
    audioFile.setSampleRate(48000);
    audioFile.setBitDepth(16);
    audioFile.setAudioBufferSize(1, numSamples);

    VelvetCrackleGenerator sut;
    for (size_t i = 0; i < numSamples; ++i)
    {
        audioFile.samples[0][i] = sut.process(3.5, 0.1f);
    }

    audioFile.save("velvet_crackle.wav");
}

int main(int /*ac*/, char* /*av*/[])
{
    modelVelvetCrackling();
}
