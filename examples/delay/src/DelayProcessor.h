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
#include "impl/DelayImpl.h"
#include "impl/FileIo.h"

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
        m_parameters.addParameterListener("gain", this);
        m_parameters.addParameterListener("dry", this);
        m_parameters.addParameterListener("wet", this);
        m_parameters.addParameterListener("timeInMs", this);
        m_parameters.addParameterListener("hostSync", this);
        m_parameters.addParameterListener("syncDivision", this);
        m_parameters.addParameterListener("feedback", this);
        m_parameters.addParameterListener("lowPass", this);
        m_parameters.addParameterListener("highPass", this);
        m_parameters.addParameterListener("allPass", this);
        m_parameters.addParameterListener("modDepth", this);
        m_parameters.addParameterListener("modSpeed", this);

        m_fileIo.initialize(m_patchIndex);
    }
    ~AudioPluginAudioProcessor() override
    {
        m_parameters.removeParameterListener("gain", this);
        m_parameters.removeParameterListener("dry", this);
        m_parameters.removeParameterListener("wet", this);
        m_parameters.removeParameterListener("timeInMs", this);
        m_parameters.removeParameterListener("hostSync", this);
        m_parameters.removeParameterListener("syncDivision", this);
        m_parameters.removeParameterListener("feedback", this);
        m_parameters.removeParameterListener("lowPass", this);
        m_parameters.removeParameterListener("highPass", this);
        m_parameters.removeParameterListener("allPass", this);
        m_parameters.removeParameterListener("modDepth", this);
        m_parameters.removeParameterListener("modSpeed", this);
    }

    void prepareToPlay(const double sampleRate, const int samplesPerBlock) override
    {
        pluginRunner = std::make_unique<DelayImpl<NumSamplesPerBlock>>(RateNormalizer::kInternalSampleRate);
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
            juce::ParameterID("gain", 1), "Gain", juce::NormalisableRange<float>(-60, 60, 0.1, 1, false), 0,
            juce::AudioParameterFloatAttributes{}.withLabel("dB").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 1) + " dB"; })));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("dry", 1), "Dry", juce::NormalisableRange<float>(-100, 12, 0.1, 1, false), 0,
            juce::AudioParameterFloatAttributes{}.withLabel("dB").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 1) + " dB"; })));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("wet", 1), "Wet", juce::NormalisableRange<float>(-100, 12, 0.1, 1, false), 0,
            juce::AudioParameterFloatAttributes{}.withLabel("dB").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 1) + " dB"; })));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("timeInMs", 1), "Time", juce::NormalisableRange<float>(1, 10000, 0.1, 0.3, false), 200,
            juce::AudioParameterFloatAttributes{}.withLabel("ms").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 1) + " ms"; })));
        params.push_back(std::make_unique<juce::AudioParameterBool>(juce::ParameterID("hostSync", 1), "Host Sync", 0));
        params.push_back(std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID("syncDivision", 1), "Sync Division",
            juce::StringArray{"1/1", "1/2", "1/2.", "1/2T", "1/4", "1/4.", "1/4T", "1/8", "1/8.", "1/8T", "1/16",
                              "1/16.", "1/16T"},
            4));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("feedback", 1), "Feedback", juce::NormalisableRange<float>(-100, 100, 0.1, 1, false), 0,
            juce::AudioParameterFloatAttributes{}.withLabel("%").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 1) + " %"; })));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("lowPass", 1), "Low pass cutoff",
            juce::NormalisableRange<float>(100, 20000, 1, 0.3, false), 12000,
            juce::AudioParameterFloatAttributes{}.withLabel("Hz").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 0) + " Hz"; })));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("highPass", 1), "High pass cutoff",
            juce::NormalisableRange<float>(20, 20000, 1, 0.3, false), 50,
            juce::AudioParameterFloatAttributes{}.withLabel("Hz").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 0) + " Hz"; })));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("allPass", 1), "All pass cutoff",
            juce::NormalisableRange<float>(100, 20000, 1, 0.3, false), 1500,
            juce::AudioParameterFloatAttributes{}.withLabel("Hz").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 0) + " Hz"; })));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("modDepth", 1), "Modulation depth", juce::NormalisableRange<float>(0, 100, 0.1, 1, false),
            0,
            juce::AudioParameterFloatAttributes{}.withLabel("%").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 1) + " %"; })));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("modSpeed", 1), "Modulation speed",
            juce::NormalisableRange<float>(0.05, 20, 0.1, 0.3, false), 0.25,
            juce::AudioParameterFloatAttributes{}.withLabel("Hz").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 1) + " Hz"; })));

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
            {"gain",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setGain(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::gain, v);
             }},
            {"dry",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setDry(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::dry, v);
             }},
            {"wet",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setWet(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::wet, v);
             }},
            {"timeInMs",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setTimeInMs(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::timeInMs, v);
             }},
            {"hostSync",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setHostSync(static_cast<bool>(v));
                 p.m_fileIo.updateParameter(PatchParameters::Id::hostSync, v);
             }},
            {"syncDivision",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setSyncDivision(static_cast<int>(v));
                 p.m_fileIo.updateParameter(PatchParameters::Id::syncDivision, v);
             }},
            {"feedback",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setFeedback(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::feedback, v);
             }},
            {"lowPass",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setLowPass(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::lowPass, v);
             }},
            {"highPass",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setHighPass(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::highPass, v);
             }},
            {"allPass",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setAllPass(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::allPass, v);
             }},
            {"modDepth",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setModDepth(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::modDepth, v);
             }},
            {"modSpeed",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setModSpeed(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::modSpeed, v);
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
        if (auto* p = m_parameters.getParameter("gain"))
        {
            const auto& range = m_parameters.getParameterRange("gain");
            float normalized = range.convertTo0to1(params.gain);
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("dry"))
        {
            const auto& range = m_parameters.getParameterRange("dry");
            float normalized = range.convertTo0to1(params.dry);
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("wet"))
        {
            const auto& range = m_parameters.getParameterRange("wet");
            float normalized = range.convertTo0to1(params.wet);
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("timeInMs"))
        {
            const auto& range = m_parameters.getParameterRange("timeInMs");
            float normalized = range.convertTo0to1(params.timeInMs);
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("hostSync"))
        {
            const auto& range = m_parameters.getParameterRange("hostSync");
            float normalized = range.convertTo0to1(params.hostSync);
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("syncDivision"))
        {
            const auto& range = m_parameters.getParameterRange("syncDivision");
            float normalized = range.convertTo0to1(params.syncDivision);
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("feedback"))
        {
            const auto& range = m_parameters.getParameterRange("feedback");
            float normalized = range.convertTo0to1(params.feedback);
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("lowPass"))
        {
            const auto& range = m_parameters.getParameterRange("lowPass");
            float normalized = range.convertTo0to1(params.lowPass);
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("highPass"))
        {
            const auto& range = m_parameters.getParameterRange("highPass");
            float normalized = range.convertTo0to1(params.highPass);
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("allPass"))
        {
            const auto& range = m_parameters.getParameterRange("allPass");
            float normalized = range.convertTo0to1(params.allPass);
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("modDepth"))
        {
            const auto& range = m_parameters.getParameterRange("modDepth");
            float normalized = range.convertTo0to1(params.modDepth);
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("modSpeed"))
        {
            const auto& range = m_parameters.getParameterRange("modSpeed");
            float normalized = range.convertTo0to1(params.modSpeed);
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
        if (auto* playHead = getPlayHead())
        {
            if (const auto position = playHead->getPosition())
            {
                auto transport = pluginRunner->hostTransport();
                ++transport.updateCount;
                transport.isPlaying = position->getIsPlaying();
                if (const auto bpm = position->getBpm())
                {
                    transport.bpm = *bpm;
                }
                if (const auto ppq = position->getPpqPosition())
                {
                    transport.ppqPosition = *ppq;
                }
                if (const auto timeSig = position->getTimeSignature())
                {
                    transport.beatsPerBar = static_cast<float>(timeSig->numerator);
                }
                pluginRunner->setHostTransport(transport);
            }
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

    [[nodiscard]] const std::vector<float>& getWaveDataToShow()
    {
        return pluginRunner->visualizeWaveData();
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
    std::unique_ptr<DelayImpl<NumSamplesPerBlock>> pluginRunner;
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
