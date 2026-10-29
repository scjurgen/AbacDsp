#pragma once
/*
 * AUTO GENERATED,
 * NOT A GOOD IDEA TO CHANGE STUFF HERE
 * Keep the file readonly
 */

#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <span>

#include "Analysis/EnvelopeFollower.h"
#include "Analysis/Spectrogram.h"
#include "SamplerateConverter/InternalRateNormalizingProcessor.h"
#include "UiElements.h"
#include "impl/CcMapping.h"
#include "impl/CcSettings.h"
#include "impl/FileIo.h"
#include "impl/ListenAndPlayImpl.h"

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
        m_parameters.addParameterListener("bpm", this);
        m_parameters.addParameterListener("clickOn", this);
        m_parameters.addParameterListener("latencyComp", this);
        m_parameters.addParameterListener("instrument", this);
        m_parameters.addParameterListener("input", this);
        m_parameters.addParameterListener("outputVolume", this);
        m_parameters.addParameterListener("inputGain", this);
        m_parameters.addParameterListener("instrumentLevel", this);
        m_parameters.addParameterListener("clickLevel", this);
        m_parameters.addParameterListener("listen", this);
        m_parameters.addParameterListener("play", this);
        m_parameters.addParameterListener("replay", this);
        m_parameters.addParameterListener("stop", this);
        m_parameters.addParameterListener("next", this);
        m_parameters.addParameterListener("reference", this);
        m_parameters.addParameterListener("saveTakes", this);

        m_fileIo.initialize(m_patchIndex);
        initAuthoringServer();
    }
    ~AudioPluginAudioProcessor() override
    {
        m_parameters.removeParameterListener("bpm", this);
        m_parameters.removeParameterListener("clickOn", this);
        m_parameters.removeParameterListener("latencyComp", this);
        m_parameters.removeParameterListener("instrument", this);
        m_parameters.removeParameterListener("input", this);
        m_parameters.removeParameterListener("outputVolume", this);
        m_parameters.removeParameterListener("inputGain", this);
        m_parameters.removeParameterListener("instrumentLevel", this);
        m_parameters.removeParameterListener("clickLevel", this);
        m_parameters.removeParameterListener("listen", this);
        m_parameters.removeParameterListener("play", this);
        m_parameters.removeParameterListener("replay", this);
        m_parameters.removeParameterListener("stop", this);
        m_parameters.removeParameterListener("next", this);
        m_parameters.removeParameterListener("reference", this);
        m_parameters.removeParameterListener("saveTakes", this);
    }

    void prepareToPlay(const double sampleRate, const int samplesPerBlock) override
    {
        pluginRunner = std::make_unique<ListenAndPlayImpl<NumSamplesPerBlock>>(RateNormalizer::kInternalSampleRate);
        pluginRunner->setImportResolver([](const std::string_view name) { return FileIo::resolveLibraryScript(name); });
        if (!m_fileIo.currentScript().empty())
        {
            pluginRunner->setScript(m_fileIo.currentScript());
        }
        restoreProgress();

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
            juce::ParameterID("bpm", 1), juce::String::fromUTF8("BPM"),
            juce::NormalisableRange<float>(40, 200, 0.1, 1, false), 100,
            juce::AudioParameterFloatAttributes{}.withLabel("BPM").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 1) + " BPM"; })));
        params.push_back(std::make_unique<juce::AudioParameterBool>(juce::ParameterID("clickOn", 1),
                                                                    juce::String::fromUTF8("Click"), 1));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("latencyComp", 1), juce::String::fromUTF8("Latency Comp"),
            juce::NormalisableRange<float>(-50, 150, 0.1, 1, false), 0,
            juce::AudioParameterFloatAttributes{}.withLabel("ms").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 1) + " ms"; })));
        params.push_back(std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID("instrument", 1), juce::String::fromUTF8("Instrument"),
            juce::StringArray{juce::String::fromUTF8("Guitar"), juce::String::fromUTF8("Bass"),
                              juce::String::fromUTF8("Harp"), juce::String::fromUTF8("Nylon"),
                              juce::String::fromUTF8("Ukulele"), juce::String::fromUTF8("Music box")},
            0));
        params.push_back(std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID("input", 1), juce::String::fromUTF8("Input"),
            juce::StringArray{juce::String::fromUTF8("Audio"), juce::String::fromUTF8("MIDI")}, 0));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("outputVolume", 1), juce::String::fromUTF8("Volume"),
            juce::NormalisableRange<float>(-60, 12, 0.1, 1, false), 0,
            juce::AudioParameterFloatAttributes{}.withLabel("dB").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 1) + " dB"; })));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("inputGain", 1), juce::String::fromUTF8("Input Gain"),
            juce::NormalisableRange<float>(-24, 24, 0.1, 1, false), 0,
            juce::AudioParameterFloatAttributes{}.withLabel("dB").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 1) + " dB"; })));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("instrumentLevel", 1), juce::String::fromUTF8("Instr. Level"),
            juce::NormalisableRange<float>(-24, 12, 0.1, 1, false), 0,
            juce::AudioParameterFloatAttributes{}.withLabel("dB").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 1) + " dB"; })));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("clickLevel", 1), juce::String::fromUTF8("Click Level"),
            juce::NormalisableRange<float>(-24, 12, 0.1, 1, false), 0,
            juce::AudioParameterFloatAttributes{}.withLabel("dB").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 1) + " dB"; })));
        params.push_back(std::make_unique<juce::AudioParameterBool>(juce::ParameterID("listen", 1),
                                                                    juce::String::fromUTF8("Listen"), 0));
        params.push_back(std::make_unique<juce::AudioParameterBool>(juce::ParameterID("play", 1),
                                                                    juce::String::fromUTF8("Play"), 0));
        params.push_back(std::make_unique<juce::AudioParameterBool>(juce::ParameterID("replay", 1),
                                                                    juce::String::fromUTF8("Replay Take"), 0));
        params.push_back(std::make_unique<juce::AudioParameterBool>(juce::ParameterID("stop", 1),
                                                                    juce::String::fromUTF8("Stop"), 0));
        params.push_back(std::make_unique<juce::AudioParameterBool>(juce::ParameterID("next", 1),
                                                                    juce::String::fromUTF8("Next Phrase"), 0));
        params.push_back(std::make_unique<juce::AudioParameterBool>(juce::ParameterID("reference", 1),
                                                                    juce::String::fromUTF8("Reference"), 0));
        params.push_back(std::make_unique<juce::AudioParameterBool>(juce::ParameterID("saveTakes", 1),
                                                                    juce::String::fromUTF8("Save Takes"), 0));

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
            {"bpm",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setBpm(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::bpm, v);
             }},
            {"clickOn",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setClickOn(static_cast<bool>(v));
                 p.m_fileIo.updateParameter(PatchParameters::Id::clickOn, v);
             }},
            {"latencyComp",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setLatencyComp(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::latencyComp, v);
             }},
            {"instrument",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setInstrument(static_cast<size_t>(v));
                 p.m_fileIo.updateParameter(PatchParameters::Id::instrument, v);
             }},
            {"input",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setInput(static_cast<size_t>(v));
                 p.m_fileIo.updateParameter(PatchParameters::Id::input, v);
             }},
            {"outputVolume",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setOutputVolume(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::outputVolume, v);
             }},
            {"inputGain",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setInputGain(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::inputGain, v);
             }},
            {"instrumentLevel",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setInstrumentLevel(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::instrumentLevel, v);
             }},
            {"clickLevel",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setClickLevel(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::clickLevel, v);
             }},
            {"listen",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setListen(static_cast<bool>(v));
                 p.m_fileIo.updateParameter(PatchParameters::Id::listen, v);
             }},
            {"play",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setPlay(static_cast<bool>(v));
                 p.m_fileIo.updateParameter(PatchParameters::Id::play, v);
             }},
            {"replay",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setReplay(static_cast<bool>(v));
                 p.m_fileIo.updateParameter(PatchParameters::Id::replay, v);
             }},
            {"stop",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setStop(static_cast<bool>(v));
                 p.m_fileIo.updateParameter(PatchParameters::Id::stop, v);
             }},
            {"next",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setNext(static_cast<bool>(v));
                 p.m_fileIo.updateParameter(PatchParameters::Id::next, v);
             }},
            {"reference",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setReference(static_cast<bool>(v));
                 p.m_fileIo.updateParameter(PatchParameters::Id::reference, v);
             }},
            {"saveTakes",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setSaveTakes(static_cast<bool>(v));
                 p.m_fileIo.updateParameter(PatchParameters::Id::saveTakes, v);
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
        if (auto* p = m_parameters.getParameter("bpm"))
        {
            const auto& range = m_parameters.getParameterRange("bpm");
            float normalized = range.convertTo0to1(static_cast<float>(params.bpm));
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("clickOn"))
        {
            const auto& range = m_parameters.getParameterRange("clickOn");
            float normalized = range.convertTo0to1(static_cast<float>(params.clickOn));
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("latencyComp"))
        {
            const auto& range = m_parameters.getParameterRange("latencyComp");
            float normalized = range.convertTo0to1(static_cast<float>(params.latencyComp));
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("instrument"))
        {
            const auto& range = m_parameters.getParameterRange("instrument");
            float normalized = range.convertTo0to1(static_cast<float>(params.instrument));
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("input"))
        {
            const auto& range = m_parameters.getParameterRange("input");
            float normalized = range.convertTo0to1(static_cast<float>(params.input));
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("outputVolume"))
        {
            const auto& range = m_parameters.getParameterRange("outputVolume");
            float normalized = range.convertTo0to1(static_cast<float>(params.outputVolume));
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("inputGain"))
        {
            const auto& range = m_parameters.getParameterRange("inputGain");
            float normalized = range.convertTo0to1(static_cast<float>(params.inputGain));
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("instrumentLevel"))
        {
            const auto& range = m_parameters.getParameterRange("instrumentLevel");
            float normalized = range.convertTo0to1(static_cast<float>(params.instrumentLevel));
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("clickLevel"))
        {
            const auto& range = m_parameters.getParameterRange("clickLevel");
            float normalized = range.convertTo0to1(static_cast<float>(params.clickLevel));
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("listen"))
        {
            const auto& range = m_parameters.getParameterRange("listen");
            float normalized = range.convertTo0to1(static_cast<float>(params.listen));
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("play"))
        {
            const auto& range = m_parameters.getParameterRange("play");
            float normalized = range.convertTo0to1(static_cast<float>(params.play));
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("replay"))
        {
            const auto& range = m_parameters.getParameterRange("replay");
            float normalized = range.convertTo0to1(static_cast<float>(params.replay));
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("stop"))
        {
            const auto& range = m_parameters.getParameterRange("stop");
            float normalized = range.convertTo0to1(static_cast<float>(params.stop));
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("next"))
        {
            const auto& range = m_parameters.getParameterRange("next");
            float normalized = range.convertTo0to1(static_cast<float>(params.next));
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("reference"))
        {
            const auto& range = m_parameters.getParameterRange("reference");
            float normalized = range.convertTo0to1(static_cast<float>(params.reference));
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("saveTakes"))
        {
            const auto& range = m_parameters.getParameterRange("saveTakes");
            float normalized = range.convertTo0to1(static_cast<float>(params.saveTakes));
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
        m_authoringServer.cpuLoadPercent = [] { return 0.f; };
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


#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-conversion"

    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) override
    {
        juce::ScopedNoDenormals noDenormals;
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
                pluginRunner->processMidiEvent(std::span{msg.data, static_cast<size_t>(msg.numBytes)},
                                               toInternalSampleOffset(msg.samplePosition));
            }
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
    }

