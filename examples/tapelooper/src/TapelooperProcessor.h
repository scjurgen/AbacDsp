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
        m_parameters.addParameterListener("grooveHumanizePush", this);
        m_parameters.addParameterListener("grooveHumanizeLife", this);
        m_parameters.addParameterListener("trackGainA", this);
        m_parameters.addParameterListener("trackGainB", this);
        m_parameters.addParameterListener("trackGainC", this);
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
        m_parameters.removeParameterListener("grooveHumanizePush", this);
        m_parameters.removeParameterListener("grooveHumanizeLife", this);
        m_parameters.removeParameterListener("trackGainA", this);
        m_parameters.removeParameterListener("trackGainB", this);
        m_parameters.removeParameterListener("trackGainC", this);
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
        pluginRunner = std::make_unique<TapeLooperImpl<NumSamplesPerBlock>>(RateNormalizer::kInternalSampleRate);
        pluginRunner->setImportResolver([](const std::string_view name) { return FileIo::resolveLibraryScript(name); });
        if (!m_fileIo.currentScript().empty())
        {
            pluginRunner->setScript(m_fileIo.currentScript());
        }
        pluginRunner->setLoopsDirectory(getLoopsDirectory());

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
            juce::NormalisableRange<float>(0.25, 4.0, 0.01, 1.0, false), 1.0,
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
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("grooveHumanizePush", 1), juce::String::fromUTF8("Groove Push"),
            juce::NormalisableRange<float>(-100, 100, 1, 1, false), 0,
            juce::AudioParameterFloatAttributes{}.withLabel("%").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 1) + " %"; })));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("grooveHumanizeLife", 1), juce::String::fromUTF8("Groove Life"),
            juce::NormalisableRange<float>(0, 100, 1, 1, false), 100,
            juce::AudioParameterFloatAttributes{}.withLabel("%").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 1) + " %"; })));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("trackGainA", 1), juce::String::fromUTF8("Track Gain A"),
            juce::NormalisableRange<float>(-60, 12, 0.1, 1, false), 0,
            juce::AudioParameterFloatAttributes{}.withLabel("dB").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 1) + " dB"; })));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("trackGainB", 1), juce::String::fromUTF8("Track Gain B"),
            juce::NormalisableRange<float>(-60, 12, 0.1, 1, false), 0,
            juce::AudioParameterFloatAttributes{}.withLabel("dB").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 1) + " dB"; })));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("trackGainC", 1), juce::String::fromUTF8("Track Gain C"),
            juce::NormalisableRange<float>(-60, 12, 0.1, 1, false), 0,
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
            {"grooveHumanizePush",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setGrooveHumanizePush(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::grooveHumanizePush, v);
             }},
            {"grooveHumanizeLife",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setGrooveHumanizeLife(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::grooveHumanizeLife, v);
             }},
            {"trackGainA",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setTrackGainA(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::trackGainA, v);
             }},
            {"trackGainB",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setTrackGainB(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::trackGainB, v);
             }},
            {"trackGainC",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setTrackGainC(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::trackGainC, v);
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
        if (auto* p = m_parameters.getParameter("tapeSpeed"))
        {
            const auto& range = m_parameters.getParameterRange("tapeSpeed");
            float normalized = range.convertTo0to1(static_cast<float>(params.tapeSpeed));
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("bars"))
        {
            const auto& range = m_parameters.getParameterRange("bars");
            float normalized = range.convertTo0to1(static_cast<float>(params.bars));
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("inputGain"))
        {
            const auto& range = m_parameters.getParameterRange("inputGain");
            float normalized = range.convertTo0to1(static_cast<float>(params.inputGain));
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("grooveLevel"))
        {
            const auto& range = m_parameters.getParameterRange("grooveLevel");
            float normalized = range.convertTo0to1(static_cast<float>(params.grooveLevel));
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("recordA"))
        {
            const auto& range = m_parameters.getParameterRange("recordA");
            float normalized = range.convertTo0to1(static_cast<float>(params.recordA));
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("playA"))
        {
            const auto& range = m_parameters.getParameterRange("playA");
            float normalized = range.convertTo0to1(static_cast<float>(params.playA));
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("clearA"))
        {
            const auto& range = m_parameters.getParameterRange("clearA");
            float normalized = range.convertTo0to1(static_cast<float>(params.clearA));
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("recordB"))
        {
            const auto& range = m_parameters.getParameterRange("recordB");
            float normalized = range.convertTo0to1(static_cast<float>(params.recordB));
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("playB"))
        {
            const auto& range = m_parameters.getParameterRange("playB");
            float normalized = range.convertTo0to1(static_cast<float>(params.playB));
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("clearB"))
        {
            const auto& range = m_parameters.getParameterRange("clearB");
            float normalized = range.convertTo0to1(static_cast<float>(params.clearB));
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("recordC"))
        {
            const auto& range = m_parameters.getParameterRange("recordC");
            float normalized = range.convertTo0to1(static_cast<float>(params.recordC));
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("playC"))
        {
            const auto& range = m_parameters.getParameterRange("playC");
            float normalized = range.convertTo0to1(static_cast<float>(params.playC));
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("clearC"))
        {
            const auto& range = m_parameters.getParameterRange("clearC");
            float normalized = range.convertTo0to1(static_cast<float>(params.clearC));
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("groovePlay"))
        {
            const auto& range = m_parameters.getParameterRange("groovePlay");
            float normalized = range.convertTo0to1(static_cast<float>(params.groovePlay));
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
        if (auto* p = m_parameters.getParameter("grooveHumanizePush"))
        {
            const auto& range = m_parameters.getParameterRange("grooveHumanizePush");
            float normalized = range.convertTo0to1(static_cast<float>(params.grooveHumanizePush));
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("grooveHumanizeLife"))
        {
            const auto& range = m_parameters.getParameterRange("grooveHumanizeLife");
            float normalized = range.convertTo0to1(static_cast<float>(params.grooveHumanizeLife));
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("trackGainA"))
        {
            const auto& range = m_parameters.getParameterRange("trackGainA");
            float normalized = range.convertTo0to1(static_cast<float>(params.trackGainA));
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("trackGainB"))
        {
            const auto& range = m_parameters.getParameterRange("trackGainB");
            float normalized = range.convertTo0to1(static_cast<float>(params.trackGainB));
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("trackGainC"))
        {
            const auto& range = m_parameters.getParameterRange("trackGainC");
            float normalized = range.convertTo0to1(static_cast<float>(params.trackGainC));
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

    [[nodiscard]] const std::vector<float>& getWaveDataToShow()
    {
        return pluginRunner->visualizeWaveData();
    }
    [[nodiscard]] bool isGroovePlaying() const noexcept
    {
        return pluginRunner && pluginRunner->isGroovePlaying();
    }
    [[nodiscard]] bool canEditBpm() const noexcept
    {
        return pluginRunner && pluginRunner->canEditBpm();
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
    [[nodiscard]] TapeLooperScriptEngine::UiParamSlots getLuaUiParamSlots() const
    {
        return pluginRunner ? pluginRunner->uiParamSlots() : TapeLooperScriptEngine::UiParamSlots{};
    }
    [[nodiscard]] size_t getSamplesPerBar() const noexcept
    {
        return pluginRunner ? pluginRunner->getSamplesPerBar() : 0u;
    }
    [[nodiscard]] int getBarBeats() const noexcept
    {
        return pluginRunner ? pluginRunner->getBarBeats() : 4;
    }
    [[nodiscard]] float getBarPhase() const noexcept
    {
        return pluginRunner ? pluginRunner->getBarPhase() : 0.f;
    }
    [[nodiscard]] std::vector<float> getLoopWaveform() const
    {
        return pluginRunner ? pluginRunner->getLoopWaveform() : std::vector<float>{};
    }
    [[nodiscard]] float getPlayheadNormalized() const noexcept
    {
        return pluginRunner ? pluginRunner->getPlayheadNormalized() : 0.f;
    }
    [[nodiscard]] int getOuterRingBars() const noexcept
    {
        return pluginRunner ? pluginRunner->getOuterRingBars() : 1;
    }
    [[nodiscard]] AbacDsp::SpectrumImageSet getSpectrogramData() const
    {
        return pluginRunner ? pluginRunner->getSpectrogramData() : AbacDsp::SpectrumImageSet{};
    }
    [[nodiscard]] int getTrackClockStateA() const noexcept
    {
        return pluginRunner ? pluginRunner->getTrackClockStateA() : 0;
    }
    [[nodiscard]] int getTrackClockStateB() const noexcept
    {
        return pluginRunner ? pluginRunner->getTrackClockStateB() : 0;
    }
    [[nodiscard]] int getTrackClockStateC() const noexcept
    {
        return pluginRunner ? pluginRunner->getTrackClockStateC() : 0;
    }
    [[nodiscard]] int getGrooveClockState() const noexcept
    {
        return pluginRunner ? pluginRunner->getGrooveClockState() : 0;
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
    static std::string getLoopsDirectory()
    {
        auto base = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory);
#if JUCE_MAC
        base = base.getChildFile("Application Support");
#endif
        const auto dir = base.getChildFile("AbacDsp").getChildFile("Tapelooper").getChildFile("loops");
        dir.createDirectory();
        return dir.getFullPathName().toStdString();
    }
    [[nodiscard]] std::vector<juce::String> listLoopNames() const
    {
        std::vector<juce::String> result;
        if (pluginRunner)
        {
            for (const auto& n : pluginRunner->listLoopNames())
            {
                result.push_back(juce::String(n));
            }
        }
        return result;
    }
    [[nodiscard]] juce::String getCurrentLoopName() const
    {
        return pluginRunner ? juce::String(pluginRunner->currentLoopName()) : juce::String();
    }
    void saveLoopAs(const juce::String& name)
    {
        if (pluginRunner)
        {
            pluginRunner->requestSaveLoopAs(name.toStdString(), m_fileIo.currentParametersAsJson());
        }
    }
    void applyLoadedLoopPatchParams(const juce::String& json)
    {
        if (m_fileIo.loadParametersFromJson(json.toStdString()))
        {
            applyLoadedParametersToHost();
        }
    }
    void requestLoadLoop(const juce::String& name)
    {
        if (pluginRunner)
        {
            pluginRunner->requestLoadLoop(name.toStdString());
        }
    }
    bool deleteLoopNamed(const juce::String& name)
    {
        return pluginRunner && pluginRunner->deleteLoopNamed(name.toStdString());
    }
    bool renameLoop(const juce::String& oldName, const juce::String& newName)
    {
        return pluginRunner && pluginRunner->renameLoopNamed(oldName.toStdString(), newName.toStdString());
    }
    [[nodiscard]] TapeLooperImpl<NumSamplesPerBlock>::LoopLoadOutcome consumeLoopLoadOutcome() const
    {
        return pluginRunner ? pluginRunner->consumeLoopLoadOutcome()
                            : TapeLooperImpl<NumSamplesPerBlock>::LoopLoadOutcome{};
    }
    void resolveLoopLoadBpm(const float bpm)
    {
        if (pluginRunner)
        {
            pluginRunner->resolveLoopLoadBpm(bpm);
        }
    }
    [[nodiscard]] juce::String getBarBeatLabel() const
    {
        return pluginRunner ? juce::String(pluginRunner->getBarBeatLabel()) : juce::String();
    }
    [[nodiscard]] const std::vector<size_t>& getSpectrogramSliceBuckets() const noexcept
    {
        static const std::vector<size_t> empty{};
        return pluginRunner ? pluginRunner->getSpectrogramSliceBuckets() : empty;
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
    std::unique_ptr<TapeLooperImpl<NumSamplesPerBlock>> pluginRunner;

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
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioPluginAudioProcessor)
};
