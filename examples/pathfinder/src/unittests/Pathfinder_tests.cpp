#include <array>
#include <cmath>
#include <random>
#include <string>
#include <vector>

#include "gtest/gtest.h"

#include "Audio/AudioBuffer.h"
#include "impl/PathfinderImpl.h"

namespace
{

constexpr size_t BlockSize{16};
constexpr float SampleRate{48000.f};
constexpr size_t kSettleBlocks{300}; // 4800 samples: well past the 30 ms fade.

using Impl = PathfinderImpl<BlockSize>;

void feedNoiseBlocks(Impl& impl, const size_t numBlocks, std::mt19937& rng)
{
    std::uniform_real_distribution<float> dist{-1.f, 1.f};
    AbacDsp::AudioBuffer<2, BlockSize> in{};
    AbacDsp::AudioBuffer<2, BlockSize> out{};
    for (size_t b = 0; b < numBlocks; ++b)
    {
        for (size_t i = 0; i < BlockSize; ++i)
        {
            in(i, 0) = dist(rng);
            in(i, 1) = dist(rng);
        }
        impl.processBlock(in, out);
        for (size_t i = 0; i < BlockSize; ++i)
        {
            ASSERT_TRUE(std::isfinite(out(i, 0)));
            ASSERT_TRUE(std::isfinite(out(i, 1)));
            ASSERT_LT(std::abs(out(i, 0)), 10.f);
            ASSERT_LT(std::abs(out(i, 1)), 10.f);
        }
    }
}

// Left output of the last processed block for a constant input on both channels.
float settledLevel(Impl& impl, const float input, const size_t numBlocks = kSettleBlocks)
{
    AbacDsp::AudioBuffer<2, BlockSize> in{};
    AbacDsp::AudioBuffer<2, BlockSize> out{};
    for (size_t i = 0; i < BlockSize; ++i)
    {
        in(i, 0) = input;
        in(i, 1) = input;
    }
    for (size_t b = 0; b < numBlocks; ++b)
    {
        impl.processBlock(in, out);
    }
    return out(BlockSize - 1, 0);
}

std::string gainScript(const std::string& gainDb, const std::string& extra = "")
{
    return "return {\n"
           "  io = { inputs = { \"inL\", \"inR\" }, outputs = { \"outL\", \"outR\" } },\n"
           "  nodes = { { id = \"g\", type = \"Gain\", params = { gainDb = " +
           gainDb +
           " } } },\n"
           "  edges = {\n"
           "    { from = \"inL\", to = \"g.inL\" }, { from = \"inR\", to = \"g.inR\" },\n"
           "    { from = \"g.outL\", to = \"outL\" }, { from = \"g.outR\", to = \"outR\" },\n"
           "  },\n" +
           extra + "}\n";
}

// Two Gain stages in series: knob 1 ("coarse", -60 to 0 dB) drives the first, knob 2 ("fine") the second.
const std::string kMacroScript = R"lua(
return {
  io = { inputs = { "inL", "inR" }, outputs = { "outL", "outR" } },
  nodes = {
    { id = "a", type = "Gain" },
    { id = "b", type = "Gain" },
  },
  edges = {
    { from = "inL", to = "a.inL" }, { from = "inR", to = "a.inR" },
    { from = "a.outL", to = "b.inL" }, { from = "a.outR", to = "b.inR" },
    { from = "b.outL", to = "outL" }, { from = "b.outR", to = "outR" },
  },
  macros = {
    { id = "coarse", label = "Coarse", unit = "dB", min = -60, max = 0, default = -6,
      targets = { { to = "a.gainDb", min = -60, max = 0 } } },
    { id = "fine", label = "Fine", default = 0.5, targets = { { to = "b.gainDb", min = -60, max = 0 } } },
  },
}
)lua";

} // namespace

TEST(PathfinderImplTest, StaysFiniteAcrossFullKnobSweep)
{
    std::mt19937 rng{1};
    Impl impl{SampleRate};
    for (const float knob : {0.f, 0.25f, 0.5f, 0.75f, 1.f})
    {
        impl.setLuaParam1(knob);
        impl.setLuaParam2(knob);
        impl.setLuaParam3(knob);
        impl.setLuaParam4(knob);
        feedNoiseBlocks(impl, 50, rng);
    }
}

