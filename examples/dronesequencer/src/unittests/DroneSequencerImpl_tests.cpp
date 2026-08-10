#include <gtest/gtest.h>

#include "Audio/AudioBuffer.h"
#include "impl/DroneSequencerImpl.h"

namespace
{
constexpr float kSampleRate{48000.f};
constexpr size_t kBlockSize{16};
using TestImpl = DroneSequencerImpl<kBlockSize>;

void processOneBlock(TestImpl& impl)
{
    const AbacDsp::AudioBuffer<2, kBlockSize> in{};
    AbacDsp::AudioBuffer<2, kBlockSize> out{};
    impl.processBlock(in, out);
}
}

TEST(DroneSequencerImpl, ChangedPoolParameterReachesCallbackWithinOneBlock)
{
    TestImpl impl(kSampleRate);
    ASSERT_TRUE(impl.setScript(R"(
        UICreateParameterSet({
            { id = "cutoff", type = "knob", range = { min = 0, max = 1, step = 0, skew = 1 }, default = 0 },
        })
        Confirmed = false
        function OnCutoffChanged(value)
            if math.abs(value - 0.75) < 1e-5 then
                Confirmed = true
            end
        end
        function NextNotes()
            if not Confirmed then
                error("OnCutoffChanged did not fire with the expected value before NextNotes")
            end
            return {}
        end
    )"));

    impl.setLuaParam1(0.75f);
    processOneBlock(impl);

    EXPECT_FALSE(impl.hasScriptError()) << impl.scriptError();
}

TEST(DroneSequencerImpl, UnchangedPoolParameterDoesNotRefireCallback)
{
    TestImpl impl(kSampleRate);
    ASSERT_TRUE(impl.setScript(R"(
        UICreateParameterSet({
            { id = "cutoff", type = "knob", range = { min = 0, max = 1, step = 0, skew = 1 }, default = 0 },
        })
        Calls = 0
        function OnCutoffChanged(value)
            Calls = Calls + 1
        end
        function NextNotes()
            if Calls > 1 then
                error("OnCutoffChanged fired more than once for an unchanged value")
            end
            return {}
        end
    )"));

    impl.setLuaParam1(0.75f);
    processOneBlock(impl);
    ASSERT_FALSE(impl.hasScriptError()) << impl.scriptError();

    processOneBlock(impl);
    EXPECT_FALSE(impl.hasScriptError()) << impl.scriptError();
}

// Regression: reloading a script must resend every claimed slot's current value once,
// even though nothing changed - a fresh script's globals reset on reload.
TEST(DroneSequencerImpl, ReloadingScriptResendsCurrentParameterValues)
{
    TestImpl impl(kSampleRate);
    const auto script = R"(
        UICreateParameterSet({
            { id = "cutoff", type = "knob", range = { min = 0, max = 1, step = 0, skew = 1 }, default = 0 },
        })
        LastCutoff = -1
        function OnCutoffChanged(value)
            LastCutoff = value
        end
        function NextNotes()
            if LastCutoff < 0 then
                error("LastCutoff was never resent after the script reloaded")
            end
            return {}
        end
    )";
    ASSERT_TRUE(impl.setScript(script));
    impl.setLuaParam1(0.75f);
    processOneBlock(impl);
    ASSERT_FALSE(impl.hasScriptError()) << impl.scriptError();

    // Reload the identical script (simulates Apply): LastCutoff resets to -1 on load.
    ASSERT_TRUE(impl.setScript(script));
    processOneBlock(impl);
    EXPECT_FALSE(impl.hasScriptError()) << impl.scriptError();
}

TEST(DroneSequencerImpl, UiParamSlotsReflectsClaimedScriptParameters)
{
    TestImpl impl(kSampleRate);
    ASSERT_TRUE(impl.setScript(R"(
        UICreateParameterSet({
            { id = "cutoff", name = "Cutoff", type = "knob",
              range = { min = 0, max = 1, step = 0, skew = 1 }, default = 0 },
        })
        function NextNotes() return {} end
    )"));

    const auto& slots = impl.uiParamSlots();
    EXPECT_TRUE(slots[0].claimed);
    EXPECT_EQ(slots[0].id, "cutoff");
    for (size_t i = 1; i < slots.size(); ++i)
    {
        EXPECT_FALSE(slots[i].claimed);
    }
}
