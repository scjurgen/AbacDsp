#pragma once

#include "AudioFile.h"

#include <span>
#include <string>
#include <vector>

namespace AudioUtility
{
class SaveWav
{
  public:
    static AudioFile<float> MonoToAudioFile(const std::span<const float>& data, const float sampleRate = 48000.f)
    {
        AudioFile<float> af;
        af.setNumChannels(1);
        af.setNumSamplesPerChannel(static_cast<int>(data.size()));
        af.samples[0].assign(data.begin(), data.end());
        af.setSampleRate(static_cast<int>(sampleRate));
        af.setBitDepth(24);
        return af;
    }

    static AudioFile<float> StereoToAudioFile(const std::span<const float>& leftData,
                                              const std::span<const float>& rightData, const float sampleRate = 48000.f)
    {
        AudioFile<float> af;
        af.setNumChannels(2);
        af.setNumSamplesPerChannel(static_cast<int>(leftData.size()));
        af.samples[0].assign(leftData.begin(), leftData.end());
        af.samples[1].assign(rightData.begin(), rightData.end());
        af.setSampleRate(static_cast<int>(sampleRate));
        af.setBitDepth(24);
        return af;
    }

    static void saveMonoAs(const std::string& filename, const std::span<const float>& data,
                           const float sampleRate = 48000.f)
    {
        auto af = MonoToAudioFile(data, sampleRate);
        af.save(filename);
    }

    static void saveStereoAs(const std::string& filename, const std::span<const float>& leftData,
                             const std::span<const float>& rightData, const float sampleRate = 48000.f)
    {
        auto af = StereoToAudioFile(leftData, rightData, sampleRate);
        af.save(filename);
    }

    static void saveMonoToMemory(std::vector<uint8_t>& target, const std::span<const float>& data,
                                 const float sampleRate = 48000.f)
    {
        auto af = MonoToAudioFile(data, sampleRate);
        af.saveToMemory(target);
    }

    static void saveStereoToMemory(std::vector<uint8_t>& target, const std::span<const float>& leftData,
                                   const std::span<const float>& rightData, const float sampleRate = 48000.f)
    {
        auto af = StereoToAudioFile(leftData, rightData, sampleRate);
        af.saveToMemory(target);
    }
};
}