TEST(PathfinderImplTest, CharacterExtremesStayFiniteWithActiveModulation)
{
    std::mt19937 rng{2};
    for (const float character : {0.f, 0.25f, 0.5f, 0.75f, 1.f})
    {
        Impl impl{SampleRate};
        impl.setLuaParam1(1.f);
        impl.setLuaParam2(0.5f);
        impl.setLuaParam3(1.f);
        impl.setLuaParam4(character);
        feedNoiseBlocks(impl, 100, rng);
    }
}

// With the modulation knobs at zero the default graph is a plain 10 ms delay plus about 68
// samples of sampler latency, so an impulse must surface there and not at zero.
TEST(PathfinderImplTest, ImpulseEnergySurfacesNearTheBaseDelayWithModulationOff)
{
    Impl impl{SampleRate};
    impl.setLuaParam1(0.f);
    impl.setLuaParam3(0.f);
    impl.setLuaParam4(0.5f);

    AbacDsp::AudioBuffer<2, BlockSize> in{};
    AbacDsp::AudioBuffer<2, BlockSize> out{};
    in(0, 0) = 1.f;
    in(0, 1) = 1.f;
    impl.processBlock(in, out);
    in(0, 0) = 0.f;
    in(0, 1) = 0.f;

    size_t peakSample = 0;
    float peakValue = 0.f;
    for (size_t b = 0; b < 1024 / BlockSize; ++b)
    {
        impl.processBlock(in, out);
        for (size_t i = 0; i < BlockSize; ++i)
        {
            if (const auto magnitude = std::abs(out(i, 0)); magnitude > peakValue)
            {
                peakValue = magnitude;
                peakSample = b * BlockSize + i;
            }
        }
    }
    EXPECT_GT(peakSample, 480u);
    EXPECT_LT(peakSample, 640u);
}

TEST(PathfinderScriptTest, TheSkeletonIsTheDefaultGraphAndApplies)
{
    Impl impl{SampleRate};
    EXPECT_TRUE(impl.setScript(Impl::scriptSkeleton()));
    EXPECT_FALSE(impl.hasScriptError());
}

TEST(PathfinderScriptTest, AppliedScriptReplacesTheGraphAfterTheFade)
{
    Impl impl{SampleRate};
    ASSERT_TRUE(impl.setScript(gainScript("-6.0")));
    EXPECT_NEAR(settledLevel(impl, 0.5f), 0.5f * std::pow(10.f, -6.f / 20.f), 1E-5f);
}

TEST(PathfinderScriptTest, AFailingScriptLeavesTheRunningGraphUntouchedAndNamesTheLine)
{
    std::mt19937 first{7};
    std::mt19937 second{7};
    Impl touched{SampleRate};
    Impl reference{SampleRate};
    feedNoiseBlocks(touched, 40, first);
    feedNoiseBlocks(reference, 40, second);

    const std::string broken = "return {\n"
                               "  io = { inputs = { \"inL\", \"inR\" }, outputs = { \"outL\", \"outR\" } },\n"
                               "  nodes = {\n"
                               "    { id = \"x\", type = \"NoSuchNode\" },\n"
                               "  },\n"
                               "}\n";
    EXPECT_FALSE(touched.setScript(broken));
    ASSERT_TRUE(touched.hasScriptError());
    EXPECT_NE(touched.scriptError().find("line 4"), std::string::npos) << touched.scriptError();
    EXPECT_NE(touched.scriptError().find("x"), std::string::npos);

    AbacDsp::AudioBuffer<2, BlockSize> inA{};
    AbacDsp::AudioBuffer<2, BlockSize> inB{};
    AbacDsp::AudioBuffer<2, BlockSize> outA{};
    AbacDsp::AudioBuffer<2, BlockSize> outB{};
    std::uniform_real_distribution<float> dist{-1.f, 1.f};
    for (size_t b = 0; b < 100; ++b)
    {
        for (size_t i = 0; i < BlockSize; ++i)
        {
            inA(i, 0) = inB(i, 0) = inA(i, 1) = inB(i, 1) = dist(first);
        }
        touched.processBlock(inA, outA);
        reference.processBlock(inA, outB);
        for (size_t i = 0; i < BlockSize; ++i)
        {
            ASSERT_EQ(outA(i, 0), outB(i, 0)) << "block " << b << " sample " << i;
        }
    }
}