#pragma GCC diagnostic pop


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
    [[nodiscard]] ListenAndPlayScriptEngine::UiParamSlots getLuaUiParamSlots() const
    {
        return pluginRunner ? pluginRunner->uiParamSlots() : ListenAndPlayScriptEngine::UiParamSlots{};
    }
    [[nodiscard]] ListenPlayDisplay::PianoRollSnapshot getPianoRollSnapshot() const
    {
        return pluginRunner ? pluginRunner->pianoRollSnapshot() : ListenPlayDisplay::PianoRollSnapshot{};
    }
    [[nodiscard]] ListenPlayDisplay::ScoreSnapshot getScoreSnapshot() const
    {
        return pluginRunner ? pluginRunner->scoreSnapshot() : ListenPlayDisplay::ScoreSnapshot{};
    }
    [[nodiscard]] uint64_t lessonRowsVersion() const
    {
        return pluginRunner ? pluginRunner->lessonRowsVersion() : uint64_t{};
    }
    void pollLesson()
    {
        if (pluginRunner)
        {
            pluginRunner->poll();
        }
    }
    [[nodiscard]] std::optional<juce::String> pollStatusText()
    {
        const std::string text = pluginRunner ? pluginRunner->statusText() : std::string{};
        if (text == m_lastStatus)
        {
            return std::nullopt;
        }
        m_lastStatus = text;
        return juce::String(text);
    }
    void pollResult()
    {
        if (!pluginRunner)
        {
            return;
        }
        if (const auto result = pluginRunner->takeNewResult())
        {
            m_lastReportHtml = result->reportHtml;
            m_resultArrived = true;
            if (pluginRunner->saveTakes())
            {
                saveTake(*result);
            }
        }
        saveProgress();
    }
    void saveTake(const TakeAnalysis::TakeResult& result)
    {
        const auto root =
            juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile("ListenAndPlay Recordings");
        const auto stamp = juce::Time::getCurrentTime().formatted("%Y%m%d-%H%M%S").toStdString();
        static_cast<void>(TakeExport::writeTake(std::filesystem::path{root.getFullPathName().toStdString()}, stamp,
                                                result, pluginRunner->takeExportContext()));
    }
    [[nodiscard]] bool consumeResultArrived()
    {
        return m_resultArrived.exchange(false);
    }
    [[nodiscard]] std::vector<ListenPlayDisplay::LessonRow> lessonRows()
    {
        return pluginRunner ? pluginRunner->lessonRows() : std::vector<ListenPlayDisplay::LessonRow>{};
    }
    bool loadLesson(const juce::String& stem)
    {
        if (!pluginRunner)
        {
            return false;
        }
        const auto lookup = FileIo::resolveLibraryScript(stem.toStdString());
        if (!lookup.source)
        {
            return false;
        }
        pluginRunner->prepareLesson(stem.toStdString());
        const bool loaded = applyScriptText(juce::String::fromUTF8(lookup.source->c_str()));
        saveProgress();
        return loaded;
    }
    bool loadRecommendedLesson()
    {
        const auto stem = pluginRunner ? pluginRunner->recommendedLesson() : std::nullopt;
        return stem && loadLesson(juce::String(*stem));
    }
    void reloadLessons()
    {
        if (pluginRunner)
        {
            pluginRunner->refreshLessons();
        }
    }
    void saveProgress()
    {
        if (pluginRunner && pluginRunner->consumeProgressDirty())
        {
            ProgressStore::save(pluginRunner->progressJson());
        }
    }
    void restoreProgress()
    {
        if (!pluginRunner)
        {
            return;
        }
        pluginRunner->setLessonLister([] { return FileIo::listLibraryScriptNames(); });
        pluginRunner->loadProgress(ProgressStore::load());
        const std::string last = pluginRunner->lastLessonStem();
        if (last.empty())
        {
            return;
        }
        const auto lookup = FileIo::resolveLibraryScript(last);
        if (lookup.source && (m_fileIo.currentScript().empty() || *lookup.source == m_fileIo.currentScript()))
        {
            loadLesson(juce::String(last));
        }
    }
    void openLastReport()
    {
        if (m_lastReportHtml.empty())
        {
            return;
        }
        auto dir =
            juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile("ListenAndPlay Analysis");
        dir.createDirectory();
        const auto file = dir.getChildFile("take-" + juce::Time::getCurrentTime().formatted("%Y%m%d-%H%M%S") + ".html");
        file.replaceWithText(juce::String::fromUTF8(m_lastReportHtml.c_str()));
        juce::URL(file).launchInDefaultBrowser();
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
    // The runner works at the internal rate; the resampler's constant FIFO delay is not included
    // here and is left to the runner's latency compensation.
    [[nodiscard]] int toInternalSampleOffset(const int hostOffset) const noexcept
    {
        return static_cast<int>(
            std::lround(static_cast<double>(hostOffset) * RateNormalizer::kInternalSampleRate / getSampleRate()));
    }

    int m_program{0};

    std::unique_ptr<RateNormalizer> fixedRunner;
    std::unique_ptr<ListenAndPlayImpl<NumSamplesPerBlock>> pluginRunner;
    std::string m_lastStatus;
    std::string m_lastReportHtml;
    std::atomic<bool> m_resultArrived{false};

    juce::AudioProcessorValueTreeState m_parameters;
    std::vector<int> m_patchIndex;
    FileIo m_fileIo;
    AuthoringAudioRecorder m_authoringRecorder;
    juce::MidiMessageCollector m_authoringMidiCollector;
    // Declared last so it is destroyed first - its destructor blocks until every
    // in-flight request finishes, and a handler reaches into this processor meanwhile.
    AuthoringHttpServer m_authoringServer;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioPluginAudioProcessor)
};
