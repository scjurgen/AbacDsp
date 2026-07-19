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
#include "impl/MiniReverbImpl.h"

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
        m_parameters.addParameterListener("order", this);
        m_parameters.addParameterListener("dry", this);
        m_parameters.addParameterListener("wet", this);
        m_parameters.addParameterListener("stereoWidth", this);
        m_parameters.addParameterListener("baseSize", this);
        m_parameters.addParameterListener("sizeFactor", this);
        m_parameters.addParameterListener("bulge", this);
        m_parameters.addParameterListener("uniqueDelay", this);
        m_parameters.addParameterListener("decay", this);
        m_parameters.addParameterListener("allPassUp", this);
        m_parameters.addParameterListener("allPassDown", this);
        m_parameters.addParameterListener("lowPass", this);
        m_parameters.addParameterListener("lowPassCount", this);
        m_parameters.addParameterListener("highPass", this);
        m_parameters.addParameterListener("highPassCount", this);
        m_parameters.addParameterListener("modulationDepth", this);
        m_parameters.addParameterListener("modulationSpeed", this);
        m_parameters.addParameterListener("reversePitch", this);
        m_parameters.addParameterListener("pitchStrength", this);
        m_parameters.addParameterListener("pitch1Inplace", this);
        m_parameters.addParameterListener("pitch2Inplace", this);

        m_fileIo.initialize(m_patchIndex);
    }
    ~AudioPluginAudioProcessor() override
    {
        m_parameters.removeParameterListener("order", this);
        m_parameters.removeParameterListener("dry", this);
        m_parameters.removeParameterListener("wet", this);
        m_parameters.removeParameterListener("stereoWidth", this);
        m_parameters.removeParameterListener("baseSize", this);
        m_parameters.removeParameterListener("sizeFactor", this);
        m_parameters.removeParameterListener("bulge", this);
        m_parameters.removeParameterListener("uniqueDelay", this);
        m_parameters.removeParameterListener("decay", this);
        m_parameters.removeParameterListener("allPassUp", this);
        m_parameters.removeParameterListener("allPassDown", this);
        m_parameters.removeParameterListener("lowPass", this);
        m_parameters.removeParameterListener("lowPassCount", this);
        m_parameters.removeParameterListener("highPass", this);
        m_parameters.removeParameterListener("highPassCount", this);
        m_parameters.removeParameterListener("modulationDepth", this);
        m_parameters.removeParameterListener("modulationSpeed", this);
        m_parameters.removeParameterListener("reversePitch", this);
        m_parameters.removeParameterListener("pitchStrength", this);
        m_parameters.removeParameterListener("pitch1Inplace", this);
        m_parameters.removeParameterListener("pitch2Inplace", this);
    }

    void prepareToPlay(const double sampleRate, const int samplesPerBlock) override
    {
        pluginRunner = std::make_unique<MiniReverbImpl<NumSamplesPerBlock>>(RateNormalizer::kInternalSampleRate);
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
            juce::ParameterID("order", 1), "Order",
            juce::StringArray{"4", "8", "12", "16", "20", "24", "32", "48", "64"}, 8));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("dry", 1), "Dry", juce::NormalisableRange<float>(-100, 12, 0.1, 1, false), 0,
            juce::AudioParameterFloatAttributes{}.withLabel("dB").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 1) + " dB"; })));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("wet", 1), "Wet", juce::NormalisableRange<float>(-100, 12, 0.1, 1, false), 0,
            juce::AudioParameterFloatAttributes{}.withLabel("dB").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 1) + " dB"; })));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("stereoWidth", 1), "Stereo Width", juce::NormalisableRange<float>(0, 100, 0.1, 1, false),
            100,
            juce::AudioParameterFloatAttributes{}.withLabel("").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 1) + " "; })));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("baseSize", 1), "Base size", juce::NormalisableRange<float>(1.0, 600, 0.01, 0.5, false),
            10,
            juce::AudioParameterFloatAttributes{}.withLabel("m").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 2) + " m"; })));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("sizeFactor", 1), "Size Factor",
            juce::NormalisableRange<float>(1.0, 20, 0.01, 1.0, false), 3.1,
            juce::AudioParameterFloatAttributes{}.withLabel("x").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 2) + " x"; })));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("bulge", 1), "Bulge", juce::NormalisableRange<float>(-1, 1, 0.01, 1, false), 0,
            juce::AudioParameterFloatAttributes{}.withLabel("").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 2) + " "; })));
        params.push_back(
            std::make_unique<juce::AudioParameterBool>(juce::ParameterID("uniqueDelay", 1), "Unique delay", 1));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("decay", 1), "Decay Low", juce::NormalisableRange<float>(0, 100000, 0.1, 0.25, false),
            2000,
            juce::AudioParameterFloatAttributes{}.withLabel("ms").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 1) + " ms"; })));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("allPassUp", 1), "All pass First",
            juce::NormalisableRange<float>(20, 20000, 1, 0.5, false), 10000,
            juce::AudioParameterFloatAttributes{}.withLabel("Hz").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 0) + " Hz"; })));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("allPassDown", 1), "All pass Last",
            juce::NormalisableRange<float>(20, 20000, 1, 0.5, false), 100,
            juce::AudioParameterFloatAttributes{}.withLabel("Hz").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 0) + " Hz"; })));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("lowPass", 1), "Low pass", juce::NormalisableRange<float>(20, 20000, 1, 0.5, false), 3000,
            juce::AudioParameterFloatAttributes{}.withLabel("Hz").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 0) + " Hz"; })));
        params.push_back(std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID("lowPassCount", 1), "Low pass count",
            juce::StringArray{"none", "one", "two", "1/4", "1/2", "3/4", "All"}, 0));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("highPass", 1), "High pass", juce::NormalisableRange<float>(20, 20000, 1, 0.5, false),
            3000,
            juce::AudioParameterFloatAttributes{}.withLabel("Hz").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 0) + " Hz"; })));
        params.push_back(std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID("highPassCount", 1), "High pass count",
            juce::StringArray{"none", "one", "two", "1/4", "1/2", "3/4", "All"}, 0));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("modulationDepth", 1), "Mod depth", juce::NormalisableRange<float>(0, 1, 0.01, 1, false),
            0.02,
            juce::AudioParameterFloatAttributes{}.withLabel("").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 2) + " "; })));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("modulationSpeed", 1), "Mod speed",
            juce::NormalisableRange<float>(0.01, 5, 0.01, 0.5, false), 0.25,
            juce::AudioParameterFloatAttributes{}.withLabel("Hz").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 2) + " Hz"; })));
        params.push_back(
            std::make_unique<juce::AudioParameterBool>(juce::ParameterID("reversePitch", 1), "Reverse pitch", 0));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("pitchStrength", 1), "Pitch Strength",
            juce::NormalisableRange<float>(0.0, 1.0, 0.01, 1, false), 0.5,
            juce::AudioParameterFloatAttributes{}.withLabel("").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 2) + " "; })));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("pitch1Inplace", 1), "Pitch 1 inplace",
            juce::NormalisableRange<float>(-12, 12, 0.01, 1, false), 0,
            juce::AudioParameterFloatAttributes{}.withLabel("st").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 2) + " st"; })));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("pitch2Inplace", 1), "Pitch 2 inplace",
            juce::NormalisableRange<float>(-12, 12, 0.01, 1, false), 0,
            juce::AudioParameterFloatAttributes{}.withLabel("st").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 2) + " st"; })));

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
            {"order",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setOrder(static_cast<size_t>(v));
                 p.m_fileIo.updateParameter(PatchParameters::Id::order, v);
             }},
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
            {"stereoWidth",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setStereoWidth(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::stereoWidth, v);
             }},
            {"baseSize",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setBaseSize(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::baseSize, v);
             }},
            {"sizeFactor",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setSizeFactor(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::sizeFactor, v);
             }},
            {"bulge",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setBulge(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::bulge, v);
             }},
            {"uniqueDelay",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setUniqueDelay(static_cast<bool>(v));
                 p.m_fileIo.updateParameter(PatchParameters::Id::uniqueDelay, v);
             }},
            {"decay",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setDecay(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::decay, v);
             }},
            {"allPassUp",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setAllPassUp(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::allPassUp, v);
             }},
            {"allPassDown",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setAllPassDown(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::allPassDown, v);
             }},
            {"lowPass",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setLowPass(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::lowPass, v);
             }},
            {"lowPassCount",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setLowPassCount(static_cast<int>(v));
                 p.m_fileIo.updateParameter(PatchParameters::Id::lowPassCount, v);
             }},
            {"highPass",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setHighPass(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::highPass, v);
             }},
            {"highPassCount",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setHighPassCount(static_cast<int>(v));
                 p.m_fileIo.updateParameter(PatchParameters::Id::highPassCount, v);
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
            {"reversePitch",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setReversePitch(static_cast<bool>(v));
                 p.m_fileIo.updateParameter(PatchParameters::Id::reversePitch, v);
             }},
            {"pitchStrength",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setPitchStrength(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::pitchStrength, v);
             }},
            {"pitch1Inplace",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setPitch1Inplace(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::pitch1Inplace, v);
             }},
            {"pitch2Inplace",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setPitch2Inplace(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::pitch2Inplace, v);
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
        if (auto* p = m_parameters.getParameter("order"))
        {
            const auto& range = m_parameters.getParameterRange("order");
            float normalized = range.convertTo0to1(params.order);
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("dry"))
        {
            const auto& range = m_parameters.getParameterRange("dry");
            float normalized = range.convertTo0to1(params.dry);
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("wet"))
        {
            const auto& range = m_parameters.getParameterRange("wet");
            float normalized = range.convertTo0to1(params.wet);
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("stereoWidth"))
        {
            const auto& range = m_parameters.getParameterRange("stereoWidth");
            float normalized = range.convertTo0to1(params.stereoWidth);
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("baseSize"))
        {
            const auto& range = m_parameters.getParameterRange("baseSize");
            float normalized = range.convertTo0to1(params.baseSize);
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("sizeFactor"))
        {
            const auto& range = m_parameters.getParameterRange("sizeFactor");
            float normalized = range.convertTo0to1(params.sizeFactor);
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("bulge"))
        {
            const auto& range = m_parameters.getParameterRange("bulge");
            float normalized = range.convertTo0to1(params.bulge);
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("uniqueDelay"))
        {
            const auto& range = m_parameters.getParameterRange("uniqueDelay");
            float normalized = range.convertTo0to1(params.uniqueDelay);
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("decay"))
        {
            const auto& range = m_parameters.getParameterRange("decay");
            float normalized = range.convertTo0to1(params.decay);
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("allPassUp"))
        {
            const auto& range = m_parameters.getParameterRange("allPassUp");
            float normalized = range.convertTo0to1(params.allPassUp);
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("allPassDown"))
        {
            const auto& range = m_parameters.getParameterRange("allPassDown");
            float normalized = range.convertTo0to1(params.allPassDown);
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("lowPass"))
        {
            const auto& range = m_parameters.getParameterRange("lowPass");
            float normalized = range.convertTo0to1(params.lowPass);
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("lowPassCount"))
        {
            const auto& range = m_parameters.getParameterRange("lowPassCount");
            float normalized = range.convertTo0to1(params.lowPassCount);
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("highPass"))
        {
            const auto& range = m_parameters.getParameterRange("highPass");
            float normalized = range.convertTo0to1(params.highPass);
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("highPassCount"))
        {
            const auto& range = m_parameters.getParameterRange("highPassCount");
            float normalized = range.convertTo0to1(params.highPassCount);
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("modulationDepth"))
        {
            const auto& range = m_parameters.getParameterRange("modulationDepth");
            float normalized = range.convertTo0to1(params.modulationDepth);
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("modulationSpeed"))
        {
            const auto& range = m_parameters.getParameterRange("modulationSpeed");
            float normalized = range.convertTo0to1(params.modulationSpeed);
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("reversePitch"))
        {
            const auto& range = m_parameters.getParameterRange("reversePitch");
            float normalized = range.convertTo0to1(params.reversePitch);
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("pitchStrength"))
        {
            const auto& range = m_parameters.getParameterRange("pitchStrength");
            float normalized = range.convertTo0to1(params.pitchStrength);
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("pitch1Inplace"))
        {
            const auto& range = m_parameters.getParameterRange("pitch1Inplace");
            float normalized = range.convertTo0to1(params.pitch1Inplace);
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("pitch2Inplace"))
        {
            const auto& range = m_parameters.getParameterRange("pitch2Inplace");
            float normalized = range.convertTo0to1(params.pitch2Inplace);
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
    std::unique_ptr<MiniReverbImpl<NumSamplesPerBlock>> pluginRunner;
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
