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
        , m_patchIndex(0, 0)
    {
        m_parameters.addParameterListener("key", this);
        m_parameters.addParameterListener("level", this);
        m_parameters.addParameterListener("tuning", this);
        m_parameters.addParameterListener("detuneString1", this);
        m_parameters.addParameterListener("detuneString2", this);
        m_parameters.addParameterListener("detuneString3", this);
        m_parameters.addParameterListener("detuneString4", this);
        m_parameters.addParameterListener("detuneString5", this);
        m_parameters.addParameterListener("pattern", this);
        m_parameters.addParameterListener("slide", this);
        m_parameters.addParameterListener("harmonicFirst", this);
        m_parameters.addParameterListener("harmonicSecond", this);
        m_parameters.addParameterListener("playStop", this);
        m_parameters.addParameterListener("picksPerMinute", this);
        m_parameters.addParameterListener("pauseLength", this);
        m_parameters.addParameterListener("attack", this);
        m_parameters.addParameterListener("decay", this);
        m_parameters.addParameterListener("levelSustain", this);
        m_parameters.addParameterListener("lfoDepth", this);
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
        m_parameters.removeParameterListener("detuneString1", this);
        m_parameters.removeParameterListener("detuneString2", this);
        m_parameters.removeParameterListener("detuneString3", this);
        m_parameters.removeParameterListener("detuneString4", this);
        m_parameters.removeParameterListener("detuneString5", this);
        m_parameters.removeParameterListener("pattern", this);
        m_parameters.removeParameterListener("slide", this);
        m_parameters.removeParameterListener("harmonicFirst", this);
        m_parameters.removeParameterListener("harmonicSecond", this);
        m_parameters.removeParameterListener("playStop", this);
        m_parameters.removeParameterListener("picksPerMinute", this);
        m_parameters.removeParameterListener("pauseLength", this);
        m_parameters.removeParameterListener("attack", this);
        m_parameters.removeParameterListener("decay", this);
        m_parameters.removeParameterListener("levelSustain", this);
        m_parameters.removeParameterListener("lfoDepth", this);
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
            juce::ParameterID("key", 1), "Key",
            juce::StringArray{"C0", "C#/Db0", "D0", "D#/Eb0", "E0", "F0", "F#/Gb0", "G0", "G#/Ab0", "A0", "Bb0", "B0",
                              "C1", "C#/Db1", "D1", "D#/Eb1", "E1", "F1", "F#/Gb1", "G1", "G#/Ab1", "A1", "Bb1", "B1",
                              "C2", "C#/Db2", "D2", "D#/Eb2", "E2", "F2", "F#/Gb2", "G2", "G#/Ab2", "A2", "Bb2", "B2",
                              "C3"},
            12));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("level", 1), "Level", juce::NormalisableRange<float>(-80, 0, 0.1, 1, false), 0,
            juce::AudioParameterFloatAttributes{}.withLabel("dB").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 1) + " dB"; })));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("tuning", 1), "Tuning", juce::NormalisableRange<float>(400, 800, 0.1, 1, false), 440,
            juce::AudioParameterFloatAttributes{}.withLabel("Hz").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 1) + " Hz"; })));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("detuneString1", 1), "Detune 1", juce::NormalisableRange<float>(-50, 50, 1, 1, false), 0,
            juce::AudioParameterFloatAttributes{}.withLabel("ct").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 0) + " ct"; })));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("detuneString2", 1), "Detune 2", juce::NormalisableRange<float>(-50, 50, 1, 1, false), 5,
            juce::AudioParameterFloatAttributes{}.withLabel("ct").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 0) + " ct"; })));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("detuneString3", 1), "Detune 3", juce::NormalisableRange<float>(-50, 50, 1, 1, false), -5,
            juce::AudioParameterFloatAttributes{}.withLabel("ct").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 0) + " ct"; })));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("detuneString4", 1), "Detune 4", juce::NormalisableRange<float>(-50, 50, 1, 1, false), 7,
            juce::AudioParameterFloatAttributes{}.withLabel("ct").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 0) + " ct"; })));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("detuneString5", 1), "Detune 5", juce::NormalisableRange<float>(-50, 50, 1, 1, false), -2,
            juce::AudioParameterFloatAttributes{}.withLabel("ct").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 0) + " ct"; })));
        params.push_back(std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID("pattern", 1), "Pattern",
            juce::StringArray{"H1 H2 1 -", "H1 H2 8 1 -", "H1 H2 8 8 1 -", "H1 H2 - 8 8 1 -"}, 0));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("slide", 1), "Slide", juce::NormalisableRange<float>(0, 100, 1, 1, false), 0,
            juce::AudioParameterFloatAttributes{}.withLabel("%").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 0) + " %"; })));
        params.push_back(std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID("harmonicFirst", 1), "Set Harmonic 1",
            juce::StringArray{"-12 sā सा", "-11", "-10 re र", "-9",     "-8 ga ग",  "-7 ma म", "-6",
                              "-5 pa प",   "-4",  "-3 dha ध", "-2",     "-1 ni नी", "0 Sā सा", "1",
                              "2 re र",    "3",   "4 ga ग",   "5 ma म", "6",        "7 pa प",  "8",
                              "9 dha ध",   "10",  "11 ni नी", "0 Sā सा"},
            7));
        params.push_back(std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID("harmonicSecond", 1), "Set Harmonic 2",
            juce::StringArray{"-12 sā सा", "-11", "-10 re र", "-9",     "-8 ga ग",  "-7 ma म", "-6",
                              "-5 pa प",   "-4",  "-3 dha ध", "-2",     "-1 ni नी", "0 Sā सा", "1",
                              "2 re र",    "3",   "4 ga ग",   "5 ma म", "6",        "7 pa प",  "8",
                              "9 dha ध",   "10",  "11 ni नी", "0 Sā सा"},
            0));
        params.push_back(std::make_unique<juce::AudioParameterBool>(juce::ParameterID("playStop", 1), "Play", 0));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("picksPerMinute", 1), "Picks/M", juce::NormalisableRange<float>(5, 400, 1, 1, false), 100,
            juce::AudioParameterFloatAttributes{}.withLabel("").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 0) + " "; })));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("pauseLength", 1), "Pause", juce::NormalisableRange<float>(1, 30000, 0.1, 0.25, false),
            10,
            juce::AudioParameterFloatAttributes{}.withLabel("ms").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 1) + " ms"; })));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("attack", 1), "Attack", juce::NormalisableRange<float>(1, 3000, 0.1, 0.35, false), 10,
            juce::AudioParameterFloatAttributes{}.withLabel("ms").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 1) + " ms"; })));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("decay", 1), "Decay", juce::NormalisableRange<float>(1, 30000, 0.1, 0.25, false), 10,
            juce::AudioParameterFloatAttributes{}.withLabel("ms").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 1) + " ms"; })));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("levelSustain", 1), "Sustain", juce::NormalisableRange<float>(0, 1, 0.01, 1, false), 0.2,
            juce::AudioParameterFloatAttributes{}.withLabel("").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 2) + " "; })));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("lfoDepth", 1), "Filter LFO Depth", juce::NormalisableRange<float>(0, 2, 0.01, 1, false),
            0.5,
            juce::AudioParameterFloatAttributes{}.withLabel("").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 2) + " "; })));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("attackFilter", 1), "Filter Attack",
            juce::NormalisableRange<float>(1, 3000, 0.1, 0.35, false), 10,
            juce::AudioParameterFloatAttributes{}.withLabel("ms").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 1) + " ms"; })));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("decayFilter", 1), "Filter Decay",
            juce::NormalisableRange<float>(1, 30000, 0.1, 0.25, false), 10,
            juce::AudioParameterFloatAttributes{}.withLabel("ms").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 1) + " ms"; })));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("levelSustainFilter", 1), "Filter Sustain",
            juce::NormalisableRange<float>(0, 1, 0.01, 1, false), 0,
            juce::AudioParameterFloatAttributes{}.withLabel("").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 2) + " "; })));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("filterCutoff", 1), "Filter Cutoff",
            juce::NormalisableRange<float>(-60, 48, 0.1, 1, false), 0,
            juce::AudioParameterFloatAttributes{}.withLabel("st").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 1) + " st"; })));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("filterResonance", 1), "Filter Resonance",
            juce::NormalisableRange<float>(0, 2, 0.01, 1, false), 0.1,
            juce::AudioParameterFloatAttributes{}.withLabel("").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 2) + " "; })));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("contourFilter", 1), "Contour F", juce::NormalisableRange<float>(-4, 4, 0.01, 1, false),
            0,
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
                 p.pluginRunner->setKey(static_cast<int>(v));
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
            {"detuneString1",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setDetuneString1(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::detuneString1, v);
             }},
            {"detuneString2",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setDetuneString2(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::detuneString2, v);
             }},
            {"detuneString3",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setDetuneString3(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::detuneString3, v);
             }},
            {"detuneString4",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setDetuneString4(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::detuneString4, v);
             }},
            {"detuneString5",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setDetuneString5(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::detuneString5, v);
             }},
            {"pattern",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setPattern(static_cast<int>(v));
                 p.m_fileIo.updateParameter(PatchParameters::Id::pattern, v);
             }},
            {"slide",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setSlide(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::slide, v);
             }},
            {"harmonicFirst",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setHarmonicFirst(static_cast<int>(v));
                 p.m_fileIo.updateParameter(PatchParameters::Id::harmonicFirst, v);
             }},
            {"harmonicSecond",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setHarmonicSecond(static_cast<int>(v));
                 p.m_fileIo.updateParameter(PatchParameters::Id::harmonicSecond, v);
             }},
            {"playStop",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setPlayStop(static_cast<bool>(v));
                 p.m_fileIo.updateParameter(PatchParameters::Id::playStop, v);
             }},
            {"picksPerMinute",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setPicksPerMinute(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::picksPerMinute, v);
             }},
            {"pauseLength",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setPauseLength(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::pauseLength, v);
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
            {"levelSustain",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setLevelSustain(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::levelSustain, v);
             }},
            {"lfoDepth",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setLfoDepth(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::lfoDepth, v);
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
        if (auto* p = m_parameters.getParameter("detuneString1"))
        {
            const auto& range = m_parameters.getParameterRange("detuneString1");
            float normalized = range.convertTo0to1(params.detuneString1);
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("detuneString2"))
        {
            const auto& range = m_parameters.getParameterRange("detuneString2");
            float normalized = range.convertTo0to1(params.detuneString2);
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("detuneString3"))
        {
            const auto& range = m_parameters.getParameterRange("detuneString3");
            float normalized = range.convertTo0to1(params.detuneString3);
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("detuneString4"))
        {
            const auto& range = m_parameters.getParameterRange("detuneString4");
            float normalized = range.convertTo0to1(params.detuneString4);
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("detuneString5"))
        {
            const auto& range = m_parameters.getParameterRange("detuneString5");
            float normalized = range.convertTo0to1(params.detuneString5);
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
        if (auto* p = m_parameters.getParameter("picksPerMinute"))
        {
            const auto& range = m_parameters.getParameterRange("picksPerMinute");
            float normalized = range.convertTo0to1(params.picksPerMinute);
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("pauseLength"))
        {
            const auto& range = m_parameters.getParameterRange("pauseLength");
            float normalized = range.convertTo0to1(params.pauseLength);
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
        if (auto* p = m_parameters.getParameter("levelSustain"))
        {
            const auto& range = m_parameters.getParameterRange("levelSustain");
            float normalized = range.convertTo0to1(params.levelSustain);
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("lfoDepth"))
        {
            const auto& range = m_parameters.getParameterRange("lfoDepth");
            float normalized = range.convertTo0to1(params.lfoDepth);
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
        if ((getTotalNumInputChannels() == 2) && (getTotalNumOutputChannels() == 2))
        {
            fixedRunner->processBlock(buffer);
        }
    }

#pragma GCC diagnostic pop


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
    std::vector<int> m_patchIndex;
    FileIo m_fileIo;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioPluginAudioProcessor)
};