TEST(PathfinderScriptTest, AGoodScriptClearsThePreviousError)
{
    Impl impl{SampleRate};
    EXPECT_FALSE(impl.setScript("this is not lua"));
    EXPECT_TRUE(impl.hasScriptError());
    EXPECT_TRUE(impl.setScript(gainScript("0.0")));
    EXPECT_FALSE(impl.hasScriptError());
    EXPECT_TRUE(impl.scriptError().empty());
}

TEST(PathfinderScriptTest, AGraphWithTheWrongShapeIsRejected)
{
    Impl impl{SampleRate};
    const std::string mono = "return { io = { inputs = { \"in\" }, outputs = { \"out\" } } }";
    EXPECT_FALSE(impl.setScript(mono));
    EXPECT_NE(impl.scriptError().find("2 inputs and 2 outputs"), std::string::npos) << impl.scriptError();
}

TEST(PathfinderScriptTest, ARunawayScriptIsRejectedInsteadOfHangingTheApplication)
{
    Impl impl{SampleRate};
    EXPECT_FALSE(impl.setScript("while true do end"));
    EXPECT_NE(impl.scriptError().find("instruction budget"), std::string::npos) << impl.scriptError();
}

TEST(PathfinderScriptTest, SizeAndNodeLimitsAreEnforced)
{
    Impl impl{SampleRate};
    const std::string huge = gainScript("0.0", "-- " + std::string(Impl::Engine::kMaxScriptBytes, 'x') + "\n");
    EXPECT_FALSE(impl.setScript(huge));
    EXPECT_NE(impl.scriptError().find("KB"), std::string::npos) << impl.scriptError();

    std::string nodes;
    for (size_t i = 0; i <= Impl::Engine::kMaxNodes; ++i)
    {
        nodes += "{ id = \"n" + std::to_string(i) + "\", type = \"Gain\" },";
    }
    const std::string many =
        "return { io = { inputs = { \"inL\", \"inR\" }, outputs = { \"outL\", \"outR\" } }, nodes = {" + nodes + "} }";
    EXPECT_FALSE(impl.setScript(many));
    EXPECT_NE(impl.scriptError().find("nodes"), std::string::npos) << impl.scriptError();
}

TEST(PathfinderScriptTest, KnobsDriveTheScriptsMacrosInDeclarationOrder)
{
    Impl impl{SampleRate};
    ASSERT_TRUE(impl.setScript(kMacroScript));

    impl.setLuaParam1(1.f);
    impl.setLuaParam2(1.f);
    EXPECT_NEAR(settledLevel(impl, 0.5f), 0.5f, 1E-4f);

    impl.setLuaParam1(0.5f); // -30 dB
    EXPECT_NEAR(settledLevel(impl, 0.5f), 0.5f * std::pow(10.f, -30.f / 20.f), 1E-4f);

    impl.setLuaParam2(0.5f); // a further -30 dB
    EXPECT_NEAR(settledLevel(impl, 0.5f), 0.5f * std::pow(10.f, -60.f / 20.f), 1E-5f);
}

TEST(PathfinderScriptTest, KnobSlotsCarryTheMacrosLabelUnitRangeAndDefault)
{
    Impl impl{SampleRate};
    ASSERT_TRUE(impl.setScript(kMacroScript));
    const auto slots = impl.uiParamSlots();
    ASSERT_TRUE(slots[0].claimed);
    EXPECT_EQ(slots[0].id, "coarse");
    EXPECT_EQ(slots[0].name, "Coarse");
    EXPECT_EQ(slots[0].unit, "dB");
    EXPECT_FLOAT_EQ(slots[0].rangeMin, -60.f);
    EXPECT_FLOAT_EQ(slots[0].rangeMax, 0.f);
    EXPECT_FLOAT_EQ(slots[0].defaultValue, -6.f);

    ASSERT_TRUE(slots[1].claimed);
    EXPECT_EQ(slots[1].name, "Fine");
    EXPECT_TRUE(slots[1].unit.empty());
    EXPECT_FLOAT_EQ(slots[1].rangeMin, 0.f);
    EXPECT_FLOAT_EQ(slots[1].rangeMax, 1.f);
    for (size_t i = 2; i < slots.size(); ++i)
    {
        EXPECT_FALSE(slots[i].claimed) << i;
    }
}

