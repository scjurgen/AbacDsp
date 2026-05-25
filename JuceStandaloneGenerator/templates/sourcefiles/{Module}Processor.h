#pragma once
/*
 * AUTO GENERATED,
 * NOT A GOOD IDEA TO CHANGE STUFF HERE
 * Keep the file readonly
 */

#include <juce_audio_processors/juce_audio_processors.h>

#include "/*INCLUDE_PEDAL*/"
#include "Analysis/EnvelopeFollower.h"
#include "Analysis/Spectrogram.h"
#include "Audio/FixedSizeProcessor.h"
#include "UiElements.h"
#include "impl/FileIo.h"

const auto CLutPreset{GuiConstants::GradientPreset::Heat};

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
        /*START_SHOWCPULOAD*/
        , m_avgCpu(8, 0)
        , m_head{0}
        , m_runningWindowCpu(8 * 300) /*END_SHOWCPULOAD*/
        /*START_SHOWVUMETER*/
        , m_envInput{AbacDsp::RmsFollower(10000), AbacDsp::RmsFollower(10000)}
        , m_envOutput{AbacDsp::RmsFollower(10000), AbacDsp::RmsFollower(10000)} /*END_SHOWVUMETER*/
                                                                                /*START_SHOWSPECTROGRAM*/
        , m_spectrogram{}                                                       /*END_SHOWSPECTROGRAM*/
        , m_patchIndex(/*PatchCount*/, 0)
    {
        /*ADD_PARAMETER_LISTENERS*/
        m_fileIo.initialize(m_patchIndex);
    }
    ~AudioPluginAudioProcessor() override
    {
        /*REMOVE_PARAMETER_LISTENERS*/
    }

    void prepareToPlay(const double sampleRate, const int samplesPerBlock) override
    {
        pluginRunner = std::make_unique</*CLASS_NAME*/>(static_cast<float>(sampleRate));
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
        /*CREATE_PARAMETER_LAYOUT*/
        return {params.begin(), params.end()};
    }
#pragma GCC diagnostic pop

    void parameterChanged(const juce::String& parameterID, float newValue) override
    {
        if (pluginRunner == nullptr)
        {
            return;
        }


        /*START_PATCHSUPPORT*/
        if (/*PatchChanged*/)
        {
            /*PatchIndexAssign*/

            if (m_fileIo.areParametersModified())
            {
                const int result = juce::NativeMessageBox::showYesNoBox(
                    juce::MessageBoxIconType::QuestionIcon, "Save Parameters",
                    "Parameters have changed, do you want to save before loading new patch?", nullptr, nullptr);
                handlePatchChange(m_patchIndex, result == 1);
            }
            else
            {
                loadPatchDirect(m_patchIndex);
            }
        }
        else
        {
            /*END_PATCHSUPPORT*/
            m_fileIo.updateParameter(parameterID.toStdString(), newValue);
            /*START_PATCHSUPPORT*/
        }
        /*END_PATCHSUPPORT*/

        static const std::map<juce::String, std::function<void(AudioPluginAudioProcessor&, float)>> parameterMap{
            /*PARAMETER_CHANGED*/
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
        const auto& params = m_fileIo.getCurrentParameters();

        // Apply loaded parameters to APVTS (triggers UI update)
        /*LoadPatches*/
    }


    /*START_SHOWCPULOAD*/
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
    /*END_SHOWCPULOAD*/

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-conversion"

    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) override
    {
        juce::ScopedNoDenormals noDenormals;
        /*START_SHOWCPULOAD*/
        const auto beginTime = std::chrono::high_resolution_clock::now();
        /*END_SHOWCPULOAD*/

        if (!midiMessages.isEmpty())
        {
            for (const auto& msg : midiMessages)
            {
                pluginRunner->processMidi(msg.data);
            }
        }
        /*START_SHOWVUMETER*/
        for (int c = 0; c < std::min(2, buffer.getNumChannels()); ++c)
        {
            m_envInput[c].feed(std::span{buffer.getReadPointer(c), static_cast<size_t>(buffer.getNumSamples())});
            m_inputDb[c].store(std::log10(m_envInput[c].getRms()) * 20.f);
        }
        /*END_SHOWVUMETER*/
        if ((getTotalNumInputChannels() == 2) && (getTotalNumOutputChannels() == 2))
        {
            fixedRunner.processBlock(buffer);
        }
        /*START_SHOWVUMETER*/
        for (int c = 0; c < std::min(2, buffer.getNumChannels()); ++c)
        {
            m_envOutput[c].feed(std::span{buffer.getReadPointer(c), static_cast<size_t>(buffer.getNumSamples())});
            m_outputDb[c].store(std::log10(m_envOutput[c].getRms()) * 20.f);
        }
        /*END_SHOWVUMETER*/
        /*START_SHOWSPECTROGRAM*/
        m_spectrogram.processBlock(buffer.getWritePointer(0), buffer.getNumSamples());
        /*END_SHOWSPECTROGRAM*/
        /*START_SHOWCPULOAD*/
        const auto endTime = std::chrono::high_resolution_clock::now();
        computeCpuLoad(std::chrono::duration_cast<std::chrono::nanoseconds>(endTime - beginTime),
                       static_cast<size_t>(buffer.getNumSamples()));
        /*END_SHOWCPULOAD*/
    }

