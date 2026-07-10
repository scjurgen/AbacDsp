#pragma once
/*
 * AUTO GENERATED,
 * NOT A GOOD IDEA TO CHANGE STUFF HERE
 * Keep the file readonly
 */

#include <juce_audio_processors/juce_audio_processors.h>

#include "Analysis/EnvelopeFollower.h"
#include "Analysis/Spectrogram.h"
#include "Audio/FixedSizeProcessor.h"
#include "UiElements.h"
#include "impl/CcMapping.h"
#include "impl/CcSettings.h"
#include "impl/FileIo.h"
#include "impl/GainImpl.h"

class AudioPluginAudioProcessor
    : public juce::AudioProcessor,
      public juce::AudioProcessorValueTreeState::Listener {
public:
  static constexpr size_t NumSamplesPerBlock = 16;
  AudioPluginAudioProcessor()
      : AudioProcessor(
            BusesProperties()
#if !JucePlugin_IsMidiEffect
#if !JucePlugin_IsSynth
                .withInput("Input", juce::AudioChannelSet::stereo(), true)
#endif
                .withOutput("Output", juce::AudioChannelSet::stereo(), true)
#endif
                ),
        fixedRunner(
            [this](const AbacDsp::AudioBuffer<2, NumSamplesPerBlock> &input,
                   AbacDsp::AudioBuffer<2, NumSamplesPerBlock> &output) {
              pluginRunner->processBlock(input, output);
            }),
        m_parameters(*this, nullptr, "PARAMETERS", createParameterLayout()),
        m_envInput{AbacDsp::RmsFollower(10000), AbacDsp::RmsFollower(10000)},
        m_envOutput{AbacDsp::RmsFollower(10000), AbacDsp::RmsFollower(10000)},
        m_spectrogram{}, m_patchIndex(1, 0) {
    m_parameters.addParameterListener("subset", this);
    m_parameters.addParameterListener("gain", this);
    m_parameters.addParameterListener("lowShelving", this);
    m_parameters.addParameterListener("highShelving", this);
    m_parameters.addParameterListener("latency", this);

    m_fileIo.initialize(m_patchIndex);
  }
  ~AudioPluginAudioProcessor() override {
    m_parameters.removeParameterListener("subset", this);
    m_parameters.removeParameterListener("gain", this);
    m_parameters.removeParameterListener("lowShelving", this);
    m_parameters.removeParameterListener("highShelving", this);
    m_parameters.removeParameterListener("latency", this);
  }

  void prepareToPlay(const double sampleRate,
                     const int samplesPerBlock) override {
    pluginRunner = std::make_unique<GainImpl<NumSamplesPerBlock>>(
        static_cast<float>(sampleRate));
    m_sampleRate = static_cast<size_t>(sampleRate);
    for (auto *param : getParameters()) {
      if (auto *p = dynamic_cast<juce::RangedAudioParameter *>(param)) {
        const auto normalizedValue = p->getValue();
        p->sendValueChangedMessageToListeners(normalizedValue);
      }
    }

    juce::ignoreUnused(samplesPerBlock);
    m_fileIo.enable();
  }

  void releaseResources() override {
    std::cout << "releaseResources: Called on shutdown" << std::endl;

    if (m_fileIo.areParametersModified()) {
      std::cout << "releaseResources: Parameters modified, autosaving"
                << std::endl;
      m_fileIo.forceSave();
    }

    pluginRunner = nullptr;
  }

  bool isBusesLayoutSupported(const BusesLayout &layouts) const override {
#if JucePlugin_IsMidiEffect
    juce::ignoreUnused(layouts);
    return true;
#else
    /* This is the place where you check if the layout is supported.
     * In this template code we only support mono or stereo.
     */
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono() &&
        layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo()) {
      return false;
    }

    /* This checks if the input layout matches the output layout */
#if !JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet()) {
      return false;
    }
#endif
    return true;
