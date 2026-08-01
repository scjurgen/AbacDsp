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
#include "pedals/LfoExplorePedal.h"

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
        , m_patchIndex(0, 0)
    {
        m_parameters.addParameterListener("wave", this);
        m_parameters.addParameterListener("vol", this);
        m_parameters.addParameterListener("frequency", this);
        m_parameters.addParameterListener("deformType", this);
        m_parameters.addParameterListener("deform", this);
        m_parameters.addParameterListener("gain", this);
        m_parameters.addParameterListener("offset", this);
        m_parameters.addParameterListener("clip", this);
        m_parameters.addParameterListener("phase", this);
        m_parameters.addParameterListener("smooth", this);
        m_parameters.addParameterListener("outputType", this);
        m_parameters.addParameterListener("discrete", this);

        m_fileIo.initialize(m_patchIndex);
    }
    ~AudioPluginAudioProcessor() override
    {
        m_parameters.removeParameterListener("wave", this);
        m_parameters.removeParameterListener("vol", this);
        m_parameters.removeParameterListener("frequency", this);
        m_parameters.removeParameterListener("deformType", this);
        m_parameters.removeParameterListener("deform", this);
        m_parameters.removeParameterListener("gain", this);
        m_parameters.removeParameterListener("offset", this);
        m_parameters.removeParameterListener("clip", this);
        m_parameters.removeParameterListener("phase", this);
        m_parameters.removeParameterListener("smooth", this);
        m_parameters.removeParameterListener("outputType", this);
        m_parameters.removeParameterListener("discrete", this);
    }

    void prepareToPlay(const double sampleRate, const int samplesPerBlock) override
    {
        pluginRunner = std::make_unique<LfoExplorePedal<NumSamplesPerBlock>>(RateNormalizer::kInternalSampleRate);

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
        params.push_back(std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID("wave", 1), "Wave",
            juce::StringArray{"Sine", "Triangle", "Saw", "Square", "Noise", "SampleHold", "SampleHoldFlipFlop",
                              "BrownNoise"},
            0));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("vol", 1), "Vol", juce::NormalisableRange<float>(-100, 12, 0.1, 1, false), 0,
            juce::AudioParameterFloatAttributes{}.withLabel("dB").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 1) + " dB"; })));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("frequency", 1), "Frequency", juce::NormalisableRange<float>(0.01, 20, 0.01, 0.5, false),
            1,
            juce::AudioParameterFloatAttributes{}.withLabel("Hz").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 2) + " Hz"; })));
        params.push_back(std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID("deformType", 1), "Deform type",
            juce::StringArray{"Square", "Power4", "Power6", "Plateau1", "Plateau2"}, 0));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("deform", 1), "Deform", juce::NormalisableRange<float>(-1, 1, 0.01, 1, false), 0,
            juce::AudioParameterFloatAttributes{}.withLabel("").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 2) + " "; })));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("gain", 1), "Gain", juce::NormalisableRange<float>(-3, 3, 0.01, 1, false), 1,
            juce::AudioParameterFloatAttributes{}.withLabel("").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 2) + " "; })));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("offset", 1), "Offset", juce::NormalisableRange<float>(-1, 1, 0.01, 1, false), 0,
            juce::AudioParameterFloatAttributes{}.withLabel("").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 2) + " "; })));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("clip", 1), "Clip", juce::NormalisableRange<float>(0, 1, 0.01, 1, false), 1,
            juce::AudioParameterFloatAttributes{}.withLabel("").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 2) + " "; })));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("phase", 1), "Phase", juce::NormalisableRange<float>(0, 360, 10, 1, false), 0,
            juce::AudioParameterFloatAttributes{}.withLabel("deg").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 0) + " deg"; })));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("smooth", 1), "Smooth", juce::NormalisableRange<float>(0, 1, 0.01, 1, false), 0,
            juce::AudioParameterFloatAttributes{}.withLabel("").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 2) + " "; })));
        params.push_back(std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID("outputType", 1), "Output Type",
            juce::StringArray{"bipolar", "unipolar", "abs", "clip negative"}, 0));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("discrete", 1), "Discrete", juce::NormalisableRange<float>(0, 32, 1, 1, false), 0,
            juce::AudioParameterFloatAttributes{}.withLabel("steps").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 0) + " steps"; })));

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
            {"wave",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setWave(static_cast<int>(v));
                 p.m_fileIo.updateParameter(PatchParameters::Id::wave, v);
             }},
            {"vol",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setVol(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::vol, v);
             }},
            {"frequency",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setFrequency(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::frequency, v);
             }},
            {"deformType",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setDeformType(static_cast<int>(v));
                 p.m_fileIo.updateParameter(PatchParameters::Id::deformType, v);
             }},
            {"deform",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setDeform(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::deform, v);
             }},
            {"gain",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setGain(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::gain, v);
             }},
            {"offset",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setOffset(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::offset, v);
             }},
            {"clip",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setClip(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::clip, v);
             }},
            {"phase",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setPhase(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::phase, v);
             }},
            {"smooth",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setSmooth(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::smooth, v);
             }},
            {"outputType",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setOutputType(static_cast<int>(v));
                 p.m_fileIo.updateParameter(PatchParameters::Id::outputType, v);
             }},
            {"discrete",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setDiscrete(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::discrete, v);
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
        if (auto* p = m_parameters.getParameter("wave"))
        {
            const auto& range = m_parameters.getParameterRange("wave");
            float normalized = range.convertTo0to1(params.wave);
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("vol"))
        {
            const auto& range = m_parameters.getParameterRange("vol");
            float normalized = range.convertTo0to1(params.vol);
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("frequency"))
        {
            const auto& range = m_parameters.getParameterRange("frequency");
            float normalized = range.convertTo0to1(params.frequency);
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("deformType"))
        {
            const auto& range = m_parameters.getParameterRange("deformType");
            float normalized = range.convertTo0to1(params.deformType);
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("deform"))
        {
            const auto& range = m_parameters.getParameterRange("deform");
            float normalized = range.convertTo0to1(params.deform);
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("gain"))
        {
            const auto& range = m_parameters.getParameterRange("gain");
            float normalized = range.convertTo0to1(params.gain);
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("offset"))
        {
            const auto& range = m_parameters.getParameterRange("offset");
            float normalized = range.convertTo0to1(params.offset);
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("clip"))
        {
            const auto& range = m_parameters.getParameterRange("clip");
            float normalized = range.convertTo0to1(params.clip);
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("phase"))
        {
            const auto& range = m_parameters.getParameterRange("phase");
            float normalized = range.convertTo0to1(params.phase);
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("smooth"))
        {
            const auto& range = m_parameters.getParameterRange("smooth");
            float normalized = range.convertTo0to1(params.smooth);
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("outputType"))
        {
            const auto& range = m_parameters.getParameterRange("outputType");
            float normalized = range.convertTo0to1(params.outputType);
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("discrete"))
        {
            const auto& range = m_parameters.getParameterRange("discrete");
            float normalized = range.convertTo0to1(params.discrete);
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
    std::unique_ptr<LfoExplorePedal<NumSamplesPerBlock>> pluginRunner;

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
    std::vector<int> m_patchIndex;
    FileIo m_fileIo;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioPluginAudioProcessor)
};
