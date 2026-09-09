-- custom-harmony: builds its own harmonic vocabulary from scratch via AddHarmonicState,
-- instead of drawing on the library's 20 built-in states, and puts two hand-picked regions
-- to use: region 1 stays diatonic (triads drawn from a single C major scale), region 5 is
-- deliberately foreign (whole-tone and diminished shapes, some without the tonal home at
-- all - the same spirit as the built-in palette's own ChromaticWeather region). It starts in
-- the diatonic region and switches to the chromatic one after 60 seconds via
-- SetHarmonyCharacter. SetHarmonyTiming speeds the organism's usual tens-of-seconds pace way
-- up so several transitions are actually audible within that time, rather than one or two.

function OnStart()
    for ch = 1, 16 do
        SetOscillator(ch, 0, { waveform = 0, level = 0.7, height = 0, cents = 0 })
        SetOscillator(ch, 1, { waveform = 1, level = 0.5, height = 12, cents = -4 })
    end
    SetOscillator(16, 0, { waveform = 0, level = 0.9, height = 0, cents = 0 })
    SetOscillator(16, 1, { waveform = 0, level = 0.0, height = 0, cents = 0 })
    SetGain(16, 4)

    SetMaterial(0.4)
    SetLight(0.45)
    SetMotion(0.35)
    SetBreath(0.5)
    SetStability(0.6)
    SetBloom(0.4) -- shorter than usual so each fast transition is still clearly heard

    SetChorus({ rateHz = 0.25, depth = 0.4, mix = 0.35 })
    SetReverb({ sizeMeters = 25, decayMs = 4500, dryDb = 0, mixDb = -13 })

    ClearHarmonicPalette()
    -- Region 1: diatonic - I, ii, IV, V, vi of C major, nothing outside the scale
    AddHarmonicState({ semitones = { 0, 4, 7 }, region = 1 })    -- I
    AddHarmonicState({ semitones = { 2, 5, 9 }, region = 1 })    -- ii
    AddHarmonicState({ semitones = { 5, 9, 12 }, region = 1 })   -- IV
    AddHarmonicState({ semitones = { 7, 11, 14 }, region = 1 })  -- V
    AddHarmonicState({ semitones = { 9, 12, 16 }, region = 1 })  -- vi
    -- Region 5: whole-tone / diminished / chromatic - two of these don't even contain the
    -- tonal home, and the last is a tight cluster around it, all deliberately foreign
    AddHarmonicState({ semitones = { 0, 2, 4, 6, 8, 10 }, region = 5 })  -- whole tone, with home
    AddHarmonicState({ semitones = { 1, 3, 5, 7, 9, 11 }, region = 5 })  -- whole tone, without home
    AddHarmonicState({ semitones = { 0, 3, 6, 9 }, region = 5 })         -- diminished 7th, with home
    AddHarmonicState({ semitones = { 1, 4, 7, 10 }, region = 5 })        -- diminished 7th, without home
    AddHarmonicState({ semitones = { 11, 0, 1 }, region = 5 })           -- tight cluster around home

    -- Much faster than the default 30s/20s/10s pace, so the region switch below actually
    -- plays through several chords rather than one or two
    SetHarmonyTiming({ dwellSeconds = 4, cooldownSeconds = 2, glideSeconds = 1.5 })

    SetHarmony(true)
    SetHarmonyHome(0)      -- C
    SetHarmonyCharacter(1) -- start diatonic
    SetPedalNote(24)       -- two octaves below home

    Timer.After(60000, function() SetHarmonyCharacter(5) end) -- then turn foreign
end
