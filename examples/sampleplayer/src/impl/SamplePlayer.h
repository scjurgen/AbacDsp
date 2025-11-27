#pragma once

#include "EffectBase.h"
#include "Analysis/Spectrogram.h"
#include "Audio/AudioBuffer.h"
#include "AudioFile/LoadOgg.h"
#include "Helpers/ConstructArray.h"
#include "SamplerateConverter/ConvertSampleBuffer.h"
#include "Sampler/SamplePlayerBasic.h"
#include "Sampler/StretchSamplePlayer.h"
#include "AmbientReverb.h"

#include <nlohmann/json.hpp>
#include <fstream>

#include <cassert>
#include <cstdint>

#include <chrono>
#include <cmath>
#include <functional>
#include <atomic>
#include <thread>
#include <mutex>

static std::string baseFolder{"/Users/scjurgen/projects/"};

static std::vector<std::string> sets{"PITCHONLY", "DOLPHINS", "WIND",      "RAINFOREST", "FOREST",
                                     "RAIN",      "THUNDER",  "TANPURA",   "STREAM",     "OCEAN",
                                     "BOWLS",     "CAFE",     "RPGFOREST", "WHITEBURSTS"};

template <size_t BlockSize>
class SamplePlayer final : public EffectBase
{
  public:
    struct SubSet
    {
        float vol{};
        float revWet{};
        float revDecay{};
        float revShimmer{};
        std::array<float, 10> semitones{};
        std::array<float, 10> chanLevel{};
        std::array<float, 10> revFeed{};
    } m_setting;

    explicit SamplePlayer(const float sampleRate)
        : EffectBase(sampleRate)
        , m_smplPlayer(AbacDsp::constructArray<AbacDsp::SamplePlayerBasic, 10>(sampleRate))
        , m_rev(sampleRate)
    {
        m_rev.setBaseSize(3.1);
        m_rev.setBulge(-0.4f);
        m_rev.setSizeFactor(10.f);
        m_rev.setDry(0.f);
        m_rev.setWet(1.f);
        m_rev.setDecay(2000.f);
        loadSets(sets[0]);
    }

    ~SamplePlayer() override
    {
        m_shouldTerminate.store(true);
        if (m_loadThread.joinable())
        {
            m_loadThread.join();
        }
    }

    void saveSubSet(const std::string& jsonFileName)
    {
        nlohmann::json j;
        j["vol"] = m_setting.vol;
        j["revWet"] = m_setting.revWet;
        j["revDecay"] = m_setting.revDecay;
        j["revShimmer"] = m_setting.revShimmer;
        j["semitones"] = m_setting.semitones;
        j["chanLevel"] = m_setting.chanLevel;
        j["revFeed"] = m_setting.revFeed;

        if (std::ofstream file(jsonFileName); file.is_open())
        {
            file << j.dump(4);
            std::cout << "Saved: " << jsonFileName << "\n";
        }
        else
        {
            std::cerr << "Error saving: " << jsonFileName << "\n";
        }
    }

    void loadSubSet(const std::string& jsonFileName)
    {
        if (std::ifstream file(jsonFileName); file.is_open())
        {
            nlohmann::json j;
            file >> j;

            m_setting.vol = j.value("vol", 1.0f);
            m_setting.revWet = j.value("revWet", 0.0f);
            m_setting.revDecay = j.value("revDecay", 1000.0f);
            m_setting.revShimmer = j.value("revShimmer", 0.0f);
            m_setting.semitones = j.value("semitones", std::array<float, 10>{});
            m_setting.chanLevel = j.value("chanLevel", std::array<float, 10>{1, 1, 1, 1, 1, 1, 1, 1, 1, 1});
            m_setting.revFeed = j.value("revFeed", std::array<float, 10>{});

            m_rev.setDecay(m_setting.revDecay);
            m_rev.setShimmer(m_setting.revShimmer);
            for (size_t i = 0; i < m_setting.semitones.size(); ++i)
            {
                m_smplPlayer[i].setPlaybackRate(std::pow(2.f, m_setting.semitones[i] / 12.f));
            }
            std::cout << "Loaded: " << jsonFileName << "\n";
            return;
        }
        std::cerr << "Error loading: " << jsonFileName << "\n";
    }

