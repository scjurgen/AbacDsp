#include <gtest/gtest.h>

#include "Helpers/StereoTrackBank.h"

namespace AbacDsp
{
namespace
{

class TrackedEffect
{
  public:
    explicit TrackedEffect(const float sampleRate)
        : m_sampleRate(sampleRate)
    {
    }

    void setCutoff(const float value) noexcept
    {
        m_cutoff = value;
    }

    [[nodiscard]] float getCutoff() const noexcept
    {
        return m_cutoff;
    }

    [[nodiscard]] float getSampleRate() const noexcept
    {
        return m_sampleRate;
    }

  private:
    float m_sampleRate;
    float m_cutoff{0.f};
};

class StereoTrackBankTest : public ::testing::Test
{
  protected:
    static constexpr float kSampleRate = 48000.f;
};

}

TEST_F(StereoTrackBankTest, ConstructsAllInstancesWithSameArgs)
{
    StereoTrackBank<TrackedEffect, 3> bank(kSampleRate);

    for (std::size_t track = 0; track < 3; ++track)
    {
        EXPECT_EQ(bank.left(track).getSampleRate(), kSampleRate);
        EXPECT_EQ(bank.right(track).getSampleRate(), kSampleRate);
    }
}

TEST_F(StereoTrackBankTest, LeftAndRightAreIndependentInstances)
{
    StereoTrackBank<TrackedEffect, 2> bank(kSampleRate);

    bank.left(0).setCutoff(100.f);
    bank.right(0).setCutoff(200.f);

    EXPECT_EQ(bank.left(0).getCutoff(), 100.f);
    EXPECT_EQ(bank.right(0).getCutoff(), 200.f);
}

TEST_F(StereoTrackBankTest, TracksAreIndependentInstances)
{
    StereoTrackBank<TrackedEffect, 2> bank(kSampleRate);

    bank.left(0).setCutoff(100.f);
    bank.left(1).setCutoff(300.f);

    EXPECT_EQ(bank.left(0).getCutoff(), 100.f);
    EXPECT_EQ(bank.left(1).getCutoff(), 300.f);
}

TEST_F(StereoTrackBankTest, ForEachVisitsEveryChannelAndTrack)
{
    StereoTrackBank<TrackedEffect, 3> bank(kSampleRate);

    int visited = 0;
    bank.forEach(
        [&visited](TrackedEffect& effect)
        {
            effect.setCutoff(42.f);
            ++visited;
        });

    EXPECT_EQ(visited, 6);
    for (std::size_t track = 0; track < 3; ++track)
    {
        EXPECT_EQ(bank.left(track).getCutoff(), 42.f);
        EXPECT_EQ(bank.right(track).getCutoff(), 42.f);
    }
}

TEST_F(StereoTrackBankTest, ForEachAtTrackVisitsOnlyThatTracksTwoChannels)
{
    StereoTrackBank<TrackedEffect, 3> bank(kSampleRate);

    int visited = 0;
    bank.forEachAtTrack(1,
                        [&visited](TrackedEffect& effect)
                        {
                            effect.setCutoff(7.f);
                            ++visited;
                        });

    EXPECT_EQ(visited, 2);
    EXPECT_EQ(bank.left(1).getCutoff(), 7.f);
    EXPECT_EQ(bank.right(1).getCutoff(), 7.f);
    EXPECT_EQ(bank.left(0).getCutoff(), 0.f);
    EXPECT_EQ(bank.right(2).getCutoff(), 0.f);
}

TEST_F(StereoTrackBankTest, TracksReturnsTemplateParameter)
{
    EXPECT_EQ((StereoTrackBank<TrackedEffect, 5>::tracks()), 5u);
}

}