#pragma GCC diagnostic pop

    /*START_SHOWCPULOAD*/
    [[nodiscard]] float getCpuLoad() const
    {
        return m_cpuLoad.load();
    }
    /*END_SHOWCPULOAD*/

    /*START_SHOWWAVEFORM*/
    [[nodiscard]] const std::vector<float>& getWaveDataToShow()
    {
        return pluginRunner->visualizeWaveData();
    }
    /*END_SHOWWAVEFORM*/
    /*EXTRA_PROCESSOR_METHODS*/
    /*START_SHOWVUMETER*/
    [[nodiscard]] std::pair<float, float> getInputDbLoad() const
    {
        return {m_inputDb[0].load(), m_inputDb[1].load()};
    }

    [[nodiscard]] std::pair<float, float> getOutputDbLoad() const
    {
        return {m_outputDb[0].load(), m_outputDb[1].load()};
    }
    /*END_SHOWVUMETER*/
    /*START_SHOWSPECTROGRAM*/
    [[nodiscard]] AbacDsp::SpectrumImageSet getSpectrogram() const
    {
        return m_spectrogram.getImageSet();
    }
    /*END_SHOWSPECTROGRAM*/

    [[nodiscard]] bool hasRunner() const
    {
        return pluginRunner.get() != nullptr;
    }
    float m_maxValue{0.f};
    /*START_SHOWCPULOAD*/
    size_t elapsedTotalNanoSeconds{0};
    size_t samplesProcessed = 0;
    /*END_SHOWCPULOAD*/

  private:
    size_t m_sampleRate{48000};

    static bool isChanged(const float a, const float b)
    {
        return std::abs(a - b) > 1E-8f;
    }

    int m_program{0};
    juce::ValueTree m_newState;

    AbacDsp::FixedSizeProcessor<2, NumSamplesPerBlock, juce::AudioBuffer<float>> fixedRunner;
    std::unique_ptr</*CLASS_NAME*/> pluginRunner;
    juce::AudioProcessorValueTreeState m_parameters;
    /*START_SHOWCPULOAD*/
    // CPU-Load
    std::atomic<float> m_cpuLoad;
    std::vector<size_t> m_avgCpu;
    size_t m_head{};
    size_t m_runningWindowCpu;
    /*END_SHOWCPULOAD*/
    /*START_SHOWVUMETER*/
    // VU-Meter
    std::atomic<float> m_inputDb[2];
    std::atomic<float> m_outputDb[2];
    std::array<AbacDsp::RmsFollower, 2> m_envInput;
    std::array<AbacDsp::RmsFollower, 2> m_envOutput;
    /*END_SHOWVUMETER*/
    /*START_SHOWSPECTROGRAM*/
    AbacDsp::SimpleSpectrogram m_spectrogram;
    /*END_SHOWSPECTROGRAM*/
    std::vector<int> m_patchIndex;
    FileIo m_fileIo;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioPluginAudioProcessor)
};
