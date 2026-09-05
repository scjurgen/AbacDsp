#include <cstddef>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <sstream>

#include "gtest/gtest.h"

#include "impl/MorphexsynthScriptEngine.h"

namespace
{
std::string readFile(const std::string& path)
{
    std::ifstream f(path);
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

// Resolves only "constants" (see base-scripts/constants.lua), reading it straight off
// disk - a minimal stand-in for the real app's FileIo::resolveLibraryScript.
MorphexsynthScriptEngine::ImportResolver testImportResolver()
{
    return [](const std::string_view name) -> ImportLookup
    {
        if (name != "constants")
        {
            return {std::nullopt, "test resolver only knows \"constants\""};
        }
        const auto source = readFile(std::string(MORPHEXSYNTH_BASE_SCRIPTS_DIR) + "/constants.lua");
        return {source, {}};
    };
}
}

TEST(MorphexsynthScriptEngineBaseScripts, MpeExpressiveLeadLoadsWithoutError)
{
    MorphexsynthScriptEngine engine;
    engine.setImportResolver(testImportResolver());
    const auto source = readFile(std::string(MORPHEXSYNTH_BASE_SCRIPTS_DIR) + "/mpe-expressive-lead.lua");
    ASSERT_FALSE(source.empty());
    EXPECT_TRUE(engine.loadScript(source)) << engine.lastError();
}

TEST(MorphexsynthScriptEngineBaseScripts, WobbleBassLoadsWithoutError)
{
    MorphexsynthScriptEngine engine;
    engine.setImportResolver(testImportResolver());
    const auto source = readFile(std::string(MORPHEXSYNTH_BASE_SCRIPTS_DIR) + "/wobble-bass.lua");
    ASSERT_FALSE(source.empty());
    EXPECT_TRUE(engine.loadScript(source)) << engine.lastError();
}

TEST(MorphexsynthScriptEngineBaseScripts, ConstantsLibraryResolvesToDocumentedValues)
{
    MorphexsynthScriptEngine engine;
    engine.setImportResolver(testImportResolver());
    ASSERT_TRUE(engine.loadScript("import \"constants\"\n"
                                  "function OnStart()\n"
                                  "    SetCtrlSlot(0, { source = CtrlSource.EnvelopeFilter, curve = CtrlCurve.Linear,\n"
                                  "                     target = CtrlTarget.FilterCutoff, valueType = "
                                  "CtrlValueType.Abs, depth = 0.5 })\n"
                                  "end\n"))
        << engine.lastError();
    engine.notifyStart();

    const auto command = engine.drainCtrlSlotCommand(0);
    ASSERT_TRUE(command.has_value());
    EXPECT_EQ(command->source, 5u);
    EXPECT_EQ(command->curve, 2u);
    EXPECT_EQ(command->target, 3u);
    EXPECT_EQ(command->valueType, 0u);
}

// Each factory-patches/*.json's own "script" field must load without a parse/runtime
// error; catches Lua syntax or API-name mistakes, not whether a patch sounds right.
TEST(MorphexsynthScriptEngineFactoryPatches, AllFactoryPatchesLoadWithoutError)
{
    const std::filesystem::path dir{MORPHEXSYNTH_FACTORY_PATCHES_DIR};
    ASSERT_TRUE(std::filesystem::exists(dir)) << dir;

    size_t checked = 0;
    for (const auto& entry : std::filesystem::directory_iterator(dir))
    {
        if (entry.path().extension() != ".json")
        {
            continue;
        }
        const auto raw = readFile(entry.path().string());
        ASSERT_FALSE(raw.empty()) << entry.path();
        const auto patch = nlohmann::json::parse(raw, nullptr, false);
        ASSERT_FALSE(patch.is_discarded()) << "invalid JSON: " << entry.path();
        ASSERT_TRUE(patch.contains("script")) << entry.path();

        MorphexsynthScriptEngine engine;
        engine.setImportResolver(testImportResolver());
        ASSERT_TRUE(engine.loadScript(patch["script"].get<std::string>()))
            << entry.path() << ": " << engine.lastError();
        // Also runs OnStart(), not just compiles it - a call to a misspelled/nonexistent
        // Lua function only ever surfaces here, since loadScript() alone never executes it.
        engine.notifyStart();
        EXPECT_FALSE(engine.hasError()) << entry.path() << ": " << engine.lastError();
        ++checked;
    }
    EXPECT_GE(checked, 1u) << "no factory patches found in " << dir;
}

TEST(MorphexsynthScriptEngine, NoCommandsPendingByDefault)
{
    MorphexsynthScriptEngine engine;
    EXPECT_FALSE(engine.drainOscillatorCommand(0).has_value());
    EXPECT_FALSE(engine.drainAmpEnvelopeCommand().has_value());
    EXPECT_FALSE(engine.drainFilterEnvelopeCommand().has_value());
    EXPECT_FALSE(engine.drainPitchEnvelopeCommand().has_value());
    EXPECT_FALSE(engine.drainLfoCommand().has_value());
    EXPECT_FALSE(engine.drainFilterCommand().has_value());
    EXPECT_FALSE(engine.drainDistortionCommand().has_value());
    EXPECT_FALSE(engine.drainCtrlSlotCommand(0).has_value());
    EXPECT_FALSE(engine.drainMpeZoneCommand().has_value());
}

TEST(MorphexsynthScriptEngine, StubScriptLoadsWithoutError)
{
    MorphexsynthScriptEngine engine;
    EXPECT_FALSE(engine.hasError());
}

TEST(MorphexsynthScriptEngine, SetOscillatorIsDrainedOnceThenClears)
{
    MorphexsynthScriptEngine engine;
    ASSERT_TRUE(engine.loadScript(
        "SetOscillator(1, { waveform = 2, detune = 3.5, level = 0.8, pwm = -0.5, pitchFactor = 2 })"));

    const auto command = engine.drainOscillatorCommand(1);
    ASSERT_TRUE(command.has_value());
    EXPECT_EQ(command->waveform, 2u);
    EXPECT_FLOAT_EQ(command->detune, 3.5f);
    EXPECT_FLOAT_EQ(command->level, 0.8f);
    EXPECT_FLOAT_EQ(command->pwm, -0.5f);
    EXPECT_FLOAT_EQ(command->pitchFactor, 2.f);
    EXPECT_FALSE(engine.drainOscillatorCommand(1).has_value());
}

TEST(MorphexsynthScriptEngine, SetOscillatorRejectsOutOfRangeIndexAndWaveform)
{
    MorphexsynthScriptEngine engine;
    ASSERT_TRUE(engine.loadScript("SetOscillator(9, { waveform = 0 })"));
    EXPECT_FALSE(engine.drainOscillatorCommand(0).has_value());

    ASSERT_TRUE(engine.loadScript("SetOscillator(0, { waveform = 99 })"));
    EXPECT_FALSE(engine.drainOscillatorCommand(0).has_value());
}

TEST(MorphexsynthScriptEngine, SetOscillatorUsesDefaultsForOmittedFields)
{
    MorphexsynthScriptEngine engine;
    ASSERT_TRUE(engine.loadScript("SetOscillator(0, {})"));

    const auto command = engine.drainOscillatorCommand(0);
    ASSERT_TRUE(command.has_value());
    EXPECT_EQ(command->waveform, 0u);
    EXPECT_FLOAT_EQ(command->detune, 0.f);
    EXPECT_FLOAT_EQ(command->level, 0.f);
    EXPECT_FLOAT_EQ(command->pitchFactor, 1.f);
}

TEST(MorphexsynthScriptEngine, SetAmpEnvelopeClampsSustainToUnitRange)
{
    MorphexsynthScriptEngine engine;
    ASSERT_TRUE(
        engine.loadScript("SetAmpEnvelope({ attackMs = 5, decayMs = 200, sustainLevel = 4, releaseMs = 300 })"));

    const auto command = engine.drainAmpEnvelopeCommand();
    ASSERT_TRUE(command.has_value());
    EXPECT_FLOAT_EQ(command->attackMs, 5.f);
    EXPECT_FLOAT_EQ(command->decayMs, 200.f);
    EXPECT_FLOAT_EQ(command->sustainLevel, 1.f);
    EXPECT_FLOAT_EQ(command->releaseMs, 300.f);
}

TEST(MorphexsynthScriptEngine, SetFilterEnvelopeIsDrainedOnceThenClears)
{
    MorphexsynthScriptEngine engine;
    ASSERT_TRUE(engine.loadScript(
        "SetFilterEnvelope({ attackMs = 1, decayMs = 2, sustainLevel = 0.3, releaseMs = 4, contour = 1.5 })"));

    const auto command = engine.drainFilterEnvelopeCommand();
    ASSERT_TRUE(command.has_value());
    EXPECT_FLOAT_EQ(command->sustainLevel, 0.3f);
    EXPECT_FLOAT_EQ(command->contour, 1.5f);
    EXPECT_FALSE(engine.drainFilterEnvelopeCommand().has_value());
}

TEST(MorphexsynthScriptEngine, SetFilterEnvelopeClampsContourToItsRange)
{
    MorphexsynthScriptEngine engine;
    ASSERT_TRUE(engine.loadScript("SetFilterEnvelope({ contour = 99 })"));

    const auto command = engine.drainFilterEnvelopeCommand();
    ASSERT_TRUE(command.has_value());
    EXPECT_FLOAT_EQ(command->contour, 4.f);

    ASSERT_TRUE(engine.loadScript("SetFilterEnvelope({ contour = -99 })"));
    const auto negativeCommand = engine.drainFilterEnvelopeCommand();
    ASSERT_TRUE(negativeCommand.has_value());
    EXPECT_FLOAT_EQ(negativeCommand->contour, -4.f);
}

TEST(MorphexsynthScriptEngine, SetPitchEnvelopeIsDrainedOnceThenClears)
{
    MorphexsynthScriptEngine engine;
    ASSERT_TRUE(engine.loadScript(
        "SetPitchEnvelope({ attackMs = 10, decayMs = 50, depthSemitones = 12, glideMsPerOctave = 80 })"));

    const auto command = engine.drainPitchEnvelopeCommand();
    ASSERT_TRUE(command.has_value());
    EXPECT_FLOAT_EQ(command->depthSemitones, 12.f);
    EXPECT_FLOAT_EQ(command->glideMsPerOctave, 80.f);
    EXPECT_FALSE(engine.drainPitchEnvelopeCommand().has_value());
}

TEST(MorphexsynthScriptEngine, SetLfoRejectsOutOfRangeWaveform)
{
    MorphexsynthScriptEngine engine;
    ASSERT_TRUE(engine.loadScript("SetLfo({ waveform = 99, speedHz = 2 })"));
    EXPECT_FALSE(engine.drainLfoCommand().has_value());
}

TEST(MorphexsynthScriptEngine, SetLfoIsDrainedOnceThenClears)
{
    MorphexsynthScriptEngine engine;
    ASSERT_TRUE(engine.loadScript(
        "SetLfo({ waveform = 3, speedHz = 4, filterDepth = 0.5, oscDepth = 0.7, keyFollow = 0.25 })"));

    const auto command = engine.drainLfoCommand();
    ASSERT_TRUE(command.has_value());
    EXPECT_EQ(command->waveform, 3u);
    EXPECT_FLOAT_EQ(command->speedHz, 4.f);
    EXPECT_FLOAT_EQ(command->filterDepth, 0.5f);
    EXPECT_FLOAT_EQ(command->oscDepth, 0.7f);
    EXPECT_FLOAT_EQ(command->keyFollow, 0.25f);
    EXPECT_FALSE(engine.drainLfoCommand().has_value());
}

TEST(MorphexsynthScriptEngine, SetFilterIsDrainedOnceThenClears)
{
    MorphexsynthScriptEngine engine;
    ASSERT_TRUE(engine.loadScript("SetFilter({ cutoff = 64, resonance = 0.9, type = \"HP2\" })"));

    const auto command = engine.drainFilterCommand();
    ASSERT_TRUE(command.has_value());
    EXPECT_FLOAT_EQ(command->cutoff, 64.f);
    EXPECT_FLOAT_EQ(command->resonance, 0.9f);
    EXPECT_EQ(command->type, "HP2");
    EXPECT_FALSE(engine.drainFilterCommand().has_value());
}

TEST(MorphexsynthScriptEngine, SetDistortionIsDrainedOnceThenClears)
{
    MorphexsynthScriptEngine engine;
    ASSERT_TRUE(engine.loadScript("SetDistortion(5)"));

    const auto command = engine.drainDistortionCommand();
    ASSERT_TRUE(command.has_value());
    EXPECT_EQ(*command, 5u);
    EXPECT_FALSE(engine.drainDistortionCommand().has_value());
}

TEST(MorphexsynthScriptEngine, SetCtrlSlotIsDrainedOnceThenClears)
{
    MorphexsynthScriptEngine engine;
    ASSERT_TRUE(engine.loadScript("SetCtrlSlot(3, { source = 1, curve = 4, target = 3, valueType = 1, depth = 0.7 })"));

    const auto command = engine.drainCtrlSlotCommand(3);
    ASSERT_TRUE(command.has_value());
    EXPECT_EQ(command->source, 1u);
    EXPECT_EQ(command->curve, 4u);
    EXPECT_EQ(command->target, 3u);
    EXPECT_EQ(command->valueType, 1u);
    EXPECT_FLOAT_EQ(command->depth, 0.7f);
    EXPECT_FALSE(engine.drainCtrlSlotCommand(3).has_value());
}

TEST(MorphexsynthScriptEngine, SetCtrlSlotRejectsOutOfRangeSlotAndEnums)
{
    MorphexsynthScriptEngine engine;
    ASSERT_TRUE(engine.loadScript("SetCtrlSlot(99, { source = 0 })"));
    EXPECT_FALSE(engine.drainCtrlSlotCommand(0).has_value());

    ASSERT_TRUE(engine.loadScript("SetCtrlSlot(0, { target = 999 })"));
    EXPECT_FALSE(engine.drainCtrlSlotCommand(0).has_value());
}

TEST(MorphexsynthScriptEngine, SetMpeZoneIsDrainedOnceThenClears)
{
    MorphexsynthScriptEngine engine;
    ASSERT_TRUE(engine.loadScript("SetMpeZone(1, 2, 8)"));

    const auto command = engine.drainMpeZoneCommand();
    ASSERT_TRUE(command.has_value());
    EXPECT_EQ(command->master, 1);
    EXPECT_EQ(command->lower, 2);
    EXPECT_EQ(command->upper, 8);
    EXPECT_FALSE(engine.drainMpeZoneCommand().has_value());
}

TEST(MorphexsynthScriptEngine, SetMpeZoneRejectsOutOfRangeChannels)
{
    MorphexsynthScriptEngine engine;
    ASSERT_TRUE(engine.loadScript("SetMpeZone(0, 2, 17)"));
    EXPECT_FALSE(engine.drainMpeZoneCommand().has_value());
}

TEST(MorphexsynthScriptEngine, SetPhaserIsDrainedOnceThenClears)
{
    MorphexsynthScriptEngine engine;
    ASSERT_TRUE(engine.loadScript("SetPhaser({ rateHz = 0.5, depth = 0.8, feedback = 0.3, mix = 0.7 })"));

    const auto command = engine.drainPhaserCommand();
    ASSERT_TRUE(command.has_value());
    EXPECT_FLOAT_EQ(command->rateHz, 0.5f);
    EXPECT_FLOAT_EQ(command->depth, 0.8f);
    EXPECT_FLOAT_EQ(command->feedback, 0.3f);
    EXPECT_FLOAT_EQ(command->mix, 0.7f);
    EXPECT_FALSE(engine.drainPhaserCommand().has_value());
}

TEST(MorphexsynthScriptEngine, SetPhaserClampsOutOfRangeFields)
{
    MorphexsynthScriptEngine engine;
    ASSERT_TRUE(engine.loadScript("SetPhaser({ rateHz = 50, depth = 5, feedback = 9, mix = -3 })"));

    const auto command = engine.drainPhaserCommand();
    ASSERT_TRUE(command.has_value());
    EXPECT_FLOAT_EQ(command->rateHz, 10.f);
    EXPECT_FLOAT_EQ(command->depth, 1.f);
    EXPECT_FLOAT_EQ(command->feedback, 1.1f);
    EXPECT_FLOAT_EQ(command->mix, 0.f);
}

TEST(MorphexsynthScriptEngine, SetChorusIsDrainedOnceThenClears)
{
    MorphexsynthScriptEngine engine;
    ASSERT_TRUE(engine.loadScript("SetChorus({ rateHz = 1.2, depth = 0.4, mix = 0.6 })"));

    const auto command = engine.drainChorusCommand();
    ASSERT_TRUE(command.has_value());
    EXPECT_FLOAT_EQ(command->rateHz, 1.2f);
    EXPECT_FLOAT_EQ(command->depth, 0.4f);
    EXPECT_FLOAT_EQ(command->mix, 0.6f);
    EXPECT_FALSE(engine.drainChorusCommand().has_value());
}

TEST(MorphexsynthScriptEngine, SetReverbIsDrainedOnceThenClears)
{
    MorphexsynthScriptEngine engine;
    ASSERT_TRUE(engine.loadScript("SetReverb({ sizeMeters = 20, decayMs = 3000, dryDb = -6, mixDb = -12 })"));

    const auto command = engine.drainReverbCommand();
    ASSERT_TRUE(command.has_value());
    EXPECT_FLOAT_EQ(command->sizeMeters, 20.f);
    EXPECT_FLOAT_EQ(command->decayMs, 3000.f);
    EXPECT_FLOAT_EQ(command->dryDb, -6.f);
    EXPECT_FLOAT_EQ(command->mixDb, -12.f);
    EXPECT_FALSE(engine.drainReverbCommand().has_value());
}

TEST(MorphexsynthScriptEngine, SetReverbClampsOutOfRangeFields)
{
    MorphexsynthScriptEngine engine;
    ASSERT_TRUE(engine.loadScript("SetReverb({ sizeMeters = 999, decayMs = -5, dryDb = -200, mixDb = 50 })"));

    const auto command = engine.drainReverbCommand();
    ASSERT_TRUE(command.has_value());
    EXPECT_FLOAT_EQ(command->sizeMeters, 60.f);
    EXPECT_FLOAT_EQ(command->decayMs, 0.f);
    EXPECT_FLOAT_EQ(command->dryDb, -100.f);
    EXPECT_FLOAT_EQ(command->mixDb, 12.f);
}

TEST(MorphexsynthScriptEngine, NonFiniteArgumentIsSilentlyDropped)
{
    MorphexsynthScriptEngine engine;
    ASSERT_TRUE(engine.loadScript("SetFilter({ cutoff = 0/0, resonance = 0.5, type = \"LP4\" })"));
    EXPECT_FALSE(engine.drainFilterCommand().has_value());
}

TEST(MorphexsynthScriptEngine, NoteOnHookFiresWithChannelNoteVelocity)
{
    MorphexsynthScriptEngine engine;
    ASSERT_TRUE(engine.loadScript("function OnNoteOn(channel, note, velocity)\n"
                                  "    SetDistortion((channel == 2 and note == 60 and velocity == 100) and 7 or 0)\n"
                                  "end\n"));

    engine.notifyNoteOn(2, 60, 100);

    const auto command = engine.drainDistortionCommand();
    ASSERT_TRUE(command.has_value());
    EXPECT_EQ(*command, 7u);
}

TEST(MorphexsynthScriptEngine, LoadTimeInfiniteLoopIsRejectedWithoutHanging)
{
    MorphexsynthScriptEngine engine;
    EXPECT_FALSE(engine.loadScript("while true do end"));
    EXPECT_TRUE(engine.hasError());
    EXPECT_NE(engine.lastError().find("infinite loop"), std::string::npos) << engine.lastError();
}

TEST(MorphexsynthScriptEngine, HandlerInfiniteLoopIsRejectedWithoutHanging)
{
    MorphexsynthScriptEngine engine;
    ASSERT_TRUE(engine.loadScript("function OnNoteOn(channel, note, velocity) while true do end end"));
    engine.notifyNoteOn(0, 60, 100);
    EXPECT_TRUE(engine.hasError());
    EXPECT_NE(engine.lastError().find("infinite loop"), std::string::npos) << engine.lastError();
}
