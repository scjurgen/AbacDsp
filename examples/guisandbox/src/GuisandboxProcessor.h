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

#include "impl/GuiSandBox.h"

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
    {
        m_parameters.addParameterListener("onOff", this);
        m_parameters.addParameterListener("input", this);
        m_parameters.addParameterListener("modulationDepth", this);
        m_parameters.addParameterListener("mix", this);
        m_parameters.addParameterListener("density", this);
        m_parameters.addParameterListener("threshold", this);
        m_parameters.addParameterListener("knee", this);
        m_parameters.addParameterListener("dropIt", this);
    }
    ~AudioPluginAudioProcessor() override = default;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override
    {
        pluginRunner = std::make_unique<GuiSandBox<NumSamplesPerBlock>>(static_cast<float>(sampleRate));
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
    }

    void releaseResources() override
    {
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
            case 1:
                return {"Program 1"};
            case 2:
                return {"Program 2"};
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
        params.push_back(std::make_unique<juce::AudioParameterBool>(juce::ParameterID("onOff", 1), "✨", 0));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("input", 1), "Input", juce::NormalisableRange<float>(0, 50, 0.1, 1, false), 2,
            juce::String("Input"), juce::AudioProcessorParameter::genericParameter,
            [](float value, float) { return juce::String(value, 2) + " dB"; }));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("modulationDepth", 1), "Depth", juce::NormalisableRange<float>(1, 50, 0.01, 0.8, false),
            2, juce::String("Depth"), juce::AudioProcessorParameter::genericParameter,
            [](float value, float) { return juce::String(value, 2) + " ms"; }));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("mix", 1), "Mix", juce::NormalisableRange<float>(-100, 100, 1, 1, false), 0.0,
            juce::String("Mix"), juce::AudioProcessorParameter::genericParameter,
            [](float value, float) { return juce::String(value, 1) + " "; }));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("density", 1), "Density", juce::NormalisableRange<float>(1, 12, 0.01, 1, false), 1,
            juce::String("Density"), juce::AudioProcessorParameter::genericParameter,
            [](float value, float) { return juce::String(value, 1) + " "; }));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("threshold", 1), "Threshold", juce::NormalisableRange<float>(1, 12, 0, 1, false), 1,
            juce::String("Threshold"), juce::AudioProcessorParameter::genericParameter,
            [](float value, float) { return juce::String(value, 4) + " "; }));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("knee", 1), "Knee", juce::NormalisableRange<float>(1, 12, 0.01, 1, false), 1,
            juce::String("Knee"), juce::AudioProcessorParameter::genericParameter,
            [](float value, float) { return juce::String(value, 2) + " dB"; }));
        params.push_back(std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID("dropIt", 1), "Dropit", juce::StringArray{"item 1", "item 2", "item 3"}, 1));

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
            {"onOff",
             [](const AudioPluginAudioProcessor& p, const float v) { p.pluginRunner->setOnOff(static_cast<bool>(v)); }},
            {"input", [](const AudioPluginAudioProcessor& p, const float v) { p.pluginRunner->setInput(v); }},
            {"modulationDepth",
             [](const AudioPluginAudioProcessor& p, const float v) { p.pluginRunner->setModulationDepth(v); }},
            {"mix", [](const AudioPluginAudioProcessor& p, const float v) { p.pluginRunner->setMix(v); }},
            {"density", [](const AudioPluginAudioProcessor& p, const float v) { p.pluginRunner->setDensity(v); }},
            {"threshold", [](const AudioPluginAudioProcessor& p, const float v) { p.pluginRunner->setThreshold(v); }},
            {"knee", [](const AudioPluginAudioProcessor& p, const float v) { p.pluginRunner->setKnee(v); }},
            {"dropIt", [](const AudioPluginAudioProcessor& p, const float v)
             { p.pluginRunner->setDropIt(static_cast<size_t>(v)); }},

        };
        if (auto it = parameterMap.find(parameterID); it != parameterMap.end())
        {
            it->second(*this, newValue);
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
    std::unique_ptr<GuiSandBox<NumSamplesPerBlock>> pluginRunner;
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
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioPluginAudioProcessor)
};
