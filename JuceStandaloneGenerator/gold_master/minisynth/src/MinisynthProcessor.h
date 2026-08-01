#pragma once
/*
 * AUTO GENERATED,
 * NOT A GOOD IDEA TO CHANGE STUFF HERE
 * Keep the file readonly
 */

#include <juce_audio_processors/juce_audio_processors.h>

#include "Analysis/EnvelopeFollower.h"
#include "Analysis/Spectrogram.h"
#include "SamplerateConverter/InternalRateNormalizingProcessor.h"
#include "UiElements.h"
#include "impl/CcMapping.h"
#include "impl/CcSettings.h"
#include "impl/FileIo.h"
#include "pedals/MiniSynthPedal.h"

class AudioPluginAudioProcessor : public juce::AudioProcessor, public juce::AudioProcessorValueTreeState::Listener
{
  public:
    static constexpr size_t NumSamplesPerBlock = 16;
    using RateNormalizer = AbacDsp::InternalRateNormalizingProcessor<2, NumSamplesPerBlock, juce::AudioBuffer<float>>;

    AudioPluginAudioProcessor()
        : AudioProcessor(BusesProperties()
#if !JucePlugin_IsMidiEffect
#if !JucePlugin_IsSynth
                             .withInput("Input", juce::AudioChannelSet::stereo(), true)
#endif
                             .withOutput("Output", juce::AudioChannelSet::stereo(), true)
#endif
                             )
        , m_parameters(*this, nullptr, "PARAMETERS", createParameterLayout())
        , m_avgCpu(8, 0)
        , m_head{0}
        , m_runningWindowCpu(8 * 300)
        , m_envInput{AbacDsp::RmsFollower(10000), AbacDsp::RmsFollower(10000)}
        , m_envOutput{AbacDsp::RmsFollower(10000), AbacDsp::RmsFollower(10000)}
        , m_spectrogram{}
        , m_patchIndex(0, 0)
    {
        m_parameters.addParameterListener("vol", this);
        m_parameters.addParameterListener("user1", this);
        m_parameters.addParameterListener("user2", this);
        m_parameters.addParameterListener("user3", this);
        m_parameters.addParameterListener("user4", this);
        m_parameters.addParameterListener("user5", this);
        m_parameters.addParameterListener("user6", this);
        m_parameters.addParameterListener("user7", this);
        m_parameters.addParameterListener("user8", this);
        m_parameters.addParameterListener("user9", this);
        m_parameters.addParameterListener("user10", this);
        m_parameters.addParameterListener("user11", this);
        m_parameters.addParameterListener("user12", this);

        m_fileIo.initialize(m_patchIndex);
    }
    ~AudioPluginAudioProcessor() override
    {
        m_parameters.removeParameterListener("vol", this);
        m_parameters.removeParameterListener("user1", this);
        m_parameters.removeParameterListener("user2", this);
        m_parameters.removeParameterListener("user3", this);
        m_parameters.removeParameterListener("user4", this);
        m_parameters.removeParameterListener("user5", this);
        m_parameters.removeParameterListener("user6", this);
        m_parameters.removeParameterListener("user7", this);
        m_parameters.removeParameterListener("user8", this);
        m_parameters.removeParameterListener("user9", this);
        m_parameters.removeParameterListener("user10", this);
        m_parameters.removeParameterListener("user11", this);
        m_parameters.removeParameterListener("user12", this);
    }

