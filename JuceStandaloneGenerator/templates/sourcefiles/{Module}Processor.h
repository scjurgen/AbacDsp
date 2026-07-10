#pragma once
/*
 * AUTO GENERATED,
 * NOT A GOOD IDEA TO CHANGE STUFF HERE
 * Keep the file readonly
 */

#include <juce_audio_processors/juce_audio_processors.h>

#include "/*INCLUDE_PEDAL*/"
#include "Analysis/EnvelopeFollower.h"
#include "Analysis/Spectrogram.h"
#include "Audio/FixedSizeProcessor.h"
#include "UiElements.h"
#include "impl/CcMapping.h"
#include "impl/CcSettings.h"
#include "impl/FileIo.h"

class AudioPluginAudioProcessor : public juce::AudioProcessor, public juce::AudioProcessorValueTreeState::Listener
{
  public:
    static constexpr size_t NumSamplesPerBlock = 16;
    AudioPluginAudioProcessor()
        : AudioProcessor(BusesProperties()
#if !JucePlugin_IsMidiEffect
#if !JucePlugin_IsSynth
                             .withInput("Input", juce::AudioChannelSet::stereo(), true)
#endif
                             .withOutput("Output", juce::AudioChannelSet::stereo(), true)
#endif
                             )
        , fixedRunner([this](const AbacDsp::AudioBuffer<2, NumSamplesPerBlock>& input,
                             AbacDsp::AudioBuffer<2, NumSamplesPerBlock>& output)
                      { pluginRunner->processBlock(input, output); })
        , m_parameters(*this, nullptr, "PARAMETERS", createParameterLayout())
        /*START_SHOWCPULOAD*/
        , m_avgCpu(8, 0)
        , m_head{0}
        , m_runningWindowCpu(8 * 300) /*END_SHOWCPULOAD*/
        /*START_SHOWVUMETER*/
        , m_envInput{AbacDsp::RmsFollower(10000), AbacDsp::RmsFollower(10000)}
        , m_envOutput{AbacDsp::RmsFollower(10000), AbacDsp::RmsFollower(10000)} /*END_SHOWVUMETER*/
                                                                                /*START_SHOWSPECTROGRAM*/
        , m_spectrogram{}                                                       /*END_SHOWSPECTROGRAM*/
        , m_patchIndex(/*PatchCount*/, 0)
    {
        /*ADD_PARAMETER_LISTENERS*/
        /*START_MIDICC*/
        for (size_t i = 0; i < /*NUM_CC_TARGETS*/; ++i)
        {
            m_ccActive[i].controller.store(kDefaultCcMappings[i].controller, std::memory_order_relaxed);
            m_ccActive[i].valueLow.store(kDefaultCcMappings[i].valueLow, std::memory_order_relaxed);
            m_ccActive[i].valueHigh.store(kDefaultCcMappings[i].valueHigh, std::memory_order_relaxed);
        }
        /*END_MIDICC*/
        m_fileIo.initialize(m_patchIndex);
    }
    ~AudioPluginAudioProcessor() override
    {
        /*REMOVE_PARAMETER_LISTENERS*/
    }

    void prepareToPlay(const double sampleRate, const int samplesPerBlock) override
    {
        pluginRunner = std::make_unique</*CLASS_NAME*/>(static_cast<float>(sampleRate));
        m_sampleRate = static_cast<size_t>(sampleRate);
        for (auto* param : getParameters())
        {
            if (auto* p = dynamic_cast<juce::RangedAudioParameter*>(param))
            {
                const auto normalizedValue = p->getValue();
                p->sendValueChangedMessageToListeners(normalizedValue);
            }
        }
        /*START_MIDICC*/
        for (const auto& entry : CcSettings::load())
        {
            for (size_t i = 0; i < /*NUM_CC_TARGETS*/; ++i)
            {
                if (kCcTargetParamIds[i] != entry.paramId)
                {
                    continue;
                }
                m_ccActive[i].controller.store(entry.controller, std::memory_order_relaxed);
                m_ccActive[i].valueLow.store(clampToParamRange(i, entry.valueLow), std::memory_order_relaxed);
                m_ccActive[i].valueHigh.store(clampToParamRange(i, entry.valueHigh), std::memory_order_relaxed);
            }
        }
        /*END_MIDICC*/

        juce::ignoreUnused(samplesPerBlock);
        m_fileIo.enable();
    }

