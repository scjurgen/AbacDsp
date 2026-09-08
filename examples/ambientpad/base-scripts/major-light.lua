-- major-light: Character = Major Light. Leans toward the bright maj7/6-9/maj9 colourings of
-- the home major triad. The Pedal is a dedicated, always-excluded-from-voice-leading bass
-- channel - set here to a genuinely low note so the register spread is always audible,
-- independent of whether the organism happens to pick one of the palette's own "/lo" bass
-- variants. Every other channel shares the same richer two-oscillator texture, since the
-- organism can trigger any of them for the moving upper structure. Brighten() shortly after
-- start pushes luminosity up further, on top of what the Character bias already favours.

function OnStart()
    for ch = 1, 16 do
        SetOscillator(ch, 0, { waveform = 0, level = 0.7, height = 0, cents = 0 })
        SetOscillator(ch, 1, { waveform = 1, level = 0.6, height = 0, cents = 3 })
    end
    SetOscillator(16, 0, { waveform = 0, level = 0.9, height = 0, cents = 0 })
    SetOscillator(16, 1, { waveform = 0, level = 0.0, height = 0, cents = 0 })
    SetGain(16, 4)

    SetMaterial(0.4)
    SetLight(0.4)
    SetMotion(0.4)
    SetBreath(0.4)
    SetStability(0.6)
    SetBloom(0.6)

    SetChorus({ rateHz = 0.4, depth = 0.5, mix = 0.4 })
    SetReverb({ sizeMeters = 18, decayMs = 3500, dryDb = 0, mixDb = -14 })

    SetHarmony(true)
    SetHarmonyHome(0)          -- C
    SetHarmonyCharacter(2)     -- Major Light
    SetPedalNote(30)           -- C1, two and a half octaves below home

    Timer.After(5000, function() Brighten() end)
end