    void loadSets(const std::string& setName)
    {
        m_shouldTerminate.store(true);
        if (m_loadThread.joinable())
        {
            m_loadThread.join();
        }

        m_shouldTerminate.store(false);
        m_loadThread = std::thread(
            [this, setName]()
            {
                std::array<std::shared_ptr<std::vector<float>>, 20> tempSamples;

                loadSet(tempSamples, 0, setName, 'a');
                if (m_shouldTerminate.load())
                {
                    return;
                }
                loadSet(tempSamples, 10, setName, 'b');
                if (m_shouldTerminate.load())
                {
                    return;
                }
                m_samples.store(tempSamples);
            });
    }

    void loadSet(std::array<std::shared_ptr<std::vector<float>>, 20>& tempSamples, const size_t baseIdx,
                 const std::string& folder, const char letter)
    {
        for (size_t i = 0; i < 10; ++i)
        {
            if (m_shouldTerminate.load())
            {
                return;
            }

            std::stringstream ss;
            ss << baseFolder << "mynoise/" << folder << "/" << i << letter << ".ogg";
            loadSample(tempSamples, baseIdx, baseIdx + i, ss.str());
        }
    }

    void loadSample(std::array<std::shared_ptr<std::vector<float>>, 20>& tempSamples, const size_t baseIdx,
                    const size_t index, const std::string& fname)
    {
        const auto t0 = std::chrono::high_resolution_clock::now();

        if (std::vector<float> sampleTmp; AudioUtility::LoadOgg::loadStereoInterleavedFromFile(fname, sampleTmp))
        {
            if (m_shouldTerminate.load())
            {
                return;
            }

            const auto t1 = std::chrono::high_resolution_clock::now();
            const auto loadTime = std::chrono::duration<double, std::milli>(t1 - t0).count();

            std::vector<float> sampleTmpSRC;
            const auto ratio = sampleRate() / static_cast<float>(AudioUtility::LoadOgg::getSampleRate(fname));
            if (ratio != 1.f)
            {
                AbacDsp::ConvertSampleBuffer::convert<2>(ratio, sampleTmp, sampleTmpSRC);

                if (m_shouldTerminate.load())
                {
                    return;
                }

                const auto t2 = std::chrono::high_resolution_clock::now();
                const auto resampleTime = std::chrono::duration<double, std::milli>(t2 - t1).count();

                tempSamples[index] = std::make_shared<std::vector<float>>(sampleTmpSRC);
                std::cout << "Loaded " << sampleTmpSRC.size() / 2 / sampleRate() << " seconds from " << fname
                          << " (load: " << loadTime << " ms, resample: " << resampleTime << " ms)\n";
            }
            else
            {
                tempSamples[index] = std::make_shared<std::vector<float>>(sampleTmp);
                std::cout << "Loaded " << sampleTmp.size() / 2 / sampleRate() << " seconds from " << fname
                          << " (load: " << loadTime << " ms)\n";
            }

            if (baseIdx == 0 && tempSamples[index] != nullptr)
            {
                m_smplPlayer[index].runStereo(tempSamples[index]);
                m_smplPlayer[index].setLoop(false);
                m_smplPlayer[index].restart();
            }
        }
        else
        {
            std::cout << "couldn't read " << fname << "\n";
        }
    }


    void setType(const size_t idx)
    {
        loadSets(sets[idx]);
    }

    [[nodiscard]] std::string getSetName() const
    {
        std::stringstream ss;
        ss << baseFolder << "/mynoise/subset." << m_index << "." << m_subSet << ".json";
        return ss.str();
    }

    void setSubset(const size_t index)
    {
        saveSubSet(getSetName());
        m_subSet = index;
        loadSubSet(getSetName());
    }

