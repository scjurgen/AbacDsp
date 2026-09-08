-- breathing-drone: a static patch, no per-note scripting - everything happens once in
-- OnStart(). Plays channel 1 immediately, leans into a slow and wide Motion setting so the
-- voice keeps changing on its own, and turns the chorus on for stereo width.

function OnStart()
    SetOscillator(1, 0, { waveform = 0, level = 0.7, height = 0, cents = 0 })   -- Saw-Sine-Square path
    SetOscillator(1, 1, { waveform = 1, level = 0.6, height = 0, cents = 3 })   -- Triangle-Sine-SharkFin path

    SetMaterial(0.4)
    SetLight(0.35)
    SetMotion(0.6)
    SetBreath(0.5)
    SetStability(0.6)
    SetBloom(0.7)

    SetChorus({ rateHz = 0.4, depth = 0.6, mix = 0.5 })
    SetReverb({ sizeMeters = 20, decayMs = 4000, dryDb = 0, mixDb = -12 })

    NoteOn(1, 69, 90)
end