    void prepareToPlay(const double sampleRate, const int samplesPerBlock) override
    {
        pluginRunner = std::make_unique<MiniSynthPedal<NumSamplesPerBlock>>(RateNormalizer::kInternalSampleRate);

        fixedRunner = std::make_unique<RateNormalizer>(static_cast<float>(sampleRate),
                                                       [this](const AbacDsp::AudioBuffer<2, NumSamplesPerBlock>& input,
                                                              AbacDsp::AudioBuffer<2, NumSamplesPerBlock>& output)
                                                       { pluginRunner->processBlock(input, output); });
        m_sampleRate = static_cast<size_t>(sampleRate);
        for (auto* param : getParameters())
        {
            if (auto* p = dynamic_cast<juce::RangedAudioParameter*>(param))
            {
                // APVTS suppresses this as a no-change re-send, so call directly.
                parameterChanged(p->paramID, p->convertFrom0to1(p->getValue()));
            }
        }

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
        fixedRunner = nullptr;
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
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("vol", 1), "Vol", juce::NormalisableRange<float>(-100, 12, 0.1, 1, false), 0,
            juce::AudioParameterFloatAttributes{}.withLabel("dB").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 1) + " dB"; })));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("user1", 1), "Ctrl 1", juce::NormalisableRange<float>(0, 100, 1, 1, false), 0,
            juce::AudioParameterFloatAttributes{}.withLabel("%").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 0) + " %"; })));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("user2", 1), "Ctrl 2", juce::NormalisableRange<float>(0, 100, 1, 1, false), 0,
            juce::AudioParameterFloatAttributes{}.withLabel("%").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 0) + " %"; })));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("user3", 1), "Ctrl 3", juce::NormalisableRange<float>(0, 100, 1, 1, false), 0,
            juce::AudioParameterFloatAttributes{}.withLabel("%").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 0) + " %"; })));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("user4", 1), "Ctrl 4", juce::NormalisableRange<float>(0, 100, 1, 1, false), 0,
            juce::AudioParameterFloatAttributes{}.withLabel("%").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 0) + " %"; })));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("user5", 1), "Ctrl 5", juce::NormalisableRange<float>(0, 100, 1, 1, false), 0,
            juce::AudioParameterFloatAttributes{}.withLabel("%").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 0) + " %"; })));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("user6", 1), "Ctrl 6", juce::NormalisableRange<float>(0, 100, 1, 1, false), 0,
            juce::AudioParameterFloatAttributes{}.withLabel("%").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 0) + " %"; })));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("user7", 1), "Ctrl 7", juce::NormalisableRange<float>(0, 100, 1, 1, false), 0,
            juce::AudioParameterFloatAttributes{}.withLabel("%").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 0) + " %"; })));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("user8", 1), "Ctrl 8", juce::NormalisableRange<float>(0, 100, 1, 1, false), 0,
            juce::AudioParameterFloatAttributes{}.withLabel("%").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 0) + " %"; })));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("user9", 1), "Ctrl 9", juce::NormalisableRange<float>(0, 100, 1, 1, false), 0,
            juce::AudioParameterFloatAttributes{}.withLabel("%").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 0) + " %"; })));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("user10", 1), "Ctrl 10", juce::NormalisableRange<float>(0, 100, 1, 1, false), 0,
            juce::AudioParameterFloatAttributes{}.withLabel("%").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 0) + " %"; })));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("user11", 1), "Ctrl 11", juce::NormalisableRange<float>(0, 100, 1, 1, false), 0,
            juce::AudioParameterFloatAttributes{}.withLabel("%").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 0) + " %"; })));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("user12", 1), "Ctrl 12", juce::NormalisableRange<float>(0, 100, 1, 1, false), 0,
            juce::AudioParameterFloatAttributes{}.withLabel("%").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 0) + " %"; })));

        return {params.begin(), params.end()};
    }
