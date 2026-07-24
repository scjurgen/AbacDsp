#include <cmath>
#include <gtest/gtest.h>

#include "Generators/ClickGenerator.h"

namespace AbacDsp::test
{

namespace
{
constexpr float kSampleRate{48000.f};

struct Captured
{
    float peak{0.f};
    float energy{0.f};
    size_t zeroCrossings{0};
    float last{0.f};
};

template <size_t N>
[[nodiscard]] Captured capture(ClickGenerator& click)
{
    Captured result{};
    float previous = 0.f;
    for (size_t i = 0; i < N; ++i)
    {
        const float value = click.step0();
        result.peak = std::max(result.peak, std::abs(value));
        result.energy += value * value;
        if ((previous < 0.f && value >= 0.f) || (previous > 0.f && value <= 0.f))
        {
            ++result.zeroCrossings;
        }
        previous = value;
        result.last = value;
    }
    return result;
}
}

TEST(ClickGeneratorTest, SilentUntilTriggered)
{
    ClickGenerator click{kSampleRate};
    const auto captured = capture<2048>(click);
    EXPECT_FLOAT_EQ(captured.peak, 0.f);
    EXPECT_FLOAT_EQ(captured.energy, 0.f);
}

TEST(ClickGeneratorTest, BeatTriggerRingsThenDecays)
{
    ClickGenerator click{kSampleRate};
    click.setVolumeDb(0.f);
    click.trigger(ClickAccent::Beat);

    const auto onset = capture<256>(click);
    EXPECT_GT(onset.peak, 0.f);

    // decaySeconds default 0.04 -> ~1920 samples at 48k; well decayed after 8192.
    const auto tail = capture<8192>(click);
    EXPECT_LT(std::abs(tail.last), onset.peak * 0.01f);
}

TEST(ClickGeneratorTest, DownbeatIsLowerPitchedThanBeat)
{
    ClickGenerator downbeat{kSampleRate};
    ClickGenerator beat{kSampleRate};
    downbeat.setVolumeDb(0.f);
    beat.setVolumeDb(0.f);
    downbeat.trigger(ClickAccent::Downbeat);
    beat.trigger(ClickAccent::Beat);

    const auto downbeatCaptured = capture<2048>(downbeat);
    const auto beatCaptured = capture<2048>(beat);

    EXPECT_LT(downbeatCaptured.zeroCrossings, beatCaptured.zeroCrossings);
}

TEST(ClickGeneratorTest, LouderVolumeProducesLargerPeak)
{
    ClickGenerator quiet{kSampleRate};
    ClickGenerator loud{kSampleRate};
    quiet.setVolumeDb(-24.f);
    loud.setVolumeDb(0.f);
    quiet.trigger(ClickAccent::Beat);
    loud.trigger(ClickAccent::Beat);

    const auto quietCaptured = capture<512>(quiet);
    const auto loudCaptured = capture<512>(loud);

    EXPECT_GT(loudCaptured.peak, quietCaptured.peak * 4.f);
}

TEST(ClickGeneratorTest, VolumeScalesLinearly)
{
    ClickGenerator reference{kSampleRate};
    ClickGenerator doubled{kSampleRate};
    reference.setVolumeDb(-6.f);
    doubled.setVolumeDb(0.f);
    reference.trigger(ClickAccent::Beat);
    doubled.trigger(ClickAccent::Beat);

    const auto referencePeak = capture<512>(reference).peak;
    const auto doubledPeak = capture<512>(doubled).peak;

    // +6 dB is a linear factor of ~1.995; the resonator is linear in the initial state.
    EXPECT_NEAR(doubledPeak / referencePeak, std::pow(10.f, 6.f / 20.f), 0.02f);
}

TEST(ClickGeneratorTest, TriggerSubMatchesSubAccent)
{
    ClickGenerator viaAccent{kSampleRate};
    ClickGenerator viaHelper{kSampleRate};
    viaAccent.setSubVolumeDb(-6.f);
    viaHelper.setSubVolumeDb(-6.f);
    viaAccent.trigger(ClickAccent::Sub);
    viaHelper.triggerSub();

    for (size_t i = 0; i < 512; ++i)
    {
        EXPECT_FLOAT_EQ(viaAccent.step0(), viaHelper.step0());
    }
}

TEST(ClickGeneratorTest, SubVoiceIsIndependentOfBeatVoice)
{
    ClickGenerator click{kSampleRate};
    click.setVolumeDb(-60.f);
    click.setSubVolumeDb(0.f);
    click.trigger(ClickAccent::Sub);

    const auto captured = capture<512>(click);
    EXPECT_GT(captured.peak, 0.1f);
}

TEST(ClickGeneratorTest, NoneAccentIsSilent)
{
    ClickGenerator click{kSampleRate};
    click.setVolumeDb(0.f);
    click.trigger(ClickAccent::None);

    const auto captured = capture<512>(click);
    EXPECT_FLOAT_EQ(captured.peak, 0.f);
}

}
