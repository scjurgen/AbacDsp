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
#include "impl/TanpuraImpl.h"

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
        , m_spectrogram{}
        , m_patchIndex(0, 0)
    {
        m_parameters.addParameterListener("key", this);
        m_parameters.addParameterListener("level", this);
        m_parameters.addParameterListener("tuning", this);
        m_parameters.addParameterListener("detune", this);
        m_parameters.addParameterListener("reverbDry", this);
        m_parameters.addParameterListener("reverbWet", this);
        m_parameters.addParameterListener("reverbSize", this);
        m_parameters.addParameterListener("reverbDecay", this);
        m_parameters.addParameterListener("reverbShelfLow", this);
        m_parameters.addParameterListener("reverbShelfHigh", this);
        m_parameters.addParameterListener("pattern", this);
        m_parameters.addParameterListener("slide", this);
        m_parameters.addParameterListener("slideTime", this);
        m_parameters.addParameterListener("harmonicFirst", this);
        m_parameters.addParameterListener("harmonicSecond", this);
        m_parameters.addParameterListener("playStop", this);
        m_parameters.addParameterListener("humanizeTiming", this);
        m_parameters.addParameterListener("humanizeLevel", this);
        m_parameters.addParameterListener("bpm", this);
        m_parameters.addParameterListener("hostSync", this);
        m_parameters.addParameterListener("pluckDivision", this);
        m_parameters.addParameterListener("pauseDivision", this);
        m_parameters.addParameterListener("attack", this);
        m_parameters.addParameterListener("decay", this);
        m_parameters.addParameterListener("decayOctave", this);
        m_parameters.addParameterListener("damper", this);
        m_parameters.addParameterListener("levelSustain", this);
        m_parameters.addParameterListener("sustainHumanize", this);
        m_parameters.addParameterListener("lfoDepth", this);
        m_parameters.addParameterListener("lfoSpeed", this);
        m_parameters.addParameterListener("lfoSpeedVariation", this);
        m_parameters.addParameterListener("attackFilter", this);
        m_parameters.addParameterListener("decayFilter", this);
        m_parameters.addParameterListener("levelSustainFilter", this);
        m_parameters.addParameterListener("filterCutoff", this);
        m_parameters.addParameterListener("filterResonance", this);
        m_parameters.addParameterListener("contourFilter", this);

        m_fileIo.initialize(m_patchIndex);
    }
    ~AudioPluginAudioProcessor() override
    {
        m_parameters.removeParameterListener("key", this);
        m_parameters.removeParameterListener("level", this);
        m_parameters.removeParameterListener("tuning", this);
        m_parameters.removeParameterListener("detune", this);
        m_parameters.removeParameterListener("reverbDry", this);
        m_parameters.removeParameterListener("reverbWet", this);
        m_parameters.removeParameterListener("reverbSize", this);
        m_parameters.removeParameterListener("reverbDecay", this);
        m_parameters.removeParameterListener("reverbShelfLow", this);
        m_parameters.removeParameterListener("reverbShelfHigh", this);
        m_parameters.removeParameterListener("pattern", this);
        m_parameters.removeParameterListener("slide", this);
        m_parameters.removeParameterListener("slideTime", this);
        m_parameters.removeParameterListener("harmonicFirst", this);
        m_parameters.removeParameterListener("harmonicSecond", this);
        m_parameters.removeParameterListener("playStop", this);
        m_parameters.removeParameterListener("humanizeTiming", this);
        m_parameters.removeParameterListener("humanizeLevel", this);
        m_parameters.removeParameterListener("bpm", this);
        m_parameters.removeParameterListener("hostSync", this);
        m_parameters.removeParameterListener("pluckDivision", this);
        m_parameters.removeParameterListener("pauseDivision", this);
        m_parameters.removeParameterListener("attack", this);
        m_parameters.removeParameterListener("decay", this);
        m_parameters.removeParameterListener("decayOctave", this);
        m_parameters.removeParameterListener("damper", this);
        m_parameters.removeParameterListener("levelSustain", this);
        m_parameters.removeParameterListener("sustainHumanize", this);
        m_parameters.removeParameterListener("lfoDepth", this);
        m_parameters.removeParameterListener("lfoSpeed", this);
        m_parameters.removeParameterListener("lfoSpeedVariation", this);
        m_parameters.removeParameterListener("attackFilter", this);
        m_parameters.removeParameterListener("decayFilter", this);
        m_parameters.removeParameterListener("levelSustainFilter", this);
        m_parameters.removeParameterListener("filterCutoff", this);
        m_parameters.removeParameterListener("filterResonance", this);
        m_parameters.removeParameterListener("contourFilter", this);
    }

    void prepareToPlay(const double sampleRate, const int samplesPerBlock) override
    {
        pluginRunner = std::make_unique<TanpuraImpl<NumSamplesPerBlock>>(RateNormalizer::kInternalSampleRate);

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
            juce::ParameterID("key", 1), juce::String::fromUTF8("Key"),
            juce::StringArray{
                juce::String::fromUTF8("C0"),     juce::String::fromUTF8("C#/Db0"), juce::String::fromUTF8("D0"),
                juce::String::fromUTF8("D#/Eb0"), juce::String::fromUTF8("E0"),     juce::String::fromUTF8("F0"),
                juce::String::fromUTF8("F#/Gb0"), juce::String::fromUTF8("G0"),     juce::String::fromUTF8("G#/Ab0"),
                juce::String::fromUTF8("A0"),     juce::String::fromUTF8("Bb0"),    juce::String::fromUTF8("B0"),
                juce::String::fromUTF8("C1"),     juce::String::fromUTF8("C#/Db1"), juce::String::fromUTF8("D1"),
                juce::String::fromUTF8("D#/Eb1"), juce::String::fromUTF8("E1"),     juce::String::fromUTF8("F1"),
                juce::String::fromUTF8("F#/Gb1"), juce::String::fromUTF8("G1"),     juce::String::fromUTF8("G#/Ab1"),
                juce::String::fromUTF8("A1"),     juce::String::fromUTF8("Bb1"),    juce::String::fromUTF8("B1"),
                juce::String::fromUTF8("C2"),     juce::String::fromUTF8("C#/Db2"), juce::String::fromUTF8("D2"),
                juce::String::fromUTF8("D#/Eb2"), juce::String::fromUTF8("E2"),     juce::String::fromUTF8("F2"),
                juce::String::fromUTF8("F#/Gb2"), juce::String::fromUTF8("G2"),     juce::String::fromUTF8("G#/Ab2"),
                juce::String::fromUTF8("A2"),     juce::String::fromUTF8("Bb2"),    juce::String::fromUTF8("B2"),
                juce::String::fromUTF8("C3")},
            12));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("level", 1), juce::String::fromUTF8("Level"),
            juce::NormalisableRange<float>(-80, 0, 0.1, 1, false), 0,
            juce::AudioParameterFloatAttributes{}.withLabel("dB").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 1) + " dB"; })));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("tuning", 1), juce::String::fromUTF8("Tuning"),
            juce::NormalisableRange<float>(400, 800, 0.1, 1, false), 440,
            juce::AudioParameterFloatAttributes{}.withLabel("Hz").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 1) + " Hz"; })));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("detune", 1), juce::String::fromUTF8("Detune"),
            juce::NormalisableRange<float>(0, 100, 1, 1, false), 5,
            juce::AudioParameterFloatAttributes{}.withLabel("ct").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 0) + " ct"; })));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("reverbDry", 1), juce::String::fromUTF8("Reverb Dry"),
            juce::NormalisableRange<float>(-100, 12, 0.1, 1, false), 0,
            juce::AudioParameterFloatAttributes{}.withLabel("dB").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 1) + " dB"; })));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("reverbWet", 1), juce::String::fromUTF8("Reverb Wet"),
            juce::NormalisableRange<float>(-100, 12, 0.1, 1, false), -100,
            juce::AudioParameterFloatAttributes{}.withLabel("dB").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 1) + " dB"; })));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("reverbSize", 1), juce::String::fromUTF8("Reverb Size"),
            juce::NormalisableRange<float>(1, 330, 0.1, 0.4, false), 30,
            juce::AudioParameterFloatAttributes{}.withLabel("m").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 1) + " m"; })));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("reverbDecay", 1), juce::String::fromUTF8("Reverb Decay"),
            juce::NormalisableRange<float>(1, 100000, 1, 0.2, false), 2000,
            juce::AudioParameterFloatAttributes{}.withLabel("ms").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 0) + " ms"; })));
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
        params.push_back(std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID("pattern", 1), juce::String::fromUTF8("Pattern"),
            juce::StringArray{juce::String::fromUTF8("H1 H2 1 -"), juce::String::fromUTF8("H1 H2 8 1 -"),
                              juce::String::fromUTF8("H1 H2 8 8 1 -"), juce::String::fromUTF8("H1 H2 - 8 8 1 -")},
            0));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("slide", 1), juce::String::fromUTF8("Slide"),
            juce::NormalisableRange<float>(0, 100, 1, 1, false), 0,
            juce::AudioParameterFloatAttributes{}.withLabel("%").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 0) + " %"; })));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("slideTime", 1), juce::String::fromUTF8("Slide Time"),
            juce::NormalisableRange<float>(1, 3000, 0.1, 0.35, false), 150,
            juce::AudioParameterFloatAttributes{}.withLabel("ms").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 1) + " ms"; })));
        params.push_back(std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID("harmonicFirst", 1), juce::String::fromUTF8("Set Harmonic 1"),
            juce::StringArray{juce::String::fromUTF8("-12 sā सा"), juce::String::fromUTF8("-11"),
                              juce::String::fromUTF8("-10 re र"),  juce::String::fromUTF8("-9"),
                              juce::String::fromUTF8("-8 ga ग"),   juce::String::fromUTF8("-7 ma म"),
                              juce::String::fromUTF8("-6"),        juce::String::fromUTF8("-5 pa प"),
                              juce::String::fromUTF8("-4"),        juce::String::fromUTF8("-3 dha ध"),
                              juce::String::fromUTF8("-2"),        juce::String::fromUTF8("-1 ni नी"),
                              juce::String::fromUTF8("0 Sā सा"),   juce::String::fromUTF8("1"),
                              juce::String::fromUTF8("2 re र"),    juce::String::fromUTF8("3"),
                              juce::String::fromUTF8("4 ga ग"),    juce::String::fromUTF8("5 ma म"),
                              juce::String::fromUTF8("6"),         juce::String::fromUTF8("7 pa प"),
                              juce::String::fromUTF8("8"),         juce::String::fromUTF8("9 dha ध"),
                              juce::String::fromUTF8("10"),        juce::String::fromUTF8("11 ni नी"),
                              juce::String::fromUTF8("12 Sā सा")},
            7));
        params.push_back(std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID("harmonicSecond", 1), juce::String::fromUTF8("Set Harmonic 2"),
            juce::StringArray{juce::String::fromUTF8("-12 sā सा"), juce::String::fromUTF8("-11"),
                              juce::String::fromUTF8("-10 re र"),  juce::String::fromUTF8("-9"),
                              juce::String::fromUTF8("-8 ga ग"),   juce::String::fromUTF8("-7 ma म"),
                              juce::String::fromUTF8("-6"),        juce::String::fromUTF8("-5 pa प"),
                              juce::String::fromUTF8("-4"),        juce::String::fromUTF8("-3 dha ध"),
                              juce::String::fromUTF8("-2"),        juce::String::fromUTF8("-1 ni नी"),
                              juce::String::fromUTF8("0 Sā सा"),   juce::String::fromUTF8("1"),
                              juce::String::fromUTF8("2 re र"),    juce::String::fromUTF8("3"),
                              juce::String::fromUTF8("4 ga ग"),    juce::String::fromUTF8("5 ma म"),
                              juce::String::fromUTF8("6"),         juce::String::fromUTF8("7 pa प"),
                              juce::String::fromUTF8("8"),         juce::String::fromUTF8("9 dha ध"),
                              juce::String::fromUTF8("10"),        juce::String::fromUTF8("11 ni नी"),
                              juce::String::fromUTF8("12 Sā सा")},
            0));
        params.push_back(std::make_unique<juce::AudioParameterBool>(juce::ParameterID("playStop", 1),
                                                                    juce::String::fromUTF8("Play"), 0));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("humanizeTiming", 1), juce::String::fromUTF8("Humanize Timing"),
            juce::NormalisableRange<float>(0, 100, 1, 1, false), 0,
            juce::AudioParameterFloatAttributes{}.withLabel("%").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 0) + " %"; })));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("humanizeLevel", 1), juce::String::fromUTF8("Humanize Level"),
            juce::NormalisableRange<float>(0, 100, 1, 1, false), 0,
            juce::AudioParameterFloatAttributes{}.withLabel("%").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 0) + " %"; })));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("bpm", 1), juce::String::fromUTF8("BPM"),
            juce::NormalisableRange<float>(40, 250, 0.1, 1, false), 120,
            juce::AudioParameterFloatAttributes{}.withLabel("BPM").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 1) + " BPM"; })));
        params.push_back(std::make_unique<juce::AudioParameterBool>(juce::ParameterID("hostSync", 1),
                                                                    juce::String::fromUTF8("Host Sync"), 0));
        params.push_back(std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID("pluckDivision", 1), juce::String::fromUTF8("Pluck Division"),
            juce::StringArray{
                juce::String::fromUTF8("1/1"), juce::String::fromUTF8("1/2"), juce::String::fromUTF8("1/2."),
                juce::String::fromUTF8("1/2T"), juce::String::fromUTF8("1/4"), juce::String::fromUTF8("1/4."),
                juce::String::fromUTF8("1/4T"), juce::String::fromUTF8("1/8"), juce::String::fromUTF8("1/8."),
                juce::String::fromUTF8("1/8T"), juce::String::fromUTF8("1/16"), juce::String::fromUTF8("1/16."),
                juce::String::fromUTF8("1/16T")},
            4));
        params.push_back(std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID("pauseDivision", 1), juce::String::fromUTF8("Pause Division"),
            juce::StringArray{
                juce::String::fromUTF8("1/1"), juce::String::fromUTF8("1/2"), juce::String::fromUTF8("1/2."),
                juce::String::fromUTF8("1/2T"), juce::String::fromUTF8("1/4"), juce::String::fromUTF8("1/4."),
                juce::String::fromUTF8("1/4T"), juce::String::fromUTF8("1/8"), juce::String::fromUTF8("1/8."),
                juce::String::fromUTF8("1/8T"), juce::String::fromUTF8("1/16"), juce::String::fromUTF8("1/16."),
                juce::String::fromUTF8("1/16T")},
            4));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("attack", 1), juce::String::fromUTF8("Attack"),
            juce::NormalisableRange<float>(1, 3000, 0.1, 0.35, false), 10,
            juce::AudioParameterFloatAttributes{}.withLabel("ms").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 1) + " ms"; })));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("decay", 1), juce::String::fromUTF8("Decay"),
            juce::NormalisableRange<float>(1, 100000, 1, 0.25, false), 10,
            juce::AudioParameterFloatAttributes{}.withLabel("ms").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 1) + " ms"; })));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("decayOctave", 1), juce::String::fromUTF8("Decay Octave"),
            juce::NormalisableRange<float>(0, 2, 0.01, 1, false), 1,
            juce::AudioParameterFloatAttributes{}.withLabel("").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 2) + " "; })));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("damper", 1), juce::String::fromUTF8("Damper"),
            juce::NormalisableRange<float>(0, 1, 0.01, 1, false), 0,
            juce::AudioParameterFloatAttributes{}.withLabel("").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 2) + " "; })));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("levelSustain", 1), juce::String::fromUTF8("Sustain"),
            juce::NormalisableRange<float>(0, 1, 0.01, 1, false), 0.2,
            juce::AudioParameterFloatAttributes{}.withLabel("").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 2) + " "; })));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("sustainHumanize", 1), juce::String::fromUTF8("Sustain Humanize"),
            juce::NormalisableRange<float>(0, 100, 1, 1, false), 0,
            juce::AudioParameterFloatAttributes{}.withLabel("%").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 0) + " %"; })));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("lfoDepth", 1), juce::String::fromUTF8("Filter LFO Depth"),
            juce::NormalisableRange<float>(0, 2, 0.01, 1, false), 0.5,
            juce::AudioParameterFloatAttributes{}.withLabel("").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 2) + " "; })));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("lfoSpeed", 1), juce::String::fromUTF8("Filter LFO Speed"),
            juce::NormalisableRange<float>(0.01, 20, 0.01, 0.25, false), 0.5,
            juce::AudioParameterFloatAttributes{}.withLabel("Hz").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 2) + " Hz"; })));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("lfoSpeedVariation", 1), juce::String::fromUTF8("Filter LFO Variation"),
            juce::NormalisableRange<float>(0, 100, 1, 1, false), 0,
            juce::AudioParameterFloatAttributes{}.withLabel("%").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 0) + " %"; })));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("attackFilter", 1), juce::String::fromUTF8("Filter Attack"),
            juce::NormalisableRange<float>(1, 3000, 0.1, 0.35, false), 10,
            juce::AudioParameterFloatAttributes{}.withLabel("ms").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 1) + " ms"; })));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("decayFilter", 1), juce::String::fromUTF8("Filter Decay"),
            juce::NormalisableRange<float>(1, 30000, 0.1, 0.25, false), 10,
            juce::AudioParameterFloatAttributes{}.withLabel("ms").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 1) + " ms"; })));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("levelSustainFilter", 1), juce::String::fromUTF8("Filter Sustain"),
            juce::NormalisableRange<float>(0, 1, 0.01, 1, false), 0,
            juce::AudioParameterFloatAttributes{}.withLabel("").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 2) + " "; })));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("filterCutoff", 1), juce::String::fromUTF8("Filter Cutoff"),
            juce::NormalisableRange<float>(-60, 48, 0.1, 1, false), 0,
            juce::AudioParameterFloatAttributes{}.withLabel("st").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 1) + " st"; })));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("filterResonance", 1), juce::String::fromUTF8("Filter Resonance"),
            juce::NormalisableRange<float>(0, 2, 0.01, 1, false), 0.1,
            juce::AudioParameterFloatAttributes{}.withLabel("").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 2) + " "; })));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("contourFilter", 1), juce::String::fromUTF8("Contour F"),
            juce::NormalisableRange<float>(-4, 4, 0.01, 1, false), 0,
            juce::AudioParameterFloatAttributes{}.withLabel("oct").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 2) + " oct"; })));

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
            {"key",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setKey(static_cast<size_t>(v));
                 p.m_fileIo.updateParameter(PatchParameters::Id::key, v);
             }},
            {"level",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setLevel(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::level, v);
             }},
            {"tuning",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setTuning(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::tuning, v);
             }},
            {"detune",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setDetune(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::detune, v);
             }},
            {"reverbDry",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setReverbDry(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::reverbDry, v);
             }},
            {"reverbWet",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setReverbWet(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::reverbWet, v);
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
            {"pattern",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setPattern(static_cast<size_t>(v));
                 p.m_fileIo.updateParameter(PatchParameters::Id::pattern, v);
             }},
            {"slide",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setSlide(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::slide, v);
             }},
            {"slideTime",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setSlideTime(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::slideTime, v);
             }},
            {"harmonicFirst",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setHarmonicFirst(static_cast<size_t>(v));
                 p.m_fileIo.updateParameter(PatchParameters::Id::harmonicFirst, v);
             }},
            {"harmonicSecond",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setHarmonicSecond(static_cast<size_t>(v));
                 p.m_fileIo.updateParameter(PatchParameters::Id::harmonicSecond, v);
             }},
            {"playStop",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setPlayStop(static_cast<bool>(v));
                 p.m_fileIo.updateParameter(PatchParameters::Id::playStop, v);
             }},
            {"humanizeTiming",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setHumanizeTiming(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::humanizeTiming, v);
             }},
            {"humanizeLevel",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setHumanizeLevel(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::humanizeLevel, v);
             }},
            {"bpm",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setBpm(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::bpm, v);
             }},
            {"hostSync",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setHostSync(static_cast<bool>(v));
                 p.m_fileIo.updateParameter(PatchParameters::Id::hostSync, v);
             }},
            {"pluckDivision",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setPluckDivision(static_cast<int>(v));
                 p.m_fileIo.updateParameter(PatchParameters::Id::pluckDivision, v);
             }},
            {"pauseDivision",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setPauseDivision(static_cast<int>(v));
                 p.m_fileIo.updateParameter(PatchParameters::Id::pauseDivision, v);
             }},
            {"attack",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setAttack(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::attack, v);
             }},
            {"decay",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setDecay(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::decay, v);
             }},
            {"decayOctave",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setDecayOctave(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::decayOctave, v);
             }},
            {"damper",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setDamper(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::damper, v);
             }},
            {"levelSustain",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setLevelSustain(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::levelSustain, v);
             }},
            {"sustainHumanize",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setSustainHumanize(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::sustainHumanize, v);
             }},
            {"lfoDepth",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setLfoDepth(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::lfoDepth, v);
             }},
            {"lfoSpeed",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setLfoSpeed(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::lfoSpeed, v);
             }},
            {"lfoSpeedVariation",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setLfoSpeedVariation(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::lfoSpeedVariation, v);
             }},
            {"attackFilter",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setAttackFilter(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::attackFilter, v);
             }},
            {"decayFilter",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setDecayFilter(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::decayFilter, v);
             }},
            {"levelSustainFilter",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setLevelSustainFilter(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::levelSustainFilter, v);
             }},
            {"filterCutoff",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setFilterCutoff(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::filterCutoff, v);
             }},
            {"filterResonance",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setFilterResonance(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::filterResonance, v);
             }},
            {"contourFilter",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setContourFilter(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::contourFilter, v);
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
        if (auto* p = m_parameters.getParameter("key"))
        {
            const auto& range = m_parameters.getParameterRange("key");
            float normalized = range.convertTo0to1(params.key);
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("level"))
        {
            const auto& range = m_parameters.getParameterRange("level");
            float normalized = range.convertTo0to1(params.level);
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("tuning"))
        {
            const auto& range = m_parameters.getParameterRange("tuning");
            float normalized = range.convertTo0to1(params.tuning);
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("detune"))
        {
            const auto& range = m_parameters.getParameterRange("detune");
            float normalized = range.convertTo0to1(params.detune);
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("reverbDry"))
        {
            const auto& range = m_parameters.getParameterRange("reverbDry");
            float normalized = range.convertTo0to1(params.reverbDry);
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("reverbWet"))
        {
            const auto& range = m_parameters.getParameterRange("reverbWet");
            float normalized = range.convertTo0to1(params.reverbWet);
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("reverbSize"))
        {
            const auto& range = m_parameters.getParameterRange("reverbSize");
            float normalized = range.convertTo0to1(params.reverbSize);
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("reverbDecay"))
        {
            const auto& range = m_parameters.getParameterRange("reverbDecay");
            float normalized = range.convertTo0to1(params.reverbDecay);
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("reverbShelfLow"))
        {
            const auto& range = m_parameters.getParameterRange("reverbShelfLow");
            float normalized = range.convertTo0to1(params.reverbShelfLow);
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("reverbShelfHigh"))
        {
            const auto& range = m_parameters.getParameterRange("reverbShelfHigh");
            float normalized = range.convertTo0to1(params.reverbShelfHigh);
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("pattern"))
        {
            const auto& range = m_parameters.getParameterRange("pattern");
            float normalized = range.convertTo0to1(params.pattern);
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("slide"))
        {
            const auto& range = m_parameters.getParameterRange("slide");
            float normalized = range.convertTo0to1(params.slide);
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("slideTime"))
        {
            const auto& range = m_parameters.getParameterRange("slideTime");
            float normalized = range.convertTo0to1(params.slideTime);
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("harmonicFirst"))
        {
            const auto& range = m_parameters.getParameterRange("harmonicFirst");
            float normalized = range.convertTo0to1(params.harmonicFirst);
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("harmonicSecond"))
        {
            const auto& range = m_parameters.getParameterRange("harmonicSecond");
            float normalized = range.convertTo0to1(params.harmonicSecond);
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("playStop"))
        {
            const auto& range = m_parameters.getParameterRange("playStop");
            float normalized = range.convertTo0to1(params.playStop);
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("humanizeTiming"))
        {
            const auto& range = m_parameters.getParameterRange("humanizeTiming");
            float normalized = range.convertTo0to1(params.humanizeTiming);
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("humanizeLevel"))
        {
            const auto& range = m_parameters.getParameterRange("humanizeLevel");
            float normalized = range.convertTo0to1(params.humanizeLevel);
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("bpm"))
        {
            const auto& range = m_parameters.getParameterRange("bpm");
            float normalized = range.convertTo0to1(params.bpm);
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("hostSync"))
        {
            const auto& range = m_parameters.getParameterRange("hostSync");
            float normalized = range.convertTo0to1(params.hostSync);
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("pluckDivision"))
        {
            const auto& range = m_parameters.getParameterRange("pluckDivision");
            float normalized = range.convertTo0to1(params.pluckDivision);
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("pauseDivision"))
        {
            const auto& range = m_parameters.getParameterRange("pauseDivision");
            float normalized = range.convertTo0to1(params.pauseDivision);
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("attack"))
        {
            const auto& range = m_parameters.getParameterRange("attack");
            float normalized = range.convertTo0to1(params.attack);
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("decay"))
        {
            const auto& range = m_parameters.getParameterRange("decay");
            float normalized = range.convertTo0to1(params.decay);
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("decayOctave"))
        {
            const auto& range = m_parameters.getParameterRange("decayOctave");
            float normalized = range.convertTo0to1(params.decayOctave);
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("damper"))
        {
            const auto& range = m_parameters.getParameterRange("damper");
            float normalized = range.convertTo0to1(params.damper);
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("levelSustain"))
        {
            const auto& range = m_parameters.getParameterRange("levelSustain");
            float normalized = range.convertTo0to1(params.levelSustain);
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("sustainHumanize"))
        {
            const auto& range = m_parameters.getParameterRange("sustainHumanize");
            float normalized = range.convertTo0to1(params.sustainHumanize);
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("lfoDepth"))
        {
            const auto& range = m_parameters.getParameterRange("lfoDepth");
            float normalized = range.convertTo0to1(params.lfoDepth);
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("lfoSpeed"))
        {
            const auto& range = m_parameters.getParameterRange("lfoSpeed");
            float normalized = range.convertTo0to1(params.lfoSpeed);
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("lfoSpeedVariation"))
        {
            const auto& range = m_parameters.getParameterRange("lfoSpeedVariation");
            float normalized = range.convertTo0to1(params.lfoSpeedVariation);
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("attackFilter"))
        {
            const auto& range = m_parameters.getParameterRange("attackFilter");
            float normalized = range.convertTo0to1(params.attackFilter);
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("decayFilter"))
        {
            const auto& range = m_parameters.getParameterRange("decayFilter");
            float normalized = range.convertTo0to1(params.decayFilter);
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("levelSustainFilter"))
        {
            const auto& range = m_parameters.getParameterRange("levelSustainFilter");
            float normalized = range.convertTo0to1(params.levelSustainFilter);
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("filterCutoff"))
        {
            const auto& range = m_parameters.getParameterRange("filterCutoff");
            float normalized = range.convertTo0to1(params.filterCutoff);
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("filterResonance"))
        {
            const auto& range = m_parameters.getParameterRange("filterResonance");
            float normalized = range.convertTo0to1(params.filterResonance);
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("contourFilter"))
        {
            const auto& range = m_parameters.getParameterRange("contourFilter");
            float normalized = range.convertTo0to1(params.contourFilter);
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
        m_spectrogram.processBlock(std::span{buffer.getReadPointer(0), static_cast<size_t>(buffer.getNumSamples())});
    }

#pragma GCC diagnostic pop


    [[nodiscard]] float getCurrentBpm() const noexcept
    {
        return pluginRunner ? pluginRunner->currentBpm() : 120.f;
    }
    [[nodiscard]] bool isHostSynced() const noexcept
    {
        return pluginRunner && pluginRunner->isHostSynced();
    }
    [[nodiscard]] bool getIsEffectivelyPlaying() const noexcept
    {
        return pluginRunner && pluginRunner->effectivePlaying();
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

  private:
    size_t m_sampleRate{48000};

    static bool isChanged(const float a, const float b)
    {
        return std::abs(a - b) > 1E-8f;
    }

    int m_program{0};

    std::unique_ptr<RateNormalizer> fixedRunner;
    std::unique_ptr<TanpuraImpl<NumSamplesPerBlock>> pluginRunner;

    juce::AudioProcessorValueTreeState m_parameters;
    AbacDsp::SimpleSpectrogram m_spectrogram;
    std::vector<int> m_patchIndex;
    FileIo m_fileIo;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioPluginAudioProcessor)
};
