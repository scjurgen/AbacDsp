-- pedal-modulation: demonstrates that Home, Character and Pedal can be moved together to
-- modulate the whole instrument to a new key, with the pedal always giving the new tonal
-- centre a clear, dedicated bass presence. Three stages, one minute each, looping: C major,
-- D minor (also centred on D), F major, then back to C. The Pedal is a dedicated,
-- always-excluded-from-voice-leading bass channel - moving it together with Home keeps the
-- bass and the organism's own harmony choices pointing at the same key at all times.

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
    ToC()
end

function ToC()
    SetHarmonyHome(0)          -- C
    SetHarmonyCharacter(2)     -- Major Light
    SetPedalNote(30)           -- C1
    Timer.After(60000, function() ToD() end)
end

function ToD()
    SetHarmonyHome(2)          -- D
    SetHarmonyCharacter(1)     -- Minor Home
    SetPedalNote(32)           -- D1
    Timer.After(60000, function() ToF() end)
end

function ToF()
    SetHarmonyHome(5)          -- F
    SetHarmonyCharacter(2)     -- Major Light
    SetPedalNote(35)           -- F1
    Timer.After(60000, function() ToC() end)
end
