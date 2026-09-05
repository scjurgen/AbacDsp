#pragma once
/*
 * AUTO GENERATED,
 * NOT A GOOD IDEA TO CHANGE STUFF HERE
 * Keep the file readonly
 */

#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_audio_processors/juce_audio_processors.h>

#include "Analysis/EnvelopeFollower.h"
#include "Analysis/Spectrogram.h"
#include "SamplerateConverter/InternalRateNormalizingProcessor.h"
#include "UiElements.h"
#include "impl/CcMapping.h"
#include "impl/CcSettings.h"
#include "impl/FileIo.h"
#include "impl/MorphexsynthImpl.h"

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
        m_parameters.addParameterListener("vol", this);
        m_parameters.addParameterListener("cutoff", this);
        m_parameters.addParameterListener("resonance", this);
        m_parameters.addParameterListener("reverbSize", this);
        m_parameters.addParameterListener("reverbDecay", this);
        m_parameters.addParameterListener("reverbMix", this);
        m_parameters.addParameterListener("reverbDry", this);
        m_parameters.addParameterListener("luaParam1", this);
        m_parameters.addParameterListener("luaParam2", this);
        m_parameters.addParameterListener("luaParam3", this);
        m_parameters.addParameterListener("luaParam4", this);
        m_parameters.addParameterListener("luaParam5", this);
        m_parameters.addParameterListener("luaParam6", this);
        m_parameters.addParameterListener("luaParam7", this);
        m_parameters.addParameterListener("luaParam8", this);

        for (size_t i = 0; i < 8; ++i)
        {
            m_ccActive[i].controller.store(kDefaultCcMappings[i].controller, std::memory_order_relaxed);
            m_ccActive[i].valueLow.store(kDefaultCcMappings[i].valueLow, std::memory_order_relaxed);
            m_ccActive[i].valueHigh.store(kDefaultCcMappings[i].valueHigh, std::memory_order_relaxed);
        }
        m_fileIo.initialize(m_patchIndex);
        initAuthoringServer();
    }
    ~AudioPluginAudioProcessor() override
    {
        m_parameters.removeParameterListener("vol", this);
        m_parameters.removeParameterListener("cutoff", this);
        m_parameters.removeParameterListener("resonance", this);
        m_parameters.removeParameterListener("reverbSize", this);
        m_parameters.removeParameterListener("reverbDecay", this);
        m_parameters.removeParameterListener("reverbMix", this);
        m_parameters.removeParameterListener("reverbDry", this);
        m_parameters.removeParameterListener("luaParam1", this);
        m_parameters.removeParameterListener("luaParam2", this);
        m_parameters.removeParameterListener("luaParam3", this);
        m_parameters.removeParameterListener("luaParam4", this);
        m_parameters.removeParameterListener("luaParam5", this);
        m_parameters.removeParameterListener("luaParam6", this);
        m_parameters.removeParameterListener("luaParam7", this);
        m_parameters.removeParameterListener("luaParam8", this);
    }

    void prepareToPlay(const double sampleRate, const int samplesPerBlock) override
    {
        pluginRunner = std::make_unique<MorphexsynthImpl<NumSamplesPerBlock>>(RateNormalizer::kInternalSampleRate);
        pluginRunner->setImportResolver([](const std::string_view name) { return FileIo::resolveLibraryScript(name); });
        if (!m_fileIo.currentScript().empty())
        {
            pluginRunner->setScript(m_fileIo.currentScript());
        }

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
        for (const auto& entry : CcSettings::load())
        {
            for (size_t i = 0; i < 8; ++i)
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
        m_authoringMidiCollector.reset(sampleRate);
        m_authoringMidiCollector.ensureStorageAllocated(2048);
        m_authoringRecorder.prepare(sampleRate, getTotalNumOutputChannels());

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
            juce::ParameterID("vol", 1), juce::String::fromUTF8("Vol"),
            juce::NormalisableRange<float>(-100, 12, 0.1, 1, false), 0,
            juce::AudioParameterFloatAttributes{}.withLabel("dB").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 1) + " dB"; })));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("cutoff", 1), juce::String::fromUTF8("Cutoff"),
            juce::NormalisableRange<float>(0, 127, 1, 1, false), 72,
            juce::AudioParameterFloatAttributes{}.withLabel("").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 0) + " "; })));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("resonance", 1), juce::String::fromUTF8("Reso"),
            juce::NormalisableRange<float>(0, 120, 1, 1, false), 0,
            juce::AudioParameterFloatAttributes{}.withLabel("%").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 0) + " %"; })));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("reverbSize", 1), juce::String::fromUTF8("Rev Size"),
            juce::NormalisableRange<float>(1, 60, 0.1, 1, false), 12,
            juce::AudioParameterFloatAttributes{}.withLabel("m").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 1) + " m"; })));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("reverbDecay", 1), juce::String::fromUTF8("Rev Decay"),
            juce::NormalisableRange<float>(0, 20000, 1, 0.5, false), 1500,
            juce::AudioParameterFloatAttributes{}.withLabel("ms").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 0) + " ms"; })));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("reverbMix", 1), juce::String::fromUTF8("Rev Mix"),
            juce::NormalisableRange<float>(-100, 12, 0.1, 1, false), -100,
            juce::AudioParameterFloatAttributes{}.withLabel("dB").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 1) + " dB"; })));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("reverbDry", 1), juce::String::fromUTF8("Rev Dry"),
            juce::NormalisableRange<float>(-100, 12, 0.1, 1, false), 0,
            juce::AudioParameterFloatAttributes{}.withLabel("dB").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 1) + " dB"; })));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("luaParam1", 1), juce::String::fromUTF8("Lua Param 1"),
            juce::NormalisableRange<float>(0, 1, 0, 1, false), 0,
            juce::AudioParameterFloatAttributes{}.withLabel("").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 2) + " "; })));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("luaParam2", 1), juce::String::fromUTF8("Lua Param 2"),
            juce::NormalisableRange<float>(0, 1, 0, 1, false), 0,
            juce::AudioParameterFloatAttributes{}.withLabel("").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 2) + " "; })));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("luaParam3", 1), juce::String::fromUTF8("Lua Param 3"),
            juce::NormalisableRange<float>(0, 1, 0, 1, false), 0,
            juce::AudioParameterFloatAttributes{}.withLabel("").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 2) + " "; })));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("luaParam4", 1), juce::String::fromUTF8("Lua Param 4"),
            juce::NormalisableRange<float>(0, 1, 0, 1, false), 0,
            juce::AudioParameterFloatAttributes{}.withLabel("").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 2) + " "; })));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("luaParam5", 1), juce::String::fromUTF8("Lua Param 5"),
            juce::NormalisableRange<float>(0, 1, 0, 1, false), 0,
            juce::AudioParameterFloatAttributes{}.withLabel("").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 2) + " "; })));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("luaParam6", 1), juce::String::fromUTF8("Lua Param 6"),
            juce::NormalisableRange<float>(0, 1, 0, 1, false), 0,
            juce::AudioParameterFloatAttributes{}.withLabel("").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 2) + " "; })));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("luaParam7", 1), juce::String::fromUTF8("Lua Param 7"),
            juce::NormalisableRange<float>(0, 1, 0, 1, false), 0,
            juce::AudioParameterFloatAttributes{}.withLabel("").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 2) + " "; })));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("luaParam8", 1), juce::String::fromUTF8("Lua Param 8"),
            juce::NormalisableRange<float>(0, 1, 0, 1, false), 0,
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
            {"vol",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setVol(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::vol, v);
             }},
            {"cutoff",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setCutoff(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::cutoff, v);
             }},
            {"resonance",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setResonance(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::resonance, v);
             }},
            {"reverbSize",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setReverbSize(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::reverbSize, v);
             }},
            {"reverbDecay",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setReverbDecay(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::reverbDecay, v);
             }},
            {"reverbMix",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setReverbMix(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::reverbMix, v);
             }},
            {"reverbDry",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setReverbDry(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::reverbDry, v);
             }},
            {"luaParam1",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setLuaParam1(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::luaParam1, v);
             }},
            {"luaParam2",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setLuaParam2(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::luaParam2, v);
             }},
            {"luaParam3",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setLuaParam3(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::luaParam3, v);
             }},
            {"luaParam4",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setLuaParam4(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::luaParam4, v);
             }},
            {"luaParam5",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setLuaParam5(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::luaParam5, v);
             }},
            {"luaParam6",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setLuaParam6(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::luaParam6, v);
             }},
            {"luaParam7",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setLuaParam7(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::luaParam7, v);
             }},
            {"luaParam8",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setLuaParam8(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::luaParam8, v);
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
            float normalized = range.convertTo0to1(static_cast<float>(params.vol));
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("cutoff"))
        {
            const auto& range = m_parameters.getParameterRange("cutoff");
            float normalized = range.convertTo0to1(static_cast<float>(params.cutoff));
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("resonance"))
        {
            const auto& range = m_parameters.getParameterRange("resonance");
            float normalized = range.convertTo0to1(static_cast<float>(params.resonance));
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("reverbSize"))
        {
            const auto& range = m_parameters.getParameterRange("reverbSize");
            float normalized = range.convertTo0to1(static_cast<float>(params.reverbSize));
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("reverbDecay"))
        {
            const auto& range = m_parameters.getParameterRange("reverbDecay");
            float normalized = range.convertTo0to1(static_cast<float>(params.reverbDecay));
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("reverbMix"))
        {
            const auto& range = m_parameters.getParameterRange("reverbMix");
            float normalized = range.convertTo0to1(static_cast<float>(params.reverbMix));
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("reverbDry"))
        {
            const auto& range = m_parameters.getParameterRange("reverbDry");
            float normalized = range.convertTo0to1(static_cast<float>(params.reverbDry));
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("luaParam1"))
        {
            const auto& range = m_parameters.getParameterRange("luaParam1");
            float normalized = range.convertTo0to1(static_cast<float>(params.luaParam1));
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("luaParam2"))
        {
            const auto& range = m_parameters.getParameterRange("luaParam2");
            float normalized = range.convertTo0to1(static_cast<float>(params.luaParam2));
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("luaParam3"))
        {
            const auto& range = m_parameters.getParameterRange("luaParam3");
            float normalized = range.convertTo0to1(static_cast<float>(params.luaParam3));
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("luaParam4"))
        {
            const auto& range = m_parameters.getParameterRange("luaParam4");
            float normalized = range.convertTo0to1(static_cast<float>(params.luaParam4));
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("luaParam5"))
        {
            const auto& range = m_parameters.getParameterRange("luaParam5");
            float normalized = range.convertTo0to1(static_cast<float>(params.luaParam5));
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("luaParam6"))
        {
            const auto& range = m_parameters.getParameterRange("luaParam6");
            float normalized = range.convertTo0to1(static_cast<float>(params.luaParam6));
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("luaParam7"))
        {
            const auto& range = m_parameters.getParameterRange("luaParam7");
            float normalized = range.convertTo0to1(static_cast<float>(params.luaParam7));
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("luaParam8"))
        {
            const auto& range = m_parameters.getParameterRange("luaParam8");
            float normalized = range.convertTo0to1(static_cast<float>(params.luaParam8));
            p->setValueNotifyingHost(normalized);
        }

        if (pluginRunner != nullptr && !params.script.empty())
        {
            pluginRunner->setScript(params.script);
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


    [[nodiscard]] juce::String getScriptText() const
    {
        return juce::String(m_fileIo.currentScript());
    }

    bool applyScriptText(const juce::String& text)
    {
        if (pluginRunner == nullptr)
        {
            return false;
        }
        const bool ok = pluginRunner->setScript(text.toStdString());
        if (ok)
        {
            m_fileIo.updateScript(text.toStdString());
        }
        return ok;
    }

    [[nodiscard]] std::vector<juce::String> listScriptNames() const
    {
        std::vector<juce::String> result;
        for (const auto& n : m_fileIo.listScriptNames())
        {
            result.push_back(juce::String(n));
        }
        return result;
    }

    [[nodiscard]] juce::String getCurrentScriptName() const
    {
        return juce::String(m_fileIo.currentScriptName());
    }

    [[nodiscard]] std::vector<juce::String> getLibraryScriptNames() const
    {
        std::vector<juce::String> result;
        for (const auto& n : FileIo::listLibraryScriptNames())
        {
            result.push_back(juce::String(n));
        }
        return result;
    }

    [[nodiscard]] juce::String getLibraryScriptText(const juce::String& name) const
    {
        const auto lookup = FileIo::resolveLibraryScript(name.toStdString());
        return lookup.source ? juce::String(*lookup.source) : "-- not found: " + name;
    }

    bool requestLoadScript(const juce::String& name)
    {
        if (!m_fileIo.loadScriptNamed(name.toStdString()))
        {
            return false;
        }
        return applyScriptText(juce::String(m_fileIo.currentScript()));
    }

    bool saveCurrentScriptAs(const juce::String& name)
    {
        return m_fileIo.saveScriptNamed(name.toStdString());
    }

    bool deleteScriptNamed(const juce::String& name)
    {
        return m_fileIo.deleteScriptNamed(name.toStdString());
    }

    bool renameScript(const juce::String& oldName, const juce::String& newName)
    {
        return m_fileIo.renameScriptNamed(oldName.toStdString(), newName.toStdString());
    }

    bool saveUserLibraryScript(const juce::String& name, const juce::String& content)
    {
        return FileIo::saveUserLibraryScript(name.toStdString(), content.toStdString());
    }

    // Raw JSON of a saved named patch, for the Authoring HTTP API's GET /patches/{name}.
    [[nodiscard]] std::optional<juce::String> getPatchJson(const juce::String& name) const
    {
        const auto json = m_fileIo.readPatchJson(name.toStdString());
        return json ? std::optional<juce::String>(juce::String(*json)) : std::nullopt;
    }

    // Loads and applies a named patch directly, no "save unsaved changes first?" prompt -
    // requestLoadPatch()'s modal dialog would otherwise block the HTTP request forever.
    bool applyPatchNamed(const juce::String& name)
    {
        if (!m_fileIo.loadPatchNamed(name.toStdString()))
        {
            return false;
        }
        applyLoadedParametersToHost();
        return true;
    }

    // Raw source of an installed library, for the Authoring HTTP API's GET /libraries/{name}
    // - unlike getLibraryScriptText(), which returns a friendly placeholder comment on a
    // miss (for the in-app dropdown), this reports a real 404 the HTTP layer can act on.
    [[nodiscard]] std::optional<juce::String> getLibraryScriptSource(const juce::String& name) const
    {
        const auto lookup = FileIo::resolveLibraryScript(name.toStdString());
        return lookup.source ? std::optional<juce::String>(juce::String(*lookup.source)) : std::nullopt;
    }

    // Mirrors the old LlmAssistWatcher's applyLibraryAndPull(): saves to Library/User/,
    // then re-applies the current script so any import "{name}" is genuinely re-resolved
    // and validated - compiled/error describe that re-apply, not the library file alone.
    std::pair<bool, juce::String> applyLibraryScript(const juce::String& name, const juce::String& content)
    {
        if (!saveUserLibraryScript(name, content))
        {
            return {false, "failed to save library script (invalid name or write error)"};
        }
        const bool compiled = applyScriptText(getScriptText());
        return {compiled, compiled ? juce::String{} : juce::String(scriptErrorMessage())};
    }

    // Cross-thread-safe by design (see juce_MidiMessageCollector.h) - AuthoringHttpServer
    // calls this straight from its own worker thread, no message-thread hop needed.
    void injectAuthoringMidi(const juce::MidiMessage& message)
    {
        m_authoringMidiCollector.addMessageToQueue(message);
    }

    // For the Authoring HTTP API's POST /record/start - see AuthoringHttpServer.h's
    // authoringRecordingsDirectory() for why the target folder is shared, not chosen here.
    AuthoringRecordStartResult startAuthoringRecording()
    {
        const auto file = authoringRecordingsDirectory(JucePlugin_Name)
                              .getChildFile("rec-" + juce::String(currentProcessId()) + "-" +
                                            juce::String(juce::Time::currentTimeMillis()) + ".wav");
        return m_authoringRecorder.start(file);
    }

    AuthoringRecordStopResult stopAuthoringRecording()
    {
        return m_authoringRecorder.stop();
    }

    // Wires the callback surface AuthoringHttpServer needs to reach the running instance's
    // script/patch state - same callbacks the old LlmAssistWatcher used, plus context reads.
    void initAuthoringServer()
    {
        m_authoringServer.applyScriptText = [this](const juce::String& text) { return applyScriptText(text); };
        m_authoringServer.scriptErrorMessage = [this] { return juce::String(scriptErrorMessage()); };
        m_authoringServer.hasScriptError = [this] { return hasScriptError(); };
        m_authoringServer.currentScriptText = [this] { return getScriptText(); };
        m_authoringServer.currentScriptName = [this] { return getCurrentScriptName(); };
        m_authoringServer.currentPatchName = [this] { return getCurrentPatchName(); };
        m_authoringServer.libraryScriptNames = [this] { return getLibraryScriptNames(); };
        m_authoringServer.uiParamSlots = [this]
        {
            const auto slots = getLuaUiParamSlots();
            return std::vector<LuaUiParamSlot>(slots.begin(), slots.end());
        };
        m_authoringServer.cpuLoadPercent = [this] { return getCpuLoad(); };
        m_authoringServer.wrapperTypeDescription = [this]
        { return juce::AudioProcessor::getWrapperTypeDescription(wrapperType); };
        m_authoringServer.patchNames = [this] { return listPatchNames(); };
        m_authoringServer.patchJson = [this](const juce::String& name) { return getPatchJson(name); };
        m_authoringServer.savePatchNamed = [this](const juce::String& name) { return saveCurrentPatchAs(name); };
        m_authoringServer.loadPatchNamed = [this](const juce::String& name) { return applyPatchNamed(name); };
        m_authoringServer.deletePatchNamed = [this](const juce::String& name) { return deletePatchNamed(name); };
        m_authoringServer.libraryScriptSource = [this](const juce::String& name)
        { return getLibraryScriptSource(name); };
        m_authoringServer.applyLibraryScript = [this](const juce::String& name, const juce::String& content)
        { return applyLibraryScript(name, content); };
        m_authoringServer.injectMidi = [this](const juce::MidiMessage& message) { injectAuthoringMidi(message); };
        m_authoringServer.startRecording = [this] { return startAuthoringRecording(); };
        m_authoringServer.stopRecording = [this] { return stopAuthoringRecording(); };
        m_authoringServer.isRecordingActive = [this] { return m_authoringRecorder.isRecording(); };
        m_authoringServer.recordingElapsedSeconds = [this] { return m_authoringRecorder.elapsedSeconds(); };
    }

    // Never auto-started from a saved setting - Authoring Mode requires an explicit
    // toggle every session (see the Editor's Authoring menu), the same "always off on a
    // fresh instance" guarantee the old LlmAssistWatcher's m_llmAssistActive already had.
    bool setAuthoringModeEnabled(const bool enabled)
    {
        if (enabled)
        {
            return m_authoringServer.start(JucePlugin_Name);
        }
        m_authoringServer.stop();
        return false;
    }

    [[nodiscard]] bool isAuthoringModeEnabled() const noexcept
    {
        return m_authoringServer.isRunning();
    }

    [[nodiscard]] juce::URL authoringDashboardUrl() const
    {
        return m_authoringServer.dashboardUrl();
    }


    void computeCpuLoad(std::chrono::nanoseconds elapsed, size_t numSamples)
    {
        samplesProcessed += numSamples;
        elapsedTotalNanoSeconds += static_cast<size_t>(elapsed.count());
        constexpr float secondsPoll = 0.5f;
        if (static_cast<float>(samplesProcessed) > static_cast<float>(m_sampleRate) * secondsPoll)
        {
            const auto pRate = static_cast<float>(100.0 * static_cast<double>(elapsedTotalNanoSeconds) /
                                                  (secondsPoll * 1'000'000'000.0));
            m_runningWindowCpu += static_cast<size_t>(pRate * 100.f);
            m_runningWindowCpu -= m_avgCpu[m_head];
            m_avgCpu[m_head++] = static_cast<size_t>(pRate * 100.f);
            m_head = m_head % m_avgCpu.size();
            m_cpuLoad.store(static_cast<float>(m_runningWindowCpu) * 0.01f / static_cast<float>(m_avgCpu.size()));
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
        // Merges HTTP-injected MIDI (POST /midi) into the real buffer below; gated on
        // Authoring Mode so a normal shipped instance pays no cost when it's off.
        if (isAuthoringModeEnabled())
        {
            m_authoringMidiCollector.removeNextBlockOfMessages(midiMessages, buffer.getNumSamples());
        }

        if (!midiMessages.isEmpty())
        {
            for (const auto& msg : midiMessages)
            {
                pluginRunner->processMidi(msg.data);
                if ((msg.data[0] & 0xF0) == 0xB0)
                {
                    handleMidiCc(msg.data[1], msg.data[2]);
                }
            }
        }
        for (int c = 0; c < std::min(2, buffer.getNumChannels()); ++c)
        {
            m_envInput[c].feed(std::span{buffer.getReadPointer(c), static_cast<size_t>(buffer.getNumSamples())});
            m_inputDb[c].store(std::log10(m_envInput[c].getRms()) * 20.f);
        }
        if (getTotalNumOutputChannels() == 2)
        {
            fixedRunner->processBlock(buffer);
        }
        // Feeds POST /record's WAV capture when a recording is in progress; pushBlock()
        // itself is a no-op otherwise, and this whole call costs nothing when Authoring
        // Mode is off.
        if (isAuthoringModeEnabled())
        {
            m_authoringRecorder.pushBlock(buffer);
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

    [[nodiscard]] bool hasScriptError() const noexcept
    {
        return pluginRunner && pluginRunner->hasScriptError();
    }
    [[nodiscard]] std::string scriptErrorMessage() const
    {
        return pluginRunner ? pluginRunner->scriptError() : std::string{};
    }
    [[nodiscard]] std::string getScriptSkeleton() const
    {
        return pluginRunner ? pluginRunner->scriptSkeleton() : std::string{};
    }
    [[nodiscard]] MorphexsynthScriptEngine::UiParamSlots getLuaUiParamSlots() const
    {
        return pluginRunner ? pluginRunner->uiParamSlots() : MorphexsynthScriptEngine::UiParamSlots{};
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
    std::unique_ptr<MorphexsynthImpl<NumSamplesPerBlock>> pluginRunner;

    juce::AudioProcessorValueTreeState m_parameters;
    struct CcSlot
    {
        std::atomic<int> controller{-1};
        std::atomic<float> valueLow{0.f};
        std::atomic<float> valueHigh{0.f};
    };
    std::array<CcSlot, 8> m_ccActive{};
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
        overrides.reserve(8);
        for (size_t i = 0; i < 8; ++i)
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
        for (size_t i = 0; i < 8; ++i)
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
    AuthoringAudioRecorder m_authoringRecorder;
    juce::MidiMessageCollector m_authoringMidiCollector;
    // Declared last so it is destroyed first - its destructor blocks until every
    // in-flight request finishes, and a handler reaches into this processor meanwhile.
    AuthoringHttpServer m_authoringServer;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioPluginAudioProcessor)
};