    void setSync(const bool syncNext)
    {
        m_syncNext = syncNext;
    }

    void setVol(const float valueDb)
    {
        m_setting.vol = std::pow(10.f, valueDb / 20.f);
    }

    void setReverbLevelWet(const float valueDb)
    {
        m_setting.revWet = std::pow(10.f, valueDb / 20.f);
    }

    void setReverbShimmer(const float value)
    {
        m_rev.setShimmer(value / 100.f);
    }

    void setReverbDecay(const float timeMs)
    {
        m_rev.setDecay(timeMs);
    }

    void setChannelLevel(const size_t index, const float valueDb)
    {
        m_setting.chanLevel[index] = valueDb <= -99.f ? 0 : std::pow(10.f, valueDb / 20.f);
    }

    void setSolo(size_t channel)
    {
        m_soloChannel = channel;
    }

    void setVol1(const float value)
    {
        setChannelLevel(0, value);
    }

    void setVol2(const float value)
    {
        setChannelLevel(1, value);
    }

    void setVol3(const float value)
    {
        setChannelLevel(2, value);
    }

    void setVol4(const float value)
    {
        setChannelLevel(3, value);
    }

    void setVol5(const float value)
    {
        setChannelLevel(4, value);
    }

    void setVol6(const float value)
    {
        setChannelLevel(5, value);
    }

    void setVol7(const float value)
    {
        setChannelLevel(6, value);
    }

    void setVol8(const float value)
    {
        setChannelLevel(7, value);
    }

    void setVol9(const float value)
    {
        setChannelLevel(8, value);
    }

    void setVol10(const float value)
    {
        setChannelLevel(9, value);
    }
    void setRevFeed(size_t idx, const float valueDb)
    {
        m_setting.revFeed[idx] = std::pow(10.f, valueDb / 20.f);
    }

    void setRevFeed1(const float valueDb)
    {
        setRevFeed(0, valueDb);
    }
    void setRevFeed2(const float valueDb)
    {
        setRevFeed(1, valueDb);
    }
    void setRevFeed3(const float valueDb)
    {
        setRevFeed(2, valueDb);
    }
    void setRevFeed4(const float valueDb)
    {
        setRevFeed(3, valueDb);
    }
    void setRevFeed5(const float valueDb)
    {
        setRevFeed(4, valueDb);
    }
    void setRevFeed6(const float valueDb)
    {
        setRevFeed(5, valueDb);
    }
    void setRevFeed7(const float valueDb)
    {
        setRevFeed(6, valueDb);
    }
    void setRevFeed8(const float valueDb)
    {
        setRevFeed(7, valueDb);
    }
    void setRevFeed9(const float valueDb)
    {
        setRevFeed(8, valueDb);
    }
    void setRevFeed10(const float valueDb)
    {
        setRevFeed(9, valueDb);
    }
    void setPitch(size_t index, const float semitones)
    {
        m_setting.semitones[index] = semitones;
        m_smplPlayer[index].setPlaybackRate(std::pow(2.f, semitones / 12.f));
    }

