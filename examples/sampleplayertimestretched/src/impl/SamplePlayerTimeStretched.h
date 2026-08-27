#pragma once

#include <array>
#include <cmath>
#include <memory>
#include <tuple>
#include <vector>

#include "AudioFile/LoadWav.h"

#include "Audio/AudioBuffer.h"
#include "EffectBase.h"
#include "Sampler/ResamplingPitchShifter.h"
#include "Sampler/StretchedSampleProducer.h"

template <size_t BlockSize>
class SamplePlayerTimeStretched final : public EffectBase
{
  public:
    explicit SamplePlayerTimeStretched(const float sampleRate)
        : EffectBase(sampleRate)
        , m_producer(sampleRate)
        , m_player(&m_producer, 4096)
    {
        std::array samples{"samples/drossel.wav", "samples/quena-c4.wav", "samples/hh_crash_foot_mono.WAV",
                           "samples/bassclarinet_c3.wav"};
        const auto loadInterleavedStereo = [](const char* filename)
        {
            const auto [left, right] = AudioUtility::LoadWav::loadStereoFromFile(filename);
            std::vector<float> interleaved(left.size() * 2);
            for (size_t s = 0; s < left.size(); ++s)
            {
                interleaved[s * 2] = left[s];
                interleaved[s * 2 + 1] = right[s];
            }
            return interleaved;
        };
        for (size_t i = 0; i < samples.size(); ++i)
        {
            m_sample[i] = std::make_shared<std::vector<float>>(loadInterleavedStereo(samples[i]));
        }
        m_producer.setRawStereoSample(m_sample[m_currentSampleBuffer]);
    }

    void setVol(const float value)
    {
        m_vol = std::pow(10.f, value / 20.f);
    }

    void setType(const size_t value)
    {
        m_type = value;
        m_currentSampleBuffer = value;
        m_producer.setRawStereoSample(m_sample[value]);
    }

    void setPosition(const float value)
    {
        m_position = value * 0.01f;
        const auto pos =
            static_cast<size_t>(m_position * static_cast<float>(m_sample[m_currentSampleBuffer]->size()) / 2.f);
        m_producer.setPosition(pos, false);
    }

    void setAdvance(const float value)
    {
        m_advance = value;
        m_producer.setFeedRate(m_advance);
    }

    void processBlock([[maybe_unused]] const AbacDsp::AudioBuffer<2, BlockSize>& in,
                      AbacDsp::AudioBuffer<2, BlockSize>& out)
    {
        if (m_producer.isDone())
        {
            m_producer.setPosition(
                static_cast<size_t>(m_position * static_cast<float>(m_sample[m_currentSampleBuffer]->size()) / 2.f),
                true);
        }
        std::array<std::array<float, BlockSize>, 2> tmp;
        std::ignore = m_player.produceSamples(tmp[0].data(), tmp[1].data(), BlockSize);
        for (size_t i = 0; i < BlockSize; ++i)
        {
            out(i, 0) = tmp[0][i] * m_vol;
            out(i, 1) = tmp[1][i] * m_vol;
        }
    }

  private:
    float m_vol{};
    size_t m_type{};
    float m_position{};
    float m_advance{};
    std::array<std::shared_ptr<std::vector<float>>, 4> m_sample{};
    size_t m_currentSampleBuffer{0};
    AbacDsp::StretchedSampleProducer m_producer;
    AbacDsp::ResamplingPitchShifter<AbacDsp::StretchedSampleProducer> m_player;
};
