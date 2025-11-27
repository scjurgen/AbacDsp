#pragma once

#include <string>
#include <vector>
#include <span>
#include <stdexcept>
#include <cstring>

#include <vorbis/codec.h>
#include <vorbis/vorbisfile.h>

namespace AudioUtility
{

class LoadOgg
{
  public:
    struct OggInfo
    {
        int sampleRate;
        int channels;
        int64_t numSamples;
    };

    static OggInfo getInfo(const std::string& filename)
    {
        OggVorbis_File vf;
        if (ov_fopen(filename.c_str(), &vf) < 0)
        {
            return {0, 0, 0};
        }

        vorbis_info* vi = ov_info(&vf, -1);
        int64_t totalSamples = ov_pcm_total(&vf, -1);

        OggInfo info{static_cast<int>(vi->rate), vi->channels, totalSamples};
        ov_clear(&vf);
        return info;
    }

    static std::vector<float> loadMonoFromFile(const std::string& filename)
    {
        OggVorbis_File vf;
        if (ov_fopen(filename.c_str(), &vf) < 0)
        {
            return {};
        }

        vorbis_info* vi = ov_info(&vf, -1);
        const auto totalSamples = static_cast<size_t>(ov_pcm_total(&vf, -1));

        std::vector<float> result(totalSamples);

        int current_section;
        size_t samplesRead = 0;

        while (samplesRead < totalSamples)
        {
            float** pcm;
            long samples = ov_read_float(&vf, &pcm, 4096, &current_section);

            if (samples <= 0)
            {
                break;
            }

            for (long i = 0; i < samples && samplesRead < totalSamples; ++i)
            {
                result[samplesRead++] = pcm[0][i];
            }
        }

        ov_clear(&vf);
        result.resize(samplesRead);
        return result;
    }

    static std::pair<std::vector<float>, std::vector<float>> loadStereoFromFile(const std::string& filename)
    {
        OggVorbis_File vf;
        if (ov_fopen(filename.c_str(), &vf) < 0)
        {
            return {{}, {}};
        }

        vorbis_info* vi = ov_info(&vf, -1);
        const auto totalSamples = static_cast<size_t>(ov_pcm_total(&vf, -1));

        std::vector<float> left(totalSamples);
        std::vector<float> right(totalSamples);

        int current_section;
        size_t samplesRead = 0;

        while (samplesRead < totalSamples)
        {
            float** pcm;
            long samples = ov_read_float(&vf, &pcm, 4096, &current_section);

            if (samples <= 0)
            {
                break;
            }

            for (long i = 0; i < samples && samplesRead < totalSamples; ++i)
            {
                left[samplesRead] = pcm[0][i];
                right[samplesRead] = vi->channels > 1 ? pcm[1][i] : pcm[0][i];
                ++samplesRead;
            }
        }

        ov_clear(&vf);
        left.resize(samplesRead);
        right.resize(samplesRead);
        return {left, right};
    }

    static bool loadStereoInterleavedFromFile(const std::string& filename, std::vector<float>& target,
                                              size_t padding = 2) // padding for resample interpolation
    {
        OggVorbis_File vf;
        if (ov_fopen(filename.c_str(), &vf) < 0)
        {
            return false;
        }

        vorbis_info* vi = ov_info(&vf, -1);
        if (vi->channels != 2)
        {
            ov_clear(&vf);
            auto r = loadMonoFromFile(filename);
            target.resize((r.size() + padding * 2) * 2);
            std::ranges::fill(target, 0.f);
            for (size_t i = 0; i < r.size(); ++i)
            {
                target[(padding + i) * 2] = r[i];
                target[(padding + i) * 2 + 1] = r[i];
            }

            return true;
        }

        const auto totalSamples = static_cast<size_t>(ov_pcm_total(&vf, -1));

        target.resize(totalSamples * 2);

        int current_section;
        size_t samplesRead = 0;

        while (samplesRead < totalSamples)
        {
            float** pcm;
            long samples = ov_read_float(&vf, &pcm, 4096, &current_section);

            if (samples <= 0)
            {
                break;
            }

            for (long i = 0; i < samples && samplesRead < totalSamples; ++i)
            {
                target[samplesRead * 2] = pcm[0][i];
                target[samplesRead * 2 + 1] = pcm[1][i];
                ++samplesRead;
            }
        }

        ov_clear(&vf);
        target.resize(samplesRead * 2);
        return true;
    }

    static int getSampleRate(const std::string& filename)
    {
        return getInfo(filename).sampleRate;
    }

    static int getNumChannels(const std::string& filename)
    {
        return getInfo(filename).channels;
    }

    static int64_t getNumSamplesPerChannel(const std::string& filename)
    {
        return getInfo(filename).numSamples;
    }
};

} // namespace AudioUtility
