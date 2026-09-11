-- drone-scene: a soothing, mostly-static drone built from three AddHarmonicState-authored
-- states (near-unison, +5th, +4th) tagged low density/tension and high ambiguity via the
-- optional wish-axis tag fields, so the organism genuinely prefers to linger there under a
-- calm climate rather than merely tolerating it through region bonus alone. An Impulse
-- dropdown (Lua Controls area) lets a player nudge it live - same vocabulary as
-- harmonic-scene.lua.

-- One dropdown for all 9 gestures, since only 8 dynamic-UI slots exist in total - picking a
-- different item each time fires it; re-picking the same item twice in a row does not.
UICreateParameterSet({
    { id = "impulse", name = "Impulse", type = "drop",
      items = { "Stay", "Lean", "Open", "Gather", "Darken", "Brighten", "Disturb", "Arrive", "Release" },
      default = 0 },
})

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

    -- Three states only: a near-unison drone, and the same six voices with one note lifted to
    -- a 5th or a 4th. Tags are hand-set, not left at the default: very low density/tension,
    -- high ambiguity, so the trio is genuinely favoured under a calm climate.
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
end
