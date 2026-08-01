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
#include "impl/PingSynthExplorerPedal.h"

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
        m_parameters.addParameterListener("mode", this);
        m_parameters.addParameterListener("vol", this);
        m_parameters.addParameterListener("reverbLevel", this);
        m_parameters.addParameterListener("x", this);
        m_parameters.addParameterListener("y", this);
        m_parameters.addParameterListener("attack", this);
        m_parameters.addParameterListener("decay", this);
        m_parameters.addParameterListener("decaySkew", this);
        m_parameters.addParameterListener("spread", this);
        m_parameters.addParameterListener("type", this);
        m_parameters.addParameterListener("skew", this);
        m_parameters.addParameterListener("Spread", this);
        m_parameters.addParameterListener("randPower", this);
        m_parameters.addParameterListener("randExcitation", this);
        m_parameters.addParameterListener("softExcitation", this);
        m_parameters.addParameterListener("sparkleTime", this);
        m_parameters.addParameterListener("sparkleRand", this);
        m_parameters.addParameterListener("minHarmonics", this);
        m_parameters.addParameterListener("maxHarmonics", this);
        m_parameters.addParameterListener("minPbNote", this);
        m_parameters.addParameterListener("maxPbNote", this);
        m_parameters.addParameterListener("rangePb", this);

        m_fileIo.initialize(m_patchIndex);
    }
    ~AudioPluginAudioProcessor() override
    {
        m_parameters.removeParameterListener("mode", this);
        m_parameters.removeParameterListener("vol", this);
        m_parameters.removeParameterListener("reverbLevel", this);
        m_parameters.removeParameterListener("x", this);
        m_parameters.removeParameterListener("y", this);
        m_parameters.removeParameterListener("attack", this);
        m_parameters.removeParameterListener("decay", this);
        m_parameters.removeParameterListener("decaySkew", this);
        m_parameters.removeParameterListener("spread", this);
        m_parameters.removeParameterListener("type", this);
        m_parameters.removeParameterListener("skew", this);
        m_parameters.removeParameterListener("Spread", this);
        m_parameters.removeParameterListener("randPower", this);
        m_parameters.removeParameterListener("randExcitation", this);
        m_parameters.removeParameterListener("softExcitation", this);
        m_parameters.removeParameterListener("sparkleTime", this);
        m_parameters.removeParameterListener("sparkleRand", this);
        m_parameters.removeParameterListener("minHarmonics", this);
        m_parameters.removeParameterListener("maxHarmonics", this);
        m_parameters.removeParameterListener("minPbNote", this);
        m_parameters.removeParameterListener("maxPbNote", this);
        m_parameters.removeParameterListener("rangePb", this);
    }

    void prepareToPlay(const double sampleRate, const int samplesPerBlock) override
    {
        pluginRunner =
            std::make_unique<PingSynthExplorerPedal<NumSamplesPerBlock>>(RateNormalizer::kInternalSampleRate);

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
        params.push_back(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID("mode", 1), "Mode",
                                                                      juce::StringArray{"classic", "mpe"}, 1));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("vol", 1), "Vol", juce::NormalisableRange<float>(-100, 12, 0.1, 1, false), 0,
            juce::AudioParameterFloatAttributes{}.withLabel("dB").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 1) + " dB"; })));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("reverbLevel", 1), "Reverb", juce::NormalisableRange<float>(-120, 0, 1, 1, false), -24,
            juce::AudioParameterFloatAttributes{}.withLabel("dB").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 0) + " dB"; })));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("x", 1), "X", juce::NormalisableRange<float>(0, 100, 0.1, 1, false), 25,
            juce::AudioParameterFloatAttributes{}.withLabel("%").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 1) + " %"; })));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("y", 1), "Y", juce::NormalisableRange<float>(0, 100, 0.1, 1, false), 25,
            juce::AudioParameterFloatAttributes{}.withLabel("%").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 1) + " %"; })));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("attack", 1), "Attack", juce::NormalisableRange<float>(0, 1000, 0.1, 0.5, false), 1,
            juce::AudioParameterFloatAttributes{}.withLabel("ms").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 1) + " ms"; })));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("decay", 1), "Decay", juce::NormalisableRange<float>(0, 100, 0.1, 1, false), 2,
            juce::AudioParameterFloatAttributes{}.withLabel("%").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 1) + " %"; })));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("decaySkew", 1), "Decay Skew", juce::NormalisableRange<float>(-100, 100, 0.1, 1, false),
            0,
            juce::AudioParameterFloatAttributes{}.withLabel("%").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 1) + " %"; })));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("spread", 1), "Spread", juce::NormalisableRange<float>(0, 100, 0.1, 1, false), 0,
            juce::AudioParameterFloatAttributes{}.withLabel("%").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 1) + " %"; })));
        params.push_back(std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID("type", 1), "Type #1",
            juce::StringArray{"odd", "even", "stretched", "pythagorean", "bell", "cymbal", "plate", "golden",
                              "fibonacci", "prime"},
            0));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("skew", 1), "Skew Factor", juce::NormalisableRange<float>(-100, 100, 0.1, 1, false), 0,
            juce::AudioParameterFloatAttributes{}.withLabel("%").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 1) + " %"; })));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("Spread", 1), "Rand Spread", juce::NormalisableRange<float>(0, 100, 0.1, 1, false), 0,
            juce::AudioParameterFloatAttributes{}.withLabel("%").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 1) + " %"; })));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("randPower", 1), "Rand Power", juce::NormalisableRange<float>(0, 100, 0.1, 1, false), 0,
            juce::AudioParameterFloatAttributes{}.withLabel("%").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 1) + " %"; })));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("randExcitation", 1), "Rand Excitation",
            juce::NormalisableRange<float>(0, 100, 0.1, 1, false), 0,
            juce::AudioParameterFloatAttributes{}.withLabel("%").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 1) + " %"; })));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("softExcitation", 1), "Soft Excitation",
            juce::NormalisableRange<float>(0, 1, 0.01, 1, false), 0,
            juce::AudioParameterFloatAttributes{}.withLabel("%").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 4) + " %"; })));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("sparkleTime", 1), "Sparkle Time",
            juce::NormalisableRange<float>(-1000, 1000, 0.1, 1, false), 0,
            juce::AudioParameterFloatAttributes{}.withLabel("ms").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 1) + " ms"; })));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("sparkleRand", 1), "Sparkle Rand", juce::NormalisableRange<float>(0, 100, 0.1, 1, false),
            0,
            juce::AudioParameterFloatAttributes{}.withLabel("%").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 1) + " %"; })));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("minHarmonics", 1), "Min Harmonics", juce::NormalisableRange<float>(1, 100, 1, 1, false),
            5,
            juce::AudioParameterFloatAttributes{}.withLabel("").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 1) + " "; })));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("maxHarmonics", 1), "Max Harmonics", juce::NormalisableRange<float>(1, 100, 1, 1, false),
            10,
            juce::AudioParameterFloatAttributes{}.withLabel("").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 1) + " "; })));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("minPbNote", 1), "Min PB note", juce::NormalisableRange<float>(1, 127, 1, 1, false), 25,
            juce::AudioParameterFloatAttributes{}.withLabel("").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 1) + " "; })));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("maxPbNote", 1), "Max PB note", juce::NormalisableRange<float>(1, 127, 1, 1, false), 120,
            juce::AudioParameterFloatAttributes{}.withLabel("").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 1) + " "; })));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("rangePb", 1), "Range PB", juce::NormalisableRange<float>(0, 24, 1, 1, false), 12,
            juce::AudioParameterFloatAttributes{}.withLabel("").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 1) + " "; })));

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
            {"mode",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setMode(static_cast<int>(v));
                 p.m_fileIo.updateParameter(PatchParameters::Id::mode, v);
             }},
            {"vol",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setVol(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::vol, v);
             }},
            {"reverbLevel",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setReverbLevel(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::reverbLevel, v);
             }},
            {"x",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setX(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::x, v);
             }},
            {"y",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setY(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::y, v);
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
            {"decaySkew",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setDecaySkew(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::decaySkew, v);
             }},
            {"spread",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setSpread(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::spread, v);
             }},
            {"type",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setType(static_cast<int>(v));
                 p.m_fileIo.updateParameter(PatchParameters::Id::type, v);
             }},
            {"skew",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setSkew(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::skew, v);
             }},
            {"Spread",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setSpread(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::Spread, v);
             }},
            {"randPower",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setRandPower(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::randPower, v);
             }},
            {"randExcitation",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setRandExcitation(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::randExcitation, v);
             }},
            {"softExcitation",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setSoftExcitation(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::softExcitation, v);
             }},
            {"sparkleTime",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setSparkleTime(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::sparkleTime, v);
             }},
            {"sparkleRand",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setSparkleRand(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::sparkleRand, v);
             }},
            {"minHarmonics",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setMinHarmonics(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::minHarmonics, v);
             }},
            {"maxHarmonics",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setMaxHarmonics(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::maxHarmonics, v);
             }},
            {"minPbNote",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setMinPbNote(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::minPbNote, v);
             }},
            {"maxPbNote",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setMaxPbNote(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::maxPbNote, v);
             }},
            {"rangePb",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setRangePb(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::rangePb, v);
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
        if (auto* p = m_parameters.getParameter("mode"))
        {
            const auto& range = m_parameters.getParameterRange("mode");
            float normalized = range.convertTo0to1(params.mode);
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("vol"))
        {
            const auto& range = m_parameters.getParameterRange("vol");
            float normalized = range.convertTo0to1(params.vol);
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("reverbLevel"))
        {
            const auto& range = m_parameters.getParameterRange("reverbLevel");
            float normalized = range.convertTo0to1(params.reverbLevel);
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("x"))
        {
            const auto& range = m_parameters.getParameterRange("x");
            float normalized = range.convertTo0to1(params.x);
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("y"))
        {
            const auto& range = m_parameters.getParameterRange("y");
            float normalized = range.convertTo0to1(params.y);
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
        if (auto* p = m_parameters.getParameter("decaySkew"))
        {
            const auto& range = m_parameters.getParameterRange("decaySkew");
            float normalized = range.convertTo0to1(params.decaySkew);
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("spread"))
        {
            const auto& range = m_parameters.getParameterRange("spread");
            float normalized = range.convertTo0to1(params.spread);
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("type"))
        {
            const auto& range = m_parameters.getParameterRange("type");
            float normalized = range.convertTo0to1(params.type);
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("skew"))
        {
            const auto& range = m_parameters.getParameterRange("skew");
            float normalized = range.convertTo0to1(params.skew);
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("Spread"))
        {
            const auto& range = m_parameters.getParameterRange("Spread");
            float normalized = range.convertTo0to1(params.Spread);
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("randPower"))
        {
            const auto& range = m_parameters.getParameterRange("randPower");
            float normalized = range.convertTo0to1(params.randPower);
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("randExcitation"))
        {
            const auto& range = m_parameters.getParameterRange("randExcitation");
            float normalized = range.convertTo0to1(params.randExcitation);
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("softExcitation"))
        {
            const auto& range = m_parameters.getParameterRange("softExcitation");
            float normalized = range.convertTo0to1(params.softExcitation);
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("sparkleTime"))
        {
            const auto& range = m_parameters.getParameterRange("sparkleTime");
            float normalized = range.convertTo0to1(params.sparkleTime);
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("sparkleRand"))
        {
            const auto& range = m_parameters.getParameterRange("sparkleRand");
            float normalized = range.convertTo0to1(params.sparkleRand);
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("minHarmonics"))
        {
            const auto& range = m_parameters.getParameterRange("minHarmonics");
            float normalized = range.convertTo0to1(params.minHarmonics);
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("maxHarmonics"))
        {
            const auto& range = m_parameters.getParameterRange("maxHarmonics");
            float normalized = range.convertTo0to1(params.maxHarmonics);
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("minPbNote"))
        {
            const auto& range = m_parameters.getParameterRange("minPbNote");
            float normalized = range.convertTo0to1(params.minPbNote);
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("maxPbNote"))
        {
            const auto& range = m_parameters.getParameterRange("maxPbNote");
            float normalized = range.convertTo0to1(params.maxPbNote);
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("rangePb"))
        {
            const auto& range = m_parameters.getParameterRange("rangePb");
            float normalized = range.convertTo0to1(params.rangePb);
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
    std::unique_ptr<PingSynthExplorerPedal<NumSamplesPerBlock>> pluginRunner;

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
