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
#include "impl/ResonikImpl.h"

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
        m_parameters.addParameterListener("numChains", this);
        m_parameters.addParameterListener("dry", this);
        m_parameters.addParameterListener("wet", this);
        m_parameters.addParameterListener("lowFreq", this);
        m_parameters.addParameterListener("highFreq", this);
        m_parameters.addParameterListener("distribution", this);
        m_parameters.addParameterListener("decayMin", this);
        m_parameters.addParameterListener("decayMax", this);
        m_parameters.addParameterListener("gainMin", this);
        m_parameters.addParameterListener("gainMax", this);
        m_parameters.addParameterListener("delayMin", this);
        m_parameters.addParameterListener("delayMax", this);
        m_parameters.addParameterListener("q", this);

        m_fileIo.initialize(m_patchIndex);
    }
    ~AudioPluginAudioProcessor() override
    {
        m_parameters.removeParameterListener("numChains", this);
        m_parameters.removeParameterListener("dry", this);
        m_parameters.removeParameterListener("wet", this);
        m_parameters.removeParameterListener("lowFreq", this);
        m_parameters.removeParameterListener("highFreq", this);
        m_parameters.removeParameterListener("distribution", this);
        m_parameters.removeParameterListener("decayMin", this);
        m_parameters.removeParameterListener("decayMax", this);
        m_parameters.removeParameterListener("gainMin", this);
        m_parameters.removeParameterListener("gainMax", this);
        m_parameters.removeParameterListener("delayMin", this);
        m_parameters.removeParameterListener("delayMax", this);
        m_parameters.removeParameterListener("q", this);
    }

    void prepareToPlay(const double sampleRate, const int samplesPerBlock) override
    {
        pluginRunner = std::make_unique<ResonikImpl<NumSamplesPerBlock>>(RateNormalizer::kInternalSampleRate);
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
            juce::ParameterID("numChains", 1), "Chains", juce::NormalisableRange<float>(1, 100, 1, 0.5, false), 48,
            juce::AudioParameterFloatAttributes{}.withLabel("").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 0) + " "; })));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("dry", 1), "Dry", juce::NormalisableRange<float>(-100, 12, 0.1, 1, false), 0,
            juce::AudioParameterFloatAttributes{}.withLabel("dB").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 1) + " dB"; })));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("wet", 1), "Wet", juce::NormalisableRange<float>(-100, 12, 0.1, 1, false), -6,
            juce::AudioParameterFloatAttributes{}.withLabel("dB").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 1) + " dB"; })));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("lowFreq", 1), "Low Freq", juce::NormalisableRange<float>(20, 20000, 1, 0.5, false), 80,
            juce::AudioParameterFloatAttributes{}.withLabel("Hz").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 0) + " Hz"; })));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("highFreq", 1), "High Freq", juce::NormalisableRange<float>(20, 20000, 1, 0.5, false),
            6000,
            juce::AudioParameterFloatAttributes{}.withLabel("Hz").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 0) + " Hz"; })));
        params.push_back(std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID("distribution", 1), "Distribution", juce::StringArray{"Linear", "Logarithmic"}, 1));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("decayMin", 1), "Decay Min", juce::NormalisableRange<float>(0.01, 20, 0.001, 0.3, false),
            0.3,
            juce::AudioParameterFloatAttributes{}.withLabel("s").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 2) + " s"; })));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("decayMax", 1), "Decay Max", juce::NormalisableRange<float>(0.01, 20, 0.001, 0.3, false),
            4.0,
            juce::AudioParameterFloatAttributes{}.withLabel("s").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 2) + " s"; })));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("gainMin", 1), "Gain Min", juce::NormalisableRange<float>(-60, 12, 0.1, 1, false), -18,
            juce::AudioParameterFloatAttributes{}.withLabel("dB").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 1) + " dB"; })));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("gainMax", 1), "Gain Max", juce::NormalisableRange<float>(-60, 12, 0.1, 1, false), 0,
            juce::AudioParameterFloatAttributes{}.withLabel("dB").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 1) + " dB"; })));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("delayMin", 1), "Delay Min", juce::NormalisableRange<float>(0, 2000, 1, 0.4, false), 0,
            juce::AudioParameterFloatAttributes{}.withLabel("ms").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 0) + " ms"; })));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("delayMax", 1), "Delay Max", juce::NormalisableRange<float>(0, 2000, 1, 0.4, false), 300,
            juce::AudioParameterFloatAttributes{}.withLabel("ms").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 0) + " ms"; })));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("q", 1), "Q", juce::NormalisableRange<float>(0.3, 50, 0.01, 0.35, false), 6,
            juce::AudioParameterFloatAttributes{}.withLabel("").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 2) + " "; })));

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
            {"numChains",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setNumChains(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::numChains, v);
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
            {"lowFreq",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setLowFreq(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::lowFreq, v);
             }},
            {"highFreq",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setHighFreq(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::highFreq, v);
             }},
            {"distribution",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setDistribution(static_cast<int>(v));
                 p.m_fileIo.updateParameter(PatchParameters::Id::distribution, v);
             }},
            {"decayMin",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setDecayMin(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::decayMin, v);
             }},
            {"decayMax",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setDecayMax(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::decayMax, v);
             }},
            {"gainMin",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setGainMin(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::gainMin, v);
             }},
            {"gainMax",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setGainMax(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::gainMax, v);
             }},
            {"delayMin",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setDelayMin(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::delayMin, v);
             }},
            {"delayMax",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setDelayMax(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::delayMax, v);
             }},
            {"q",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setQ(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::q, v);
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
        if (auto* p = m_parameters.getParameter("numChains"))
        {
            const auto& range = m_parameters.getParameterRange("numChains");
            float normalized = range.convertTo0to1(params.numChains);
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
        if (auto* p = m_parameters.getParameter("lowFreq"))
        {
            const auto& range = m_parameters.getParameterRange("lowFreq");
            float normalized = range.convertTo0to1(params.lowFreq);
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("highFreq"))
        {
            const auto& range = m_parameters.getParameterRange("highFreq");
            float normalized = range.convertTo0to1(params.highFreq);
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("distribution"))
        {
            const auto& range = m_parameters.getParameterRange("distribution");
            float normalized = range.convertTo0to1(params.distribution);
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("decayMin"))
        {
            const auto& range = m_parameters.getParameterRange("decayMin");
            float normalized = range.convertTo0to1(params.decayMin);
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("decayMax"))
        {
            const auto& range = m_parameters.getParameterRange("decayMax");
            float normalized = range.convertTo0to1(params.decayMax);
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("gainMin"))
        {
            const auto& range = m_parameters.getParameterRange("gainMin");
            float normalized = range.convertTo0to1(params.gainMin);
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("gainMax"))
        {
            const auto& range = m_parameters.getParameterRange("gainMax");
            float normalized = range.convertTo0to1(params.gainMax);
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("delayMin"))
        {
            const auto& range = m_parameters.getParameterRange("delayMin");
            float normalized = range.convertTo0to1(params.delayMin);
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("delayMax"))
        {
            const auto& range = m_parameters.getParameterRange("delayMax");
            float normalized = range.convertTo0to1(params.delayMax);
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("q"))
        {
            const auto& range = m_parameters.getParameterRange("q");
            float normalized = range.convertTo0to1(params.q);
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
    std::unique_ptr<ResonikImpl<NumSamplesPerBlock>> pluginRunner;
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
