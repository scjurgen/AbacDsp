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
#include "impl/SamplePlayer.h"

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
        m_parameters.addParameterListener("sync", this);
        m_parameters.addParameterListener("type", this);
        m_parameters.addParameterListener("solo", this);
        m_parameters.addParameterListener("vol", this);
        m_parameters.addParameterListener("reverbLevelWet", this);
        m_parameters.addParameterListener("reverbDecay", this);
        m_parameters.addParameterListener("reverbShimmer", this);
        m_parameters.addParameterListener("vol1", this);
        m_parameters.addParameterListener("vol2", this);
        m_parameters.addParameterListener("vol3", this);
        m_parameters.addParameterListener("vol4", this);
        m_parameters.addParameterListener("vol5", this);
        m_parameters.addParameterListener("vol6", this);
        m_parameters.addParameterListener("vol7", this);
        m_parameters.addParameterListener("vol8", this);
        m_parameters.addParameterListener("vol9", this);
        m_parameters.addParameterListener("vol10", this);
        m_parameters.addParameterListener("revFeed1", this);
        m_parameters.addParameterListener("revFeed2", this);
        m_parameters.addParameterListener("revFeed3", this);
        m_parameters.addParameterListener("revFeed4", this);
        m_parameters.addParameterListener("revFeed5", this);
        m_parameters.addParameterListener("revFeed6", this);
        m_parameters.addParameterListener("revFeed7", this);
        m_parameters.addParameterListener("revFeed8", this);
        m_parameters.addParameterListener("revFeed9", this);
        m_parameters.addParameterListener("revFeed10", this);
        m_parameters.addParameterListener("pitch1", this);
        m_parameters.addParameterListener("pitch2", this);
        m_parameters.addParameterListener("pitch3", this);
        m_parameters.addParameterListener("pitch4", this);
        m_parameters.addParameterListener("pitch5", this);
        m_parameters.addParameterListener("pitch6", this);
        m_parameters.addParameterListener("pitch7", this);
        m_parameters.addParameterListener("pitch8", this);
        m_parameters.addParameterListener("pitch9", this);
        m_parameters.addParameterListener("pitch10", this);

        m_fileIo.initialize(m_patchIndex);
    }
    ~AudioPluginAudioProcessor() override
    {
        m_parameters.removeParameterListener("sync", this);
        m_parameters.removeParameterListener("type", this);
        m_parameters.removeParameterListener("solo", this);
        m_parameters.removeParameterListener("vol", this);
        m_parameters.removeParameterListener("reverbLevelWet", this);
        m_parameters.removeParameterListener("reverbDecay", this);
        m_parameters.removeParameterListener("reverbShimmer", this);
        m_parameters.removeParameterListener("vol1", this);
        m_parameters.removeParameterListener("vol2", this);
        m_parameters.removeParameterListener("vol3", this);
        m_parameters.removeParameterListener("vol4", this);
        m_parameters.removeParameterListener("vol5", this);
        m_parameters.removeParameterListener("vol6", this);
        m_parameters.removeParameterListener("vol7", this);
        m_parameters.removeParameterListener("vol8", this);
        m_parameters.removeParameterListener("vol9", this);
        m_parameters.removeParameterListener("vol10", this);
        m_parameters.removeParameterListener("revFeed1", this);
        m_parameters.removeParameterListener("revFeed2", this);
        m_parameters.removeParameterListener("revFeed3", this);
        m_parameters.removeParameterListener("revFeed4", this);
        m_parameters.removeParameterListener("revFeed5", this);
        m_parameters.removeParameterListener("revFeed6", this);
        m_parameters.removeParameterListener("revFeed7", this);
        m_parameters.removeParameterListener("revFeed8", this);
        m_parameters.removeParameterListener("revFeed9", this);
        m_parameters.removeParameterListener("revFeed10", this);
        m_parameters.removeParameterListener("pitch1", this);
        m_parameters.removeParameterListener("pitch2", this);
        m_parameters.removeParameterListener("pitch3", this);
        m_parameters.removeParameterListener("pitch4", this);
        m_parameters.removeParameterListener("pitch5", this);
        m_parameters.removeParameterListener("pitch6", this);
        m_parameters.removeParameterListener("pitch7", this);
        m_parameters.removeParameterListener("pitch8", this);
        m_parameters.removeParameterListener("pitch9", this);
        m_parameters.removeParameterListener("pitch10", this);
    }

    void prepareToPlay(const double sampleRate, const int samplesPerBlock) override
    {
        pluginRunner = std::make_unique<SamplePlayer<NumSamplesPerBlock>>(RateNormalizer::kInternalSampleRate);

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
        params.push_back(std::make_unique<juce::AudioParameterBool>(juce::ParameterID("sync", 1), "Sync", 0));
        params.push_back(std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID("type", 1), "Type",
            juce::StringArray{"PITCHONLY", "DOLPHINS", "WIND", "RAINFOREST", "FOREST", "RAIN", "THUNDER", "TANPURA",
                              "STREAM", "OCEAN", "BOWLS", "CAFE", "RPGFOREST", "WHITEPULSE"},
            0));
        params.push_back(std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID("solo", 1), "Solo",
            juce::StringArray{"None", "Track 1", "Track 2", "Track 3", "Track 4", "Track 5", "Track 6", "Track 7",
                              "Track 8", "Track 9", "Track 10"},
            0));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("vol", 1), "Vol", juce::NormalisableRange<float>(-100, 12, 0.1, 1, false), 0,
            juce::AudioParameterFloatAttributes{}.withLabel("dB").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 1) + " dB"; })));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("reverbLevelWet", 1), "Reverb Wet", juce::NormalisableRange<float>(-120, 0, 1, 1, false),
            -120,
            juce::AudioParameterFloatAttributes{}.withLabel("dB").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 0) + " dB"; })));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("reverbDecay", 1), "Rev Decay",
            juce::NormalisableRange<float>(100, 100000, 1, 0.2, false), 1000,
            juce::AudioParameterFloatAttributes{}.withLabel("ms").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 0) + " ms"; })));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("reverbShimmer", 1), "Rev Shimmer", juce::NormalisableRange<float>(0, 100, 1, 1, false),
            0,
            juce::AudioParameterFloatAttributes{}.withLabel("%").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 0) + " %"; })));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("vol1", 1), "Vol 1", juce::NormalisableRange<float>(-100, 0, 0.1, 1, false), 0,
            juce::AudioParameterFloatAttributes{}.withLabel("dB").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 1) + " dB"; })));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("vol2", 1), "Vol 2", juce::NormalisableRange<float>(-100, 0, 0.1, 1, false), 0,
            juce::AudioParameterFloatAttributes{}.withLabel("dB").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 1) + " dB"; })));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("vol3", 1), "Vol 3", juce::NormalisableRange<float>(-100, 0, 0.1, 1, false), 0,
            juce::AudioParameterFloatAttributes{}.withLabel("dB").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 1) + " dB"; })));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("vol4", 1), "Vol 4", juce::NormalisableRange<float>(-100, 0, 0.1, 1, false), 0,
            juce::AudioParameterFloatAttributes{}.withLabel("dB").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 1) + " dB"; })));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("vol5", 1), "Vol 5", juce::NormalisableRange<float>(-100, 0, 0.1, 1, false), 0,
            juce::AudioParameterFloatAttributes{}.withLabel("dB").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 1) + " dB"; })));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("vol6", 1), "Vol 6", juce::NormalisableRange<float>(-100, 0, 0.1, 1, false), 0,
            juce::AudioParameterFloatAttributes{}.withLabel("dB").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 1) + " dB"; })));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("vol7", 1), "Vol 7", juce::NormalisableRange<float>(-100, 0, 0.1, 1, false), 0,
            juce::AudioParameterFloatAttributes{}.withLabel("dB").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 1) + " dB"; })));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("vol8", 1), "Vol 8", juce::NormalisableRange<float>(-100, 0, 0.1, 1, false), 0,
            juce::AudioParameterFloatAttributes{}.withLabel("dB").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 1) + " dB"; })));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("vol9", 1), "Vol 9", juce::NormalisableRange<float>(-100, 0, 0.1, 1, false), 0,
            juce::AudioParameterFloatAttributes{}.withLabel("dB").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 1) + " dB"; })));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("vol10", 1), "Vol 10", juce::NormalisableRange<float>(-100, 0, 0.1, 1, false), 0,
            juce::AudioParameterFloatAttributes{}.withLabel("dB").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 1) + " dB"; })));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("revFeed1", 1), "Rev 1", juce::NormalisableRange<float>(-100, 0, 1, 1, false), 0,
            juce::AudioParameterFloatAttributes{}.withLabel("dB").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 0) + " dB"; })));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("revFeed2", 1), "Rev 2", juce::NormalisableRange<float>(-100, 0, 1, 1, false), 0,
            juce::AudioParameterFloatAttributes{}.withLabel("dB").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 0) + " dB"; })));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("revFeed3", 1), "Rev 3", juce::NormalisableRange<float>(-100, 0, 1, 1, false), 0,
            juce::AudioParameterFloatAttributes{}.withLabel("dB").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 0) + " dB"; })));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("revFeed4", 1), "Rev 4", juce::NormalisableRange<float>(-100, 0, 1, 1, false), 0,
            juce::AudioParameterFloatAttributes{}.withLabel("dB").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 0) + " dB"; })));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("revFeed5", 1), "Rev 5", juce::NormalisableRange<float>(-100, 0, 1, 1, false), 0,
            juce::AudioParameterFloatAttributes{}.withLabel("dB").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 0) + " dB"; })));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("revFeed6", 1), "Rev 6", juce::NormalisableRange<float>(-100, 0, 1, 1, false), 0,
            juce::AudioParameterFloatAttributes{}.withLabel("dB").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 0) + " dB"; })));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("revFeed7", 1), "Rev 7", juce::NormalisableRange<float>(-100, 0, 1, 1, false), 0,
            juce::AudioParameterFloatAttributes{}.withLabel("dB").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 0) + " dB"; })));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("revFeed8", 1), "Rev 8", juce::NormalisableRange<float>(-100, 0, 1, 1, false), 0,
            juce::AudioParameterFloatAttributes{}.withLabel("dB").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 0) + " dB"; })));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("revFeed9", 1), "Rev 9", juce::NormalisableRange<float>(-100, 0, 1, 1, false), 0,
            juce::AudioParameterFloatAttributes{}.withLabel("dB").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 0) + " dB"; })));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("revFeed10", 1), "Rev 10", juce::NormalisableRange<float>(-100, 0, 1, 1, false), 0,
            juce::AudioParameterFloatAttributes{}.withLabel("dB").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 0) + " dB"; })));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("pitch1", 1), "Pch 1", juce::NormalisableRange<float>(-48, 48, 1, 1, false), 0,
            juce::AudioParameterFloatAttributes{}.withLabel("").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 0) + " "; })));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("pitch2", 1), "Pch 2", juce::NormalisableRange<float>(-48, 48, 1, 1, false), 0,
            juce::AudioParameterFloatAttributes{}.withLabel("").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 0) + " "; })));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("pitch3", 1), "Pch 3", juce::NormalisableRange<float>(-48, 48, 1, 1, false), 0,
            juce::AudioParameterFloatAttributes{}.withLabel("").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 0) + " "; })));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("pitch4", 1), "Pch 4", juce::NormalisableRange<float>(-48, 48, 1, 1, false), 0,
            juce::AudioParameterFloatAttributes{}.withLabel("").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 0) + " "; })));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("pitch5", 1), "Pch 5", juce::NormalisableRange<float>(-48, 48, 1, 1, false), 0,
            juce::AudioParameterFloatAttributes{}.withLabel("").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 0) + " "; })));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("pitch6", 1), "Pch 6", juce::NormalisableRange<float>(-48, 48, 1, 1, false), 0,
            juce::AudioParameterFloatAttributes{}.withLabel("").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 0) + " "; })));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("pitch7", 1), "Pch 7", juce::NormalisableRange<float>(-48, 48, 1, 1, false), 0,
            juce::AudioParameterFloatAttributes{}.withLabel("").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 0) + " "; })));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("pitch8", 1), "Pch 8", juce::NormalisableRange<float>(-48, 48, 1, 1, false), 0,
            juce::AudioParameterFloatAttributes{}.withLabel("").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 0) + " "; })));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("pitch9", 1), "Pch 9", juce::NormalisableRange<float>(-48, 48, 1, 1, false), 0,
            juce::AudioParameterFloatAttributes{}.withLabel("").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 0) + " "; })));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("pitch10", 1), "Pch 10", juce::NormalisableRange<float>(-48, 48, 1, 1, false), 0,
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
            {"sync",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setSync(static_cast<bool>(v));
                 p.m_fileIo.updateParameter(PatchParameters::Id::sync, v);
             }},
            {"type",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setType(static_cast<size_t>(v));
                 p.m_fileIo.updateParameter(PatchParameters::Id::type, v);
             }},
            {"solo",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setSolo(static_cast<size_t>(v));
                 p.m_fileIo.updateParameter(PatchParameters::Id::solo, v);
             }},
            {"vol",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setVol(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::vol, v);
             }},
            {"reverbLevelWet",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setReverbLevelWet(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::reverbLevelWet, v);
             }},
            {"reverbDecay",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setReverbDecay(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::reverbDecay, v);
             }},
            {"reverbShimmer",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setReverbShimmer(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::reverbShimmer, v);
             }},
            {"vol1",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setVol1(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::vol1, v);
             }},
            {"vol2",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setVol2(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::vol2, v);
             }},
            {"vol3",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setVol3(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::vol3, v);
             }},
            {"vol4",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setVol4(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::vol4, v);
             }},
            {"vol5",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setVol5(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::vol5, v);
             }},
            {"vol6",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setVol6(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::vol6, v);
             }},
            {"vol7",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setVol7(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::vol7, v);
             }},
            {"vol8",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setVol8(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::vol8, v);
             }},
            {"vol9",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setVol9(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::vol9, v);
             }},
            {"vol10",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setVol10(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::vol10, v);
             }},
            {"revFeed1",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setRevFeed1(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::revFeed1, v);
             }},
            {"revFeed2",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setRevFeed2(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::revFeed2, v);
             }},
            {"revFeed3",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setRevFeed3(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::revFeed3, v);
             }},
            {"revFeed4",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setRevFeed4(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::revFeed4, v);
             }},
            {"revFeed5",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setRevFeed5(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::revFeed5, v);
             }},
            {"revFeed6",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setRevFeed6(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::revFeed6, v);
             }},
            {"revFeed7",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setRevFeed7(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::revFeed7, v);
             }},
            {"revFeed8",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setRevFeed8(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::revFeed8, v);
             }},
            {"revFeed9",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setRevFeed9(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::revFeed9, v);
             }},
            {"revFeed10",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setRevFeed10(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::revFeed10, v);
             }},
            {"pitch1",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setPitch1(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::pitch1, v);
             }},
            {"pitch2",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setPitch2(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::pitch2, v);
             }},
            {"pitch3",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setPitch3(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::pitch3, v);
             }},
            {"pitch4",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setPitch4(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::pitch4, v);
             }},
            {"pitch5",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setPitch5(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::pitch5, v);
             }},
            {"pitch6",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setPitch6(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::pitch6, v);
             }},
            {"pitch7",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setPitch7(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::pitch7, v);
             }},
            {"pitch8",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setPitch8(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::pitch8, v);
             }},
            {"pitch9",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setPitch9(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::pitch9, v);
             }},
            {"pitch10",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setPitch10(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::pitch10, v);
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
        if (auto* p = m_parameters.getParameter("sync"))
        {
            const auto& range = m_parameters.getParameterRange("sync");
            float normalized = range.convertTo0to1(params.sync);
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("type"))
        {
            const auto& range = m_parameters.getParameterRange("type");
            float normalized = range.convertTo0to1(params.type);
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("solo"))
        {
            const auto& range = m_parameters.getParameterRange("solo");
            float normalized = range.convertTo0to1(params.solo);
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("vol"))
        {
            const auto& range = m_parameters.getParameterRange("vol");
            float normalized = range.convertTo0to1(params.vol);
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("reverbLevelWet"))
        {
            const auto& range = m_parameters.getParameterRange("reverbLevelWet");
            float normalized = range.convertTo0to1(params.reverbLevelWet);
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("reverbDecay"))
        {
            const auto& range = m_parameters.getParameterRange("reverbDecay");
            float normalized = range.convertTo0to1(params.reverbDecay);
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("reverbShimmer"))
        {
            const auto& range = m_parameters.getParameterRange("reverbShimmer");
            float normalized = range.convertTo0to1(params.reverbShimmer);
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("vol1"))
        {
            const auto& range = m_parameters.getParameterRange("vol1");
            float normalized = range.convertTo0to1(params.vol1);
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("vol2"))
        {
            const auto& range = m_parameters.getParameterRange("vol2");
            float normalized = range.convertTo0to1(params.vol2);
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("vol3"))
        {
            const auto& range = m_parameters.getParameterRange("vol3");
            float normalized = range.convertTo0to1(params.vol3);
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("vol4"))
        {
            const auto& range = m_parameters.getParameterRange("vol4");
            float normalized = range.convertTo0to1(params.vol4);
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("vol5"))
        {
            const auto& range = m_parameters.getParameterRange("vol5");
            float normalized = range.convertTo0to1(params.vol5);
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("vol6"))
        {
            const auto& range = m_parameters.getParameterRange("vol6");
            float normalized = range.convertTo0to1(params.vol6);
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("vol7"))
        {
            const auto& range = m_parameters.getParameterRange("vol7");
            float normalized = range.convertTo0to1(params.vol7);
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("vol8"))
        {
            const auto& range = m_parameters.getParameterRange("vol8");
            float normalized = range.convertTo0to1(params.vol8);
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("vol9"))
        {
            const auto& range = m_parameters.getParameterRange("vol9");
            float normalized = range.convertTo0to1(params.vol9);
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("vol10"))
        {
            const auto& range = m_parameters.getParameterRange("vol10");
            float normalized = range.convertTo0to1(params.vol10);
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("revFeed1"))
        {
            const auto& range = m_parameters.getParameterRange("revFeed1");
            float normalized = range.convertTo0to1(params.revFeed1);
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("revFeed2"))
        {
            const auto& range = m_parameters.getParameterRange("revFeed2");
            float normalized = range.convertTo0to1(params.revFeed2);
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("revFeed3"))
        {
            const auto& range = m_parameters.getParameterRange("revFeed3");
            float normalized = range.convertTo0to1(params.revFeed3);
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("revFeed4"))
        {
            const auto& range = m_parameters.getParameterRange("revFeed4");
            float normalized = range.convertTo0to1(params.revFeed4);
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("revFeed5"))
        {
            const auto& range = m_parameters.getParameterRange("revFeed5");
            float normalized = range.convertTo0to1(params.revFeed5);
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("revFeed6"))
        {
            const auto& range = m_parameters.getParameterRange("revFeed6");
            float normalized = range.convertTo0to1(params.revFeed6);
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("revFeed7"))
        {
            const auto& range = m_parameters.getParameterRange("revFeed7");
            float normalized = range.convertTo0to1(params.revFeed7);
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("revFeed8"))
        {
            const auto& range = m_parameters.getParameterRange("revFeed8");
            float normalized = range.convertTo0to1(params.revFeed8);
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("revFeed9"))
        {
            const auto& range = m_parameters.getParameterRange("revFeed9");
            float normalized = range.convertTo0to1(params.revFeed9);
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("revFeed10"))
        {
            const auto& range = m_parameters.getParameterRange("revFeed10");
            float normalized = range.convertTo0to1(params.revFeed10);
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("pitch1"))
        {
            const auto& range = m_parameters.getParameterRange("pitch1");
            float normalized = range.convertTo0to1(params.pitch1);
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("pitch2"))
        {
            const auto& range = m_parameters.getParameterRange("pitch2");
            float normalized = range.convertTo0to1(params.pitch2);
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("pitch3"))
        {
            const auto& range = m_parameters.getParameterRange("pitch3");
            float normalized = range.convertTo0to1(params.pitch3);
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("pitch4"))
        {
            const auto& range = m_parameters.getParameterRange("pitch4");
            float normalized = range.convertTo0to1(params.pitch4);
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("pitch5"))
        {
            const auto& range = m_parameters.getParameterRange("pitch5");
            float normalized = range.convertTo0to1(params.pitch5);
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("pitch6"))
        {
            const auto& range = m_parameters.getParameterRange("pitch6");
            float normalized = range.convertTo0to1(params.pitch6);
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("pitch7"))
        {
            const auto& range = m_parameters.getParameterRange("pitch7");
            float normalized = range.convertTo0to1(params.pitch7);
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("pitch8"))
        {
            const auto& range = m_parameters.getParameterRange("pitch8");
            float normalized = range.convertTo0to1(params.pitch8);
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("pitch9"))
        {
            const auto& range = m_parameters.getParameterRange("pitch9");
            float normalized = range.convertTo0to1(params.pitch9);
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("pitch10"))
        {
            const auto& range = m_parameters.getParameterRange("pitch10");
            float normalized = range.convertTo0to1(params.pitch10);
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
        if (getTotalNumOutputChannels() == 2)
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
    std::unique_ptr<SamplePlayer<NumSamplesPerBlock>> pluginRunner;

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
