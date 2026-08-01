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
#include "impl/TapeDelay.h"

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
        m_parameters.addParameterListener("feedGain", this);
        m_parameters.addParameterListener("tapeSpeed", this);
        m_parameters.addParameterListener("wow", this);
        m_parameters.addParameterListener("hysteresis", this);
        m_parameters.addParameterListener("saturation", this);
        m_parameters.addParameterListener("noiseFloor", this);
        m_parameters.addParameterListener("noiseDistribution", this);
        m_parameters.addParameterListener("delayTime1", this);
        m_parameters.addParameterListener("delayTime2", this);
        m_parameters.addParameterListener("delayTime3", this);
        m_parameters.addParameterListener("delayTime4", this);
        m_parameters.addParameterListener("delayLevel1", this);
        m_parameters.addParameterListener("delayLevel2", this);
        m_parameters.addParameterListener("delayLevel3", this);
        m_parameters.addParameterListener("delayLevel4", this);
        m_parameters.addParameterListener("feedback1", this);
        m_parameters.addParameterListener("feedback2", this);
        m_parameters.addParameterListener("feedback3", this);
        m_parameters.addParameterListener("feedback4", this);

        m_fileIo.initialize(m_patchIndex);
    }
    ~AudioPluginAudioProcessor() override
    {
        m_parameters.removeParameterListener("feedGain", this);
        m_parameters.removeParameterListener("tapeSpeed", this);
        m_parameters.removeParameterListener("wow", this);
        m_parameters.removeParameterListener("hysteresis", this);
        m_parameters.removeParameterListener("saturation", this);
        m_parameters.removeParameterListener("noiseFloor", this);
        m_parameters.removeParameterListener("noiseDistribution", this);
        m_parameters.removeParameterListener("delayTime1", this);
        m_parameters.removeParameterListener("delayTime2", this);
        m_parameters.removeParameterListener("delayTime3", this);
        m_parameters.removeParameterListener("delayTime4", this);
        m_parameters.removeParameterListener("delayLevel1", this);
        m_parameters.removeParameterListener("delayLevel2", this);
        m_parameters.removeParameterListener("delayLevel3", this);
        m_parameters.removeParameterListener("delayLevel4", this);
        m_parameters.removeParameterListener("feedback1", this);
        m_parameters.removeParameterListener("feedback2", this);
        m_parameters.removeParameterListener("feedback3", this);
        m_parameters.removeParameterListener("feedback4", this);
    }

    void prepareToPlay(const double sampleRate, const int samplesPerBlock) override
    {
        pluginRunner = std::make_unique<TapeDelay<NumSamplesPerBlock>>(RateNormalizer::kInternalSampleRate);

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
            juce::ParameterID("feedGain", 1), "Feed", juce::NormalisableRange<float>(-60, 12, 0.1, 1, false), 0,
            juce::AudioParameterFloatAttributes{}.withLabel("dB").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 1) + " dB"; })));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("tapeSpeed", 1), "Tape speed", juce::NormalisableRange<float>(0.5, 60.0, 0.1, 1.0, false),
            7.5,
            juce::AudioParameterFloatAttributes{}.withLabel("IPS").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 0) + " IPS"; })));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("wow", 1), "WOW", juce::NormalisableRange<float>(0, 1, 0.01, 1.0, false), 0,
            juce::AudioParameterFloatAttributes{}.withLabel("").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 0) + " "; })));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("hysteresis", 1), "Hysteresis", juce::NormalisableRange<float>(0, 1, 0.01, 1.0, false), 0,
            juce::AudioParameterFloatAttributes{}.withLabel("").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 0) + " "; })));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("saturation", 1), "Saturation", juce::NormalisableRange<float>(0, 1, 0.01, 1.0, false), 0,
            juce::AudioParameterFloatAttributes{}.withLabel("").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 0) + " "; })));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("noiseFloor", 1), "Noise floor", juce::NormalisableRange<float>(0, 1, 0.01, 1.0, false),
            0,
            juce::AudioParameterFloatAttributes{}.withLabel("").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 0) + " "; })));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("noiseDistribution", 1), "Noise distribution",
            juce::NormalisableRange<float>(0, 1, 0.01, 1.0, false), 0,
            juce::AudioParameterFloatAttributes{}.withLabel("").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 0) + " "; })));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("delayTime1", 1), "Delay time 1",
            juce::NormalisableRange<float>(0, 1000, 0.1, 0.4, false), 200,
            juce::AudioParameterFloatAttributes{}.withLabel("ms").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 0) + " ms"; })));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("delayTime2", 1), "Delay time 2",
            juce::NormalisableRange<float>(0, 1000, 0.1, 0.4, false), 100,
            juce::AudioParameterFloatAttributes{}.withLabel("ms").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 0) + " ms"; })));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("delayTime3", 1), "Delay time 3",
            juce::NormalisableRange<float>(0, 1000, 0.1, 0.4, false), 100,
            juce::AudioParameterFloatAttributes{}.withLabel("ms").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 0) + " ms"; })));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("delayTime4", 1), "Tape time 4",
            juce::NormalisableRange<float>(1000, 10000, 0.1, 0.4, false), 2000,
            juce::AudioParameterFloatAttributes{}.withLabel("ms").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 0) + " ms"; })));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("delayLevel1", 1), "Delay level 1",
            juce::NormalisableRange<float>(-72, 12, 0.1, 1.0, false), 0,
            juce::AudioParameterFloatAttributes{}.withLabel("dB").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 0) + " dB"; })));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("delayLevel2", 1), "Delay level 2",
            juce::NormalisableRange<float>(-72, 12, 0.1, 1.0, false), -6,
            juce::AudioParameterFloatAttributes{}.withLabel("dB").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 0) + " dB"; })));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("delayLevel3", 1), "Delay level 3",
            juce::NormalisableRange<float>(-72, 12, 0.1, 1.0, false), -12,
            juce::AudioParameterFloatAttributes{}.withLabel("dB").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 0) + " dB"; })));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("delayLevel4", 1), "Delay level 4",
            juce::NormalisableRange<float>(-72, 12, 0.1, 1.0, false), -18,
            juce::AudioParameterFloatAttributes{}.withLabel("dB").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 0) + " dB"; })));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("feedback1", 1), "Feedback 1", juce::NormalisableRange<float>(0, 1, 0.01, 1.0, false), 0,
            juce::AudioParameterFloatAttributes{}.withLabel("").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 0) + " "; })));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("feedback2", 1), "Feedback 2", juce::NormalisableRange<float>(0, 1, 0.01, 1.0, false), 0,
            juce::AudioParameterFloatAttributes{}.withLabel("").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 0) + " "; })));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("feedback3", 1), "Feedback 3", juce::NormalisableRange<float>(0, 1, 0.01, 1.0, false), 0,
            juce::AudioParameterFloatAttributes{}.withLabel("").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 0) + " "; })));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("feedback4", 1), "Feedback 4", juce::NormalisableRange<float>(0, 1, 0.01, 1.0, false), 0,
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
            {"feedGain",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setFeedGain(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::feedGain, v);
             }},
            {"tapeSpeed",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setTapeSpeed(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::tapeSpeed, v);
             }},
            {"wow",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setWow(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::wow, v);
             }},
            {"hysteresis",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setHysteresis(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::hysteresis, v);
             }},
            {"saturation",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setSaturation(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::saturation, v);
             }},
            {"noiseFloor",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setNoiseFloor(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::noiseFloor, v);
             }},
            {"noiseDistribution",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setNoiseDistribution(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::noiseDistribution, v);
             }},
            {"delayTime1",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setDelayTime1(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::delayTime1, v);
             }},
            {"delayTime2",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setDelayTime2(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::delayTime2, v);
             }},
            {"delayTime3",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setDelayTime3(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::delayTime3, v);
             }},
            {"delayTime4",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setDelayTime4(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::delayTime4, v);
             }},
            {"delayLevel1",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setDelayLevel1(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::delayLevel1, v);
             }},
            {"delayLevel2",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setDelayLevel2(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::delayLevel2, v);
             }},
            {"delayLevel3",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setDelayLevel3(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::delayLevel3, v);
             }},
            {"delayLevel4",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setDelayLevel4(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::delayLevel4, v);
             }},
            {"feedback1",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setFeedback1(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::feedback1, v);
             }},
            {"feedback2",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setFeedback2(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::feedback2, v);
             }},
            {"feedback3",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setFeedback3(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::feedback3, v);
             }},
            {"feedback4",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setFeedback4(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::feedback4, v);
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
        if (auto* p = m_parameters.getParameter("feedGain"))
        {
            const auto& range = m_parameters.getParameterRange("feedGain");
            float normalized = range.convertTo0to1(params.feedGain);
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("tapeSpeed"))
        {
            const auto& range = m_parameters.getParameterRange("tapeSpeed");
            float normalized = range.convertTo0to1(params.tapeSpeed);
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("wow"))
        {
            const auto& range = m_parameters.getParameterRange("wow");
            float normalized = range.convertTo0to1(params.wow);
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("hysteresis"))
        {
            const auto& range = m_parameters.getParameterRange("hysteresis");
            float normalized = range.convertTo0to1(params.hysteresis);
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("saturation"))
        {
            const auto& range = m_parameters.getParameterRange("saturation");
            float normalized = range.convertTo0to1(params.saturation);
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("noiseFloor"))
        {
            const auto& range = m_parameters.getParameterRange("noiseFloor");
            float normalized = range.convertTo0to1(params.noiseFloor);
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("noiseDistribution"))
        {
            const auto& range = m_parameters.getParameterRange("noiseDistribution");
            float normalized = range.convertTo0to1(params.noiseDistribution);
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("delayTime1"))
        {
            const auto& range = m_parameters.getParameterRange("delayTime1");
            float normalized = range.convertTo0to1(params.delayTime1);
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("delayTime2"))
        {
            const auto& range = m_parameters.getParameterRange("delayTime2");
            float normalized = range.convertTo0to1(params.delayTime2);
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("delayTime3"))
        {
            const auto& range = m_parameters.getParameterRange("delayTime3");
            float normalized = range.convertTo0to1(params.delayTime3);
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("delayTime4"))
        {
            const auto& range = m_parameters.getParameterRange("delayTime4");
            float normalized = range.convertTo0to1(params.delayTime4);
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("delayLevel1"))
        {
            const auto& range = m_parameters.getParameterRange("delayLevel1");
            float normalized = range.convertTo0to1(params.delayLevel1);
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("delayLevel2"))
        {
            const auto& range = m_parameters.getParameterRange("delayLevel2");
            float normalized = range.convertTo0to1(params.delayLevel2);
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("delayLevel3"))
        {
            const auto& range = m_parameters.getParameterRange("delayLevel3");
            float normalized = range.convertTo0to1(params.delayLevel3);
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("delayLevel4"))
        {
            const auto& range = m_parameters.getParameterRange("delayLevel4");
            float normalized = range.convertTo0to1(params.delayLevel4);
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("feedback1"))
        {
            const auto& range = m_parameters.getParameterRange("feedback1");
            float normalized = range.convertTo0to1(params.feedback1);
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("feedback2"))
        {
            const auto& range = m_parameters.getParameterRange("feedback2");
            float normalized = range.convertTo0to1(params.feedback2);
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("feedback3"))
        {
            const auto& range = m_parameters.getParameterRange("feedback3");
            float normalized = range.convertTo0to1(params.feedback3);
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("feedback4"))
        {
            const auto& range = m_parameters.getParameterRange("feedback4");
            float normalized = range.convertTo0to1(params.feedback4);
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

    [[nodiscard]] const std::vector<float>& getWaveDataToShow()
    {
        return pluginRunner->visualizeWaveData();
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
    std::unique_ptr<TapeDelay<NumSamplesPerBlock>> pluginRunner;

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
