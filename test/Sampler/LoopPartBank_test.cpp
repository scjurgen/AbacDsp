#include <gtest/gtest.h>

#include "Sampler/LoopPartBank.h"

namespace AbacDsp::test
{

namespace
{
constexpr size_t kBlock = 16;
using Bank = LoopPartBank<kBlock>;
using Buffer = AudioBuffer<2, kBlock>;

void feed(LoopRecorder<kBlock>& rec, const size_t frames, const float value)
{
    size_t done = 0;
    while (done < frames)
    {
        Buffer in{};
        Buffer out{};
        for (size_t i = 0; i < kBlock; ++i)
        {
            in(i, 0) = value;
            in(i, 1) = value;
        }
        rec.processBlock(in, out);
        done += kBlock;
    }
}

void recordInto(LoopRecorder<kBlock>& rec, const size_t frames, const float value)
{
    rec.beginRecord();
    feed(rec, frames, value);
    rec.stopRecordFree();
}
}

TEST(LoopPartBankTest, StartsWithFourEmptyPartsAndPartAActive)
{
    Bank bank(48000.f, 10.f);
    EXPECT_EQ(Bank::kMaxParts, 4u);
    EXPECT_EQ(bank.activeIndex(), 0u);
    for (size_t i = 0; i < Bank::kMaxParts; ++i)
    {
        EXPECT_FALSE(bank.hasContent(i)) << "part " << i;
        EXPECT_EQ(bank.loopLengthFrames(i), 0u) << "part " << i;
    }
}

TEST(LoopPartBankTest, PartsAreIndependentRecorders)
{
    Bank bank(48000.f, 10.f);
    recordInto(bank.part(0), kBlock, 1.f);
    recordInto(bank.part(2), 2 * kBlock, 5.f);

    EXPECT_TRUE(bank.hasContent(0));
    EXPECT_EQ(bank.loopLengthFrames(0), kBlock);
    EXPECT_FALSE(bank.hasContent(1));
    EXPECT_TRUE(bank.hasContent(2));
    EXPECT_EQ(bank.loopLengthFrames(2), 2 * kBlock);
    EXPECT_FALSE(bank.hasContent(3));

    EXPECT_FLOAT_EQ(bank.part(0).sample(0, 0), 1.f);
    EXPECT_FLOAT_EQ(bank.part(2).sample(0, 0), 5.f);
}

TEST(LoopPartBankTest, ActiveTracksSetActiveIndex)
{
    Bank bank(48000.f, 10.f);
    recordInto(bank.part(1), kBlock, 3.f);

    EXPECT_EQ(&bank.active(), &bank.part(0));

    bank.setActiveIndex(1);
    EXPECT_EQ(bank.activeIndex(), 1u);
    EXPECT_EQ(&bank.active(), &bank.part(1));
    EXPECT_TRUE(bank.active().hasLoop());
    EXPECT_FLOAT_EQ(bank.active().sample(0, 0), 3.f);
}

TEST(LoopPartBankTest, SetActiveIndexDoesNotTouchAnyPartState)
{
    Bank bank(48000.f, 10.f);
    recordInto(bank.part(0), kBlock, 1.f);

    bank.setActiveIndex(3); // part 3 is empty, untouched by the switch itself
    EXPECT_EQ(bank.activeIndex(), 3u);
    EXPECT_FALSE(bank.active().hasLoop());
    EXPECT_TRUE(bank.hasContent(0)) << "switching away from part 0 must not clear it";
    EXPECT_FLOAT_EQ(bank.part(0).sample(0, 0), 1.f);
}

TEST(LoopPartBankTest, ConstAccessorsReturnSameUnderlyingParts)
{
    Bank bank(48000.f, 10.f);
    recordInto(bank.part(0), kBlock, 7.f);

    const Bank& constBank = bank;
    EXPECT_TRUE(constBank.hasContent(0));
    EXPECT_FLOAT_EQ(constBank.part(0).sample(0, 0), 7.f);
    EXPECT_FLOAT_EQ(constBank.active().sample(0, 0), 7.f);
}

}
