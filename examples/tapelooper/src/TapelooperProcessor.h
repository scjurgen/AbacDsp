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
#include "impl/TapeLooperImpl.h"

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
        m_parameters.addParameterListener("tapeSpeed", this);
        m_parameters.addParameterListener("bars", this);
        m_parameters.addParameterListener("inputGain", this);
        m_parameters.addParameterListener("grooveLevel", this);
        m_parameters.addParameterListener("recordA", this);
        m_parameters.addParameterListener("playA", this);
        m_parameters.addParameterListener("clearA", this);
        m_parameters.addParameterListener("recordB", this);
        m_parameters.addParameterListener("playB", this);
        m_parameters.addParameterListener("clearB", this);
        m_parameters.addParameterListener("recordC", this);
        m_parameters.addParameterListener("playC", this);
        m_parameters.addParameterListener("clearC", this);
        m_parameters.addParameterListener("groovePlay", this);
        m_parameters.addParameterListener("bpm", this);
        m_parameters.addParameterListener("grooveVariation", this);

        m_fileIo.initialize(m_patchIndex);
    }
    ~AudioPluginAudioProcessor() override
    {
        m_parameters.removeParameterListener("tapeSpeed", this);
        m_parameters.removeParameterListener("bars", this);
        m_parameters.removeParameterListener("inputGain", this);
        m_parameters.removeParameterListener("grooveLevel", this);
        m_parameters.removeParameterListener("recordA", this);
        m_parameters.removeParameterListener("playA", this);
        m_parameters.removeParameterListener("clearA", this);
        m_parameters.removeParameterListener("recordB", this);
        m_parameters.removeParameterListener("playB", this);
        m_parameters.removeParameterListener("clearB", this);
        m_parameters.removeParameterListener("recordC", this);
        m_parameters.removeParameterListener("playC", this);
        m_parameters.removeParameterListener("clearC", this);
        m_parameters.removeParameterListener("groovePlay", this);
        m_parameters.removeParameterListener("bpm", this);
        m_parameters.removeParameterListener("grooveVariation", this);
    }

    void prepareToPlay(const double sampleRate, const int samplesPerBlock) override
    {
        pluginRunner = std::make_unique<TapeLooperImpl<NumSamplesPerBlock>>(RateNormalizer::kInternalSampleRate);

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
            juce::ParameterID("tapeSpeed", 1), juce::String::fromUTF8("Tape Speed"),
            juce::NormalisableRange<float>(0.1, 4.0, 0.01, 1.0, false), 1.0,
            juce::AudioParameterFloatAttributes{}.withLabel("x").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 2) + " x"; })));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("bars", 1), juce::String::fromUTF8("Bars"),
            juce::NormalisableRange<float>(1, 32, 1, 1, false), 8,
            juce::AudioParameterFloatAttributes{}.withLabel("").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 0) + " "; })));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("inputGain", 1), juce::String::fromUTF8("Input Level"),
            juce::NormalisableRange<float>(-60, 12, 0.1, 1, false), 0,
            juce::AudioParameterFloatAttributes{}.withLabel("dB").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 1) + " dB"; })));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("grooveLevel", 1), juce::String::fromUTF8("Groove Level"),
            juce::NormalisableRange<float>(-60, 12, 0.1, 1, false), 0,
            juce::AudioParameterFloatAttributes{}.withLabel("dB").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 1) + " dB"; })));
        params.push_back(std::make_unique<juce::AudioParameterBool>(juce::ParameterID("recordA", 1),
                                                                    juce::String::fromUTF8("Rec A"), 0));
        params.push_back(std::make_unique<juce::AudioParameterBool>(juce::ParameterID("playA", 1),
                                                                    juce::String::fromUTF8("Play A"), 0));
        params.push_back(std::make_unique<juce::AudioParameterBool>(juce::ParameterID("clearA", 1),
                                                                    juce::String::fromUTF8("Clear A"), 0));
        params.push_back(std::make_unique<juce::AudioParameterBool>(juce::ParameterID("recordB", 1),
                                                                    juce::String::fromUTF8("Rec B"), 0));
        params.push_back(std::make_unique<juce::AudioParameterBool>(juce::ParameterID("playB", 1),
                                                                    juce::String::fromUTF8("Play B"), 0));
        params.push_back(std::make_unique<juce::AudioParameterBool>(juce::ParameterID("clearB", 1),
                                                                    juce::String::fromUTF8("Clear B"), 0));
        params.push_back(std::make_unique<juce::AudioParameterBool>(juce::ParameterID("recordC", 1),
                                                                    juce::String::fromUTF8("Rec C"), 0));
        params.push_back(std::make_unique<juce::AudioParameterBool>(juce::ParameterID("playC", 1),
                                                                    juce::String::fromUTF8("Play C"), 0));
        params.push_back(std::make_unique<juce::AudioParameterBool>(juce::ParameterID("clearC", 1),
                                                                    juce::String::fromUTF8("Clear C"), 0));
        params.push_back(std::make_unique<juce::AudioParameterBool>(juce::ParameterID("groovePlay", 1),
                                                                    juce::String::fromUTF8("Groove"), 0));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("bpm", 1), juce::String::fromUTF8("BPM"),
            juce::NormalisableRange<float>(50, 250, 0.5, 1, false), 120,
            juce::AudioParameterFloatAttributes{}.withLabel("BPM").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 1) + " BPM"; })));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("grooveVariation", 1), juce::String::fromUTF8("Groove Var"),
            juce::NormalisableRange<float>(0, 31, 1, 1, false), 0,
            juce::AudioParameterFloatAttributes{}.withLabel("").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 0) + " "; })));

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
            {"tapeSpeed",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setTapeSpeed(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::tapeSpeed, v);
             }},
            {"bars",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setBars(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::bars, v);
             }},
            {"inputGain",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setInputGain(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::inputGain, v);
             }},
            {"grooveLevel",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setGrooveLevel(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::grooveLevel, v);
             }},
            {"recordA",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setRecordA(static_cast<bool>(v));
                 p.m_fileIo.updateParameter(PatchParameters::Id::recordA, v);
             }},
            {"playA",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setPlayA(static_cast<bool>(v));
                 p.m_fileIo.updateParameter(PatchParameters::Id::playA, v);
             }},
            {"clearA",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setClearA(static_cast<bool>(v));
                 p.m_fileIo.updateParameter(PatchParameters::Id::clearA, v);
             }},
            {"recordB",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setRecordB(static_cast<bool>(v));
                 p.m_fileIo.updateParameter(PatchParameters::Id::recordB, v);
             }},
            {"playB",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setPlayB(static_cast<bool>(v));
                 p.m_fileIo.updateParameter(PatchParameters::Id::playB, v);
             }},
            {"clearB",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setClearB(static_cast<bool>(v));
                 p.m_fileIo.updateParameter(PatchParameters::Id::clearB, v);
             }},
            {"recordC",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setRecordC(static_cast<bool>(v));
                 p.m_fileIo.updateParameter(PatchParameters::Id::recordC, v);
             }},
            {"playC",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setPlayC(static_cast<bool>(v));
                 p.m_fileIo.updateParameter(PatchParameters::Id::playC, v);
             }},
            {"clearC",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setClearC(static_cast<bool>(v));
                 p.m_fileIo.updateParameter(PatchParameters::Id::clearC, v);
             }},
            {"groovePlay",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setGroovePlay(static_cast<bool>(v));
                 p.m_fileIo.updateParameter(PatchParameters::Id::groovePlay, v);
             }},
            {"bpm",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setBpm(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::bpm, v);
             }},
            {"grooveVariation",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setGrooveVariation(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::grooveVariation, v);
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
        if (auto* p = m_parameters.getParameter("tapeSpeed"))
        {
            const auto& range = m_parameters.getParameterRange("tapeSpeed");
            float normalized = range.convertTo0to1(params.tapeSpeed);
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("bars"))
        {
            const auto& range = m_parameters.getParameterRange("bars");
            float normalized = range.convertTo0to1(params.bars);
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("inputGain"))
        {
            const auto& range = m_parameters.getParameterRange("inputGain");
            float normalized = range.convertTo0to1(params.inputGain);
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("grooveLevel"))
        {
            const auto& range = m_parameters.getParameterRange("grooveLevel");
            float normalized = range.convertTo0to1(params.grooveLevel);
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("recordA"))
        {
            const auto& range = m_parameters.getParameterRange("recordA");
            float normalized = range.convertTo0to1(params.recordA);
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("playA"))
        {
            const auto& range = m_parameters.getParameterRange("playA");
            float normalized = range.convertTo0to1(params.playA);
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("clearA"))
        {
            const auto& range = m_parameters.getParameterRange("clearA");
            float normalized = range.convertTo0to1(params.clearA);
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("recordB"))
        {
            const auto& range = m_parameters.getParameterRange("recordB");
            float normalized = range.convertTo0to1(params.recordB);
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("playB"))
        {
            const auto& range = m_parameters.getParameterRange("playB");
            float normalized = range.convertTo0to1(params.playB);
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("clearB"))
        {
            const auto& range = m_parameters.getParameterRange("clearB");
            float normalized = range.convertTo0to1(params.clearB);
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("recordC"))
        {
            const auto& range = m_parameters.getParameterRange("recordC");
            float normalized = range.convertTo0to1(params.recordC);
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("playC"))
        {
            const auto& range = m_parameters.getParameterRange("playC");
            float normalized = range.convertTo0to1(params.playC);
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("clearC"))
        {
            const auto& range = m_parameters.getParameterRange("clearC");
            float normalized = range.convertTo0to1(params.clearC);
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("groovePlay"))
        {
            const auto& range = m_parameters.getParameterRange("groovePlay");
            float normalized = range.convertTo0to1(params.groovePlay);
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("bpm"))
        {
            const auto& range = m_parameters.getParameterRange("bpm");
            float normalized = range.convertTo0to1(params.bpm);
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("grooveVariation"))
        {
            const auto& range = m_parameters.getParameterRange("grooveVariation");
            float normalized = range.convertTo0to1(params.grooveVariation);
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
                transport.isLooping = position->getIsLooping();
                transport.isRecording = position->getIsRecording();
                if (const auto bpm = position->getBpm())
                {
                    transport.bpm = *bpm;
                }
                if (const auto ppq = position->getPpqPosition())
                {
                    transport.ppqPosition = *ppq;
                }
                if (const auto timeInSeconds = position->getTimeInSeconds())
                {
                    transport.timeInSeconds = *timeInSeconds;
                }
                if (const auto timeSig = position->getTimeSignature())
                {
                    transport.beatsPerBar = static_cast<float>(timeSig->numerator);
                    transport.timeSigDenominator = timeSig->denominator;
                }
                pluginRunner->setHostTransport(transport);
            }
        }
        if (getTotalNumOutputChannels() == 2)
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

    [[nodiscard]] bool isGroovePlaying() const noexcept
    {
        return pluginRunner && pluginRunner->isGroovePlaying();
    }
    [[nodiscard]] bool canEditBpm() const noexcept
    {
        return pluginRunner && pluginRunner->canEditBpm();
    }
    [[nodiscard]] std::vector<juce::String> listGrooveNames() const
    {
        std::vector<juce::String> result;
        if (pluginRunner)
        {
            for (const auto& n : pluginRunner->listGrooveNames())
            {
                result.push_back(juce::String(n));
            }
        }
        return result;
    }
    [[nodiscard]] juce::String getCurrentGrooveName() const
    {
        return pluginRunner ? juce::String(pluginRunner->currentGrooveName()) : juce::String();
    }
    void requestLoadGroove(const juce::String& styleName, const int variationIndex)
    {
        if (pluginRunner)
        {
            pluginRunner->requestLoadGroove(styleName.toStdString(), static_cast<unsigned>(variationIndex));
        }
    }
    [[nodiscard]] juce::String consumeGrooveInfoText() const
    {
        return pluginRunner ? juce::String(pluginRunner->consumeGrooveInfoText()) : juce::String();
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
    std::unique_ptr<TapeLooperImpl<NumSamplesPerBlock>> pluginRunner;

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