    void setPitch1(const float semitones)
    {
        setPitch(0, semitones);
    }
    void setPitch2(const float semitones)
    {
        setPitch(1, semitones);
    }
    void setPitch3(const float semitones)
    {
        setPitch(2, semitones);
    }
    void setPitch4(const float semitones)
    {
        setPitch(3, semitones);
    }
    void setPitch5(const float semitones)
    {
        setPitch(4, semitones);
    }
    void setPitch6(const float semitones)
    {
        setPitch(5, semitones);
    }
    void setPitch7(const float semitones)
    {
        setPitch(6, semitones);
    }
    void setPitch8(const float semitones)
    {
        setPitch(7, semitones);
    }
    void setPitch9(const float semitones)
    {
        setPitch(8, semitones);
    }
    void setPitch10(const float semitones)
    {
        setPitch(9, semitones);
    }
    void checkDone()
    {
        auto samples = m_samples.load();
        if (m_syncNext)
        {
            if (m_smplPlayer[0].isDone())
            {
                for (size_t i = 0; i < m_smplPlayer.size(); ++i)
                {
                    auto idx = 10 * (rand() % 2); // NOLINT
                    if (samples[i + idx] != nullptr)
                    {
                        m_smplPlayer[i].runStereo(samples[i + idx]);
                        m_smplPlayer[i].setLoop(false);
                        m_smplPlayer[i].restart();
                    }
                }
            }
        }
        else
        {
            for (size_t i = 0; i < m_smplPlayer.size(); ++i)
            {
                if (m_smplPlayer[i].isDone())
                {
                    auto idx = 10 * (rand() % 2); // NOLINT
                    if (samples[i + idx] != nullptr)
                    {
                        m_smplPlayer[i].runStereo(samples[i + idx]);
                        m_smplPlayer[i].setLoop(false);
                        m_smplPlayer[i].restart();
                    }
                }
            }
        }
    }
    void processBlock(const AbacDsp::AudioBuffer<2, BlockSize>& in, AbacDsp::AudioBuffer<2, BlockSize>& out)
    {
        std::array<std::array<float, BlockSize>, 2> sum{};
        AbacDsp::AudioBuffer<2, BlockSize> revSum{};
        AbacDsp::AudioBuffer<2, BlockSize> revWet{};

        for (size_t c = 0; c < m_smplPlayer.size(); ++c)
        {
            if (m_setting.chanLevel[c] < 0.0001f)
            {
                continue;
            }
            std::array<std::array<float, BlockSize>, 2> tmp{};
            m_smplPlayer[c].processBlock(tmp[0].data(), tmp[1].data(), BlockSize);
            for (size_t i = 0; i < BlockSize; ++i)
            {
                const auto chanGain = m_soloChannel == 0 ? m_setting.chanLevel[c]
                                                         : (m_soloChannel == c + 1 ? m_setting.chanLevel[c] : 0.f);
                sum[0][i] += tmp[0][i] * chanGain;
                sum[1][i] += tmp[1][i] * chanGain;
                const auto revGain = m_soloChannel == 0 ? 1.f : (m_soloChannel == c + 1 ? 1.f : 0.f);
                revSum(i, 0) += tmp[0][i] * m_setting.revFeed[c] * revGain;
                revSum(i, 1) += tmp[1][i] * m_setting.revFeed[c] * revGain;
            }
        }
        for (size_t i = 0; i < BlockSize; ++i)
        {
            out(i, 0) = sum[0][i] * m_setting.vol;
            out(i, 1) = sum[1][i] * m_setting.vol;
        }

        m_rev.processBlock(revSum, revWet);
        for (size_t i = 0; i < BlockSize; ++i)
        {
            out(i, 0) = sum[0][i] * m_setting.vol + revWet[i][0] * m_setting.revWet;
            out(i, 1) = sum[1][i] * m_setting.vol + revWet[i][1] * m_setting.revWet;
        }
        checkDone();
    }

  private:
    struct AtomicSampleArray
    {
        AtomicSampleArray() = default;

        AtomicSampleArray(const AtomicSampleArray&) = delete;
        AtomicSampleArray& operator=(const AtomicSampleArray&) = delete;
        void store(const std::array<std::shared_ptr<std::vector<float>>, 20>& samples)
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_data = samples;
        }

        std::array<std::shared_ptr<std::vector<float>>, 20> load() const
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            return m_data;
        }

      private:
        mutable std::mutex m_mutex;
        std::array<std::shared_ptr<std::vector<float>>, 20> m_data;
    };

    size_t m_index{0};
    size_t m_subSet{0};
    size_t m_soloChannel{};

    std::vector<float> m_sample;

    AtomicSampleArray m_samples;
    std::array<AbacDsp::SamplePlayerBasic, 10> m_smplPlayer;
    AmbientReverb<BlockSize> m_rev;
    std::thread m_loadThread;
    std::atomic<bool> m_shouldTerminate{false};
    bool m_syncNext{false};
};
