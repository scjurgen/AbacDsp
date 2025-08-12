#include "Samplerate/SrConverter.h"
#include "AudioFile.h"
#include "SaveAsWave.h"
#include "FftSmall.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include "Samplerate/sinc_69_768.h"
#include "Samplerate/sinc_33_512.h"
#include "Samplerate/sinc_21_512.h"
#include "Samplerate/sinc_13_256.h"
#include "Samplerate/sinc_11_128.h"
#include "Samplerate/sinc_8_128.h"
#include "Samplerate/sinc_7_128.h"
#include "Samplerate/sinc_6_128.h"
#include "Samplerate/sinc_5_128.h"
#include "Samplerate/sinc_4_128.h"
#include "Samplerate/sinc_3_128.h"
#include "Samplerate/sinc_2_128.h"

namespace AbacDsp
{
const std::vector<SincFilter> sincFilterSet{
    SincFilter{init_69_768}, SincFilter{init_33_512}, SincFilter{init_21_512}, SincFilter{init_13_256},
    SincFilter{init_11_128}, SincFilter{init_8_128}, SincFilter{init_7_128}, SincFilter{init_6_128},
    SincFilter{init_5_128}, SincFilter{init_4_128}, SincFilter{init_3_128}, SincFilter{init_2_128},
};
}

#define BUFFER_LEN 16

typedef struct
{
    size_t currentIndex;
    float m_buffer[BUFFER_LEN * 2];
} AudioCbData;

typedef struct
{
    AudioCbData sf;
    AbacDsp::SrConverter srConverter;
} SrConverterCbData;

static SrConverterCbData dataProvider{};
static float phase = 0;

static long sampleRateConverterInputCb(void* cb_data, float** audio)
{
    AudioCbData* data = (AudioCbData*) cb_data;
    int readFrames = 0;
    *audio = &data->m_buffer[0];

    for (size_t i = 0; i < BUFFER_LEN; i += 2)
    {
        phase += 0.1f;
        data->m_buffer[i] = 0;
        data->m_buffer[i + 1] = sin(phase);
        data->currentIndex++;
        readFrames++;
        if (data->currentIndex >= 1024)
        {
            break;
        }
    }
    return readFrames;
}

#define VARISPEED_BLOCK_LEN 16
auto pitchRatio = 0.4f; // > 1 slow down

static int varPitchGetData(float* samples, size_t outFrames)
{
    for (size_t outFrameIndex = 0; outFrameIndex < outFrames; outFrameIndex += VARISPEED_BLOCK_LEN)
    {
        pitchRatio *= 1.01f;
        if (pitchRatio >= 4.f)
        {
            pitchRatio = 4.f;
        }
        float* output = samples + outFrameIndex * 2;
        int rc = dataProvider.srConverter.fetchBlock(pitchRatio, output, VARISPEED_BLOCK_LEN, 2);
        if (rc < VARISPEED_BLOCK_LEN)
        {
            return 0;
        }
    }
    return outFrames;
}


int main(int argc, char* argv[])
{
    int sincindex = 5;

    dataProvider.srConverter.setCallback(sampleRateConverterInputCb, &dataProvider);
    dataProvider.srConverter.setCoeffsIndex(sincindex);
    std::vector<float> total(0);
    std::vector<float> result(48000);

    varPitchGetData(result.data(), result.size() / 2);
    total.insert(total.end(), result.begin(), result.end());
    varPitchGetData(result.data(), result.size() / 2);
    total.insert(total.end(), result.begin(), result.end());
    varPitchGetData(result.data(), result.size() / 2);
    total.insert(total.end(), result.begin(), result.end());

    saveWavFile("test", 48000, total, 2);
    return 0;
}