    void releaseResources() override
    {
        std::cout << "releaseResources: Called on shutdown" << std::endl;

        if (m_fileIo.areParametersModified())
        {
            std::cout << "releaseResources: Parameters modified, autosaving" << std::endl;
            m_fileIo.forceSave();
        }

        pluginRunner = nullptr;
    }

    bool isBusesLayoutSupported(const BusesLayout& layouts) const override
    {
#if JucePlugin_IsMidiEffect
        juce::ignoreUnused(layouts);
        return true;
#else
        /* This is the place where you check if the layout is supported.
         * In this template code we only support mono or stereo.
         */
        if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono() &&
            layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        {
            return false;
        }

        /* This checks if the input layout matches the output layout */
#if !JucePlugin_IsSynth
        if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        {
            return false;
        }
#endif
        return true;
#endif
    }

    juce::AudioProcessorEditor* createEditor() override;

    bool hasEditor() const override
    {
        return true;
    }

    const juce::String getName() const override
    {
        return JucePlugin_Name;
    }

    bool acceptsMidi() const override
    {
#if JucePlugin_WantsMidiInput
        return true;
#else
        return false;
#endif
    }

    bool producesMidi() const override
    {
#if JucePlugin_ProducesMidiOutput
        return true;
#else
        return false;
#endif
    }

    bool isMidiEffect() const override
    {
#if JucePlugin_IsMidiEffect
        return true;
#else
        return false;
#endif
    }

    double getTailLengthSeconds() const override
    {
        return 2.0;
    }

    int getNumPrograms() override
    {
        return 1;
        /* NB: some hosts don't cope very well if you tell them there are 0 programs,
                   so this should be at least 1, even if you're not really implementing programs.
        */
    }

    int getCurrentProgram() override
    {
        return 0;
    }

    void setCurrentProgram(const int index) override
    {
        m_program = index;
    }

    const juce::String getProgramName(const int index) override
    {
        switch (index)
        {
            case 0:
                return {"Program 0"};
            default:
                return {"Program unknown"};
        }
    }

    void changeProgramName(int index, const juce::String& newName) override
    {
        juce::ignoreUnused(index, newName);
    }

    void getStateInformation(juce::MemoryBlock& destData) override
    {
        auto state = m_parameters.copyState();
        std::unique_ptr<juce::XmlElement> xml(state.createXml());
        if (xml != nullptr)
        {
            copyXmlToBinary(*xml, destData);
        }
    }

    void setStateInformation(const void* data, int sizeInBytes) override
    {
        std::unique_ptr xmlState(getXmlFromBinary(data, sizeInBytes));

        if (xmlState != nullptr)
        {
            if (xmlState->hasTagName(m_parameters.state.getType()))
            {
                // Hosts may call setStateInformation() from any thread, so replaceState()
                // (which touches editor-attached listeners) must hop to the message thread
                // rather than running here directly or being deferred to some arbitrary
                // future prepareToPlay(), which let a stale restore clobber values set in
                // between (observed via auval's parameter-retention test).
                juce::ValueTree newState = juce::ValueTree::fromXml(*xmlState);
                juce::MessageManager::callAsync([this, newState] { m_parameters.replaceState(newState); });
            }
        }
    }

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wimplicit-float-conversion"
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
    {
        std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;
        /*CREATE_PARAMETER_LAYOUT*/
        return {params.begin(), params.end()};
    }
#pragma GCC diagnostic pop

    void parameterChanged(const juce::String& parameterID, float newValue) override
    {
        if (pluginRunner == nullptr)
        {
            return;
        }


        /*START_PATCHSUPPORT*/
        if (/*PatchChanged*/)
        {
            bool patchIndexChanged = false;
            /*PatchIndexAssign*/

            if (patchIndexChanged)
            {
                if (m_fileIo.areParametersModified())
                {
                    juce::NativeMessageBox::showAsync(
                        juce::MessageBoxOptions()
                            .withIconType(juce::MessageBoxIconType::QuestionIcon)
                            .withTitle("Save Parameters")
                            .withMessage("Parameters have changed, do you want to save before loading new patch?")
                            .withButton("Yes")
                            .withButton("No"),
                        [this, patchIndex = m_patchIndex](int result)
                        {
                            // showAsync returns the plain index of the clicked button (0 = "Yes", 1 = "No").
                            handlePatchChange(patchIndex, result == 0);
                        });
                }
                else
                {
                    loadPatchDirect(m_patchIndex);
                }
            }
        }
        /*END_PATCHSUPPORT*/

        static const std::map<juce::String, std::function<void(AudioPluginAudioProcessor&, float)>> parameterMap{
            /*PARAMETER_CHANGED*/
        };
        if (auto it = parameterMap.find(parameterID); it != parameterMap.end())
        {
            it->second(*this, newValue);
        }
    }