#endif
  }

  juce::AudioProcessorEditor *createEditor() override;

  bool hasEditor() const override { return true; }

  const juce::String getName() const override { return JucePlugin_Name; }

  bool acceptsMidi() const override {
#if JucePlugin_WantsMidiInput
    return true;
#else
    return false;
#endif
  }

  bool producesMidi() const override {
#if JucePlugin_ProducesMidiOutput
    return true;
#else
    return false;
#endif
  }

  bool isMidiEffect() const override {
#if JucePlugin_IsMidiEffect
    return true;
#else
    return false;
#endif
  }

  double getTailLengthSeconds() const override { return 2.0; }

  int getNumPrograms() override {
    return 1;
    /* NB: some hosts don't cope very well if you tell them there are 0
       programs, so this should be at least 1, even if you're not really
       implementing programs.
    */
  }

  int getCurrentProgram() override { return 0; }

  void setCurrentProgram(const int index) override { m_program = index; }

  const juce::String getProgramName(const int index) override {
    switch (index) {
    case 0:
      return {"Program 0"};
    default:
      return {"Program unknown"};
    }
  }

  void changeProgramName(int index, const juce::String &newName) override {
    juce::ignoreUnused(index, newName);
  }

  void getStateInformation(juce::MemoryBlock &destData) override {
    auto state = m_parameters.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    if (xml != nullptr) {
      copyXmlToBinary(*xml, destData);
    }
  }

  void setStateInformation(const void *data, int sizeInBytes) override {
    std::unique_ptr xmlState(getXmlFromBinary(data, sizeInBytes));

    if (xmlState != nullptr) {
      if (xmlState->hasTagName(m_parameters.state.getType())) {
        // Hosts may call setStateInformation() from any thread, so
        // replaceState() (which touches editor-attached listeners) must hop to
        // the message thread rather than running here directly or being
        // deferred to some arbitrary future prepareToPlay(), which let a stale
        // restore clobber values set in between (observed via auval's
        // parameter-retention test).
        juce::ValueTree newState = juce::ValueTree::fromXml(*xmlState);
        juce::MessageManager::callAsync(
            [this, newState] { m_parameters.replaceState(newState); });
      }
    }
  }

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wimplicit-float-conversion"
  juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout() {
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID("subset", 1), "Subset",
        juce::StringArray{"#1", "#2", "#3", "#4", "#5"}, 0));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("gain", 1), "Gain",
        juce::NormalisableRange<float>(-60, 60, 0.1, 1, false), 0,
        juce::AudioParameterFloatAttributes{}
            .withLabel("dB")
            .withStringFromValueFunction([](float value, int) {
              return juce::String(value, 1) + " dB";
            })));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("lowShelving", 1), "Low",
        juce::NormalisableRange<float>(-24, 24, 0.1, 1, false), 0,
        juce::AudioParameterFloatAttributes{}
            .withLabel("dB")
            .withStringFromValueFunction([](float value, int) {
              return juce::String(value, 1) + " dB";
            })));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("highShelving", 1), "High",
        juce::NormalisableRange<float>(-24, 24, 0.1, 1, false), 0,
        juce::AudioParameterFloatAttributes{}
            .withLabel("dB")
            .withStringFromValueFunction([](float value, int) {
              return juce::String(value, 1) + " dB";
            })));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("latency", 1), "Latency",
        juce::NormalisableRange<float>(0, 100, 0.1, 0.25, false), 0,
        juce::AudioParameterFloatAttributes{}
            .withLabel("ms")
            .withStringFromValueFunction([](float value, int) {
              return juce::String(value, 1) + " ms";
            })));

    return {params.begin(), params.end()};
  }
