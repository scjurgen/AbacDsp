#include <gtest/gtest.h>
#include <vector>

#include "impl/HihatClassifier.h"

using namespace DrumInput;

namespace
{
constexpr float kSampleRate{48000.f};
constexpr int64_t kGraceSamples{144}; // 3 ms at 48 kHz

struct Fixture
{
    Fixture()
    {
        classifier.configure(HihatSettings{}, kSampleRate);
    }

    [[nodiscard]] auto sink()
    {
        return [this](const ClassifiedHihat& hit) { emitted.push_back(hit); };
    }

    HihatClassifier classifier;
    std::vector<ClassifiedHihat> emitted;
};
}

TEST(HihatClassifierTest, ThresholdsSplitClosedHalfOpenAndOpen)
{
    Fixture f;
    EXPECT_EQ(f.classifier.classify(127), HihatArticulation::Closed);
    EXPECT_EQ(f.classifier.classify(126), HihatArticulation::Closed);
    EXPECT_EQ(f.classifier.classify(125), HihatArticulation::HalfOpen);
    EXPECT_EQ(f.classifier.classify(121), HihatArticulation::HalfOpen);
    EXPECT_EQ(f.classifier.classify(120), HihatArticulation::Open);
    EXPECT_EQ(f.classifier.classify(0), HihatArticulation::Open);
}

TEST(HihatClassifierTest, PedalIsAssumedClosedBeforeTheFirstCc)
{
    Fixture f;
    f.classifier.onStick(100, 1000, f.sink());
    f.classifier.advance(2000, f.sink());
    ASSERT_EQ(f.emitted.size(), 1u);
    EXPECT_EQ(f.emitted[0].articulation, HihatArticulation::Closed);
    EXPECT_EQ(f.emitted[0].pedal, 127);
}

TEST(HihatClassifierTest, HitIsEmittedOnlyAfterItsGraceWindow)
{
    Fixture f;
    f.classifier.onPedal(0, 0, f.sink());
    f.classifier.onStick(90, 1000, f.sink());
    f.classifier.advance(1000 + kGraceSamples, f.sink());
    EXPECT_TRUE(f.emitted.empty());
    f.classifier.advance(1000 + kGraceSamples + 1, f.sink());
    ASSERT_EQ(f.emitted.size(), 1u);
    EXPECT_EQ(f.emitted[0].time, 1000);
    EXPECT_EQ(f.emitted[0].velocity, 90);
    EXPECT_EQ(f.emitted[0].articulation, HihatArticulation::Open);
}

TEST(HihatClassifierTest, PedalCcInsideTheGraceWindowStillCountsForTheHit)
{
    Fixture f;
    f.classifier.onStick(100, 1000, f.sink());
    f.classifier.onPedal(0, 1000 + kGraceSamples, f.sink());
    f.classifier.advance(5000, f.sink());
    ASSERT_EQ(f.emitted.size(), 1u);
    EXPECT_EQ(f.emitted[0].articulation, HihatArticulation::Open);
    EXPECT_EQ(f.emitted[0].pedal, 0);
}

TEST(HihatClassifierTest, PedalCcAfterTheGraceWindowEmitsTheHitWithTheOldValue)
{
    Fixture f;
    f.classifier.onStick(100, 1000, f.sink());
    f.classifier.onPedal(0, 1000 + kGraceSamples + 1, f.sink());
    ASSERT_EQ(f.emitted.size(), 1u);
    EXPECT_EQ(f.emitted[0].articulation, HihatArticulation::Closed);
    EXPECT_EQ(f.classifier.pedal(), 0);
}

TEST(HihatClassifierTest, HalfOpenPedalGivesHalfOpen)
{
    Fixture f;
    f.classifier.onPedal(123, 0, f.sink());
    f.classifier.onStick(100, 1000, f.sink());
    f.classifier.advance(5000, f.sink());
    ASSERT_EQ(f.emitted.size(), 1u);
    EXPECT_EQ(f.emitted[0].articulation, HihatArticulation::HalfOpen);
}

TEST(HihatClassifierTest, ASecondStickHitFlushesThePendingOne)
{
    Fixture f;
    f.classifier.onStick(100, 1000, f.sink());
    f.classifier.onStick(80, 1010, f.sink());
    ASSERT_EQ(f.emitted.size(), 1u);
    EXPECT_EQ(f.emitted[0].time, 1000);
    f.classifier.advance(5000, f.sink());
    ASSERT_EQ(f.emitted.size(), 2u);
    EXPECT_EQ(f.emitted[1].time, 1010);
}

TEST(HihatClassifierTest, ZeroGraceEmitsImmediately)
{
    HihatClassifier classifier;
    HihatSettings settings{};
    settings.ccGraceMs = 0.f;
    classifier.configure(settings, kSampleRate);
    std::vector<ClassifiedHihat> emitted;
    classifier.onStick(100, 1000, [&](const ClassifiedHihat& hit) { emitted.push_back(hit); });
    EXPECT_EQ(emitted.size(), 1u);
}

TEST(HihatClassifierTest, ResetDropsThePendingHit)
{
    Fixture f;
    f.classifier.onStick(100, 1000, f.sink());
    f.classifier.reset();
    f.classifier.advance(5000, f.sink());
    EXPECT_TRUE(f.emitted.empty());
}
