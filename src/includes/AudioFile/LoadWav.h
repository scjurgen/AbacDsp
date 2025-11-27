#pragma once

#include "AudioFile.h"

#include <string>
#include <vector>
#include <span>

namespace AudioUtility
{

class LoadWav
{
  public:
    static AudioFile<float> loadAudioFile(const std::string& filename)
    {
        AudioFile<float> af;
        af.load(filename);
        return af;
    }

    static std::vector<float> loadMonoFromFile(const std::string& filename)
    {
        auto af = loadAudioFile(filename);
        if (af.getNumChannels() == 0 || af.getNumSamplesPerChannel() == 0)
            return {};

        return af.samples[0];
    }

    static std::pair<std::vector<float>, std::vector<float>> loadStereoFromFile(const std::string& filename)
    {
        auto af = loadAudioFile(filename);
        if (af.getNumChannels() < 2 || af.getNumSamplesPerChannel() == 0)
            return {{}, {}};

        return {af.samples[0], af.samples[1]};
    }

    static std::vector<std::vector<float>> loadMultiChannelFromFile(const std::string& filename)
    {
        auto af = loadAudioFile(filename);
        if (af.getNumChannels() == 0 || af.getNumSamplesPerChannel() == 0)
            return {};

        return af.samples;
    }

    static AudioFile<float> loadFromMemory(const std::vector<uint8_t>& data)
    {
        AudioFile<float> af;
        af.loadFromMemory(data);
        return af;
    }

    static std::vector<float> loadMonoFromMemory(const std::vector<uint8_t>& data)
    {
        auto af = loadFromMemory(data);
        if (af.getNumChannels() == 0 || af.getNumSamplesPerChannel() == 0)
            return {};

        return af.samples[0];
    }

    static std::pair<std::vector<float>, std::vector<float>> loadStereoFromMemory(const std::vector<uint8_t>& data)
    {
        auto af = loadFromMemory(data);
        if (af.getNumChannels() < 2 || af.getNumSamplesPerChannel() == 0)
            return {{}, {}};

        return {af.samples[0], af.samples[1]};
    }

    static std::vector<std::vector<float>> loadMultiChannelFromMemory(const std::vector<uint8_t>& data)
    {
        auto af = loadFromMemory(data);
        if (af.getNumChannels() == 0 || af.getNumSamplesPerChannel() == 0)
            return {};

        return af.samples;
    }
};

} // namespace AudioUtility