    void handlePatchChange(const std::vector<int>& newPatchIndex, bool shouldSave)
    {
        if (shouldSave)
        {
            m_fileIo.forceSave();
        }

        loadPatchDirect(newPatchIndex);
    }

    void loadPatchDirect(const std::vector<int>& patchIndex)
    {
        m_fileIo.loadPatchDirect(patchIndex);
        applyLoadedParametersToHost();
    }

    // Pushes m_fileIo's currently loaded patch into the APVTS (triggers UI update); shared by
    // both the fixed-slot patch selector and any named-patch browser using m_fileIo directly.
    void applyLoadedParametersToHost()
    {
        const auto& params = m_fileIo.getCurrentParameters();
        /*LoadPatches*/
    }

    /*START_PRESETBROWSER*/
    [[nodiscard]] std::vector<juce::String> listPatchNames() const
    {
        std::vector<juce::String> result;
        for (const auto& n : m_fileIo.listPatchNames())
        {
            result.push_back(juce::String(n));
        }
        return result;
    }

    [[nodiscard]] juce::String getCurrentPatchName() const
    {
        return juce::String(m_fileIo.currentPatchName());
    }

    void requestLoadPatch(const juce::String& name)
    {
        if (m_fileIo.areParametersModified())
        {
            juce::NativeMessageBox::showAsync(
                juce::MessageBoxOptions()
                    .withIconType(juce::MessageBoxIconType::QuestionIcon)
                    .withTitle("Save Parameters")
                    .withMessage("Parameters have changed, do you want to save before loading this patch?")
                    .withButton("Yes")
                    .withButton("No"),
                [this, name](int result) { finishLoadNamedPatch(name, result == 0); });
        }
        else
        {
            finishLoadNamedPatch(name, false);
        }
    }

    void finishLoadNamedPatch(const juce::String& name, bool shouldSave)
    {
        if (shouldSave)
        {
            m_fileIo.forceSave();
        }
        if (m_fileIo.loadPatchNamed(name.toStdString()))
        {
            applyLoadedParametersToHost();
        }
    }

    bool saveCurrentPatchAs(const juce::String& name)
    {
        return m_fileIo.savePatchNamed(name.toStdString());
    }

    bool deletePatchNamed(const juce::String& name)
    {
        return m_fileIo.deletePatchNamed(name.toStdString());
    }
    /*END_PRESETBROWSER*/