TEST(PathfinderScriptTest, ASecondScriptReplacesTheKnobsTheFirstOffered)
{
    Impl impl{SampleRate};
    ASSERT_TRUE(impl.setScript(kMacroScript));
    ASSERT_TRUE(impl.uiParamSlots()[1].claimed);

    ASSERT_TRUE(impl.setScript(gainScript("0.0")));
    for (const auto& slot : impl.uiParamSlots())
    {
        EXPECT_FALSE(slot.claimed);
    }
}

TEST(PathfinderScriptTest, MoreMacrosThanKnobsLeavesTheExtrasOutWithAWarning)
{
    Impl impl{SampleRate};
    std::string macros;
    for (size_t i = 0; i < Impl::Engine::kKnobCount + 2; ++i)
    {
        macros += "{ id = \"m" + std::to_string(i) + "\", targets = { { to = \"g.gainDb\" } } },";
    }
    ASSERT_TRUE(impl.setScript(gainScript("0.0", "macros = {" + macros + "},\n")));
    size_t claimed = 0;
    for (const auto& slot : impl.uiParamSlots())
    {
        claimed += slot.claimed ? 1 : 0;
    }
    EXPECT_EQ(claimed, Impl::Engine::kKnobCount);
    EXPECT_NE(impl.scriptWarnings().find("no free knob slot"), std::string::npos) << impl.scriptWarnings();
}

TEST(PathfinderScriptTest, TheDefaultScriptOffersDepthSpeedAggressivityAndCharacterAsKnobs)
{
    Impl impl{SampleRate};
    const auto slots = impl.uiParamSlots();
    const std::array<std::string, 4> expectedIds{"depth", "speed", "aggressivity", "character"};
    for (size_t i = 0; i < expectedIds.size(); ++i)
    {
        ASSERT_TRUE(slots[i].claimed) << i;
        EXPECT_EQ(slots[i].id, expectedIds[i]);
        EXPECT_FALSE(slots[i].unit.empty()) << expectedIds[i];
    }
    EXPECT_FALSE(slots[4].claimed);
}

// The blueprint gives the first knob parameters their starting raw value (its "defaults" list for
// luaControls), so a new instance starts on the default script's declared defaults. Keep both in step.
TEST(PathfinderScriptTest, TheBlueprintsKnobDefaultsMatchTheDefaultScriptsMacroDefaults)
{
    Impl impl{SampleRate};
    const std::array<float, 4> blueprintDefaults{0.65f, 0.44444f, 0.1f, 0.5f};
    const auto slots = impl.uiParamSlots();
    for (size_t i = 0; i < blueprintDefaults.size(); ++i)
    {
        const float normalized = (slots[i].defaultValue - slots[i].rangeMin) / (slots[i].rangeMax - slots[i].rangeMin);
        EXPECT_NEAR(normalized, blueprintDefaults[i], 1E-4f) << slots[i].id;
    }
}

TEST(PathfinderScriptTest, ARetiredGraphIsCollectedOnceAfterTheSwapCompletes)
{
    Impl impl{SampleRate};
    ASSERT_TRUE(impl.setScript(gainScript("0.0")));
    EXPECT_FALSE(impl.collectRetired()); // still fading: nothing retired yet
    static_cast<void>(settledLevel(impl, 0.5f));
    EXPECT_TRUE(impl.collectRetired());
    EXPECT_FALSE(impl.collectRetired());
}

TEST(PathfinderScriptTest, ASecondScriptReplacesAFirstThatWasNotAdoptedYet)
{
    Impl impl{SampleRate};
    ASSERT_TRUE(impl.setScript(gainScript("-6.0")));
    ASSERT_TRUE(impl.setScript(gainScript("-12.0")));
    EXPECT_NEAR(settledLevel(impl, 0.5f), 0.5f * std::pow(10.f, -12.f / 20.f), 1E-5f);
}

TEST(PathfinderScriptTest, AScriptAppliedAfterAFinishedSwapIsAdoptedWithoutAnExplicitCollect)
{
    Impl impl{SampleRate};
    ASSERT_TRUE(impl.setScript(gainScript("-6.0")));
    static_cast<void>(settledLevel(impl, 0.5f)); // the first swap is done, its old graph retired
    ASSERT_TRUE(impl.setScript(gainScript("-12.0")));
    EXPECT_NEAR(settledLevel(impl, 0.5f), 0.5f * std::pow(10.f, -12.f / 20.f), 1E-5f);
}
