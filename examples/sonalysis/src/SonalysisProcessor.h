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
#include "impl/SonalysisImpl.h"

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
        m_parameters.addParameterListener("peakFall", this);
        m_parameters.addParameterListener("rmsWindow", this);
        m_parameters.addParameterListener("peakHold", this);
        m_parameters.addParameterListener("holdReset", this);
        m_parameters.addParameterListener("fftSize", this);
        m_parameters.addParameterListener("fftFall", this);
        m_parameters.addParameterListener("spectrumPeakHold", this);
        m_parameters.addParameterListener("referencePitch", this);
        m_parameters.addParameterListener("pitchSource", this);
        m_parameters.addParameterListener("beatReset", this);
        m_parameters.addParameterListener("thru", this);

        m_fileIo.initialize(m_patchIndex);
    }
    ~AudioPluginAudioProcessor() override
    {
        m_parameters.removeParameterListener("peakFall", this);
        m_parameters.removeParameterListener("rmsWindow", this);
        m_parameters.removeParameterListener("peakHold", this);
        m_parameters.removeParameterListener("holdReset", this);
        m_parameters.removeParameterListener("fftSize", this);
        m_parameters.removeParameterListener("fftFall", this);
        m_parameters.removeParameterListener("spectrumPeakHold", this);
        m_parameters.removeParameterListener("referencePitch", this);
        m_parameters.removeParameterListener("pitchSource", this);
        m_parameters.removeParameterListener("beatReset", this);
        m_parameters.removeParameterListener("thru", this);
    }

    void prepareToPlay(const double sampleRate, const int samplesPerBlock) override
    {
        pluginRunner = std::make_unique<SonalysisImpl<NumSamplesPerBlock>>(RateNormalizer::kInternalSampleRate);

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
            juce::ParameterID("peakFall", 1), juce::String::fromUTF8("Peak Fall"),
            juce::StringArray{juce::String::fromUTF8("6 dB/s"), juce::String::fromUTF8("11.8 dB/s (PPM)"),
                              juce::String::fromUTF8("24 dB/s"), juce::String::fromUTF8("48 dB/s")},
            1));
        params.push_back(std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID("rmsWindow", 1), juce::String::fromUTF8("RMS Window"),
            juce::StringArray{juce::String::fromUTF8("50 ms"), juce::String::fromUTF8("300 ms (VU)"),
                              juce::String::fromUTF8("1 s")},
            1));
        params.push_back(std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID("peakHold", 1), juce::String::fromUTF8("Peak Hold"),
            juce::StringArray{juce::String::fromUTF8("Off"), juce::String::fromUTF8("1 s"),
                              juce::String::fromUTF8("3 s"), juce::String::fromUTF8("Infinite")},
            2));
        params.push_back(std::make_unique<juce::AudioParameterBool>(juce::ParameterID("holdReset", 1),
                                                                    juce::String::fromUTF8("Reset Hold"), 0));
        params.push_back(std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID("fftSize", 1), juce::String::fromUTF8("FFT Size"),
            juce::StringArray{juce::String::fromUTF8("2048"), juce::String::fromUTF8("4096"),
                              juce::String::fromUTF8("8192"), juce::String::fromUTF8("16384")},
            2));
        params.push_back(std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID("fftFall", 1), juce::String::fromUTF8("FFT Fall"),
            juce::StringArray{juce::String::fromUTF8("Instant"), juce::String::fromUTF8("12 dB/s"),
                              juce::String::fromUTF8("24 dB/s"), juce::String::fromUTF8("48 dB/s")},
            2));
        params.push_back(std::make_unique<juce::AudioParameterBool>(juce::ParameterID("spectrumPeakHold", 1),
                                                                    juce::String::fromUTF8("Peak Curve"), 0));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("referencePitch", 1), juce::String::fromUTF8("A4"),
            juce::NormalisableRange<float>(415, 466, 0.1, 1, false), 440,
            juce::AudioParameterFloatAttributes{}.withLabel("Hz").withStringFromValueFunction(
                [](float value, int) { return juce::String(value, 1) + " Hz"; })));
        params.push_back(std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID("pitchSource", 1), juce::String::fromUTF8("Pitch Source"),
            juce::StringArray{juce::String::fromUTF8("L+R"), juce::String::fromUTF8("L"), juce::String::fromUTF8("R")},
            0));
        params.push_back(std::make_unique<juce::AudioParameterBool>(juce::ParameterID("beatReset", 1),
                                                                    juce::String::fromUTF8("Reset Beat"), 0));
        params.push_back(std::make_unique<juce::AudioParameterBool>(juce::ParameterID("thru", 1),
                                                                    juce::String::fromUTF8("Thru"), 0));

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
            {"peakFall",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setPeakFall(static_cast<size_t>(v));
                 p.m_fileIo.updateParameter(PatchParameters::Id::peakFall, v);
             }},
            {"rmsWindow",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setRmsWindow(static_cast<size_t>(v));
                 p.m_fileIo.updateParameter(PatchParameters::Id::rmsWindow, v);
             }},
            {"peakHold",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setPeakHold(static_cast<size_t>(v));
                 p.m_fileIo.updateParameter(PatchParameters::Id::peakHold, v);
             }},
            {"holdReset",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setHoldReset(static_cast<bool>(v));
                 p.m_fileIo.updateParameter(PatchParameters::Id::holdReset, v);
             }},
            {"fftSize",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setFftSize(static_cast<size_t>(v));
                 p.m_fileIo.updateParameter(PatchParameters::Id::fftSize, v);
             }},
            {"fftFall",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setFftFall(static_cast<size_t>(v));
                 p.m_fileIo.updateParameter(PatchParameters::Id::fftFall, v);
             }},
            {"spectrumPeakHold",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setSpectrumPeakHold(static_cast<bool>(v));
                 p.m_fileIo.updateParameter(PatchParameters::Id::spectrumPeakHold, v);
             }},
            {"referencePitch",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setReferencePitch(v);
                 p.m_fileIo.updateParameter(PatchParameters::Id::referencePitch, v);
             }},
            {"pitchSource",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setPitchSource(static_cast<size_t>(v));
                 p.m_fileIo.updateParameter(PatchParameters::Id::pitchSource, v);
             }},
            {"beatReset",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setBeatReset(static_cast<bool>(v));
                 p.m_fileIo.updateParameter(PatchParameters::Id::beatReset, v);
             }},
            {"thru",
             [](AudioPluginAudioProcessor& p, const float v)
             {
                 p.pluginRunner->setThru(static_cast<bool>(v));
                 p.m_fileIo.updateParameter(PatchParameters::Id::thru, v);
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
        if (auto* p = m_parameters.getParameter("peakFall"))
        {
            const auto& range = m_parameters.getParameterRange("peakFall");
            float normalized = range.convertTo0to1(static_cast<float>(params.peakFall));
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("rmsWindow"))
        {
            const auto& range = m_parameters.getParameterRange("rmsWindow");
            float normalized = range.convertTo0to1(static_cast<float>(params.rmsWindow));
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("peakHold"))
        {
            const auto& range = m_parameters.getParameterRange("peakHold");
            float normalized = range.convertTo0to1(static_cast<float>(params.peakHold));
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("holdReset"))
        {
            const auto& range = m_parameters.getParameterRange("holdReset");
            float normalized = range.convertTo0to1(static_cast<float>(params.holdReset));
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("fftSize"))
        {
            const auto& range = m_parameters.getParameterRange("fftSize");
            float normalized = range.convertTo0to1(static_cast<float>(params.fftSize));
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("fftFall"))
        {
            const auto& range = m_parameters.getParameterRange("fftFall");
            float normalized = range.convertTo0to1(static_cast<float>(params.fftFall));
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("spectrumPeakHold"))
        {
            const auto& range = m_parameters.getParameterRange("spectrumPeakHold");
            float normalized = range.convertTo0to1(static_cast<float>(params.spectrumPeakHold));
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("referencePitch"))
        {
            const auto& range = m_parameters.getParameterRange("referencePitch");
            float normalized = range.convertTo0to1(static_cast<float>(params.referencePitch));
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("pitchSource"))
        {
            const auto& range = m_parameters.getParameterRange("pitchSource");
            float normalized = range.convertTo0to1(static_cast<float>(params.pitchSource));
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("beatReset"))
        {
            const auto& range = m_parameters.getParameterRange("beatReset");
            float normalized = range.convertTo0to1(static_cast<float>(params.beatReset));
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("thru"))
        {
            const auto& range = m_parameters.getParameterRange("thru");
            float normalized = range.convertTo0to1(static_cast<float>(params.thru));
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
            }
        }
        if (getTotalNumOutputChannels() == 2)
        {
            fixedRunner->processBlock(buffer);
        }
    }

