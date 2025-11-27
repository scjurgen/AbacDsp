#pragma once

#include "Reverbs/FdnTankSpiced.h"
#include "BlockProcessors/BlockProcHighpass.h"
#include "BlockProcessors/BlockProcLowpass.h"
#include "BlockProcessors/BlockProcPitch.h"


template <size_t BlockSize>
class AmbientReverb final
{
  public:
    static constexpr size_t ORDER{32};

    using Highpass = AbacDsp::BlockProc::Highpass<BlockSize>;
    using Lowpass = AbacDsp::BlockProc::Lowpass<BlockSize>;
    using Pitcher = AbacDsp::BlockProc::Pitch<BlockSize>;

    explicit AmbientReverb(const float sampleRate)
        : m_fdnTank{sampleRate}
    {
        size_t indexCallback = 0;
        for (size_t i = 0; i < m_lp.size(); ++i, ++indexCallback)
        {
            m_lp[i] = std::make_shared<Lowpass>(sampleRate);
            m_lp[i]->setCutoff(20000.f);
            m_fdnTank.setDelayCallback(indexCallback, m_lp[i]);
        }
        for (size_t i = 0; i < m_hp.size(); ++i, ++indexCallback)
        {
            m_hp[i] = std::make_shared<Highpass>(sampleRate);
            m_hp[i]->setCutoff(22.f);
            m_fdnTank.setDelayCallback(indexCallback, m_hp[i]);
        }
        for (size_t i = 0; i < m_pdl.size(); ++i, ++indexCallback)
        {
            m_pdl[i] = std::make_shared<Pitcher>(sampleRate);
            m_fdnTank.setDelayCallback(indexCallback, m_pdl[i]);
        }
        setPitch1Inplace(12);
        setPitch2Inplace(-12);
        setReversePitch(true);
    }

    void setDry(const float value)
    {
        m_dry = value;
    }

    void setWet(const float value)
    {
        m_wet = value;
    }

    void setShimmer(const float v)
    {
        for (auto& pdl : m_pdl)
        {
            pdl->setPitchMix(v);
        }
    }

    void setBaseSize(const float valueMeters)
    {
        m_baseSize = valueMeters;
        m_fdnTank.setMinSize(valueMeters);
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
        m_pdl[2]->setPitch(semiTones);
    }

    void setPitch2Inplace(const float semiTones)
    {
        m_pdl[1]->setPitch(semiTones);
        m_pdl[3]->setPitch(semiTones);
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
            for (size_t c = 0; c < 2; ++c)
            {
                out(i, c) = m_dry * in(i, c) + m_wet * res[c][i];
            }
        }
    }

  private:
    bool m_order{};
    float m_dry{};
    float m_wet{};
    float m_baseSize{};
    float m_sizeFactor{};
    AbacDsp::FdnTankSpiced<100000, ORDER, BlockSize> m_fdnTank;
    std::array<std::shared_ptr<Highpass>, 2> m_hp;
    std::array<std::shared_ptr<Lowpass>, 2> m_lp;
    std::array<std::shared_ptr<Pitcher>, 4> m_pdl;
};