#pragma GCC diagnostic pop

    void parameterChanged(const juce::String& parameterID, float newValue) override
    {
        if (pluginRunner == nullptr)
        {
            return;
        }


        static const std::map<juce::String, std::function<void(AudioPluginAudioProcessor&, float)>> parameterMap{
            {"vol",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setVol(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::vol, v);
             }},
            {"user1",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setUser1(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::user1, v);
             }},
            {"user2",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setUser2(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::user2, v);
             }},
            {"user3",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setUser3(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::user3, v);
             }},
            {"user4",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setUser4(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::user4, v);
             }},
            {"user5",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setUser5(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::user5, v);
             }},
            {"user6",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setUser6(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::user6, v);
             }},
            {"user7",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setUser7(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::user7, v);
             }},
            {"user8",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setUser8(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::user8, v);
             }},
            {"user9",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setUser9(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::user9, v);
             }},
            {"user10",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setUser10(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::user10, v);
             }},
            {"user11",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setUser11(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::user11, v);
             }},
            {"user12",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setUser12(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::user12, v);
             }},

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
        if (auto* p = m_parameters.getParameter("vol"))
        {
            const auto& range = m_parameters.getParameterRange("vol");
            float normalized = range.convertTo0to1(params.vol);
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("user1"))
        {
            const auto& range = m_parameters.getParameterRange("user1");
            float normalized = range.convertTo0to1(params.user1);
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("user2"))
        {
            const auto& range = m_parameters.getParameterRange("user2");
            float normalized = range.convertTo0to1(params.user2);
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("user3"))
        {
            const auto& range = m_parameters.getParameterRange("user3");
            float normalized = range.convertTo0to1(params.user3);
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("user4"))
        {
            const auto& range = m_parameters.getParameterRange("user4");
            float normalized = range.convertTo0to1(params.user4);
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("user5"))
        {
            const auto& range = m_parameters.getParameterRange("user5");
            float normalized = range.convertTo0to1(params.user5);
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("user6"))
        {
            const auto& range = m_parameters.getParameterRange("user6");
            float normalized = range.convertTo0to1(params.user6);
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("user7"))
        {
            const auto& range = m_parameters.getParameterRange("user7");
            float normalized = range.convertTo0to1(params.user7);
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("user8"))
        {
            const auto& range = m_parameters.getParameterRange("user8");
            float normalized = range.convertTo0to1(params.user8);
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("user9"))
        {
            const auto& range = m_parameters.getParameterRange("user9");
            float normalized = range.convertTo0to1(params.user9);
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("user10"))
        {
            const auto& range = m_parameters.getParameterRange("user10");
            float normalized = range.convertTo0to1(params.user10);
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("user11"))
        {
            const auto& range = m_parameters.getParameterRange("user11");
            float normalized = range.convertTo0to1(params.user11);
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("user12"))
        {
            const auto& range = m_parameters.getParameterRange("user12");
            float normalized = range.convertTo0to1(params.user12);
            p->setValueNotifyingHost(normalized);
        }
    }

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

    bool renamePatch(const juce::String& oldName, const juce::String& newName)
    {
        return m_fileIo.renamePatchNamed(oldName.toStdString(), newName.toStdString());
    }

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

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-conversion"

    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) override
    {
        juce::ScopedNoDenormals noDenormals;
        const auto beginTime = std::chrono::high_resolution_clock::now();

        if (!midiMessages.isEmpty())
        {
            for (const auto& msg : midiMessages)
            {
                pluginRunner->processMidi(msg.data);
            }
        }
        for (int c = 0; c < std::min(2, buffer.getNumChannels()); ++c)
        {
            m_envInput[c].feed(std::span{buffer.getReadPointer(c), static_cast<size_t>(buffer.getNumSamples())});
            m_inputDb[c].store(std::log10(m_envInput[c].getRms()) * 20.f);
        }
        if ((getTotalNumInputChannels() == 2) && (getTotalNumOutputChannels() == 2))
        {
            fixedRunner->processBlock(buffer);
        }
        for (int c = 0; c < std::min(2, buffer.getNumChannels()); ++c)
        {
            m_envOutput[c].feed(std::span{buffer.getReadPointer(c), static_cast<size_t>(buffer.getNumSamples())});
            m_outputDb[c].store(std::log10(m_envOutput[c].getRms()) * 20.f);
        }
        m_spectrogram.processBlock(std::span{buffer.getReadPointer(0), static_cast<size_t>(buffer.getNumSamples())});
        const auto endTime = std::chrono::high_resolution_clock::now();
        computeCpuLoad(std::chrono::duration_cast<std::chrono::nanoseconds>(endTime - beginTime),
                       static_cast<size_t>(buffer.getNumSamples()));
    }

#pragma GCC diagnostic pop

    [[nodiscard]] float getCpuLoad() const
    {
        return m_cpuLoad.load();
    }


    [[nodiscard]] std::pair<float, float> getInputDbLoad() const
    {
        return {m_inputDb[0].load(), m_inputDb[1].load()};
    }

    [[nodiscard]] std::pair<float, float> getOutputDbLoad() const
    {
        return {m_outputDb[0].load(), m_outputDb[1].load()};
    }
    [[nodiscard]] AbacDsp::SpectrumImageSet getSpectrogram() const
    {
        return m_spectrogram.getImageSet();
    }

    [[nodiscard]] bool hasRunner() const
    {
        return pluginRunner.get() != nullptr;
    }
    float m_maxValue{0.f};
    size_t elapsedTotalNanoSeconds{0};
    size_t samplesProcessed = 0;

  private:
    size_t m_sampleRate{48000};

    static bool isChanged(const float a, const float b)
    {
        return std::abs(a - b) > 1E-8f;
    }

    int m_program{0};

    std::unique_ptr<RateNormalizer> fixedRunner;
    std::unique_ptr<MiniSynthPedal<NumSamplesPerBlock>> pluginRunner;

    juce::AudioProcessorValueTreeState m_parameters;
    // CPU-Load
    std::atomic<float> m_cpuLoad;
    std::vector<size_t> m_avgCpu;
    size_t m_head{};
    size_t m_runningWindowCpu;
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
