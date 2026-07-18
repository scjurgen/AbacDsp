#pragma once

#include "Analysis/Spectrogram.h"
#include "Audio/AudioBuffer.h"
#include "EffectBase.h"

#include <cassert>
#include <cmath>
#include <cstdint>
#include <functional>

template <size_t BlockSize> class GenericImpl final : public EffectBase {
public:
  explicit GenericImpl(const float sampleRate) : EffectBase(sampleRate) {}
  void setDry(const float value) { m_dry = std::pow(10.f, value / 20.f); }
  void setWet(const float value) { m_wet = std::pow(10.f, value / 20.f); }
  void setPreDelay(const float value) { m_preDelay = value; }
  void setElements(const float value) { m_elements = value; }
  void setFeedback(const float value) { m_feedback = value; }
  void setBulge(const float value) { m_bulge = value; }
  void setBottomSize(const float value) { m_bottomSize = value; }
  void setTopSize(const float value) { m_topSize = value; }
  void setModulationDepth(const float value) { m_modulationDepth = value; }
  void setModulationSpeed(const float value) { m_modulationSpeed = value; }
  void setLowPass(const float value) { m_lowPass = value; }
  void setMix(const float value) { m_mix = value; }
  void setPitch(const float value) { m_pitch = value; }
  void setPsola(const bool value) { m_psola = value; }
  void setFdnMix(const float value) { m_fdnMix = std::pow(10.f, value / 20.f); }
  void setFdnSize(const float value) { m_fdnSize = value; }
  void setFdnDecay(const float value) { m_fdnDecay = value; }

  void processBlock(const AbacDsp::AudioBuffer<2, BlockSize> &in,
                    AbacDsp::AudioBuffer<2, BlockSize> &out) {
    for (size_t i = 0; i < BlockSize; ++i) {
      out(i, 0) = in(i, 0);
      out(i, 1) = in(i, 1);
    }
  }

private:
  float m_dry{};
  float m_wet{};
  float m_preDelay{};
  float m_elements{};
  float m_feedback{};
  float m_bulge{};
  float m_bottomSize{};
  float m_topSize{};
  float m_modulationDepth{};
  float m_modulationSpeed{};
  float m_lowPass{};
  float m_mix{};
  float m_pitch{};
  bool m_psola{};
  float m_fdnMix{};
  float m_fdnSize{};
  float m_fdnDecay{};
};