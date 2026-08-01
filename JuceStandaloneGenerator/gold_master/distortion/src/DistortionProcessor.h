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
#include "impl/GenericImpl.h"

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
        m_parameters.addParameterListener("level", this);
        m_parameters.addParameterListener("type", this);
        m_parameters.addParameterListener("preboostLow", this);
        m_parameters.addParameterListener("preboostHigh", this);
        m_parameters.addParameterListener("crossOver", this);
        m_parameters.addParameterListener("cut", this);

        m_fileIo.initialize(m_patchIndex);
    }
    ~AudioPluginAudioProcessor() override
    {
        m_parameters.removeParameterListener("level", this);
        m_parameters.removeParameterListener("type", this);
        m_parameters.removeParameterListener("preboostLow", this);
        m_parameters.removeParameterListener("preboostHigh", this);
        m_parameters.removeParameterListener("crossOver", this);
        m_parameters.removeParameterListener("cut", this);
    }

    void prepareToPlay(const double sampleRate, const int samplesPerBlock) override
    {
        pluginRunner = std::make_unique<GenericImpl<NumSamplesPerBlock>>(RateNormalizer::kInternalSampleRate);

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
            juce::ParameterID("level", 1), "Level", juce::NormalisableRange<float>(-60, 12, 0.1, 1, false), 0,
            juce::AudioParameterFloatAttributes{}.withLabel("dB").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 1) + " dB"; })));
        params.push_back(std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID("type", 1), "Type", juce::StringArray{"linear cut", "tanh", "atan", "1/sqrt", "rectify"},
            0));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("preboostLow", 1), "Preboost Low", juce::NormalisableRange<float>(-32, 32, 0.1, 1, false),
            0,
            juce::AudioParameterFloatAttributes{}.withLabel("dB").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 1) + " dB"; })));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("preboostHigh", 1), "Preboost High",
            juce::NormalisableRange<float>(-32, 32, 0.1, 1, false), 0,
            juce::AudioParameterFloatAttributes{}.withLabel("dB").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 1) + " dB"; })));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("crossOver", 1), "Cross Over", juce::NormalisableRange<float>(100, 10000, 1, 0.5, false),
            800,
            juce::AudioParameterFloatAttributes{}.withLabel("Hz").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 0) + " Hz"; })));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("cut", 1), "Cut", juce::NormalisableRange<float>(100, 20000, 1, 0.5, false), 8000,
            juce::AudioParameterFloatAttributes{}.withLabel("Hz").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 0) + " Hz"; })));

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
            {"level",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setLevel(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::level, v);
             }},
            {"type",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setType(static_cast<int>(v));
                 p.m_fileIo.updateParameter(PatchParameters::Id::type, v);
             }},
            {"preboostLow",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setPreboostLow(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::preboostLow, v);
             }},
            {"preboostHigh",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setPreboostHigh(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::preboostHigh, v);
             }},
            {"crossOver",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setCrossOver(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::crossOver, v);
             }},
            {"cut",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setCut(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::cut, v);
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
        if (auto* p = m_parameters.getParameter("level"))
        {
            const auto& range = m_parameters.getParameterRange("level");
            float normalized = range.convertTo0to1(params.level);
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("type"))
        {
            const auto& range = m_parameters.getParameterRange("type");
            float normalized = range.convertTo0to1(params.type);
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("preboostLow"))
        {
            const auto& range = m_parameters.getParameterRange("preboostLow");
            float normalized = range.convertTo0to1(params.preboostLow);
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("preboostHigh"))
        {
            const auto& range = m_parameters.getParameterRange("preboostHigh");
            float normalized = range.convertTo0to1(params.preboostHigh);
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("crossOver"))
        {
            const auto& range = m_parameters.getParameterRange("crossOver");
            float normalized = range.convertTo0to1(params.crossOver);
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("cut"))
        {
            const auto& range = m_parameters.getParameterRange("cut");
            float normalized = range.convertTo0to1(params.cut);
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
        if ((getTotalNumInputChannels() == 2) && (getTotalNumOutputChannels() == 2))
        {
            fixedRunner->processBlock(buffer);
        }
    }

#pragma GCC diagnostic pop


    [[nodiscard]] const std::vector<float>& getWaveDataToShow()
    {
        return pluginRunner->visualizeWaveData();
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
    std::unique_ptr<GenericImpl<NumSamplesPerBlock>> pluginRunner;

    juce::AudioProcessorValueTreeState m_parameters;
    std::vector<int> m_patchIndex;
    FileIo m_fileIo;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioPluginAudioProcessor)
};
