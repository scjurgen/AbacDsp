#pragma once

/*
 * For audio applications, the consensus strongly favors Hermite-based methods.
 */
#include <future>
#include <string>
#include <thread>
#include <vector>

#include "AudioFile.h"

class AudiofileIO
{
  public:
    struct Stereo
    {
        float left;
        float right;
    };

    static size_t loadStereoWaveFile(const std::string& filename, float* target, size_t maxSize)
    {
        if (AudioFile<float> af; af.load(filename))
        {
            const size_t idxLeft = af.isStereo() ? 1 : 0;
            const auto mSize = std::min(static_cast<size_t>(af.getNumSamplesPerChannel()), maxSize / 2);
            for (size_t i = 0; i < mSize; ++i)
            {
                target[i * 2] = std::clamp(af.samples[0][i], -1.f, 1.f);
                target[i * 2 + 1] = std::clamp(af.samples[idxLeft][i], -1.f, 1.f);
            }
            return mSize;
        }
        return 0;
    }

    static size_t loadStereoWaveFileAlloc(const std::string& filename, std::vector<float>& target)
    {
        AudioFile<float> af;
        if (af.load(filename))
        {
            const size_t idxLeft = af.isStereo() ? 1 : 0;
            target.resize(static_cast<size_t>(af.getNumSamplesPerChannel()) * 2, 0.f);
            for (size_t i = 0; i < static_cast<size_t>(af.getNumSamplesPerChannel()); ++i)
            {
                target[i * 2] = std::clamp(af.samples[0][i], -1.f, 1.f);
                target[i * 2 + 1] = std::clamp(af.samples[idxLeft][i], -1.f, 1.f);
            }
            return target.size();
        }
        return 0;
    }
};


template <size_t MaxVoices>
class SamplePlayer
{
  public:
    void triggerVoice(unsigned /*channel*/, const float* data, const size_t size, const float gain)
    {
        for (size_t i = 0; i < MaxVoices; ++i)
        {
            if (m_voices[i].isDone())
            {
                m_voices[i].run(data, size, gain);
                m_activeVoice[i] = m_steadyTime;
                m_activeVoiceData[i] = data;
                return;
            }
        }
        // find same dataEntry
        for (size_t i = 0; i < MaxVoices; ++i)
        {
            if (m_activeVoiceData[i] == data)
            {
                m_voices[i].run(data, size, gain);
                m_activeVoiceData[i] = data;
                m_activeVoice[i] = m_steadyTime;
                return;
            }
        }

        // throw out oldest
        size_t lowest = m_activeVoice[0];
        size_t index = 0;
        for (size_t i = 1; i < MaxVoices; ++i)
        {
            if (lowest > m_activeVoice[i])
            {
                index = i;
                lowest = m_activeVoice[i];
            }
        }
        m_voices[index].run(data, size, gain);
        m_activeVoiceData[index] = data;
        m_activeVoice[index] = m_steadyTime;
    }

    void processBlock(float* targetLeft, float* targetRight, size_t numSamples)
    {
        m_steadyTime++;
        for (size_t i = 0; i < MaxVoices; ++i)
        {
            m_voices[i].processBlock(targetLeft, targetRight, numSamples);
            if (m_voices[i].isDone())
            {
                m_activeVoice[i] = 0;
            }
        }
    }
    size_t m_steadyTime{0};
    std::array<size_t, MaxVoices> m_activeVoice{};
    std::array<const float*, MaxVoices> m_activeVoiceData{};
    std::array<BasicSamplePlayer, MaxVoices> m_voices{};
};


template <size_t BASENOTE, size_t MIDI_STEPS, size_t ROUND_ROBINS>
class Instrument
{
  public:
    explicit Instrument(const std::string& basefilepath)
        : m_basefilepath(basefilepath)
        , m_loadingThread{}
        , m_loadingComplete(false)
    {
        if (ROUND_ROBINS > 1)
        {
            roundRobinShuffle.resize(9377, 0);
            std::generate(roundRobinShuffle.begin(), roundRobinShuffle.end(),
                          []()
                          {
                              return static_cast<unsigned>(rand()) % ROUND_ROBINS; // NOLINT
                          });
        }
        else
        {
            roundRobinShuffle.push_back(0);
        }
    }

    ~Instrument()
    {
        if (m_loadingThread.joinable())
        {
            m_loadingThread.join();
        }
    }

    void load(const std::function<void()>& callback)
    {
        m_loadingThread = std::thread(&Instrument::loadBank, this, callback);
    }

    void loadBank(const std::function<void()>& callback)
    {
        for (size_t i = 0; i < MIDI_STEPS * ROUND_ROBINS; ++i)
        {
            char sampleFileName[256];
            snprintf(sampleFileName, sizeof(sampleFileName), "%s/S_%05d.WAV", m_basefilepath.c_str(),
                     static_cast<int>(i + 1));
            if (AudiofileIO::loadStereoWaveFileAlloc(sampleFileName, m_sample[i]))
            {
                //   print_log("loaded %s %ld", sampleFileName, m_sample[i].size() / 2);
            }
            else
            {
                //     print_log("could not load %s", sampleFileName);
            }
        }
        m_loadingComplete = true;
        // print_log("loading completed");
        callback();
    }

    std::vector<float>* getSample(unsigned midiHeight)
    {
        if (!m_loadingComplete)
        {
            return &empty;
        }
        if (midiHeight < m_baseNote)
        {
            return &empty;
        }
        auto pIndex = (midiHeight - m_baseNote) * ROUND_ROBINS + roundRobinShuffle[m_roundRobin++];
        m_roundRobin = m_roundRobin % roundRobinShuffle.size();
        if (pIndex >= m_sample.size())
        {
            return &empty;
        }
        return &m_sample[pIndex];
    }
    size_t m_roundRobin{0};

    std::vector<float> empty{};

    std::array<std::vector<float>, MIDI_STEPS * ROUND_ROBINS> m_sample{};
    size_t m_baseNote = BASENOTE;
    std::vector<unsigned> roundRobinShuffle{};
    std::string m_basefilepath;
    std::thread m_loadingThread;
    std::atomic<bool> m_loadingComplete;
};