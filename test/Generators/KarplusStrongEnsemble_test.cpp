#include <cmath>

#include "gtest/gtest.h"

#include "Generators/KarplusStrongEnsemble.h"

namespace AbacDsp::Test
{

namespace
{
constexpr float kSampleRate = 48000.f;
using TestEnsemble = KarplusStrongEnsemble<5, 10000>;
}

TEST(KarplusStrongEnsemble, numVoicesMatchesTemplateParameter)
{
    EXPECT_EQ(TestEnsemble::numVoices(), 5u);
}

TEST(KarplusStrongEnsemble, isAnyActiveTracksIndividualVoices)
{
    TestEnsemble ensemble{kSampleRate};
    EXPECT_FALSE(ensemble.isAnyActive());

    ensemble.voice(2).trigger(60.f, 1.f);
    EXPECT_TRUE(ensemble.isAnyActive());

    ensemble.voice(2).muteString();
    EXPECT_FALSE(ensemble.isAnyActive());
}

TEST(KarplusStrongEnsemble, summedOutputStaysBoundedAndFiniteWithAllVoicesPlucked)
{
    TestEnsemble ensemble{kSampleRate};
    for (size_t i = 0; i < TestEnsemble::numVoices(); ++i)
    {
        ensemble.voice(i).setPluckType(PluckType::WhiteRoundRobin);
        ensemble.voice(i).trigger(48.f + static_cast<float>(i) * 3.f, 1.f);
    }
    for (int i = 0; i < 20000; ++i)
    {
        const float v = ensemble.step();
        ASSERT_TRUE(std::isfinite(v));
        ASSERT_LE(std::abs(v), static_cast<float>(TestEnsemble::numVoices()) * 4.f);
    }
}

TEST(KarplusStrongEnsemble, unpluckedVoicesContributeSilence)
{
    TestEnsemble ensemble{kSampleRate};
    ensemble.voice(0).setPluckType(PluckType::WhiteStatic);
    ensemble.voice(0).trigger(60.f, 1.f);

    for (int i = 0; i < 100; ++i)
    {
        const float ensembleOut = ensemble.step();
        ASSERT_TRUE(std::isfinite(ensembleOut));
    }
    EXPECT_TRUE(ensemble.isAnyActive());
}

TEST(KarplusStrongEnsemble, detunedVoicesProduceDifferentOutputThanIdenticalVoices)
{
    TestEnsemble identical{kSampleRate};
    for (size_t i = 0; i < TestEnsemble::numVoices(); ++i)
    {
        identical.voice(i).setPluckType(PluckType::WhiteStatic);
        identical.voice(i).trigger(60.f, 1.f);
    }

    TestEnsemble detuned{kSampleRate};
    for (size_t i = 0; i < TestEnsemble::numVoices(); ++i)
    {
        detuned.voice(i).setPluckType(PluckType::WhiteStatic);
        detuned.voice(i).trigger(60.f, 1.f);
        detuned.voice(i).bendInCents(static_cast<float>(i) * 7.f);
    }

    bool anyDifferent = false;
    for (int i = 0; i < 5000; ++i)
    {
        if (identical.step() != detuned.step())
        {
            anyDifferent = true;
        }
    }
    EXPECT_TRUE(anyDifferent);
}

}
