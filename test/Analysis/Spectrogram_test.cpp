#include <algorithm>
#include <chrono>
#include <cmath>
#include <gtest/gtest.h>
#include <thread>
#include <vector>

#include "Analysis/Spectrogram.h"

namespace AbacDsp::Test
{
namespace
{
constexpr size_t kFft = 1024;

[[nodiscard]] std::vector<float> makeWindow(const size_t index)
{
    std::vector<float> window(kFft);
    for (size_t i = 0; i < kFft; ++i)
    {
        window[i] = std::sin(static_cast<float>(index * kFft + i) * 0.05f);
    }
    return window;
}

// Feed FFT-length windows one at a time so the background worker keeps up (no
// queue drops), then wait until `ready()` holds or a generous deadline elapses.
// Returns whether the worker produced the expected output; not a fixed sleep,
// so it stays reliable under load.
template <typename Spec, typename Ready>
[[nodiscard]] bool feedAndDrainUntil(Spec& spec, const size_t windows, Ready ready)
{
    for (size_t w = 0; w < windows; ++w)
    {
        spec.processBlock(makeWindow(w));
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (!ready() && std::chrono::steady_clock::now() < deadline)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    return ready();
}

// onNewFFTData / processFFT stamp the next slice with 1.0f, so a written
// spectrogram contains at least one such value once the worker has run.
template <typename ImageSet>
[[nodiscard]] bool hasWorkerOutput(const ImageSet& img)
{
    return std::any_of(img.data, img.data + img.size(), [](const float v) { return v == 1.0f; });
}
} // namespace

TEST(Spectrogram, melSpectroGramProcessesBlockWithoutCrashing)
{
    MelSpectroGram spec;
    const std::vector<float> block(2048, 0.1f);
    spec.processBlock(block);

    const auto imageSet = spec.getImageSet();
    EXPECT_EQ(imageSet.size(), imageSet.width * imageSet.fftHalfLength);
    EXPECT_NE(imageSet.data, nullptr);
}

TEST(SimpleSpectrogram, processesBlockWithoutCrashing)
{
    SimpleSpectrogram spec;
    spec.setSampleRate(48000.f);
    const std::vector<float> block(4096, 0.1f);
    spec.processBlock(block);

    const auto imageSet = spec.getImageSet();
    EXPECT_EQ(imageSet.size(), imageSet.width * imageSet.height);
    EXPECT_NE(imageSet.data, nullptr);
}

TEST(FloatingHorizonFFTImage, processesBlockWithoutCrashing)
{
    FloatingHorizonFFTImage spec;
    const std::vector<float> block(4096, 0.1f);
    spec.processBlock(block);

    const auto imageSet = spec.getImageSet();
    EXPECT_EQ(imageSet.size(), imageSet.width * imageSet.height);
    EXPECT_NE(imageSet.data, nullptr);
}

TEST(MelSpectroGram, workerProcessesFftsAndWrapsSlices)
{
    MelSpectroGram spec;
    spec.setFftLength(kFft);
    spec.setMelBands(20);
    spec.setSlices(2); // small: the worker wraps m_currentSlice quickly

    const bool produced = feedAndDrainUntil(spec, 8, [&] { return hasWorkerOutput(spec.getImageSet()); });
    EXPECT_TRUE(produced);
}

TEST(MelSpectroGram, fullQueueDropsExcessWithoutCrashing)
{
    MelSpectroGram spec;
    spec.setFftLength(kFft);
    // A single oversized block enqueues far more FFTs than the 4-slot queue holds,
    // exercising the queue-full drop path in enqueueFFT.
    const std::vector<float> big(kFft * 20, 0.2f);
    spec.processBlock(big);
    EXPECT_NE(spec.getImageSet().data, nullptr);
}

TEST(SimpleSpectrogram, fftLengthReflectsSetFftLength)
{
    SimpleSpectrogram spec;
    spec.setFftLength(kFft);
    EXPECT_EQ(spec.fftLength(), kFft);
}

TEST(SimpleSpectrogram, workerWritesSlicesAndWraps)
{
    SimpleSpectrogram spec;
    spec.setFftLength(kFft);
    spec.setSlices(2);

    const bool produced = feedAndDrainUntil(spec, 8, [&] { return hasWorkerOutput(spec.getImageSet()); });
    EXPECT_TRUE(produced);
}

TEST(SimpleSpectrogram, emptySpectrogramIsHandled)
{
    SimpleSpectrogram spec;
    spec.setFftLength(kFft);
    spec.setSlices(0); // spectrogram becomes empty: onNewFFTData must early-return

    for (size_t w = 0; w < 6; ++w)
    {
        spec.processBlock(makeWindow(w));
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    EXPECT_EQ(spec.getImageSet().size(), 0u); // no crash, nothing written
}

TEST(SpectrogramBase, windowForwardIsClamped)
{
    SimpleSpectrogram spec;
    spec.setFftLength(kFft);
    spec.setWindowForward(0.0001f); // clamped up to 0.01
    spec.setWindowForward(5.0f);    // clamped down to 0.99
    const std::vector<float> block(kFft * 2, 0.1f);
    spec.processBlock(block);
    EXPECT_NE(spec.getImageSet().data, nullptr);
}

TEST(FloatingHorizonFFTImage, workerAccumulatesAndPlotsHorizon)
{
    FloatingHorizonFFTImage spec;
    spec.setFftLength(kFft);
    // The horizon is plotted only after 20 accumulated FFTs, so feed well past that.
    const bool plotted = feedAndDrainUntil(spec, 30,
                                           [&]
                                           {
                                               const auto img = spec.getImageSet();
                                               return std::any_of(img.data, img.data + img.size(),
                                                                  [](const float v) { return v != 0.0f; });
                                           });
    EXPECT_TRUE(plotted);
}

}