#pragma GCC diagnostic pop


    [[nodiscard]] Sonalysis::LevelSnapshot getLevelSnapshot() const
    {
        return pluginRunner ? pluginRunner->levelSnapshot() : Sonalysis::LevelSnapshot{};
    }
    [[nodiscard]] Sonalysis::SpectrumSnapshot getSpectrumSnapshot() const
    {
        return pluginRunner ? pluginRunner->spectrumSnapshot() : Sonalysis::SpectrumSnapshot{};
    }
    [[nodiscard]] Sonalysis::PitchSnapshot getPitchSnapshot() const
    {
        return pluginRunner ? pluginRunner->pitchSnapshot() : Sonalysis::PitchSnapshot{};
    }
    [[nodiscard]] Sonalysis::XySnapshot getXySnapshot() const
    {
        return pluginRunner ? pluginRunner->xySnapshot() : Sonalysis::XySnapshot{};
    }
    [[nodiscard]] Sonalysis::BeatSnapshot getBeatSnapshot() const
    {
        return pluginRunner ? pluginRunner->beatSnapshot() : Sonalysis::BeatSnapshot{};
    }
    [[nodiscard]] uint64_t getDroppedFrames() const
    {
        return pluginRunner ? pluginRunner->droppedFrames() : uint64_t{};
    }
    void setPitchTrackingEnabled(const bool enabled) noexcept
    {
        if (pluginRunner)
        {
            pluginRunner->setPitchTrackingEnabled(enabled);
        }
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
    std::unique_ptr<SonalysisImpl<NumSamplesPerBlock>> pluginRunner;

    juce::AudioProcessorValueTreeState m_parameters;
    std::vector<int> m_patchIndex;
    FileIo m_fileIo;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioPluginAudioProcessor)
};
