#include <gtest/gtest.h>

#include "impl/NotePattern.h"

using namespace NotePattern;

TEST(NotePatternTest, DefaultPatternIsInactiveAndEmpty)
{
    const Pattern pattern{};
    EXPECT_FALSE(pattern.active);
    EXPECT_EQ(pattern.noteCount, 0u);
    EXPECT_EQ(pattern.beatsPerBar, 4u);
    EXPECT_EQ(pattern.patternBars, 1u);
    EXPECT_FLOAT_EQ(pattern.pickupBeats, 0.f);
    EXPECT_TRUE(pattern.clickDefault);
}

TEST(NotePatternTest, PatternBeatsIsBarsTimesBeatsPerBar)
{
    Pattern pattern{};
    pattern.beatsPerBar = 3;
    pattern.patternBars = 2;
    EXPECT_FLOAT_EQ(pattern.patternBeats(), 6.f);
}

TEST(NotePatternTest, FocusFromIndexAcceptsEveryNamedFocusOnly)
{
    for (size_t i = 0; i < kFocusNames.size(); ++i)
    {
        const auto focus = focusFromIndex(static_cast<long long>(i));
        ASSERT_TRUE(focus.has_value());
        EXPECT_EQ(static_cast<size_t>(*focus), i);
    }
    EXPECT_FALSE(focusFromIndex(-1).has_value());
    EXPECT_FALSE(focusFromIndex(static_cast<long long>(kFocusNames.size())).has_value());
}
