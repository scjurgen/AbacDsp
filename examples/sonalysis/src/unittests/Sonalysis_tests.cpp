#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <functional>
#include <gtest/gtest.h>
#include <numbers>
#include <thread>
#include <vector>

#include "impl/SonalysisAnalysisEngine.h"
#include "impl/SonalysisImpl.h"

namespace
{
constexpr float kSampleRate{48000.f};
constexpr size_t kBlockSize{16};

using Engine = Sonalysis::AnalysisEngine;

[[nodiscard]] float sineAt(const float amplitude, const float frequency, const size_t n)
{
    return amplitude * std::sin(2.f * std::numbers::pi_v<float> * frequency * static_cast<float>(n) / kSampleRate);
}

[[nodiscard]] std::vector<float> interleave(const std::function<float(size_t)>& left,
                                            const std::function<float(size_t)>& right, const float seconds)
{
    std::vector<float> out(2 * static_cast<size_t>(seconds * kSampleRate));
    for (size_t n = 0; n < out.size() / 2; ++n)
    {
        out[2 * n] = left(n);
        out[2 * n + 1] = right(n);
    }
    return out;
}

[[nodiscard]] std::vector<float> stereoSine(const float amplitude, const float frequency, const float seconds)
{
    const auto tone = [=](const size_t n) { return sineAt(amplitude, frequency, n); };
    return interleave(tone, tone, seconds);
}

[[nodiscard]] float clickTrain(const double bpm, const size_t n)
{
    const auto period = static_cast<size_t>(std::llround(60.0 * kSampleRate / bpm));
    const size_t phase = n % period;
    return phase < 480 ? 0.5f * std::exp(-static_cast<float>(phase) / 80.f) * std::sin(static_cast<float>(phase) * 0.3f)
                       : 0.f;
}

[[nodiscard]] Sonalysis::Settings pitchSettings(const Sonalysis::PitchSource source, const float referenceA4 = 440.f)
{
    Sonalysis::Settings settings{};
    settings.pitchTracking = true;
    settings.pitchSource = source;
    settings.referenceA4 = referenceA4;
    return settings;
}

[[nodiscard]] size_t loudestBand(const Sonalysis::SpectrumSnapshot& spectrum)
{
    return static_cast<size_t>(std::ranges::max_element(spectrum.bandsDb) - spectrum.bandsDb.begin());
}

[[nodiscard]] bool waitFor(const std::function<bool()>& condition)
{
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (std::chrono::steady_clock::now() < deadline)
    {
        if (condition())
        {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    return false;
}

void feedImpl(SonalysisImpl<kBlockSize>& impl, const std::vector<float>& interleaved)
{
    AbacDsp::AudioBuffer<2, kBlockSize> in;
    AbacDsp::AudioBuffer<2, kBlockSize> out;
    for (size_t frame = 0; frame + kBlockSize <= interleaved.size() / 2; frame += kBlockSize)
    {
        for (size_t i = 0; i < kBlockSize; ++i)
        {
            in(i, 0) = interleaved[2 * (frame + i)];
            in(i, 1) = interleaved[2 * (frame + i) + 1];
        }
        impl.processBlock(in, out);
    }
}
}

TEST(SonalysisEngineTest, SilenceShowsNothing)
{
    Engine engine(kSampleRate);
    engine.apply(pitchSettings(Sonalysis::PitchSource::Mix));
    engine.analyse(std::vector<float>(2 * 24000, 0.f));
    for (const auto& channel : engine.levels().channels)
    {
        EXPECT_FLOAT_EQ(channel.peakDb, AbacDsp::LevelMeter::kFloorDb);
        EXPECT_FALSE(channel.clipped);
    }
    EXPECT_FALSE(engine.pitch().valid);
    EXPECT_FALSE(engine.beat().valid);
    EXPECT_EQ(engine.beat().onsetCount, 0u);
}

TEST(SonalysisEngineTest, LevelsArePerChannel)
{
    Engine engine(kSampleRate);
    engine.analyse(
        interleave([](const size_t n) { return sineAt(0.5f, 1000.f, n); }, [](const size_t) { return 0.f; }, 0.5f));
    const auto levels = engine.levels();
    EXPECT_NEAR(levels.channels[0].peakDb, -6.0206f, 0.01f);
    EXPECT_NEAR(levels.channels[0].rmsDb, -9.0309f, 0.05f);
    EXPECT_FLOAT_EQ(levels.channels[1].peakDb, AbacDsp::LevelMeter::kFloorDb);
}

TEST(SonalysisEngineTest, HoldResetDropsHoldToCurrentPeak)
{
    Engine engine(kSampleRate);
    engine.analyse(stereoSine(0.9f, 500.f, 0.2f));
    engine.analyse(std::vector<float>(2 * 24000, 0.f));
    const auto held = engine.levels().channels[0];
    EXPECT_NEAR(held.holdDb, 20.f * std::log10(0.9f), 0.01f);
    engine.resetHold();
    const auto reset = engine.levels().channels[0];
    EXPECT_FLOAT_EQ(reset.holdDb, reset.peakDb);
}

TEST(SonalysisEngineTest, SpectrumPeaksAtTheToneOfTheMix)
{
    Engine engine(kSampleRate);
    Sonalysis::Settings settings{};
    settings.fftSize = 4096;
    engine.apply(settings);
    engine.analyse(stereoSine(0.5f, 1000.f, 0.5f));
    const auto spectrum = engine.spectrum();
    EXPECT_EQ(spectrum.fftSize, 4096u);
    const size_t band = loudestBand(spectrum);
    EXPECT_NEAR(spectrum.bandCenters[band], 1000.f, 30.f);
    EXPECT_NEAR(spectrum.bandsDb[band], -6.0206f, 0.35f);
}

TEST(SonalysisEngineTest, PitchNeedsTrackingEnabled)
{
    Engine engine(kSampleRate);
    engine.analyse(stereoSine(0.5f, 220.f, 0.5f));
    EXPECT_FALSE(engine.pitch().valid);
}

TEST(SonalysisEngineTest, PitchReadsNoteAndCents)
{
    Engine engine(kSampleRate);
    engine.apply(pitchSettings(Sonalysis::PitchSource::Mix));
    engine.analyse(stereoSine(0.5f, 220.f, 0.5f));
    const auto pitch = engine.pitch();
    ASSERT_TRUE(pitch.valid);
    EXPECT_EQ(pitch.reading.name(), "A");
    EXPECT_EQ(pitch.reading.octave(), 3);
    EXPECT_NEAR(pitch.reading.cents, 0.f, 1.f);
}

TEST(SonalysisEngineTest, PitchFollowsTheReference)
{
    Engine engine(kSampleRate);
    engine.apply(pitchSettings(Sonalysis::PitchSource::Mix, 443.f));
    engine.analyse(stereoSine(0.5f, 220.f, 0.5f));
    const auto pitch = engine.pitch();
    ASSERT_TRUE(pitch.valid);
    EXPECT_NEAR(pitch.reading.cents, -11.76f, 1.f);
    EXPECT_FLOAT_EQ(pitch.referenceA4, 443.f);
}

TEST(SonalysisEngineTest, PitchTracksBassE1)
{
    Engine engine(kSampleRate);
    engine.apply(pitchSettings(Sonalysis::PitchSource::Mix));
    engine.analyse(stereoSine(0.5f, 41.2034f, 1.f));
    const auto pitch = engine.pitch();
    ASSERT_TRUE(pitch.valid);
    EXPECT_EQ(pitch.reading.name(), "E");
    EXPECT_EQ(pitch.reading.octave(), 1);
    EXPECT_NEAR(pitch.reading.cents, 0.f, 2.f);
}

TEST(SonalysisEngineTest, PitchSourceSelectsTheChannel)
{
    const auto material = interleave([](const size_t n) { return sineAt(0.5f, 220.f, n); },
                                     [](const size_t n) { return sineAt(0.5f, 330.f, n); }, 0.5f);
    Engine left(kSampleRate);
    left.apply(pitchSettings(Sonalysis::PitchSource::Left));
    left.analyse(material);
    ASSERT_TRUE(left.pitch().valid);
    EXPECT_NEAR(left.pitch().reading.frequency, 220.f, 0.5f);

    Engine right(kSampleRate);
    right.apply(pitchSettings(Sonalysis::PitchSource::Right));
    right.analyse(material);
    ASSERT_TRUE(right.pitch().valid);
    EXPECT_NEAR(right.pitch().reading.frequency, 330.f, 0.5f);
}

TEST(SonalysisEngineTest, BeatFinderReadsAClickTrain)
{
    Engine engine(kSampleRate);
    const auto click = [](const size_t n) { return clickTrain(132.0, n); };
    engine.analyse(interleave(click, click, 8.f));
    const auto beat = engine.beat();
    ASSERT_TRUE(beat.valid);
    EXPECT_NEAR(beat.bpm, 132.f, 0.05f);
    EXPECT_LT(beat.jitterMs, 0.1f);
    EXPECT_GE(beat.onsetCount, 17u);

    engine.resetBeat();
    EXPECT_FALSE(engine.beat().valid);
}

TEST(SonalysisEngineTest, BeatFinderHearsAHardPannedPulse)
{
    Engine engine(kSampleRate);
    engine.analyse(
        interleave([](const size_t) { return 0.f; }, [](const size_t n) { return clickTrain(90.0, n); }, 8.f));
    ASSERT_TRUE(engine.beat().valid);
    EXPECT_NEAR(engine.beat().bpm, 90.f, 0.05f);
}

TEST(SonalysisEngineTest, XyHoldsTheNewestPointsInOrder)
{
    Engine engine(kSampleRate);
    const auto ramp = [](const size_t n) { return static_cast<float>(n) * 1e-4f; };
    const auto material = interleave(ramp, [](const size_t n) { return -static_cast<float>(n) * 1e-4f; }, 0.1f);
    engine.analyse(material);
    const auto xy = engine.xy();
    const size_t frames = material.size() / 2;
    EXPECT_FLOAT_EQ(xy.points.back().x, ramp(frames - 1));
    EXPECT_FLOAT_EQ(xy.points.back().y, -ramp(frames - 1));
    EXPECT_FLOAT_EQ(xy.points.front().x, ramp(frames - Sonalysis::kXyPoints));
}

TEST(SonalysisImplTest, OutputIsSilentUnlessThru)
{
    SonalysisImpl<kBlockSize> impl(kSampleRate);
    AbacDsp::AudioBuffer<2, kBlockSize> in;
    AbacDsp::AudioBuffer<2, kBlockSize> out;
    for (size_t i = 0; i < kBlockSize; ++i)
    {
        in(i, 0) = 0.25f;
        in(i, 1) = -0.5f;
    }
    impl.processBlock(in, out);
    EXPECT_FLOAT_EQ(out(3, 0), 0.f);
    EXPECT_FLOAT_EQ(out(3, 1), 0.f);

    impl.setThru(true);
    impl.processBlock(in, out);
    EXPECT_FLOAT_EQ(out(3, 0), 0.25f);
    EXPECT_FLOAT_EQ(out(3, 1), -0.5f);
}

TEST(SonalysisImplTest, WorkerPublishesLevelsAndSpectrum)
{
    SonalysisImpl<kBlockSize> impl(kSampleRate);
    feedImpl(impl, stereoSine(0.5f, 1000.f, 0.3f));
    EXPECT_TRUE(waitFor([&impl] { return impl.levelSnapshot().channels[1].peakDb > -6.1f; }));
    EXPECT_TRUE(
        waitFor([&impl] { return impl.spectrumSnapshot().bandsDb[loudestBand(impl.spectrumSnapshot())] > -6.5f; }));
    EXPECT_EQ(impl.droppedFrames(), 0u);
}

TEST(SonalysisImplTest, PitchIsPublishedOnlyWhileTrackingIsEnabled)
{
    SonalysisImpl<kBlockSize> impl(kSampleRate);
    feedImpl(impl, stereoSine(0.5f, 440.f, 0.3f));
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT_FALSE(impl.pitchSnapshot().valid);

    impl.setPitchTrackingEnabled(true);
    impl.setReferencePitch(442.f);
    feedImpl(impl, stereoSine(0.5f, 440.f, 0.3f));
    ASSERT_TRUE(waitFor([&impl] { return impl.pitchSnapshot().valid; }));
    EXPECT_EQ(impl.pitchSnapshot().reading.midiNote, 69);
    EXPECT_NEAR(impl.pitchSnapshot().reading.cents, -7.85f, 1.f);
}

TEST(SonalysisImplTest, ResetRequestsReachTheWorker)
{
    SonalysisImpl<kBlockSize> impl(kSampleRate);
    impl.setPeakHold(3);
    feedImpl(impl, stereoSine(0.9f, 500.f, 0.1f));
    ASSERT_TRUE(waitFor([&impl] { return impl.levelSnapshot().channels[0].holdDb > -1.f; }));
    feedImpl(impl, std::vector<float>(2 * 9600, 0.f));
    ASSERT_TRUE(waitFor([&impl] { return impl.levelSnapshot().channels[0].peakDb < -2.f; }));
    impl.setHoldReset(true);
    impl.setHoldReset(false);
    EXPECT_TRUE(waitFor([&impl] { return impl.levelSnapshot().channels[0].holdDb < -1.f; }));
}
