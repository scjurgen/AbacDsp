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

#include "impl/MiniReverbImpl.h"

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
        m_parameters.addParameterListener("dry", this);
        m_parameters.addParameterListener("wet", this);
        m_parameters.addParameterListener("baseSize", this);
        m_parameters.addParameterListener("sizeFactor", this);
        m_parameters.addParameterListener("bulge", this);
        m_parameters.addParameterListener("decay", this);
        m_parameters.addParameterListener("lowPass", this);
        m_parameters.addParameterListener("highPass", this);
        m_parameters.addParameterListener("modulationDepth", this);
        m_parameters.addParameterListener("modulationSpeed", this);
        m_parameters.addParameterListener("pitchStrength", this);
        m_parameters.addParameterListener("pitch1Inplace", this);
        m_parameters.addParameterListener("pitch2Inplace", this);
    }
    ~AudioPluginAudioProcessor() override = default;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override
    {
        pluginRunner = std::make_unique<MiniReverbImpl<NumSamplesPerBlock>>(static_cast<float>(sampleRate));
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
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("dry", 1), "Dry", juce::NormalisableRange<float>(-100, 12, 0.1, 1, false), 0,
            juce::String("Dry"), juce::AudioProcessorParameter::genericParameter,
            [](float value, float) { return juce::String(value, 1) + " dB"; }));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("wet", 1), "Wet", juce::NormalisableRange<float>(-100, 12, 0.1, 1, false), 0,
            juce::String("Wet"), juce::AudioProcessorParameter::genericParameter,
            [](float value, float) { return juce::String(value, 1) + " dB"; }));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("baseSize", 1), "Base size", juce::NormalisableRange<float>(1.0, 600, 0.01, 0.5, false),
            3.5, juce::String("Base size"), juce::AudioProcessorParameter::genericParameter,
            [](float value, float) { return juce::String(value, 2) + " m"; }));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("sizeFactor", 1), "Size Factor",
            juce::NormalisableRange<float>(1.0, 20, 0.01, 1.0, false), 10.5, juce::String("Size Factor"),
            juce::AudioProcessorParameter::genericParameter,
            [](float value, float) { return juce::String(value, 2) + " x"; }));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("bulge", 1), "Bulge", juce::NormalisableRange<float>(-1, 1, 0.01, 1, false), -0.3,
            juce::String("Bulge"), juce::AudioProcessorParameter::genericParameter,
            [](float value, float) { return juce::String(value, 2) + " "; }));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("decay", 1), "Decay", juce::NormalisableRange<float>(0, 100000, 0.1, 0.25, false), 2000,
            juce::String("Decay"), juce::AudioProcessorParameter::genericParameter,
            [](float value, float) { return juce::String(value, 1) + " ms"; }));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("lowPass", 1), "Low pass", juce::NormalisableRange<float>(100, 22000, 1, 0.5, false),
            12000, juce::String("Low pass"), juce::AudioProcessorParameter::genericParameter,
            [](float value, float) { return juce::String(value, 0) + " Hz"; }));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("highPass", 1), "High pass", juce::NormalisableRange<float>(20, 22000, 1, 0.5, false), 50,
            juce::String("High pass"), juce::AudioProcessorParameter::genericParameter,
            [](float value, float) { return juce::String(value, 0) + " Hz"; }));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("modulationDepth", 1), "Mod depth", juce::NormalisableRange<float>(0, 1, 0.01, 1, false),
            0.02, juce::String("Mod depth"), juce::AudioProcessorParameter::genericParameter,
            [](float value, float) { return juce::String(value, 2) + " "; }));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("modulationSpeed", 1), "Mod speed",
            juce::NormalisableRange<float>(0.01, 5, 0.01, 0.5, false), 0.25, juce::String("Mod speed"),
            juce::AudioProcessorParameter::genericParameter,
            [](float value, float) { return juce::String(value, 2) + " Hz"; }));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("pitchStrength", 1), "Pitch Strength",
            juce::NormalisableRange<float>(0.0, 1.0, 0.01, 1, false), 0.0, juce::String("Pitch Strength"),
            juce::AudioProcessorParameter::genericParameter,
            [](float value, float) { return juce::String(value, 2) + " "; }));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("pitch1Inplace", 1), "Pitch 1 inplace",
            juce::NormalisableRange<float>(-12, 12, 0.01, 1, false), 12, juce::String("Pitch 1 inplace"),
            juce::AudioProcessorParameter::genericParameter,
            [](float value, float) { return juce::String(value, 2) + " st"; }));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("pitch2Inplace", 1), "Pitch 2 inplace",
            juce::NormalisableRange<float>(-12, 12, 0.01, 1, false), -12, juce::String("Pitch 2 inplace"),
            juce::AudioProcessorParameter::genericParameter,
            [](float value, float) { return juce::String(value, 2) + " st"; }));

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
            {"dry", [](const AudioPluginAudioProcessor& p, const float v) { p.pluginRunner->setDry(v); }},
            {"wet", [](const AudioPluginAudioProcessor& p, const float v) { p.pluginRunner->setWet(v); }},
            {"baseSize", [](const AudioPluginAudioProcessor& p, const float v) { p.pluginRunner->setBaseSize(v); }},
            {"sizeFactor", [](const AudioPluginAudioProcessor& p, const float v) { p.pluginRunner->setSizeFactor(v); }},
            {"bulge", [](const AudioPluginAudioProcessor& p, const float v) { p.pluginRunner->setBulge(v); }},
            {"decay", [](const AudioPluginAudioProcessor& p, const float v) { p.pluginRunner->setDecay(v); }},
            {"lowPass", [](const AudioPluginAudioProcessor& p, const float v) { p.pluginRunner->setLowPass(v); }},
            {"highPass", [](const AudioPluginAudioProcessor& p, const float v) { p.pluginRunner->setHighPass(v); }},
            {"modulationDepth",
             [](const AudioPluginAudioProcessor& p, const float v) { p.pluginRunner->setModulationDepth(v); }},
            {"modulationSpeed",
             [](const AudioPluginAudioProcessor& p, const float v) { p.pluginRunner->setModulationSpeed(v); }},
            {"pitchStrength",
             [](const AudioPluginAudioProcessor& p, const float v) { p.pluginRunner->setPitchStrength(v); }},
            {"pitch1Inplace",
             [](const AudioPluginAudioProcessor& p, const float v) { p.pluginRunner->setPitch1Inplace(v); }},
            {"pitch2Inplace",
             [](const AudioPluginAudioProcessor& p, const float v) { p.pluginRunner->setPitch2Inplace(v); }},

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
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioPluginAudioProcessor)
};
