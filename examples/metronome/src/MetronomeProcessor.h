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
#include "impl/MetronomeImpl.h"

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
        , m_patchIndex(1, 0)
    {
        m_parameters.addParameterListener("subset", this);
        m_parameters.addParameterListener("bpm", this);
        m_parameters.addParameterListener("dropBars", this);
        m_parameters.addParameterListener("metroVolume", this);
        m_parameters.addParameterListener("inputVolume", this);
        m_parameters.addParameterListener("subVolume", this);
        m_parameters.addParameterListener("onOff", this);
        m_parameters.addParameterListener("hostSync", this);
        m_parameters.addParameterListener("preset", this);
        m_parameters.addParameterListener("swingRatio", this);

        for (size_t i = 0; i < 5; ++i)
        {
            m_ccActive[i].controller.store(kDefaultCcMappings[i].controller, std::memory_order_relaxed);
            m_ccActive[i].valueLow.store(kDefaultCcMappings[i].valueLow, std::memory_order_relaxed);
            m_ccActive[i].valueHigh.store(kDefaultCcMappings[i].valueHigh, std::memory_order_relaxed);
        }
        m_fileIo.initialize(m_patchIndex);
    }
    ~AudioPluginAudioProcessor() override
    {
        m_parameters.removeParameterListener("subset", this);
        m_parameters.removeParameterListener("bpm", this);
        m_parameters.removeParameterListener("dropBars", this);
        m_parameters.removeParameterListener("metroVolume", this);
        m_parameters.removeParameterListener("inputVolume", this);
        m_parameters.removeParameterListener("subVolume", this);
        m_parameters.removeParameterListener("onOff", this);
        m_parameters.removeParameterListener("hostSync", this);
        m_parameters.removeParameterListener("preset", this);
        m_parameters.removeParameterListener("swingRatio", this);
    }

    void prepareToPlay(const double sampleRate, const int samplesPerBlock) override
    {
        pluginRunner = std::make_unique<MetronomeImpl<NumSamplesPerBlock>>(RateNormalizer::kInternalSampleRate);
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
            for (size_t i = 0; i < 5; ++i)
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
        params.push_back(std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID("subset", 1), "Subset", juce::StringArray{"#1", "#2", "#3", "#4", "#5"}, 0));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("bpm", 1), "BPM", juce::NormalisableRange<float>(40, 250, 0.1, 1, false), 120,
            juce::AudioParameterFloatAttributes{}.withLabel("BPM").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 1) + " BPM"; })));
        params.push_back(std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID("dropBars", 1), "Drop Bars",
            juce::StringArray{"Drop none", "Play 1 Drop 1", "Play 3 Drop 1", "Play 2 Drop 2", "Play 1 Drop 3"}, 0));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("metroVolume", 1), "Metro Volume", juce::NormalisableRange<float>(-60, 0, 0.1, 1, false),
            -6,
            juce::AudioParameterFloatAttributes{}.withLabel("dB").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 1) + " dB"; })));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("inputVolume", 1), "Input Volume", juce::NormalisableRange<float>(-60, 12, 0.1, 1, false),
            0,
            juce::AudioParameterFloatAttributes{}.withLabel("dB").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 1) + " dB"; })));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("subVolume", 1), "Sub Volume", juce::NormalisableRange<float>(-60, 0, 0.1, 1, false), -15,
            juce::AudioParameterFloatAttributes{}.withLabel("dB").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 1) + " dB"; })));
        params.push_back(std::make_unique<juce::AudioParameterBool>(juce::ParameterID("onOff", 1), "Start", 0));
        params.push_back(std::make_unique<juce::AudioParameterBool>(juce::ParameterID("hostSync", 1), "Host Sync", 0));
        params.push_back(std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID("preset", 1), "Preset",
            juce::StringArray{"3/4",           "3/4 8th",   "3/4 16th",       "3/4 shuffle",    "3/4 triplet",
                              "4/4",           "4/4 8th",   "4/4 16th",       "4/4 shuffle",    "4/4 triplet",
                              "4/4 swing",     "5/4 (3+2)", "5/4 8th (3+2)",  "5/4 (2+3)",      "5/4 8th (2+3)",
                              "6/8 in-2",      "6/8 in-6",  "7/8 (2+2+3)",    "7/8 (2+3+2)",    "7/8 (3+2+2)",
                              "9/8 in-3",      "9/8 in-9",  "11/8 (3+3+3+2)", "11/8 (3+3+2+3)", "13/8 (3+3+3+2+2)",
                              "13/8 (3+4+3+3)"},
            6));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("swingRatio", 1), "Swing", juce::NormalisableRange<float>(1.0, 2.0, 0.01, 1, false), 1.5,
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


        if (parameterID == "subset")
        {
            bool patchIndexChanged = false;
            if (parameterID == "subset")
            {
                const int newIdx = static_cast<int>(newValue);
                if (m_patchIndex[0] != newIdx)
                {
                    m_patchIndex[0] = newIdx;
                    patchIndexChanged = true;
                }
            }

            if (patchIndexChanged)
            {
                if (m_fileIo.areParametersModified())
                {
                    juce::NativeMessageBox::showAsync(
                        juce::MessageBoxOptions()
                            .withIconType(juce::MessageBoxIconType::QuestionIcon)
                            .withTitle("Save Parameters")
                            .withMessage("Parameters have changed, do you want to save before loading new patch?")
                            .withButton("Yes")
                            .withButton("No"),
                        [this, patchIndex = m_patchIndex](int result)
                        {
                            // showAsync returns the plain index of the clicked button (0 = "Yes", 1 = "No").
                            handlePatchChange(patchIndex, result == 0);
                        });
                }
                else
                {
                    loadPatchDirect(m_patchIndex);
                }
            }
        }

        static const std::map<juce::String, std::function<void(AudioPluginAudioProcessor&, float)>> parameterMap{
            {"bpm",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setBpm(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::bpm, v);
             }},
            {"dropBars",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setDropBars(static_cast<size_t>(v));
                 p.m_fileIo.updateParameter(PatchParameters::Id::dropBars, v);
             }},
            {"metroVolume",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setMetroVolume(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::metroVolume, v);
             }},
            {"inputVolume",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setInputVolume(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::inputVolume, v);
             }},
            {"subVolume",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setSubVolume(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::subVolume, v);
             }},
            {"onOff",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setOnOff(static_cast<bool>(v));
                 p.m_fileIo.updateParameter(PatchParameters::Id::onOff, v);
             }},
            {"hostSync",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setHostSync(static_cast<bool>(v));
                 p.m_fileIo.updateParameter(PatchParameters::Id::hostSync, v);
             }},
            {"preset",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setPreset(static_cast<int>(v));
                 p.m_fileIo.updateParameter(PatchParameters::Id::preset, v);
             }},
            {"swingRatio",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setSwingRatio(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::swingRatio, v);
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
            float normalized = range.convertTo0to1(params.bpm);
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("dropBars"))
        {
            const auto& range = m_parameters.getParameterRange("dropBars");
            float normalized = range.convertTo0to1(params.dropBars);
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("metroVolume"))
        {
            const auto& range = m_parameters.getParameterRange("metroVolume");
            float normalized = range.convertTo0to1(params.metroVolume);
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("inputVolume"))
        {
            const auto& range = m_parameters.getParameterRange("inputVolume");
            float normalized = range.convertTo0to1(params.inputVolume);
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("subVolume"))
        {
            const auto& range = m_parameters.getParameterRange("subVolume");
            float normalized = range.convertTo0to1(params.subVolume);
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("onOff"))
        {
            const auto& range = m_parameters.getParameterRange("onOff");
            float normalized = range.convertTo0to1(params.onOff);
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("hostSync"))
        {
            const auto& range = m_parameters.getParameterRange("hostSync");
            float normalized = range.convertTo0to1(params.hostSync);
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("preset"))
        {
            const auto& range = m_parameters.getParameterRange("preset");
            float normalized = range.convertTo0to1(params.preset);
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("swingRatio"))
        {
            const auto& range = m_parameters.getParameterRange("swingRatio");
            float normalized = range.convertTo0to1(params.swingRatio);
            p->setValueNotifyingHost(normalized);
        }
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
    [[nodiscard]] bool presetHasSwing(int idx) const noexcept
    {
        return pluginRunner && pluginRunner->isPresetSwing(idx);
    }
    [[nodiscard]] float getCurrentClickBpm() const noexcept
    {
        return pluginRunner ? pluginRunner->currentClickBpm() : 120.f;
    }
    [[nodiscard]] bool isHostSynced() const noexcept
    {
        return pluginRunner && pluginRunner->isHostSynced();
    }
    [[nodiscard]] size_t getWaveDataBeatIndex() const noexcept
    {
        return pluginRunner ? pluginRunner->getBeatIndex() : 0u;
    }
    [[nodiscard]] const std::vector<size_t>& getSubdivisionPositions() const noexcept
    {
        static const std::vector<size_t> empty{};
        return pluginRunner ? pluginRunner->getSubdivisionPositions() : empty;
    }
    [[nodiscard]] int getBarBeats() const noexcept
    {
        return pluginRunner ? pluginRunner->getBarBeats() : 4;
    }
    [[nodiscard]] float getBarPhase() const noexcept
    {
        return pluginRunner ? pluginRunner->getBarPhase() : 0.f;
    }
    [[nodiscard]] AbacDsp::SpectrumImageSet getInputSpectrogram() const
    {
        return pluginRunner ? pluginRunner->getSpectrogramData() : AbacDsp::SpectrumImageSet{};
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
    std::unique_ptr<MetronomeImpl<NumSamplesPerBlock>> pluginRunner;
    juce::AudioProcessorValueTreeState m_parameters;
    struct CcSlot
    {
        std::atomic<int> controller{-1};
        std::atomic<float> valueLow{0.f};
        std::atomic<float> valueHigh{0.f};
    };
    std::array<CcSlot, 5> m_ccActive{};
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
        overrides.reserve(5);
        for (size_t i = 0; i < 5; ++i)
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
        for (size_t i = 0; i < 5; ++i)
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
