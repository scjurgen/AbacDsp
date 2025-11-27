#pragma once
/*
 * AUTO GENERATED,
 * NOT A GOOD IDEA TO CHANGE STUFF HERE
 * Keep the file readonly
 */

#include "Analysis/EnvelopeFollower.h"
#include "Analysis/Spectrogram.h"

#include "Audio/FixedSizeProcessor.h"

#include "UiElements.h"

#include "impl/JuliaSynth.h"
#include "impl/FileIo.h"

#include <juce_audio_processors/juce_audio_processors.h>

class AudioPluginAudioProcessor : public juce::AudioProcessor, public juce::AudioProcessorValueTreeState::Listener
{
  public:
    static constexpr size_t NumSamplesPerBlock = 16;
    AudioPluginAudioProcessor()
        : AudioProcessor(BusesProperties()
#if !JucePlugin_IsMidiEffect
#if !JucePlugin_IsSynth
                             .withInput("Input", juce::AudioChannelSet::stereo(), true)
#endif
                             .withOutput("Output", juce::AudioChannelSet::stereo(), true)
#endif
                             )
        , fixedRunner([this](const AbacDsp::AudioBuffer<2, NumSamplesPerBlock>& input,
                             AbacDsp::AudioBuffer<2, NumSamplesPerBlock>& output)
                      { pluginRunner->processBlock(input, output); })
        , m_parameters(*this, nullptr, "PARAMETERS", createParameterLayout())
        , m_avgCpu(8, 0)
        , m_head{0}
        , m_runningWindowCpu(8 * 300)
        , m_envInput{AbacDsp::RmsFollower(10000), AbacDsp::RmsFollower(10000)}
        , m_envOutput{AbacDsp::RmsFollower(10000), AbacDsp::RmsFollower(10000)}
        , m_spectrogram{}
        , m_patchIndex(0, 0)
    {
        m_parameters.addParameterListener("vol", this);
        m_parameters.addParameterListener("power", this);
        m_parameters.addParameterListener("cx", this);
        m_parameters.addParameterListener("cy", this);
        m_parameters.addParameterListener("jx", this);
        m_parameters.addParameterListener("jy", this);
        m_parameters.addParameterListener("jrx", this);
        m_parameters.addParameterListener("jry", this);

        m_fileIo.initialize(m_patchIndex);
    }
    ~AudioPluginAudioProcessor() override = default;

    void prepareToPlay(const double sampleRate, const int samplesPerBlock) override
    {
        pluginRunner = std::make_unique<JuliaSynth<NumSamplesPerBlock>>(static_cast<float>(sampleRate));
        m_sampleRate = static_cast<size_t>(sampleRate);
        for (auto* param : getParameters())
        {
            if (auto* p = dynamic_cast<juce::RangedAudioParameter*>(param))
            {
                const auto normalizedValue = p->getValue();
                p->sendValueChangedMessageToListeners(normalizedValue);
            }
        }
        if (m_newState.isValid())
        {
            m_parameters.replaceState(m_newState);
        }

        juce::ignoreUnused(samplesPerBlock);
        m_fileIo.enable();
    }

    void releaseResources() override
    {
        std::cout << "releaseResources: Called on shutdown" << std::endl;

        if (m_fileIo.areParametersModified())
        {
            std::cout << "releaseResources: Parameters modified, prompting for save" << std::endl;

            int result = juce::NativeMessageBox::showYesNoBox(
                juce::MessageBoxIconType::QuestionIcon, "Save Parameters",
                "Parameters have changed. Do you want to save before exiting?", nullptr, nullptr);
            if (result == 1)
            {
                std::cout << "Saving data\n";
                m_fileIo.forceSave();
            }
        }

        pluginRunner = nullptr;
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
                m_newState = juce::ValueTree::fromXml(*xmlState);
            }
        }
    }

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wimplicit-float-conversion"
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
    {
        std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("vol", 1), "Vol", juce::NormalisableRange<float>(-100, 12, 0.1, 1, false), 0,
            juce::String("Vol"), juce::AudioProcessorParameter::genericParameter,
            [](float value, float) { return juce::String(value, 1) + " dB"; }));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("power", 1), "Power", juce::NormalisableRange<float>(1, 10, 0.1, 1, false), 2,
            juce::String("Power"), juce::AudioProcessorParameter::genericParameter,
            [](float value, float) { return juce::String(value, 1) + " "; }));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("cx", 1), "CX", juce::NormalisableRange<float>(-2, 2, 1e-05, 1, false), 0,
            juce::String("CX"), juce::AudioProcessorParameter::genericParameter,
            [](float value, float) { return juce::String(value, 5) + " "; }));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("cy", 1), "CY", juce::NormalisableRange<float>(-2, 2, 1e-05, 1, false), 0,
            juce::String("CY"), juce::AudioProcessorParameter::genericParameter,
            [](float value, float) { return juce::String(value, 5) + " "; }));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("jx", 1), "JX", juce::NormalisableRange<float>(-2, 2, 1e-05, 1, false), 0,
            juce::String("JX"), juce::AudioProcessorParameter::genericParameter,
            [](float value, float) { return juce::String(value, 5) + " "; }));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("jy", 1), "JY", juce::NormalisableRange<float>(-2, 2, 1e-05, 1, false), 0,
            juce::String("JY"), juce::AudioProcessorParameter::genericParameter,
            [](float value, float) { return juce::String(value, 5) + " "; }));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("jrx", 1), "JR-X", juce::NormalisableRange<float>(0, 2, 0.001, 1, false), 1,
            juce::String("JR-X"), juce::AudioProcessorParameter::genericParameter,
            [](float value, float) { return juce::String(value, 3) + " "; }));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("jry", 1), "JR-Y", juce::NormalisableRange<float>(0, 2, 0.001, 1, false), 1,
            juce::String("JR-Y"), juce::AudioProcessorParameter::genericParameter,
            [](float value, float) { return juce::String(value, 3) + " "; }));

        return {params.begin(), params.end()};
    }