    /*START_SHOWCPULOAD*/
    void computeCpuLoad(std::chrono::nanoseconds elapsed, size_t numSamples)
    {
        samplesProcessed += numSamples;
        elapsedTotalNanoSeconds += static_cast<size_t>(elapsed.count());
        constexpr float secondsPoll = 0.5f;
        if (samplesProcessed > m_sampleRate * secondsPoll)
        {
            const auto pRate = static_cast<float>(100.0 * static_cast<double>(elapsedTotalNanoSeconds) /
                                                  (secondsPoll * 1'000'000'000.0));
            m_runningWindowCpu += static_cast<size_t>(pRate * 100.f);
            m_runningWindowCpu -= m_avgCpu[m_head];
            m_avgCpu[m_head++] = static_cast<size_t>(pRate * 100.f);
            m_head = m_head % m_avgCpu.size();
            m_cpuLoad.store(m_runningWindowCpu * 0.01f / m_avgCpu.size());
            elapsedTotalNanoSeconds = 0;
            samplesProcessed = 0;
        }
    }
    /*END_SHOWCPULOAD*/

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-conversion"

    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) override
    {
        juce::ScopedNoDenormals noDenormals;
        /*START_SHOWCPULOAD*/
        const auto beginTime = std::chrono::high_resolution_clock::now();
        /*END_SHOWCPULOAD*/

        if (!midiMessages.isEmpty())
        {
            for (const auto& msg : midiMessages)
            {
                pluginRunner->processMidi(msg.data);
                /*START_MIDICC*/
                if ((msg.data[0] & 0xF0) == 0xB0)
                {
                    handleMidiCc(msg.data[1], msg.data[2]);
                }
                /*END_MIDICC*/
            }
        }
        /*START_SHOWVUMETER*/
        for (int c = 0; c < std::min(2, buffer.getNumChannels()); ++c)
        {
            m_envInput[c].feed(std::span{buffer.getReadPointer(c), static_cast<size_t>(buffer.getNumSamples())});
            m_inputDb[c].store(std::log10(m_envInput[c].getRms()) * 20.f);
        }
        /*END_SHOWVUMETER*/
        if ((getTotalNumInputChannels() == 2) && (getTotalNumOutputChannels() == 2))
        {
            fixedRunner.processBlock(buffer);
        }
        /*START_SHOWVUMETER*/
        for (int c = 0; c < std::min(2, buffer.getNumChannels()); ++c)
        {
            m_envOutput[c].feed(std::span{buffer.getReadPointer(c), static_cast<size_t>(buffer.getNumSamples())});
            m_outputDb[c].store(std::log10(m_envOutput[c].getRms()) * 20.f);
        }
        /*END_SHOWVUMETER*/
        /*START_SHOWSPECTROGRAM*/
        m_spectrogram.processBlock(std::span{buffer.getReadPointer(0), static_cast<size_t>(buffer.getNumSamples())});
        /*END_SHOWSPECTROGRAM*/
        /*START_SHOWCPULOAD*/
        const auto endTime = std::chrono::high_resolution_clock::now();
        computeCpuLoad(std::chrono::duration_cast<std::chrono::nanoseconds>(endTime - beginTime),
                       static_cast<size_t>(buffer.getNumSamples()));
        /*END_SHOWCPULOAD*/
    }

#pragma GCC diagnostic pop

    /*START_SHOWCPULOAD*/
    [[nodiscard]] float getCpuLoad() const
    {
        return m_cpuLoad.load();
    }
    /*END_SHOWCPULOAD*/

    /*START_SHOWWAVEFORM*/
    [[nodiscard]] const std::vector<float>& getWaveDataToShow()
    {
        return pluginRunner->visualizeWaveData();
    }
    /*END_SHOWWAVEFORM*/
    /*EXTRA_PROCESSOR_METHODS*/
    /*START_SHOWVUMETER*/
    [[nodiscard]] std::pair<float, float> getInputDbLoad() const
    {
        return {m_inputDb[0].load(), m_inputDb[1].load()};
    }

    [[nodiscard]] std::pair<float, float> getOutputDbLoad() const
    {
        return {m_outputDb[0].load(), m_outputDb[1].load()};
    }
    /*END_SHOWVUMETER*/
    /*START_SHOWSPECTROGRAM*/
    [[nodiscard]] AbacDsp::SpectrumImageSet getSpectrogram() const
    {
        return m_spectrogram.getImageSet();
    }
    /*END_SHOWSPECTROGRAM*/

    [[nodiscard]] bool hasRunner() const
    {
        return pluginRunner.get() != nullptr;
    }
    /*START_MIDICC*/
    void beginCcLearn(const CcTarget target) noexcept
    {
        m_learnTargetIndex.store(static_cast<int>(target), std::memory_order_relaxed);
    }

    [[nodiscard]] std::pair<float, float> getCcRange(const CcTarget target) const noexcept
    {
        const auto idx = static_cast<size_t>(target);
        return {m_ccActive[idx].valueLow.load(std::memory_order_relaxed),
                m_ccActive[idx].valueHigh.load(std::memory_order_relaxed)};
    }

    void setCcRange(const CcTarget target, const float lo, const float hi)
    {
        const auto idx = static_cast<size_t>(target);
        m_ccActive[idx].valueLow.store(clampToParamRange(idx, lo), std::memory_order_relaxed);
        m_ccActive[idx].valueHigh.store(clampToParamRange(idx, hi), std::memory_order_relaxed);
        saveCcSettings();
    }

    void clearCcAssignment(const CcTarget target)
    {
        m_ccActive[static_cast<size_t>(target)].controller.store(-1, std::memory_order_relaxed);
        saveCcSettings();
    }

    [[nodiscard]] int getCcController(const CcTarget target) const noexcept
    {
        return m_ccActive[static_cast<size_t>(target)].controller.load(std::memory_order_relaxed);
    }

    // Called from the message thread (Editor timer poll); safe to log/save here,
    // unlike inside handleMidiCc which runs on the audio thread.
    int consumeLastLearnedCc()
    {
        const auto idx = m_lastLearnedIndex.exchange(-1, std::memory_order_relaxed);
        if (idx >= 0)
        {
            std::cout << "MIDI CC learn: cc"
                      << m_ccActive[static_cast<size_t>(idx)].controller.load(std::memory_order_relaxed) << " -> "
                      << kCcTargetParamIds[static_cast<size_t>(idx)] << std::endl;
            saveCcSettings();
        }
        return idx;
    }
    /*END_MIDICC*/
    float m_maxValue{0.f};
    /*START_SHOWCPULOAD*/
    size_t elapsedTotalNanoSeconds{0};
    size_t samplesProcessed = 0;
    /*END_SHOWCPULOAD*/

  private:
    size_t m_sampleRate{48000};

    static bool isChanged(const float a, const float b)
    {
        return std::abs(a - b) > 1E-8f;
    }

    int m_program{0};

    AbacDsp::FixedSizeProcessor<2, NumSamplesPerBlock, juce::AudioBuffer<float>> fixedRunner;
    std::unique_ptr</*CLASS_NAME*/> pluginRunner;
    juce::AudioProcessorValueTreeState m_parameters;
    /*START_MIDICC*/
    struct CcSlot
    {
        std::atomic<int> controller{-1};
        std::atomic<float> valueLow{0.f};
        std::atomic<float> valueHigh{0.f};
    };
    std::array<CcSlot, /*NUM_CC_TARGETS*/> m_ccActive{};
    std::atomic<int> m_learnTargetIndex{-1};
    std::atomic<int> m_lastLearnedIndex{-1};

    [[nodiscard]] static float clampToParamRange(const size_t idx, const float value) noexcept
    {
        const auto& r = kCcTargetFullRange[idx];
        return std::clamp(value, r.lo, r.hi);
    }

    void saveCcSettings() const
    {
        std::vector<CcMappingOverride> overrides;
        overrides.reserve(/*NUM_CC_TARGETS*/);
        for (size_t i = 0; i < /*NUM_CC_TARGETS*/; ++i)
        {
            overrides.push_back({std::string(kCcTargetParamIds[i]),
                                 m_ccActive[i].controller.load(std::memory_order_relaxed),
                                 m_ccActive[i].valueLow.load(std::memory_order_relaxed),
                                 m_ccActive[i].valueHigh.load(std::memory_order_relaxed)});
        }
        CcSettings::save(overrides);
    }

    void handleMidiCc(const uint8_t controller, const uint8_t value7bit)
    {
        const auto learnIndex = m_learnTargetIndex.load(std::memory_order_relaxed);
        if (learnIndex >= 0)
        {
            m_ccActive[static_cast<size_t>(learnIndex)].controller.store(controller, std::memory_order_relaxed);
            m_learnTargetIndex.store(-1, std::memory_order_relaxed);
            m_lastLearnedIndex.store(learnIndex, std::memory_order_relaxed);
            return;
        }
        for (size_t i = 0; i < /*NUM_CC_TARGETS*/; ++i)
        {
            if (m_ccActive[i].controller.load(std::memory_order_relaxed) != controller)
            {
                continue;
            }
            const auto lo = m_ccActive[i].valueLow.load(std::memory_order_relaxed);
            const auto hi = m_ccActive[i].valueHigh.load(std::memory_order_relaxed);
            const auto raw = lo + (hi - lo) * (static_cast<float>(value7bit) / 127.f);
            if (auto* param =
                    m_parameters.getParameter(juce::String(kCcTargetParamIds[i].data(), kCcTargetParamIds[i].size())))
            {
                param->setValueNotifyingHost(param->convertTo0to1(raw));
            }
        }
    }
    /*END_MIDICC*/
    /*START_SHOWCPULOAD*/
    // CPU-Load
    std::atomic<float> m_cpuLoad;
    std::vector<size_t> m_avgCpu;
    size_t m_head{};
    size_t m_runningWindowCpu;
    /*END_SHOWCPULOAD*/
    /*START_SHOWVUMETER*/
    // VU-Meter
    std::atomic<float> m_inputDb[2];
    std::atomic<float> m_outputDb[2];
    std::array<AbacDsp::RmsFollower, 2> m_envInput;
    std::array<AbacDsp::RmsFollower, 2> m_envOutput;
    /*END_SHOWVUMETER*/
    /*START_SHOWSPECTROGRAM*/
    AbacDsp::SimpleSpectrogram m_spectrogram;
    /*END_SHOWSPECTROGRAM*/
    std::vector<int> m_patchIndex;
    FileIo m_fileIo;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioPluginAudioProcessor)
};
