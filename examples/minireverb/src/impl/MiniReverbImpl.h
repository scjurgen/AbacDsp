#pragma once

#include "Audio/AudioBuffer.h"
#include "BlockProcessors/BlockProcHighpass.h"
#include "BlockProcessors/BlockProcLowpass.h"
#include "BlockProcessors/BlockProcPitch.h"
#include "BlockProcessors/BlockProcVibrato.h"
#include "EffectBase.h"
#include "Parameters/SmoothingParameter.h"
#include "Reverbs/FdnTankBlockDelayWalshSIMD.h"
#include "Reverbs/FdnTankSpiced.h"
#include "Reverbs/FdnTankSpicedBase.h"

template <size_t BlockSize>
class MiniReverbImpl final : public EffectBase
{
  public:
    static constexpr size_t ORDER{32};

    using Highpass = AbacDsp::BlockProc::Highpass<BlockSize>;
    using Lowpass = AbacDsp::BlockProc::Lowpass<BlockSize>;
    using Pitcher = AbacDsp::BlockProc::Pitch<BlockSize>;
    using Vibrato = AbacDsp::BlockProc::Vibrato<BlockSize>;

    explicit MiniReverbImpl(const float sampleRate)
        : EffectBase(sampleRate)
        , m_fdnTank{sampleRate}
    {
        size_t indexCallback = 0;
        for (size_t i = 0; i < m_lp.size(); ++i, ++indexCallback)
        {
            m_lp[i] = std::make_shared<Lowpass>(sampleRate);
            m_lp[i]->setCutoff(22050.f);
            m_fdnTank.setDelayCallback(indexCallback, m_lp[i]);
        }
        for (size_t i = 0; i < m_hp.size(); ++i, ++indexCallback)
        {
            m_hp[i] = std::make_shared<Highpass>(sampleRate);
            m_hp[i]->setCutoff(12.f);
            m_fdnTank.setDelayCallback(indexCallback, m_hp[i]);
        }
        for (size_t i = 0; i < m_vib.size(); ++i, ++indexCallback)
        {
            m_vib[i] = std::make_shared<Vibrato>(sampleRate);
            m_vib[i]->setModDepth(0.f);
            m_vib[i]->setModSpeed(3.f + i);
            m_fdnTank.setDelayCallback(indexCallback, m_vib[i]);
        }

        for (size_t i = 0; i < m_pdl.size(); ++i, ++indexCallback)
        {
            m_pdl[i] = std::make_shared<Pitcher>(sampleRate);
            m_fdnTank.setDelayCallback(indexCallback, m_pdl[i]);
        }
        setPitch1Inplace(12);
        setPitch2Inplace(12);
        setReversePitch(true);
    }


    void setOrder(const size_t value)
    {
        const std::vector<size_t> orderMap{4, 8, 12, 16, 20, 24, 32, 48, 64};
        m_order = value;
        // m_fdnTank.setOrder(orderMap[value]);
    }

    void setDry(const float value)
    {
        m_dry.newTransition(std::pow(10.f, value / 20.f), kParamSmoothingSeconds, sampleRate());
    }

    void setWet(const float value)
    {
        m_wet.newTransition(std::pow(10.f, value / 20.f), kParamSmoothingSeconds, sampleRate());
    }

    void setStereoWidth(const float value)
    {
        // m_width = value;
    }

    void setBaseSize(const float value)
    {
        m_baseSize = value;
        m_fdnTank.setMinSize(value);
        m_fdnTank.setMaxSize(m_baseSize * m_sizeFactor);
    }

    void setSizeFactor(const float value)
    {
        m_sizeFactor = value;
        m_fdnTank.setMaxSize(m_baseSize * m_sizeFactor);
    }

    void setUniqueDelay(const bool value)
    {
        m_fdnTank.setUniqueDelay(value);
    }

    void setBulge(const float value)
    {
        m_fdnTank.setSpreadBulge(value);
    }

    void setDecay(const float value)
    {
        m_fdnTank.setDecay(value);
    }

    void setAllPassUp(const float value)
    {
        // m_fdnTank.setAllpassFirstCutoff(value);
    }
    void setAllPassDown(const float value)
    {
        // m_fdnTank.setAllpassLastCutoff(value);
    }
    void setLowPass(const float value)
    {
        for (auto& lp : m_lp)
        {
            lp->setCutoff(value);
        }
        // m_fdnTank.setLowpass(value);
    }
    void setHighPass(const float value)
    {
        // m_fdnTank.setHighpass(value);
    }

    void setModulationCount(const size_t value)
    {
        // m_fdnTank.setModulationCount(value);
    }

    void setLowPassCount(const float value)
    {
        // m_fdnTank.setLowPassCount(value);
    }

    void setHighPassCount(const float value)
    {
        // m_fdnTank.setHighPassCount(value);
    }

    void setModulationDepth(const float value)
    {
        // m_fdnTank.setModulationDepth(value);
    }

    void setModulationSpeed(const float value)
    {
        // m_fdnTank.setModulationSpeed(value);
    }

    void setReversePitch(const bool value)
    {
        for (auto& p : m_pdl)
        {
            p->setReverse(value);
        }
    }

    void setPitchStrength(const float value)
    {
        for (auto& p : m_pdl)
        {
            p->setPitchMix(value);
        }
    }

    void setPitch1Inplace(const float semiTones)
    {
        m_pdl[0]->setPitch(semiTones);
    }

    void setPitch2Inplace(const float semiTones)
    {
        m_pdl[1]->setPitch(semiTones);
    }

    void processBlock(const AbacDsp::AudioBuffer<2, BlockSize>& in, AbacDsp::AudioBuffer<2, BlockSize>& out)
    {
        std::array<float, BlockSize> inData{};
        for (size_t i = 0; i < BlockSize; ++i)
        {
            inData[i] = 0.03125 * (in(i, 0) + in(i, 1)); // factor is 1/ORDER
        }
        std::array<std::array<float, BlockSize>, 2> res{};

        m_fdnTank.processBlockSplit(inData.data(), res[0].data(), res[1].data());
        for (size_t i = 0; i < BlockSize; ++i)
        {
            const float dry = m_dry.getValue();
            const float wet = m_wet.getValue();
            for (size_t c = 0; c < 2; ++c)
            {
                out(i, c) = dry * in(i, c) + wet * res[c][i];
            }
        }
    }

  private:
    static constexpr float kParamSmoothingSeconds{0.01f};
    bool m_order{};
    AbacDsp::LinearSmoothing m_dry{0.f};
    AbacDsp::LinearSmoothing m_wet{0.f};
    float m_baseSize{};
    float m_sizeFactor{};
    AbacDsp::FdnTankSpiced<100000, ORDER, BlockSize> m_fdnTank;
    std::array<std::shared_ptr<Highpass>, 2> m_hp;
    std::array<std::shared_ptr<Lowpass>, 2> m_lp;
    std::array<std::shared_ptr<Pitcher>, 2> m_pdl;
    std::array<std::shared_ptr<Vibrato>, 2> m_vib;
};