#pragma GCC diagnostic pop

  void parameterChanged(const juce::String &parameterID,
                        float newValue) override {
    if (pluginRunner == nullptr) {
      return;
    }

    if (parameterID == "subset") {
      bool patchIndexChanged = false;
      if (parameterID == "subset") {
        const int newIdx = static_cast<int>(newValue);
        if (m_patchIndex[0] != newIdx) {
          m_patchIndex[0] = newIdx;
          patchIndexChanged = true;
        }
      }

      if (patchIndexChanged) {
        if (m_fileIo.areParametersModified()) {
          juce::NativeMessageBox::showAsync(
              juce::MessageBoxOptions()
                  .withIconType(juce::MessageBoxIconType::QuestionIcon)
                  .withTitle("Save Parameters")
                  .withMessage("Parameters have changed, do you want to save "
                               "before loading new patch?")
                  .withButton("Yes")
                  .withButton("No"),
              [this, patchIndex = m_patchIndex](int result) {
                // showAsync returns the plain index of the clicked button (0 =
                // "Yes", 1 = "No").
                handlePatchChange(patchIndex, result == 0);
              });
        } else {
          loadPatchDirect(m_patchIndex);
        }
      }
    }

    static const std::map<
        juce::String, std::function<void(AudioPluginAudioProcessor &, float)>>
        parameterMap{
            {"gain",
             [](AudioPluginAudioProcessor &p, const float v) {
               p.pluginRunner->setGain(v);
               p.m_fileIo.updateParameter(PatchParameters::Id::gain, v);
             }},
            {"lowShelving",
             [](AudioPluginAudioProcessor &p, const float v) {
               p.pluginRunner->setLowShelving(v);
               p.m_fileIo.updateParameter(PatchParameters::Id::lowShelving, v);
             }},
            {"highShelving",
             [](AudioPluginAudioProcessor &p, const float v) {
               p.pluginRunner->setHighShelving(v);
               p.m_fileIo.updateParameter(PatchParameters::Id::highShelving, v);
             }},
            {"latency",
             [](AudioPluginAudioProcessor &p, const float v) {
               p.pluginRunner->setLatency(v);
               p.m_fileIo.updateParameter(PatchParameters::Id::latency, v);
             }},

        };
    if (auto it = parameterMap.find(parameterID); it != parameterMap.end()) {
      it->second(*this, newValue);
    }
  }

  void handlePatchChange(const std::vector<int> &newPatchIndex,
                         bool shouldSave) {
    if (shouldSave) {
      m_fileIo.forceSave();
    }

    loadPatchDirect(newPatchIndex);
  }

  void loadPatchDirect(const std::vector<int> &patchIndex) {
    m_fileIo.loadPatchDirect(patchIndex);
    const auto &params = m_fileIo.getCurrentParameters();

    // Apply loaded parameters to APVTS (triggers UI update)
    if (auto *p = m_parameters.getParameter("gain")) {
      const auto &range = m_parameters.getParameterRange("gain");
      float normalized = range.convertTo0to1(params.gain);
      p->setValueNotifyingHost(normalized);
    }
    if (auto *p = m_parameters.getParameter("lowShelving")) {
      const auto &range = m_parameters.getParameterRange("lowShelving");
      float normalized = range.convertTo0to1(params.lowShelving);
      p->setValueNotifyingHost(normalized);
    }
    if (auto *p = m_parameters.getParameter("highShelving")) {
      const auto &range = m_parameters.getParameterRange("highShelving");
      float normalized = range.convertTo0to1(params.highShelving);
      p->setValueNotifyingHost(normalized);
    }
    if (auto *p = m_parameters.getParameter("latency")) {
      const auto &range = m_parameters.getParameterRange("latency");
      float normalized = range.convertTo0to1(params.latency);
      p->setValueNotifyingHost(normalized);
    }
  }

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-conversion"

  void processBlock(juce::AudioBuffer<float> &buffer,
                    juce::MidiBuffer &midiMessages) override {
    juce::ScopedNoDenormals noDenormals;

    if (!midiMessages.isEmpty()) {
      for (const auto &msg : midiMessages) {
        pluginRunner->processMidi(msg.data);
      }
    }
    for (int c = 0; c < std::min(2, buffer.getNumChannels()); ++c) {
      m_envInput[c].feed(
          std::span{buffer.getReadPointer(c),
                    static_cast<size_t>(buffer.getNumSamples())});
      m_inputDb[c].store(std::log10(m_envInput[c].getRms()) * 20.f);
    }
    if ((getTotalNumInputChannels() == 2) &&
        (getTotalNumOutputChannels() == 2)) {
      fixedRunner.processBlock(buffer);
    }
    for (int c = 0; c < std::min(2, buffer.getNumChannels()); ++c) {
      m_envOutput[c].feed(
          std::span{buffer.getReadPointer(c),
                    static_cast<size_t>(buffer.getNumSamples())});
      m_outputDb[c].store(std::log10(m_envOutput[c].getRms()) * 20.f);
    }
    m_spectrogram.processBlock(std::span{
        buffer.getReadPointer(0), static_cast<size_t>(buffer.getNumSamples())});
  }

#pragma GCC diagnostic pop

  [[nodiscard]] const std::vector<float> &getWaveDataToShow() {
    return pluginRunner->visualizeWaveData();
  }

  [[nodiscard]] std::pair<float, float> getInputDbLoad() const {
    return {m_inputDb[0].load(), m_inputDb[1].load()};
  }

  [[nodiscard]] std::pair<float, float> getOutputDbLoad() const {
    return {m_outputDb[0].load(), m_outputDb[1].load()};
  }
  [[nodiscard]] AbacDsp::SpectrumImageSet getSpectrogram() const {
    return m_spectrogram.getImageSet();
  }

  [[nodiscard]] bool hasRunner() const { return pluginRunner.get() != nullptr; }
  float m_maxValue{0.f};

private:
  size_t m_sampleRate{48000};

  static bool isChanged(const float a, const float b) {
    return std::abs(a - b) > 1E-8f;
  }

  int m_program{0};

  AbacDsp::FixedSizeProcessor<2, NumSamplesPerBlock, juce::AudioBuffer<float>>
      fixedRunner;
  std::unique_ptr<GainImpl<NumSamplesPerBlock>> pluginRunner;
  juce::AudioProcessorValueTreeState m_parameters;
  // VU-Meter
  std::atomic<float> m_inputDb[2];
  std::atomic<float> m_outputDb[2];
  std::array<AbacDsp::RmsFollower, 2> m_envInput;
  std::array<AbacDsp::RmsFollower, 2> m_envOutput;
  AbacDsp::SimpleSpectrogram m_spectrogram;
  std::vector<int> m_patchIndex;
  FileIo m_fileIo;
  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioPluginAudioProcessor)
};
