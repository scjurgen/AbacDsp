-- drone-chaos-cycle: alternates a calm drone section (the same trio as drone-scene.lua,
-- dwell 60s) with a short, dense chromatic-cluster disturbance (dwell 5s, glide 0.3s) -
-- Calm/Chaos knobs (Lua Controls) retune each section's length live, taking effect on its
-- next cycle, not the one already running; the Impulse dropdown still lets a player nudge
-- either section directly. Swapping ClearHarmonicPalette()+AddHarmonicState() resets the
-- organism's own current-index bookkeeping to that palette's first entry, but the
-- actually-sounding voices only move at the next real decision, via the normal crossfade -
-- the section switch itself is never an abrupt cut.

UICreateParameterSet({
    { id = "impulse", name = "Impulse", type = "drop",
      items = { "Stay", "Lean", "Open", "Gather", "Darken", "Brighten", "Disturb", "Arrive", "Release" },
      default = 0 },
    { id = "calm", name = "Calm secs", type = "knob", range = { min = 20, max = 120, step = 1, skew = 1 },
      default = 60, unit = "s" },
    { id = "chaos", name = "Chaos secs", type = "knob", range = { min = 5, max = 40, step = 1, skew = 1 },
      default = 15, unit = "s" },
})

local calmSeconds = 60
local chaosSeconds = 15

function OnImpulseChanged(value)
    if value == 0 then Stay()
    elseif value == 1 then Lean()
    elseif value == 2 then Open()
    elseif value == 3 then Gather()
    elseif value == 4 then Darken()
    elseif value == 5 then Brighten()
    elseif value == 6 then Disturb()
    elseif value == 7 then Arrive()
    elseif value == 8 then Release()
    end
end

function OnCalmChanged(value) calmSeconds = value end
function OnChaosChanged(value) chaosSeconds = value end

-- Plain globals, not `local function`, so each can call the other before both are defined -
-- Lua resolves a global by name at call time, not at the point its enclosing function is
-- declared.
function enterDrone()
    ClearHarmonicPalette()
    AddHarmonicState({ semitones = { 0.00, 0.015, -0.02, 0.03, -0.015, 0.00 }, region = 4,
                        luminosity = 0.50, minorColor = 0.50, density = 0.05, ambiguity = 0.85, tension = 0.05 })
    AddHarmonicState({ semitones = { 7.10, 0.01, 0.02, -0.10, -0.01, 0.00 }, region = 4,
                        luminosity = 0.55, minorColor = 0.50, density = 0.15, ambiguity = 0.65, tension = 0.15 })
    AddHarmonicState({ semitones = { 5.05, 0.01, 0.02, -0.10, -0.01, 0.00 }, region = 4,
                        luminosity = 0.50, minorColor = 0.50, density = 0.15, ambiguity = 0.70, tension = 0.20 })
    SetHarmonyCharacter(4)
    SetHarmonyRegionBonus(1.0)
    SetHarmonyTiming({ dwellSeconds = 60, cooldownSeconds = 40, glideSeconds = 20 })
    Timer.After(calmSeconds * 1000, enterChaos)
end

function enterChaos()
    ClearHarmonicPalette()
    AddHarmonicState({ semitones = { 0, 1, 2, 6, 7 }, region = 5,
                        luminosity = 0.25, minorColor = 0.70, density = 0.60, ambiguity = 0.90, tension = 0.90 })
    AddHarmonicState({ semitones = { -1, 0, 1, 2, 3 }, region = 5,
                        luminosity = 0.25, minorColor = 0.70, density = 0.60, ambiguity = 0.90, tension = 0.90 })
    AddHarmonicState({ semitones = { 2, 3, 8, 9, 10 }, region = 5,
                        luminosity = 0.25, minorColor = 0.70, density = 0.60, ambiguity = 0.90, tension = 0.90 })
    AddHarmonicState({ semitones = { 0, 1, 6, 7, 8 }, region = 5,
                        luminosity = 0.25, minorColor = 0.70, density = 0.60, ambiguity = 0.90, tension = 0.90 })
    SetHarmonyCharacter(5)
    SetHarmonyTiming({ dwellSeconds = 5, cooldownSeconds = 2, glideSeconds = 0.3 })
    Disturb()
    Timer.After(chaosSeconds * 1000, enterDrone)
end

function OnStart()
    for ch = 1, 16 do
        SetOscillator(ch, 0, { waveform = 0, level = 0.7, height = 0, cents = 0 })
        SetOscillator(ch, 1, { waveform = 1, level = 0.5, height = 0, cents = -4 })
        SetVolumeLfo(ch, 4 + ch / 10, 3, math.random(0, 360))
        SetCutoffLfo(ch, 30 + ch / 10, 12, math.random(0, 360))
        SetResonanceLfo(ch, 2 + ch / 10, 0.2, math.random(0, 360))
        SetPitchLfo(ch, 3 + ch / 10, 10, math.random(0, 360))
        SetMaterialLfo(ch, 2 + ch / 5, 0.8, math.random(0, 360))
    end

    SetMaterial(0.35)
    SetLight(0.3)
    SetMotion(0.2)
    SetBreath(0.6)
    SetStability(0.8)
    SetBloom(0.8)

    SetChorus({ rateHz = 0.2, depth = 0.5, mix = 0.4 })
    SetReverb({ sizeMeters = 30, decayMs = 6000, dryDb = 0, mixDb = -10 })

    SetHarmony(true)
    SetHarmonyHome(0)

    enterDrone()
end
