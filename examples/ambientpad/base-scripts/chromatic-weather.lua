-- chromatic-weather: Character = Chromatic. Leans toward brief, foreign colour that doesn't
-- keep the tonal home literally audible in the chord itself (b2maj, 6maj, b5maj7 - only the
-- bass keeps home present here). The bias is real but moderate by design (these entries fight
-- the KeepHomeAudible vow), so this stays "more foreign colour than usual", not constant
-- chromaticism. Channel 16 carries a deliberate low, boosted, single-oscillator bass on home
-- itself (not the organism's own choice - claimed as a pedal so it's never touched by
-- voice-leading), the one constant the foreign colour above keeps drifting away from. Every
-- other channel shares the same richer two-oscillator texture. Disturb() shortly after start
-- admits a bit more tension too.

function OnStart()
    for ch = 1, 16 do
        SetOscillator(ch, 0, { waveform = 0, level = 0.7, height = 0, cents = 0 })
        SetOscillator(ch, 1, { waveform = 1, level = 0.6, height = 0, cents = 3 })
    end

    SetMaterial(0.4)
    SetLight(0.4)
    SetMotion(0.4)
    SetBreath(0.4)
    SetStability(0.6)
    SetBloom(0.6)

    SetChorus({ rateHz = 0.4, depth = 0.5, mix = 0.4 })
    SetReverb({ sizeMeters = 18, decayMs = 3500, dryDb = 0, mixDb = -14 })

    SetHarmony(true)
    SetHarmonyHome(9)          -- A
    SetHarmonyCharacter(5)     -- Chromatic
    SetPedalChannels({ 16 })

    -- SetPedalChannels always triggers a newly-claimed pedal at the home register first;
    -- this deliberately overrides that a beat later with the real, low, boosted bass voice -
    -- NoteOn is applied before SetPedalChannels within a single tick, so doing both at once
    -- would have the home-register trigger win instead.
    Timer.After(50, function()
        SetOscillator(16, 0, { waveform = 0, level = 0.9, height = 0, cents = 0 })
        SetOscillator(16, 1, { waveform = 0, level = 0.0, height = 0, cents = 0 })
        SetGain(16, 4)
        NoteOn(16, 39, 100)     -- A1, two and a half octaves below home
    end)

    Timer.After(5000, function() Disturb() end)
end
