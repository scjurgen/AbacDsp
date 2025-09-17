
#include "gtest/gtest.h"

#include "Numbers/Convert.h"

namespace AbacDsp::Test
{
namespace
{
constexpr float epsilon{1E-6f};
}

TEST(ConvertTests, getPanFactorNormalized)
{
    float left, right;
    for (int i = -10; i <= 10; ++i)
    {
        auto f = static_cast<float>(i) / 10.f;
        Convert::getPanFactorNormalized(f, left, right);
        EXPECT_NEAR(left, std::cos((f + 1) / 2 * 3.1415926535 / 2.f), epsilon);
        EXPECT_NEAR(right, std::sin((f + 1) / 2 * 3.1415926535 / 2.f), epsilon);
    }
}

namespace
{
constexpr float MidiNoteC4{60.0f};
constexpr float MidiNoteCSharp4{61.0f};
constexpr float MidiNoteD4{62.0f};
constexpr float MidiNoteA4{69.0f};
constexpr float MidiNoteC5{72.0f};
constexpr float MidiNoteA5{81.0f};
constexpr float MidiNoteC6{84.0f};
}

TEST(ConvertTests, frequencyToNote)
{
    const std::vector<std::pair<float, float>> testCasesA440 = {
        {261.625565f, MidiNoteC4}, {277.182631f, MidiNoteCSharp4}, {293.664768f, MidiNoteD4}, {440.000000f, MidiNoteA4},
        {523.251131f, MidiNoteC5}, {880.000000f, MidiNoteA5},      {1046.502261f, MidiNoteC6}};

    const std::vector<std::pair<float, float>> testCasesA442 = {
        {262.814772f, MidiNoteC4}, {278.442552f, MidiNoteCSharp4}, {294.999608f, MidiNoteD4}, {442.000000f, MidiNoteA4},
        {525.629545f, MidiNoteC5}, {884.000000f, MidiNoteA5},      {1051.259090f, MidiNoteC6}};

    const std::vector<std::pair<float, float>> testCasesA443 = {
        {263.409376f, MidiNoteC4}, {279.072513f, MidiNoteCSharp4}, {295.667028f, MidiNoteD4}, {443.000000f, MidiNoteA4},
        {526.818752f, MidiNoteC5}, {886.000000f, MidiNoteA5},      {1053.637504f, MidiNoteC6}};

    for (const auto& [frequency, expectedNote] : testCasesA440)
    {
        EXPECT_NEAR(Convert::frequencyToNote(frequency, 440.0f) / expectedNote, 1.0f, epsilon);
    }

    for (const auto& [frequency, expectedNote] : testCasesA442)
    {
        EXPECT_NEAR(Convert::frequencyToNote(frequency, 442.0f) / expectedNote, 1.0f, epsilon);
    }

    for (const auto& [frequency, expectedNote] : testCasesA443)
    {
        EXPECT_NEAR(Convert::frequencyToNote(frequency, 443.0f) / expectedNote, 1.0f, epsilon);
    }

    EXPECT_NEAR(Convert::frequencyToNote(440.0f) / MidiNoteA4, 1.0f, epsilon);
}

TEST(ConvertTests, noteToFrequency)
{
    const std::vector<std::pair<float, float>> testCasesA440 = {
        {MidiNoteC4, 261.625565f}, {MidiNoteCSharp4, 277.182631f}, {MidiNoteD4, 293.664768f}, {MidiNoteA4, 440.000000f},
        {MidiNoteC5, 523.251131f}, {MidiNoteA5, 880.000000f},      {MidiNoteC6, 1046.502261f}};

    const std::vector<std::pair<float, float>> testCasesA442 = {
        {MidiNoteC4, 262.814772f}, {MidiNoteCSharp4, 278.442552f}, {MidiNoteD4, 294.999608f}, {MidiNoteA4, 442.000000f},
        {MidiNoteC5, 525.629545f}, {MidiNoteA5, 884.000000f},      {MidiNoteC6, 1051.259090f}};

    const std::vector<std::pair<float, float>> testCasesA443 = {
        {MidiNoteC4, 263.409376f}, {MidiNoteCSharp4, 279.072513f}, {MidiNoteD4, 295.667028f}, {MidiNoteA4, 443.000000f},
        {MidiNoteC5, 526.818752f}, {MidiNoteA5, 886.000000f},      {MidiNoteC6, 1053.637504f}};

    for (const auto& [note, expectedFrequency] : testCasesA440)
    {
        EXPECT_NEAR(Convert::noteToFrequency(note, 440.0f) / expectedFrequency, 1.0f, epsilon);
    }

    for (const auto& [note, expectedFrequency] : testCasesA442)
    {
        EXPECT_NEAR(Convert::noteToFrequency(note, 442.0f) / expectedFrequency, 1.0f, epsilon);
    }

    for (const auto& [note, expectedFrequency] : testCasesA443)
    {
        EXPECT_NEAR(Convert::noteToFrequency(note, 443.0f) / expectedFrequency, 1.0f, epsilon);
    }

    EXPECT_NEAR(Convert::noteToFrequency(MidiNoteA4) / 440.0f, 1.0f, epsilon);
}

TEST(ConvertTests, noteToFrequencyFloatingPoint)
{
    const std::vector<std::pair<float, float>> floatingPointCases = {
        {60.50f, 269.291780f}, {69.25f, 446.399947f}, {72.75f, 546.417437f}, {80.10f, 835.421066f}};

    for (const auto& [note, expectedFrequency] : floatingPointCases)
    {
        EXPECT_NEAR(Convert::noteToFrequency(note) / expectedFrequency, 1.0f, epsilon);
    }
}

TEST(ConvertTests, noteIntervalToRatio)
{
    EXPECT_NEAR(Convert::noteIntervalToRatio(0.0f), 1.0f, epsilon);
    EXPECT_NEAR(Convert::noteIntervalToRatio(12.0f), 2.0f, epsilon);
    EXPECT_NEAR(Convert::noteIntervalToRatio(-12.0f), 0.5f, epsilon);
    EXPECT_NEAR(Convert::noteIntervalToRatio(7.0f), std::pow(2.0f, 7.0f / 12.0f), epsilon);
    EXPECT_NEAR(Convert::noteIntervalToRatio(6.0f), std::pow(2.0f, 0.5f), epsilon);
    EXPECT_NEAR(Convert::noteIntervalToRatio(1.0f), std::pow(2.0f, 1.0f / 12.0f), epsilon);
    EXPECT_NEAR(Convert::noteIntervalToRatio(-5.0f), std::pow(2.0f, -5.0f / 12.0f), epsilon);
}

TEST(ConvertTests, ratioToNoteInterval)
{
    EXPECT_NEAR(Convert::ratioToNoteInterval(1.0f), 0.0f, epsilon);
    EXPECT_NEAR(Convert::ratioToNoteInterval(2.0f), 12.0f, epsilon);
    EXPECT_NEAR(Convert::ratioToNoteInterval(0.5f), -12.0f, epsilon);
    EXPECT_NEAR(Convert::ratioToNoteInterval(std::pow(2.0f, 7.0f / 12.0f)), 7.0f, epsilon);
    EXPECT_NEAR(Convert::ratioToNoteInterval(std::pow(2.0f, 0.5f)), 6.0f, epsilon);
    EXPECT_NEAR(Convert::ratioToNoteInterval(std::pow(2.0f, 1.0f / 12.0f)), 1.0f, epsilon);
    EXPECT_NEAR(Convert::ratioToNoteInterval(std::pow(2.0f, -5.0f / 12.0f)), -5.0f, epsilon);
}

TEST(ConvertTests, centsToRelativePitch)
{
    constexpr float epsilon{1E-7f};

    EXPECT_NEAR(Convert::centsToRelativePitch(0.0f), 1.0f, epsilon);
    EXPECT_NEAR(Convert::centsToRelativePitch(1200.0f), 2.0f, epsilon);
    EXPECT_NEAR(Convert::centsToRelativePitch(-1200.0f), 0.5f, epsilon);
    EXPECT_NEAR(Convert::centsToRelativePitch(100.0f), std::pow(2.0f, 1.0f / 12.0f), epsilon);
    EXPECT_NEAR(Convert::centsToRelativePitch(-100.0f), std::pow(2.0f, -1.0f / 12.0f), epsilon);
    EXPECT_NEAR(Convert::centsToRelativePitch(1.0f), std::pow(2.0f, 1.0f / 1200.0f), epsilon);
    EXPECT_NEAR(Convert::centsToRelativePitch(-1.0f), std::pow(2.0f, -1.0f / 1200.0f), epsilon);
}
}