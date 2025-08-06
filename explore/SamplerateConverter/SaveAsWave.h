#pragma once

#include "AudioFile.h"

#include <algorithm>
#include <concepts>


template <typename FloatType>
    requires std::floating_point<FloatType>
void saveWavFile(const std::string& filename, float sampleRate, const std::vector<FloatType>& interleavedAudioData,
                 const int channels, float normalize = 0.0f)
{
    FloatType normalizationFactor = 1.f;
    if (normalize > 0.0f)
    {
        const FloatType maxSample =
            *std::max_element(interleavedAudioData.begin(), interleavedAudioData.end(),
                              [](FloatType a, FloatType b) { return std::abs(a) < std::abs(b); });
        normalizationFactor = maxSample != 0 ? normalize / maxSample : 1.f;
    }
    std::vector<std::vector<FloatType>> normalizedData(channels);
    for (size_t i = 0; i < interleavedAudioData.size() / channels; ++i)
    {
        for (size_t c = 0; c < channels; ++c)
        {
            normalizedData[c].push_back(interleavedAudioData[i * channels + c] * normalizationFactor);
        }
    }


    AudioFile<FloatType> audioFile;
    audioFile.setSampleRate(static_cast<int>(sampleRate));
    audioFile.setNumChannels(channels);
    audioFile.setAudioBufferSize(channels, normalizedData.size());
    for (size_t c = 0; c < channels; ++c)
    {
        audioFile.samples[c] = normalizedData[c];
    }
    audioFile.save(filename + ".wav", AudioFileFormat::Wave);
}
