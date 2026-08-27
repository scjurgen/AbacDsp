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
#include "impl/GrooverImpl.h"

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
        , m_patchIndex(0, 0)
    {
        m_parameters.addParameterListener("play", this);
        m_parameters.addParameterListener("hostSync", this);
        m_parameters.addParameterListener("bpm", this);
        m_parameters.addParameterListener("grooveVariation", this);
        m_parameters.addParameterListener("outputLevel", this);
        m_parameters.addParameterListener("inputGain", this);
        m_parameters.addParameterListener("push", this);
        m_parameters.addParameterListener("life", this);

        m_fileIo.initialize(m_patchIndex);
    }
    ~AudioPluginAudioProcessor() override
    {
        m_parameters.removeParameterListener("play", this);
        m_parameters.removeParameterListener("hostSync", this);
        m_parameters.removeParameterListener("bpm", this);
        m_parameters.removeParameterListener("grooveVariation", this);
        m_parameters.removeParameterListener("outputLevel", this);
        m_parameters.removeParameterListener("inputGain", this);
        m_parameters.removeParameterListener("push", this);
        m_parameters.removeParameterListener("life", this);
    }

    void prepareToPlay(const double sampleRate, const int samplesPerBlock) override
    {
        pluginRunner = std::make_unique<GrooverImpl<NumSamplesPerBlock>>(RateNormalizer::kInternalSampleRate);

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
        params.push_back(std::make_unique<juce::AudioParameterBool>(juce::ParameterID("play", 1),
                                                                    juce::String::fromUTF8("Play"), 0));
        params.push_back(std::make_unique<juce::AudioParameterBool>(juce::ParameterID("hostSync", 1),
                                                                    juce::String::fromUTF8("Host Sync"), 0));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("bpm", 1), juce::String::fromUTF8("BPM"),
            juce::NormalisableRange<float>(50, 250, 0.5, 1, false), 120,
            juce::AudioParameterFloatAttributes{}.withLabel("BPM").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 1) + " BPM"; })));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("grooveVariation", 1), juce::String::fromUTF8("Variation"),
            juce::NormalisableRange<float>(0, 31, 1, 1, false), 0,
            juce::AudioParameterFloatAttributes{}.withLabel("").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 0) + " "; })));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("outputLevel", 1), juce::String::fromUTF8("Output Level"),
            juce::NormalisableRange<float>(-60, 12, 0.1, 1, false), 0,
            juce::AudioParameterFloatAttributes{}.withLabel("dB").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 1) + " dB"; })));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("inputGain", 1), juce::String::fromUTF8("Input Gain"),
            juce::NormalisableRange<float>(-60, 12, 0.1, 1, false), 0,
            juce::AudioParameterFloatAttributes{}.withLabel("dB").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 1) + " dB"; })));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("push", 1), juce::String::fromUTF8("Push"),
            juce::NormalisableRange<float>(-100, 100, 1, 1, false), 0,
            juce::AudioParameterFloatAttributes{}.withLabel("%").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 1) + " %"; })));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("life", 1), juce::String::fromUTF8("Life"),
            juce::NormalisableRange<float>(0, 100, 1, 1, false), 100,
            juce::AudioParameterFloatAttributes{}.withLabel("%").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 1) + " %"; })));

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
            {"play",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setPlay(static_cast<bool>(v));
                 p.m_fileIo.updateParameter(PatchParameters::Id::play, v);
             }},
            {"hostSync",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setHostSync(static_cast<bool>(v));
                 p.m_fileIo.updateParameter(PatchParameters::Id::hostSync, v);
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
            {"outputLevel",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setOutputLevel(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::outputLevel, v);
             }},
            {"inputGain",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setInputGain(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::inputGain, v);
             }},
            {"push",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setPush(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::push, v);
             }},
            {"life",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setLife(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::life, v);
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
        if (auto* p = m_parameters.getParameter("play"))
        {
            const auto& range = m_parameters.getParameterRange("play");
            float normalized = range.convertTo0to1(static_cast<float>(params.play));
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("hostSync"))
        {
            const auto& range = m_parameters.getParameterRange("hostSync");
            float normalized = range.convertTo0to1(static_cast<float>(params.hostSync));
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("bpm"))
        {
            const auto& range = m_parameters.getParameterRange("bpm");
            float normalized = range.convertTo0to1(static_cast<float>(params.bpm));
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("grooveVariation"))
        {
            const auto& range = m_parameters.getParameterRange("grooveVariation");
            float normalized = range.convertTo0to1(static_cast<float>(params.grooveVariation));
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("outputLevel"))
        {
            const auto& range = m_parameters.getParameterRange("outputLevel");
            float normalized = range.convertTo0to1(static_cast<float>(params.outputLevel));
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("inputGain"))
        {
            const auto& range = m_parameters.getParameterRange("inputGain");
            float normalized = range.convertTo0to1(static_cast<float>(params.inputGain));
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("push"))
        {
            const auto& range = m_parameters.getParameterRange("push");
            float normalized = range.convertTo0to1(static_cast<float>(params.push));
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("life"))
        {
            const auto& range = m_parameters.getParameterRange("life");
            float normalized = range.convertTo0to1(static_cast<float>(params.life));
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


#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-conversion"

    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) override
    {
        juce::ScopedNoDenormals noDenormals;

        if (!midiMessages.isEmpty())
        {
            for (const auto& msg : midiMessages)
            {
                pluginRunner->processMidi(msg.data);
            }
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
    }

#pragma GCC diagnostic pop


    [[nodiscard]] bool isGroovePlaying() const noexcept
    {
        return pluginRunner && pluginRunner->isGroovePlaying();
    }
    [[nodiscard]] bool isHostSynced() const noexcept
    {
        return pluginRunner && pluginRunner->isHostSynced();
    }
    [[nodiscard]] bool canEditBpm() const noexcept
    {
        return pluginRunner && pluginRunner->canEditBpm();
    }
    [[nodiscard]] bool isHostPresent() const noexcept
    {
        return wrapperType != juce::AudioProcessor::wrapperType_Standalone;
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


    [[nodiscard]] bool hasRunner() const
    {
        return pluginRunner.get() != nullptr;
    }
    float m_maxValue{0.f};

  private:
    size_t m_sampleRate{48000};

    static bool isChanged(const float a, const float b)
    {
        return std::abs(a - b) > 1E-8f;
    }

    int m_program{0};

    std::unique_ptr<RateNormalizer> fixedRunner;
    std::unique_ptr<GrooverImpl<NumSamplesPerBlock>> pluginRunner;

    juce::AudioProcessorValueTreeState m_parameters;
    std::vector<int> m_patchIndex;
    FileIo m_fileIo;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioPluginAudioProcessor)
};
