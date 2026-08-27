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
#include "impl/MaxDiffuserImpl.h"

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
        m_parameters.addParameterListener("dry", this);
        m_parameters.addParameterListener("wet", this);
        m_parameters.addParameterListener("preDelay", this);
        m_parameters.addParameterListener("elements", this);
        m_parameters.addParameterListener("tapSpan", this);
        m_parameters.addParameterListener("feedback", this);
        m_parameters.addParameterListener("bulge", this);
        m_parameters.addParameterListener("bottomSize", this);
        m_parameters.addParameterListener("topSize", this);
        m_parameters.addParameterListener("sizeSpread", this);
        m_parameters.addParameterListener("modulationDepth", this);
        m_parameters.addParameterListener("modulationSpeed", this);
        m_parameters.addParameterListener("lowPass", this);
        m_parameters.addParameterListener("mix", this);
        m_parameters.addParameterListener("pitch", this);
        m_parameters.addParameterListener("pitchDelay", this);
        m_parameters.addParameterListener("pitch2", this);
        m_parameters.addParameterListener("pitch2Delay", this);
        m_parameters.addParameterListener("pitchMode", this);
        m_parameters.addParameterListener("fdnMix", this);
        m_parameters.addParameterListener("fdnSize", this);
        m_parameters.addParameterListener("fdnDecay", this);
        m_parameters.addParameterListener("drive", this);
        m_parameters.addParameterListener("eqInLow", this);
        m_parameters.addParameterListener("eqInMid", this);
        m_parameters.addParameterListener("eqInHigh", this);
        m_parameters.addParameterListener("eqOutLow", this);
        m_parameters.addParameterListener("eqOutMid", this);
        m_parameters.addParameterListener("eqOutHigh", this);
        m_parameters.addParameterListener("level", this);
        m_parameters.addParameterListener("pitcherShelfLow", this);
        m_parameters.addParameterListener("pitcherShelfHigh", this);
        m_parameters.addParameterListener("extremeStereoTap", this);
        m_parameters.addParameterListener("wide", this);
        m_parameters.addParameterListener("reverbShelfLow", this);
        m_parameters.addParameterListener("reverbShelfHigh", this);

        for (size_t i = 0; i < 34; ++i)
        {
            m_ccActive[i].controller.store(kDefaultCcMappings[i].controller, std::memory_order_relaxed);
            m_ccActive[i].valueLow.store(kDefaultCcMappings[i].valueLow, std::memory_order_relaxed);
            m_ccActive[i].valueHigh.store(kDefaultCcMappings[i].valueHigh, std::memory_order_relaxed);
        }
        m_fileIo.initialize(m_patchIndex);
    }
    ~AudioPluginAudioProcessor() override
    {
        m_parameters.removeParameterListener("dry", this);
        m_parameters.removeParameterListener("wet", this);
        m_parameters.removeParameterListener("preDelay", this);
        m_parameters.removeParameterListener("elements", this);
        m_parameters.removeParameterListener("tapSpan", this);
        m_parameters.removeParameterListener("feedback", this);
        m_parameters.removeParameterListener("bulge", this);
        m_parameters.removeParameterListener("bottomSize", this);
        m_parameters.removeParameterListener("topSize", this);
        m_parameters.removeParameterListener("sizeSpread", this);
        m_parameters.removeParameterListener("modulationDepth", this);
        m_parameters.removeParameterListener("modulationSpeed", this);
        m_parameters.removeParameterListener("lowPass", this);
        m_parameters.removeParameterListener("mix", this);
        m_parameters.removeParameterListener("pitch", this);
        m_parameters.removeParameterListener("pitchDelay", this);
        m_parameters.removeParameterListener("pitch2", this);
        m_parameters.removeParameterListener("pitch2Delay", this);
        m_parameters.removeParameterListener("pitchMode", this);
        m_parameters.removeParameterListener("fdnMix", this);
        m_parameters.removeParameterListener("fdnSize", this);
        m_parameters.removeParameterListener("fdnDecay", this);
        m_parameters.removeParameterListener("drive", this);
        m_parameters.removeParameterListener("eqInLow", this);
        m_parameters.removeParameterListener("eqInMid", this);
        m_parameters.removeParameterListener("eqInHigh", this);
        m_parameters.removeParameterListener("eqOutLow", this);
        m_parameters.removeParameterListener("eqOutMid", this);
        m_parameters.removeParameterListener("eqOutHigh", this);
        m_parameters.removeParameterListener("level", this);
        m_parameters.removeParameterListener("pitcherShelfLow", this);
        m_parameters.removeParameterListener("pitcherShelfHigh", this);
        m_parameters.removeParameterListener("extremeStereoTap", this);
        m_parameters.removeParameterListener("wide", this);
        m_parameters.removeParameterListener("reverbShelfLow", this);
        m_parameters.removeParameterListener("reverbShelfHigh", this);
    }

    void prepareToPlay(const double sampleRate, const int samplesPerBlock) override
    {
        pluginRunner = std::make_unique<MaxDiffuserImpl<NumSamplesPerBlock>>(RateNormalizer::kInternalSampleRate);

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
            for (size_t i = 0; i < 34; ++i)
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
            juce::ParameterID("dry", 1), juce::String::fromUTF8("Dry"),
            juce::NormalisableRange<float>(-100, 12, 0.1, 1, false), 0,
            juce::AudioParameterFloatAttributes{}.withLabel("dB").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 1) + " dB"; })));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("wet", 1), juce::String::fromUTF8("Wet"),
            juce::NormalisableRange<float>(-100, 12, 0.1, 1, false), -6,
            juce::AudioParameterFloatAttributes{}.withLabel("dB").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 1) + " dB"; })));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("preDelay", 1), juce::String::fromUTF8("Pre Delay"),
            juce::NormalisableRange<float>(0, 1000, 1, 0.5, false), 0,
            juce::AudioParameterFloatAttributes{}.withLabel("ms").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 0) + " ms"; })));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("elements", 1), juce::String::fromUTF8("Elements"),
            juce::NormalisableRange<float>(0, 100, 1, 1, false), 6,
            juce::AudioParameterFloatAttributes{}.withLabel("").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 0) + " "; })));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("tapSpan", 1), juce::String::fromUTF8("Tap Span"),
            juce::NormalisableRange<float>(0, 100, 1, 1, false), 0,
            juce::AudioParameterFloatAttributes{}.withLabel("%").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 0) + " %"; })));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("feedback", 1), juce::String::fromUTF8("Diffusion"),
            juce::NormalisableRange<float>(-100, 100, 0.1, 1, false), 50,
            juce::AudioParameterFloatAttributes{}.withLabel("%").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 1) + " %"; })));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("bulge", 1), juce::String::fromUTF8("Bulge"),
            juce::NormalisableRange<float>(-1, 1, 0.01, 1, false), 0.46,
            juce::AudioParameterFloatAttributes{}.withLabel("").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 2) + " "; })));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("bottomSize", 1), juce::String::fromUTF8("Early Size"),
            juce::NormalisableRange<float>(0.5, 100.0, 0.1, 0.4, false), 0.7,
            juce::AudioParameterFloatAttributes{}.withLabel("m").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 1) + " m"; })));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("topSize", 1), juce::String::fromUTF8("Late Size"),
            juce::NormalisableRange<float>(0.5, 100.0, 0.1, 0.4, false), 7.0,
            juce::AudioParameterFloatAttributes{}.withLabel("m").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 1) + " m"; })));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("sizeSpread", 1), juce::String::fromUTF8("Size Spread"),
            juce::NormalisableRange<float>(0, 10, 0.01, 0.5, false), 0,
            juce::AudioParameterFloatAttributes{}.withLabel("m").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 2) + " m"; })));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("modulationDepth", 1), juce::String::fromUTF8("Mod Depth"),
            juce::NormalisableRange<float>(0, 1, 0.001, 0.35, false), 0,
            juce::AudioParameterFloatAttributes{}.withLabel("").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 3) + " "; })));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("modulationSpeed", 1), juce::String::fromUTF8("Mod Speed"),
            juce::NormalisableRange<float>(0.01, 5, 0.01, 0.5, false), 0.5,
            juce::AudioParameterFloatAttributes{}.withLabel("Hz").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 2) + " Hz"; })));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("lowPass", 1), juce::String::fromUTF8("Low Pass"),
            juce::NormalisableRange<float>(20, 20000, 1, 0.5, false), 12000,
            juce::AudioParameterFloatAttributes{}.withLabel("Hz").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 0) + " Hz"; })));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("mix", 1), juce::String::fromUTF8("Pitch Mix"),
            juce::NormalisableRange<float>(0, 100, 0.1, 1, false), 0,
            juce::AudioParameterFloatAttributes{}.withLabel("%").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 1) + " %"; })));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("pitch", 1), juce::String::fromUTF8("Pitch"),
            juce::NormalisableRange<float>(-24, 24, 0.01, 1, false), 0,
            juce::AudioParameterFloatAttributes{}.withLabel("st").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 2) + " st"; })));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("pitchDelay", 1), juce::String::fromUTF8("Pitch Delay"),
            juce::NormalisableRange<float>(0, 1000, 1, 0.5, false), 0,
            juce::AudioParameterFloatAttributes{}.withLabel("ms").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 0) + " ms"; })));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("pitch2", 1), juce::String::fromUTF8("Pitch 2"),
            juce::NormalisableRange<float>(-24, 24, 0.01, 1, false), 0,
            juce::AudioParameterFloatAttributes{}.withLabel("st").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 2) + " st"; })));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("pitch2Delay", 1), juce::String::fromUTF8("Pitch 2 Delay"),
            juce::NormalisableRange<float>(0, 1000, 1, 0.5, false), 0,
            juce::AudioParameterFloatAttributes{}.withLabel("ms").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 0) + " ms"; })));
        params.push_back(std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID("pitchMode", 1), juce::String::fromUTF8("Pitch Mode"),
            juce::StringArray{juce::String::fromUTF8("Drift"), juce::String::fromUTF8("Sync"),
                              juce::String::fromUTF8("Vocoder")},
            0));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("fdnMix", 1), juce::String::fromUTF8("Reverb Mix"),
            juce::NormalisableRange<float>(-100, 12, 0.1, 1, false), -100,
            juce::AudioParameterFloatAttributes{}.withLabel("dB").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 1) + " dB"; })));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("fdnSize", 1), juce::String::fromUTF8("Reverb Size"),
            juce::NormalisableRange<float>(1, 330, 0.1, 0.4, false), 30,
            juce::AudioParameterFloatAttributes{}.withLabel("m").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 1) + " m"; })));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("fdnDecay", 1), juce::String::fromUTF8("Reverb Decay"),
            juce::NormalisableRange<float>(1, 100000, 1, 0.2, false), 2000,
            juce::AudioParameterFloatAttributes{}.withLabel("ms").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 0) + " ms"; })));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("drive", 1), juce::String::fromUTF8("Drive"),
            juce::NormalisableRange<float>(0, 100, 0.1, 1, false), 0,
            juce::AudioParameterFloatAttributes{}.withLabel("%").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 1) + " %"; })));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("eqInLow", 1), juce::String::fromUTF8("EQ In Low"),
            juce::NormalisableRange<float>(-18, 18, 0.1, 1, false), 0,
            juce::AudioParameterFloatAttributes{}.withLabel("dB").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 1) + " dB"; })));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("eqInMid", 1), juce::String::fromUTF8("EQ In Mid"),
            juce::NormalisableRange<float>(-18, 18, 0.1, 1, false), 0,
            juce::AudioParameterFloatAttributes{}.withLabel("dB").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 1) + " dB"; })));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("eqInHigh", 1), juce::String::fromUTF8("EQ In High"),
            juce::NormalisableRange<float>(-18, 18, 0.1, 1, false), 0,
            juce::AudioParameterFloatAttributes{}.withLabel("dB").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 1) + " dB"; })));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("eqOutLow", 1), juce::String::fromUTF8("EQ Out Low"),
            juce::NormalisableRange<float>(-18, 18, 0.1, 1, false), 0,
            juce::AudioParameterFloatAttributes{}.withLabel("dB").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 1) + " dB"; })));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("eqOutMid", 1), juce::String::fromUTF8("EQ Out Mid"),
            juce::NormalisableRange<float>(-18, 18, 0.1, 1, false), 0,
            juce::AudioParameterFloatAttributes{}.withLabel("dB").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 1) + " dB"; })));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("eqOutHigh", 1), juce::String::fromUTF8("EQ Out High"),
            juce::NormalisableRange<float>(-18, 18, 0.1, 1, false), 0,
            juce::AudioParameterFloatAttributes{}.withLabel("dB").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 1) + " dB"; })));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("level", 1), juce::String::fromUTF8("Level"),
            juce::NormalisableRange<float>(-24, 12, 0.1, 1, false), 0,
            juce::AudioParameterFloatAttributes{}.withLabel("dB").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 1) + " dB"; })));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("pitcherShelfLow", 1), juce::String::fromUTF8("Pitcher Shelf Low"),
            juce::NormalisableRange<float>(-18, 18, 0.1, 1, false), 0,
            juce::AudioParameterFloatAttributes{}.withLabel("dB").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 1) + " dB"; })));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("pitcherShelfHigh", 1), juce::String::fromUTF8("Pitcher Shelf High"),
            juce::NormalisableRange<float>(-18, 18, 0.1, 1, false), 0,
            juce::AudioParameterFloatAttributes{}.withLabel("dB").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 1) + " dB"; })));
        params.push_back(std::make_unique<juce::AudioParameterBool>(juce::ParameterID("extremeStereoTap", 1),
                                                                    juce::String::fromUTF8("Extreme Stereo Tap"), 0));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("wide", 1), juce::String::fromUTF8("Wide"),
            juce::NormalisableRange<float>(-100, 100, 0.1, 1, false), 0,
            juce::AudioParameterFloatAttributes{}.withLabel("%").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 1) + " %"; })));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("reverbShelfLow", 1), juce::String::fromUTF8("Reverb Shelf Low"),
            juce::NormalisableRange<float>(-18, 18, 0.1, 1, false), 0,
            juce::AudioParameterFloatAttributes{}.withLabel("dB").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 1) + " dB"; })));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("reverbShelfHigh", 1), juce::String::fromUTF8("Reverb Shelf High"),
            juce::NormalisableRange<float>(-18, 18, 0.1, 1, false), 0,
            juce::AudioParameterFloatAttributes{}.withLabel("dB").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 1) + " dB"; })));

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
            {"preDelay",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setPreDelay(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::preDelay, v);
             }},
            {"elements",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setElements(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::elements, v);
             }},
            {"tapSpan",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setTapSpan(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::tapSpan, v);
             }},
            {"feedback",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setFeedback(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::feedback, v);
             }},
            {"bulge",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setBulge(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::bulge, v);
             }},
            {"bottomSize",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setBottomSize(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::bottomSize, v);
             }},
            {"topSize",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setTopSize(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::topSize, v);
             }},
            {"sizeSpread",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setSizeSpread(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::sizeSpread, v);
             }},
            {"modulationDepth",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setModulationDepth(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::modulationDepth, v);
             }},
            {"modulationSpeed",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setModulationSpeed(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::modulationSpeed, v);
             }},
            {"lowPass",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setLowPass(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::lowPass, v);
             }},
            {"mix",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setMix(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::mix, v);
             }},
            {"pitch",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setPitch(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::pitch, v);
             }},
            {"pitchDelay",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setPitchDelay(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::pitchDelay, v);
             }},
            {"pitch2",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setPitch2(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::pitch2, v);
             }},
            {"pitch2Delay",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setPitch2Delay(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::pitch2Delay, v);
             }},
            {"pitchMode",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setPitchMode(static_cast<int>(v));
                 p.m_fileIo.updateParameter(PatchParameters::Id::pitchMode, v);
             }},
            {"fdnMix",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setFdnMix(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::fdnMix, v);
             }},
            {"fdnSize",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setFdnSize(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::fdnSize, v);
             }},
            {"fdnDecay",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setFdnDecay(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::fdnDecay, v);
             }},
            {"drive",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setDrive(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::drive, v);
             }},
            {"eqInLow",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setEqInLow(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::eqInLow, v);
             }},
            {"eqInMid",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setEqInMid(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::eqInMid, v);
             }},
            {"eqInHigh",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setEqInHigh(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::eqInHigh, v);
             }},
            {"eqOutLow",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setEqOutLow(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::eqOutLow, v);
             }},
            {"eqOutMid",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setEqOutMid(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::eqOutMid, v);
             }},
            {"eqOutHigh",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setEqOutHigh(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::eqOutHigh, v);
             }},
            {"level",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setLevel(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::level, v);
             }},
            {"pitcherShelfLow",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setPitcherShelfLow(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::pitcherShelfLow, v);
             }},
            {"pitcherShelfHigh",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setPitcherShelfHigh(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::pitcherShelfHigh, v);
             }},
            {"extremeStereoTap",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setExtremeStereoTap(static_cast<bool>(v));
                 p.m_fileIo.updateParameter(PatchParameters::Id::extremeStereoTap, v);
             }},
            {"wide",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setWide(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::wide, v);
             }},
            {"reverbShelfLow",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setReverbShelfLow(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::reverbShelfLow, v);
             }},
            {"reverbShelfHigh",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setReverbShelfHigh(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::reverbShelfHigh, v);
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
        if (auto* p = m_parameters.getParameter("dry"))
        {
            const auto& range = m_parameters.getParameterRange("dry");
            float normalized = range.convertTo0to1(static_cast<float>(params.dry));
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("wet"))
        {
            const auto& range = m_parameters.getParameterRange("wet");
            float normalized = range.convertTo0to1(static_cast<float>(params.wet));
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("preDelay"))
        {
            const auto& range = m_parameters.getParameterRange("preDelay");
            float normalized = range.convertTo0to1(static_cast<float>(params.preDelay));
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("elements"))
        {
            const auto& range = m_parameters.getParameterRange("elements");
            float normalized = range.convertTo0to1(static_cast<float>(params.elements));
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("tapSpan"))
        {
            const auto& range = m_parameters.getParameterRange("tapSpan");
            float normalized = range.convertTo0to1(static_cast<float>(params.tapSpan));
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("feedback"))
        {
            const auto& range = m_parameters.getParameterRange("feedback");
            float normalized = range.convertTo0to1(static_cast<float>(params.feedback));
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("bulge"))
        {
            const auto& range = m_parameters.getParameterRange("bulge");
            float normalized = range.convertTo0to1(static_cast<float>(params.bulge));
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("bottomSize"))
        {
            const auto& range = m_parameters.getParameterRange("bottomSize");
            float normalized = range.convertTo0to1(static_cast<float>(params.bottomSize));
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("topSize"))
        {
            const auto& range = m_parameters.getParameterRange("topSize");
            float normalized = range.convertTo0to1(static_cast<float>(params.topSize));
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("sizeSpread"))
        {
            const auto& range = m_parameters.getParameterRange("sizeSpread");
            float normalized = range.convertTo0to1(static_cast<float>(params.sizeSpread));
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("modulationDepth"))
        {
            const auto& range = m_parameters.getParameterRange("modulationDepth");
            float normalized = range.convertTo0to1(static_cast<float>(params.modulationDepth));
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("modulationSpeed"))
        {
            const auto& range = m_parameters.getParameterRange("modulationSpeed");
            float normalized = range.convertTo0to1(static_cast<float>(params.modulationSpeed));
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("lowPass"))
        {
            const auto& range = m_parameters.getParameterRange("lowPass");
            float normalized = range.convertTo0to1(static_cast<float>(params.lowPass));
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("mix"))
        {
            const auto& range = m_parameters.getParameterRange("mix");
            float normalized = range.convertTo0to1(static_cast<float>(params.mix));
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("pitch"))
        {
            const auto& range = m_parameters.getParameterRange("pitch");
            float normalized = range.convertTo0to1(static_cast<float>(params.pitch));
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("pitchDelay"))
        {
            const auto& range = m_parameters.getParameterRange("pitchDelay");
            float normalized = range.convertTo0to1(static_cast<float>(params.pitchDelay));
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("pitch2"))
        {
            const auto& range = m_parameters.getParameterRange("pitch2");
            float normalized = range.convertTo0to1(static_cast<float>(params.pitch2));
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("pitch2Delay"))
        {
            const auto& range = m_parameters.getParameterRange("pitch2Delay");
            float normalized = range.convertTo0to1(static_cast<float>(params.pitch2Delay));
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("pitchMode"))
        {
            const auto& range = m_parameters.getParameterRange("pitchMode");
            float normalized = range.convertTo0to1(static_cast<float>(params.pitchMode));
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("fdnMix"))
        {
            const auto& range = m_parameters.getParameterRange("fdnMix");
            float normalized = range.convertTo0to1(static_cast<float>(params.fdnMix));
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("fdnSize"))
        {
            const auto& range = m_parameters.getParameterRange("fdnSize");
            float normalized = range.convertTo0to1(static_cast<float>(params.fdnSize));
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("fdnDecay"))
        {
            const auto& range = m_parameters.getParameterRange("fdnDecay");
            float normalized = range.convertTo0to1(static_cast<float>(params.fdnDecay));
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("drive"))
        {
            const auto& range = m_parameters.getParameterRange("drive");
            float normalized = range.convertTo0to1(static_cast<float>(params.drive));
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("eqInLow"))
        {
            const auto& range = m_parameters.getParameterRange("eqInLow");
            float normalized = range.convertTo0to1(static_cast<float>(params.eqInLow));
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("eqInMid"))
        {
            const auto& range = m_parameters.getParameterRange("eqInMid");
            float normalized = range.convertTo0to1(static_cast<float>(params.eqInMid));
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("eqInHigh"))
        {
            const auto& range = m_parameters.getParameterRange("eqInHigh");
            float normalized = range.convertTo0to1(static_cast<float>(params.eqInHigh));
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("eqOutLow"))
        {
            const auto& range = m_parameters.getParameterRange("eqOutLow");
            float normalized = range.convertTo0to1(static_cast<float>(params.eqOutLow));
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("eqOutMid"))
        {
            const auto& range = m_parameters.getParameterRange("eqOutMid");
            float normalized = range.convertTo0to1(static_cast<float>(params.eqOutMid));
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("eqOutHigh"))
        {
            const auto& range = m_parameters.getParameterRange("eqOutHigh");
            float normalized = range.convertTo0to1(static_cast<float>(params.eqOutHigh));
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("level"))
        {
            const auto& range = m_parameters.getParameterRange("level");
            float normalized = range.convertTo0to1(static_cast<float>(params.level));
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("pitcherShelfLow"))
        {
            const auto& range = m_parameters.getParameterRange("pitcherShelfLow");
            float normalized = range.convertTo0to1(static_cast<float>(params.pitcherShelfLow));
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("pitcherShelfHigh"))
        {
            const auto& range = m_parameters.getParameterRange("pitcherShelfHigh");
            float normalized = range.convertTo0to1(static_cast<float>(params.pitcherShelfHigh));
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("extremeStereoTap"))
        {
            const auto& range = m_parameters.getParameterRange("extremeStereoTap");
            float normalized = range.convertTo0to1(static_cast<float>(params.extremeStereoTap));
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("wide"))
        {
            const auto& range = m_parameters.getParameterRange("wide");
            float normalized = range.convertTo0to1(static_cast<float>(params.wide));
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("reverbShelfLow"))
        {
            const auto& range = m_parameters.getParameterRange("reverbShelfLow");
            float normalized = range.convertTo0to1(static_cast<float>(params.reverbShelfLow));
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("reverbShelfHigh"))
        {
            const auto& range = m_parameters.getParameterRange("reverbShelfHigh");
            float normalized = range.convertTo0to1(static_cast<float>(params.reverbShelfHigh));
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
                if ((msg.data[0] & 0xF0) == 0xB0)
                {
                    handleMidiCc(msg.data[1], msg.data[2]);
                }
            }
        }
        if (getTotalNumOutputChannels() == 2)
        {
            fixedRunner->processBlock(buffer);
        }
    }

