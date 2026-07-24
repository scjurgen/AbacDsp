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
#include "impl/LooperImpl.h"

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
        m_parameters.addParameterListener("record", this);
        m_parameters.addParameterListener("play", this);
        m_parameters.addParameterListener("overdub", this);
        m_parameters.addParameterListener("clear", this);
        m_parameters.addParameterListener("threshRec", this);
        m_parameters.addParameterListener("hostSync", this);
        m_parameters.addParameterListener("sliceMode", this);
        m_parameters.addParameterListener("sliceDivision", this);
        m_parameters.addParameterListener("bpm", this);
        m_parameters.addParameterListener("swing", this);
        m_parameters.addParameterListener("clickVolume", this);
        m_parameters.addParameterListener("loopVolume", this);
        m_parameters.addParameterListener("recThreshold", this);

        for (size_t i = 0; i < 10; ++i)
        {
            m_ccActive[i].controller.store(kDefaultCcMappings[i].controller, std::memory_order_relaxed);
            m_ccActive[i].valueLow.store(kDefaultCcMappings[i].valueLow, std::memory_order_relaxed);
            m_ccActive[i].valueHigh.store(kDefaultCcMappings[i].valueHigh, std::memory_order_relaxed);
        }
        m_fileIo.initialize(m_patchIndex);
    }
    ~AudioPluginAudioProcessor() override
    {
        m_parameters.removeParameterListener("record", this);
        m_parameters.removeParameterListener("play", this);
        m_parameters.removeParameterListener("overdub", this);
        m_parameters.removeParameterListener("clear", this);
        m_parameters.removeParameterListener("threshRec", this);
        m_parameters.removeParameterListener("hostSync", this);
        m_parameters.removeParameterListener("sliceMode", this);
        m_parameters.removeParameterListener("sliceDivision", this);
        m_parameters.removeParameterListener("bpm", this);
        m_parameters.removeParameterListener("swing", this);
        m_parameters.removeParameterListener("clickVolume", this);
        m_parameters.removeParameterListener("loopVolume", this);
        m_parameters.removeParameterListener("recThreshold", this);
    }

    void prepareToPlay(const double sampleRate, const int samplesPerBlock) override
    {
        pluginRunner = std::make_unique<LooperImpl<NumSamplesPerBlock>>(RateNormalizer::kInternalSampleRate);
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
            for (size_t i = 0; i < 10; ++i)
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
        params.push_back(std::make_unique<juce::AudioParameterBool>(juce::ParameterID("record", 1), "Record", 0));
        params.push_back(std::make_unique<juce::AudioParameterBool>(juce::ParameterID("play", 1), "Play", 0));
        params.push_back(std::make_unique<juce::AudioParameterBool>(juce::ParameterID("overdub", 1), "Overdub", 0));
        params.push_back(std::make_unique<juce::AudioParameterBool>(juce::ParameterID("clear", 1), "Clear", 0));
        params.push_back(
            std::make_unique<juce::AudioParameterBool>(juce::ParameterID("threshRec", 1), "Thresh Rec", 0));
        params.push_back(std::make_unique<juce::AudioParameterBool>(juce::ParameterID("hostSync", 1), "Host Sync", 0));
        params.push_back(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID("sliceMode", 1), "Slice Mode",
                                                                      juce::StringArray{"Grid", "Transient"}, 0));
        params.push_back(std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID("sliceDivision", 1), "Division", juce::StringArray{"1/4", "1/8", "1/16", "1/32"}, 1));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("bpm", 1), "BPM", juce::NormalisableRange<float>(50, 250, 0.1, 1, false), 120,
            juce::AudioParameterFloatAttributes{}.withLabel("BPM").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 1) + " BPM"; })));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("swing", 1), "Swing", juce::NormalisableRange<float>(0, 100, 1, 1, false), 50,
            juce::AudioParameterFloatAttributes{}.withLabel("%").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 0) + " %"; })));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("clickVolume", 1), "Click Volume", juce::NormalisableRange<float>(-60, 0, 0.1, 1, false),
            -12,
            juce::AudioParameterFloatAttributes{}.withLabel("dB").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 1) + " dB"; })));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("loopVolume", 1), "Loop Volume", juce::NormalisableRange<float>(-60, 12, 0.1, 1, false),
            0,
            juce::AudioParameterFloatAttributes{}.withLabel("dB").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 1) + " dB"; })));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("recThreshold", 1), "Rec Threshold",
            juce::NormalisableRange<float>(-60, 0, 0.1, 1, false), -36,
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
            {"record",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setRecord(static_cast<bool>(v));
                 p.m_fileIo.updateParameter(PatchParameters::Id::record, v);
             }},
            {"play",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setPlay(static_cast<bool>(v));
                 p.m_fileIo.updateParameter(PatchParameters::Id::play, v);
             }},
            {"overdub",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setOverdub(static_cast<bool>(v));
                 p.m_fileIo.updateParameter(PatchParameters::Id::overdub, v);
             }},
            {"clear",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setClear(static_cast<bool>(v));
                 p.m_fileIo.updateParameter(PatchParameters::Id::clear, v);
             }},
            {"threshRec",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setThreshRec(static_cast<bool>(v));
                 p.m_fileIo.updateParameter(PatchParameters::Id::threshRec, v);
             }},
            {"hostSync",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setHostSync(static_cast<bool>(v));
                 p.m_fileIo.updateParameter(PatchParameters::Id::hostSync, v);
             }},
            {"sliceMode",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setSliceMode(static_cast<int>(v));
                 p.m_fileIo.updateParameter(PatchParameters::Id::sliceMode, v);
             }},
            {"sliceDivision",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setSliceDivision(static_cast<int>(v));
                 p.m_fileIo.updateParameter(PatchParameters::Id::sliceDivision, v);
             }},
            {"bpm",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setBpm(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::bpm, v);
             }},
            {"swing",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setSwing(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::swing, v);
             }},
            {"clickVolume",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setClickVolume(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::clickVolume, v);
             }},
            {"loopVolume",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setLoopVolume(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::loopVolume, v);
             }},
            {"recThreshold",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setRecThreshold(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::recThreshold, v);
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
        if (auto* p = m_parameters.getParameter("record"))
        {
            const auto& range = m_parameters.getParameterRange("record");
            float normalized = range.convertTo0to1(params.record);
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("play"))
        {
            const auto& range = m_parameters.getParameterRange("play");
            float normalized = range.convertTo0to1(params.play);
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("overdub"))
        {
            const auto& range = m_parameters.getParameterRange("overdub");
            float normalized = range.convertTo0to1(params.overdub);
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("clear"))
        {
            const auto& range = m_parameters.getParameterRange("clear");
            float normalized = range.convertTo0to1(params.clear);
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("threshRec"))
        {
            const auto& range = m_parameters.getParameterRange("threshRec");
            float normalized = range.convertTo0to1(params.threshRec);
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("hostSync"))
        {
            const auto& range = m_parameters.getParameterRange("hostSync");
            float normalized = range.convertTo0to1(params.hostSync);
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("sliceMode"))
        {
            const auto& range = m_parameters.getParameterRange("sliceMode");
            float normalized = range.convertTo0to1(params.sliceMode);
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("sliceDivision"))
        {
            const auto& range = m_parameters.getParameterRange("sliceDivision");
            float normalized = range.convertTo0to1(params.sliceDivision);
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("bpm"))
        {
            const auto& range = m_parameters.getParameterRange("bpm");
            float normalized = range.convertTo0to1(params.bpm);
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("swing"))
        {
            const auto& range = m_parameters.getParameterRange("swing");
            float normalized = range.convertTo0to1(params.swing);
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("clickVolume"))
        {
            const auto& range = m_parameters.getParameterRange("clickVolume");
            float normalized = range.convertTo0to1(params.clickVolume);
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("loopVolume"))
        {
            const auto& range = m_parameters.getParameterRange("loopVolume");
            float normalized = range.convertTo0to1(params.loopVolume);
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("recThreshold"))
        {
            const auto& range = m_parameters.getParameterRange("recThreshold");
            float normalized = range.convertTo0to1(params.recThreshold);
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
        if (auto* playHead = getPlayHead())
        {
            if (const auto position = playHead->getPosition())
            {
                auto transport = pluginRunner->hostTransport();
                ++transport.updateCount;
                transport.isPlaying = position->getIsPlaying();
                if (const auto bpm = position->getBpm())
                {
                    transport.bpm = *bpm;
                }
                if (const auto ppq = position->getPpqPosition())
                {
                    transport.ppqPosition = *ppq;
                }
                if (const auto timeSig = position->getTimeSignature())
                {
                    transport.beatsPerBar = static_cast<float>(timeSig->numerator);
                }
                pluginRunner->setHostTransport(transport);
            }
        }
        if ((getTotalNumInputChannels() == 2) && (getTotalNumOutputChannels() == 2))
        {
            fixedRunner->processBlock(buffer);
        }
    }

#pragma GCC diagnostic pop


    [[nodiscard]] const std::vector<float>& getWaveDataToShow()
    {
        return pluginRunner->visualizeWaveData();
    }
    [[nodiscard]] bool isHostSynced() const noexcept
    {
        return pluginRunner && pluginRunner->isHostSynced();
    }
    [[nodiscard]] bool isRecording() const noexcept
    {
        return pluginRunner && pluginRunner->isRecording();
    }
    [[nodiscard]] bool isPlaying() const noexcept
    {
        return pluginRunner && pluginRunner->isPlaying();
    }
    [[nodiscard]] bool isOverdubbing() const noexcept
    {
        return pluginRunner && pluginRunner->isOverdubbing();
    }
    [[nodiscard]] bool isArmed() const noexcept
    {
        return pluginRunner && pluginRunner->isArmed();
    }
    [[nodiscard]] std::vector<float> getLoopWaveform() const
    {
        return pluginRunner ? pluginRunner->getLoopWaveform() : std::vector<float>{};
    }
    [[nodiscard]] std::vector<float> getSliceBoundaries() const
    {
        return pluginRunner ? pluginRunner->getSliceBoundaries() : std::vector<float>{};
    }
    [[nodiscard]] float getPlayheadNormalized() const noexcept
    {
        return pluginRunner ? pluginRunner->getPlayheadNormalized() : 0.f;
    }
    [[nodiscard]] size_t getSamplesPerBar() const noexcept
    {
        return pluginRunner ? pluginRunner->getSamplesPerBar() : 0u;
    }
    [[nodiscard]] int getBarBeats() const noexcept
    {
        return pluginRunner ? pluginRunner->getBarBeats() : 4;
    }
    [[nodiscard]] int getOuterRingBars() const noexcept
    {
        return pluginRunner ? pluginRunner->getOuterRingBars() : 1;
    }
    [[nodiscard]] AbacDsp::SpectrumImageSet getSpectrogramData() const
    {
        return pluginRunner ? pluginRunner->getSpectrogramData() : AbacDsp::SpectrumImageSet{};
    }
    [[nodiscard]] size_t getSpectrogramHeadFrames() const noexcept
    {
        return pluginRunner ? pluginRunner->getSpectrogramHeadFrames() : 0u;
    }
    [[nodiscard]] float getBarPhase() const noexcept
    {
        return pluginRunner ? pluginRunner->getBarPhase() : 0.f;
    }
    [[nodiscard]] const std::vector<size_t>& getSubdivisionPositions() const noexcept
    {
        static const std::vector<size_t> empty{};
        return pluginRunner ? pluginRunner->getSubdivisionPositions() : empty;
    }
    [[nodiscard]] juce::String getLooperStateLabel() const
    {
        return pluginRunner ? juce::String(pluginRunner->getStateLabel()) : juce::String();
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
    std::unique_ptr<LooperImpl<NumSamplesPerBlock>> pluginRunner;
    juce::AudioProcessorValueTreeState m_parameters;
    struct CcSlot
    {
        std::atomic<int> controller{-1};
        std::atomic<float> valueLow{0.f};
        std::atomic<float> valueHigh{0.f};
    };
    std::array<CcSlot, 10> m_ccActive{};
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
        overrides.reserve(10);
        for (size_t i = 0; i < 10; ++i)
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
        for (size_t i = 0; i < 10; ++i)
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
