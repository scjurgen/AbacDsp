#include <gtest/gtest.h>

#include "Sampler/GrooveNoteMap.h"

namespace AbacDsp::test
{

TEST(GrooveNoteMapTest, KnownNoteResolvesItsDocumentedTagChainMostSpecificFirst)
{
    const auto tags = tagsForGrooveNote(42); // HihatClosed
    EXPECT_EQ(tags[0], GrooveTag::HihatClosed);
    EXPECT_EQ(tags[1], GrooveTag::Hihat);
    EXPECT_EQ(tags[2], GrooveTag::Cymbal);
    EXPECT_EQ(tags[3], GrooveTag::None);
}

TEST(GrooveNoteMapTest, UnmappedNoteReturnsAllNone)
{
    const auto tags = tagsForGrooveNote(1); // not in kGrooveNoteMap
    for (const auto tag : tags)
    {
        EXPECT_EQ(tag, GrooveTag::None);
    }
}

TEST(GrooveNoteMapTest, TagFromNameResolvesKnownNamesAndRejectsUnknownOnes)
{
    EXPECT_EQ(tagFromName("kick"), GrooveTag::Kick);
    EXPECT_EQ(tagFromName("hihat_closed"), GrooveTag::HihatClosed);
    EXPECT_EQ(tagFromName("not_a_real_instrument"), GrooveTag::None);
    EXPECT_EQ(tagFromName(""), GrooveTag::None);
}

}