#pragma GCC diagnostic pop


    [[nodiscard]] std::array<float, 101> getProcessingBinLevels() const noexcept
    {
        return pluginRunner ? pluginRunner->getProcessingBinLevels() : std::array<float, 101>{};
    }
    [[nodiscard]] std::array<std::array<float, 3>, 101> getProcessingBinBandLevels() const noexcept
    {
        return pluginRunner ? pluginRunner->getProcessingBinBandLevels() : std::array<std::array<float, 3>, 101>{};
    }
    [[nodiscard]] std::array<float, 100> getElementSizesInMeters() const noexcept
    {
        return pluginRunner ? pluginRunner->getElementSizesInMeters() : std::array<float, 100>{};
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

  private:
    size_t m_sampleRate{48000};

    static bool isChanged(const float a, const float b)
    {
        return std::abs(a - b) > 1E-8f;
    }

    int m_program{0};

    std::unique_ptr<RateNormalizer> fixedRunner;
    std::unique_ptr<MaxDiffuserImpl<NumSamplesPerBlock>> pluginRunner;

    juce::AudioProcessorValueTreeState m_parameters;
    struct CcSlot
    {
        std::atomic<int> controller{-1};
        std::atomic<float> valueLow{0.f};
        std::atomic<float> valueHigh{0.f};
    };
    std::array<CcSlot, 34> m_ccActive{};
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
        overrides.reserve(34);
        for (size_t i = 0; i < 34; ++i)
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
        for (size_t i = 0; i < 34; ++i)
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
    std::vector<int> m_patchIndex;
    FileIo m_fileIo;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioPluginAudioProcessor)
};
