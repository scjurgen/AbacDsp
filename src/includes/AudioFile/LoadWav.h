#pragma once

#include <string>
#include <vector>

#include "AudioFile.h"

namespace AudioUtility
{

/**
 * @ingroup audiofile
 * @brief Reads WAV data from disk or memory into float channel vectors.
 *
 * Every accessor returns an empty result rather than throwing when the file is
 * missing or has no frames, so a caller checks one emptiness condition instead
 * of handling two failure kinds.
 */
class LoadWav
{
  public:
    [[nodiscard]] static AudioFile<float> loadAudioFile(const std::string& filename)
    {
        AudioFile<float> af;
        af.load(filename);
        return af;
    }

    [[nodiscard]] static std::vector<float> loadMonoFromFile(const std::string& filename)
    {
        auto af = loadAudioFile(filename);
        if (af.getNumChannels() == 0 || af.getNumSamplesPerChannel() == 0)
            return {};

        return af.samples[0];
    }

    [[nodiscard]] static std::pair<std::vector<float>, std::vector<float>> loadStereoFromFile(const std::string& filename)
    {
        auto af = loadAudioFile(filename);
        if (af.getNumChannels() < 2 || af.getNumSamplesPerChannel() == 0)
            return {{}, {}};

        return {af.samples[0], af.samples[1]};
    }

    [[nodiscard]] static std::vector<std::vector<float>> loadMultiChannelFromFile(const std::string& filename)
    {
        auto af = loadAudioFile(filename);
        if (af.getNumChannels() == 0 || af.getNumSamplesPerChannel() == 0)
            return {};

        return af.samples;
    }

    [[nodiscard]] static AudioFile<float> loadFromMemory(const std::vector<uint8_t>& data)
    {
        AudioFile<float> af;
        af.loadFromMemory(data);
        return af;
    }

    [[nodiscard]] static std::vector<float> loadMonoFromMemory(const std::vector<uint8_t>& data)
    {
        auto af = loadFromMemory(data);
        if (af.getNumChannels() == 0 || af.getNumSamplesPerChannel() == 0)
            return {};

        return af.samples[0];
    }

    [[nodiscard]] static std::pair<std::vector<float>, std::vector<float>> loadStereoFromMemory(const std::vector<uint8_t>& data)
    {
        auto af = loadFromMemory(data);
        if (af.getNumChannels() < 2 || af.getNumSamplesPerChannel() == 0)
            return {{}, {}};

        return {af.samples[0], af.samples[1]};
    }

    [[nodiscard]] static std::vector<std::vector<float>> loadMultiChannelFromMemory(const std::vector<uint8_t>& data)
    {
        auto af = loadFromMemory(data);
        if (af.getNumChannels() == 0 || af.getNumSamplesPerChannel() == 0)
            return {};

        return af.samples;
    }
};

}
