#pragma once

#include <atomic>
#include <cstdint>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_core/juce_core.h>
#include <memory>

#include "AuthoringHttpServerCore.h"

// Captures this instance's own post-DSP audio output to a WAV file for the Authoring HTTP
// API's POST /record/start and /record/stop - so an LLM authoring session can audition what
// a script/patch actually produced, not just whether it compiled. pushBlock() (audio thread)
// never allocates or blocks: it only ever holds the SpinLock below for the same handful of
// instructions start()/stop() need to swap which ThreadedWriter exists, and simply skips the
// block if that briefly loses the race - the actual per-sample write is JUCE's own lock-free
// FIFO, flushed to disk on a background thread. start()/stop() are message-thread only.
class AuthoringAudioRecorder
{
  public:
    AuthoringAudioRecorder()
        : m_writerThread("AuthoringRecorder")
    {
        m_writerThread.startThread();
    }

    ~AuthoringAudioRecorder()
    {
        (void) stop();
        (void) m_writerThread.stopThread(1000);
    }

    void prepare(const double sampleRate, const int numChannels) noexcept
    {
        m_sampleRate = sampleRate;
        m_numChannels = numChannels;
        m_maxSamples = static_cast<std::uint64_t>(sampleRate * kMaxRecordSeconds);
    }

    [[nodiscard]] AuthoringRecordStartResult start(const juce::File& file)
    {
        if (isRecording())
        {
            return {false, {}, "already recording"};
        }
        (void) file.getParentDirectory().createDirectory();
        auto fileStream = std::make_unique<juce::FileOutputStream>(file);
        if (!fileStream->openedOk())
        {
            return {false, {}, "could not open " + file.getFullPathName().toStdString()};
        }
        std::unique_ptr<juce::OutputStream> stream(std::move(fileStream));
        const auto options = juce::AudioFormatWriterOptions{}
                                 .withSampleRate(m_sampleRate)
                                 .withNumChannels(m_numChannels)
                                 .withBitsPerSample(32);
        auto writer = juce::WavAudioFormat().createWriterFor(stream, options);
        if (writer == nullptr)
        {
            return {false, {}, "failed to create WAV writer"};
        }

        const juce::SpinLock::ScopedLockType lock(m_writerLock);
        m_threadedWriter =
            std::make_unique<juce::AudioFormatWriter::ThreadedWriter>(writer.release(), m_writerThread, 32768);
        m_samplesWritten.store(0, std::memory_order_relaxed);
        m_capped.store(false, std::memory_order_relaxed);
        m_currentFile = file;
        m_isRecording.store(true, std::memory_order_release);
        return {true, file.getFullPathName().toStdString(), {}};
    }

    [[nodiscard]] AuthoringRecordStopResult stop()
    {
        if (!isRecording())
        {
            return {};
        }
        m_isRecording.store(false, std::memory_order_release);
        const juce::SpinLock::ScopedLockType lock(m_writerLock);
        m_threadedWriter.reset(); // flushes and finalizes the WAV header
        const auto samples = m_samplesWritten.load(std::memory_order_relaxed);
        return {true, m_currentFile.getFullPathName().toStdString(),
                m_sampleRate > 0.0 ? static_cast<double>(samples) / m_sampleRate : 0.0,
                m_capped.load(std::memory_order_relaxed)};
    }

    [[nodiscard]] bool isRecording() const noexcept
    {
        return m_isRecording.load(std::memory_order_acquire);
    }

    [[nodiscard]] float elapsedSeconds() const noexcept
    {
        if (!isRecording() || m_sampleRate <= 0.0)
        {
            return 0.f;
        }
        return static_cast<float>(static_cast<double>(m_samplesWritten.load(std::memory_order_relaxed)) / m_sampleRate);
    }

    // Audio-thread only; see the class comment above for why this never blocks or allocates.
    void pushBlock(const juce::AudioBuffer<float>& buffer) noexcept
    {
        if (!isRecording())
        {
            return;
        }
        const juce::SpinLock::ScopedTryLockType lock(m_writerLock);
        if (!lock.isLocked() || m_threadedWriter == nullptr)
        {
            return;
        }
        const auto already = m_samplesWritten.load(std::memory_order_relaxed);
        if (already >= m_maxSamples)
        {
            m_capped.store(true, std::memory_order_relaxed);
            m_isRecording.store(false, std::memory_order_release);
            return;
        }
        const auto numSamples = buffer.getNumSamples();
        m_threadedWriter->write(buffer.getArrayOfReadPointers(), numSamples);
        m_samplesWritten.store(already + static_cast<std::uint64_t>(numSamples), std::memory_order_relaxed);
    }

  private:
    static constexpr double kMaxRecordSeconds = 30.0;

    juce::TimeSliceThread m_writerThread;
    juce::SpinLock m_writerLock;
    std::unique_ptr<juce::AudioFormatWriter::ThreadedWriter> m_threadedWriter;
    std::atomic<bool> m_isRecording{false};
    std::atomic<bool> m_capped{false};
    std::atomic<std::uint64_t> m_samplesWritten{0};
    std::uint64_t m_maxSamples{0};
    double m_sampleRate{0.0};
    int m_numChannels{0};
    juce::File m_currentFile;
};