#pragma GCC diagnostic pop

    void parameterChanged(const juce::String& parameterID, float newValue) override
    {
        if (pluginRunner == nullptr)
        {
            return;
        }


        // if ()
        // {
        //     if (m_fileIo.areParametersModified())
        //     {
        //         juce::NativeMessageBox::showAsync(
        //             juce::MessageBoxOptions()
        //                 .withTitle("Save Parameters")
        //                 .withMessage("Parameters have changed, do you want to save before loading new patch?")
        //                 .withButton("Yes")
        //                 .withButton("No")
        //                 .withIconType(juce::MessageBoxIconType::QuestionIcon),
        //             [this, pi = m_patchIndex](int result) { handlePatchChangeAsync(pi, result == 0); });
        //     }
        //     else
        //     {
        //         loadPatchDirect(m_patchIndex);
        //     }
        // }
        // else
        {
            m_fileIo.updateParameter(parameterID.toStdString(), newValue);
        }

        static const std::map<juce::String, std::function<void(AudioPluginAudioProcessor&, float)>> parameterMap{
            {"vol", [](const AudioPluginAudioProcessor& p, const float v) { p.pluginRunner->setVol(v); }},
            {"power", [](const AudioPluginAudioProcessor& p, const float v) { p.pluginRunner->setPower(v); }},
            {"cx", [](const AudioPluginAudioProcessor& p, const float v) { p.pluginRunner->setCx(v); }},
            {"cy", [](const AudioPluginAudioProcessor& p, const float v) { p.pluginRunner->setCy(v); }},
            {"jx", [](const AudioPluginAudioProcessor& p, const float v) { p.pluginRunner->setJx(v); }},
            {"jy", [](const AudioPluginAudioProcessor& p, const float v) { p.pluginRunner->setJy(v); }},
            {"jrx", [](const AudioPluginAudioProcessor& p, const float v) { p.pluginRunner->setJrx(v); }},
            {"jry", [](const AudioPluginAudioProcessor& p, const float v) { p.pluginRunner->setJry(v); }},

        };
        if (auto it = parameterMap.find(parameterID); it != parameterMap.end())
        {
            it->second(*this, newValue);
        }
    }

    // Helper to handle patch change after dialog response
    void handlePatchChangeAsync(const std::vector<int>& newPatchIndex, bool shouldSave)
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
        m_fileIo.loadPatchDirect(patchIndex);
        const auto& params = m_fileIo.getCurrentParameters();

        // Apply loaded parameters to APVTS (triggers UI update)
        if (auto* p = m_parameters.getParameter("vol"))
        {
            const auto& range = m_parameters.getParameterRange("vol");
            float normalized = range.convertTo0to1(params.vol);
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("power"))
        {
            const auto& range = m_parameters.getParameterRange("power");
            float normalized = range.convertTo0to1(params.power);
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("cx"))
        {
            const auto& range = m_parameters.getParameterRange("cx");
            float normalized = range.convertTo0to1(params.cx);
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("cy"))
        {
            const auto& range = m_parameters.getParameterRange("cy");
            float normalized = range.convertTo0to1(params.cy);
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("jx"))
        {
            const auto& range = m_parameters.getParameterRange("jx");
            float normalized = range.convertTo0to1(params.jx);
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("jy"))
        {
            const auto& range = m_parameters.getParameterRange("jy");
            float normalized = range.convertTo0to1(params.jy);
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("jrx"))
        {
            const auto& range = m_parameters.getParameterRange("jrx");
            float normalized = range.convertTo0to1(params.jrx);
            p->setValueNotifyingHost(normalized);
        }
        if (auto* p = m_parameters.getParameter("jry"))
        {
            const auto& range = m_parameters.getParameterRange("jry");
            float normalized = range.convertTo0to1(params.jry);
            p->setValueNotifyingHost(normalized);
        }
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
            m_envInput[c].feed(buffer.getReadPointer(c), buffer.getNumSamples());
            m_inputDb[c].store(std::log10(m_envInput[c].getRms()) * 20.f);
        }
        if ((getTotalNumInputChannels() == 2) && (getTotalNumOutputChannels() == 2))
        {
            fixedRunner.processBlock(buffer);
        }
        for (int c = 0; c < std::min(2, buffer.getNumChannels()); ++c)
        {
            m_envOutput[c].feed(buffer.getReadPointer(c), buffer.getNumSamples());
            m_outputDb[c].store(std::log10(m_envOutput[c].getRms()) * 20.f);
        }
        m_spectrogram.processBlock(buffer.getWritePointer(0), buffer.getNumSamples());
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

    [[nodiscard]] const std::vector<float>& getWaveDataToShow()
    {
        return pluginRunner->visualizeWaveData();
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
    juce::ValueTree m_newState;

    AbacDsp::FixedSizeProcessor<2, NumSamplesPerBlock, juce::AudioBuffer<float>> fixedRunner;
    std::unique_ptr<JuliaSynth<NumSamplesPerBlock>> pluginRunner;
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
