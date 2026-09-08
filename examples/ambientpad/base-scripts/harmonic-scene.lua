-- harmonic-scene: hands the instrument over to the harmonic organism instead of playing a
-- single fixed note. Channel 10 becomes a sustained E pedal (triggered automatically the
-- moment it's added to the pedal set); every other channel is left for the organism itself
-- to trigger/glide/release as it moves through the palette. A few impulse gestures on a
-- timer show the performance vocabulary - in real use these come from the player, either the
-- Impulse dropdown below (Lua Controls area) or the Home/Harmony dials.

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
    SetOscillator(1, 0, { waveform = 0, level = 0.7, height = 0, cents = 0 })   -- Saw-Sine-Square path
    SetOscillator(1, 1, { waveform = 1, level = 0.6, height = 0, cents = 3 })   -- Triangle-Sine-SharkFin path

    SetMaterial(0.4)
    SetLight(0.4)
    SetMotion(0.4)
    SetBreath(0.4)
    SetStability(0.7)
    SetBloom(0.7)

    SetChorus({ rateHz = 0.4, depth = 0.6, mix = 0.5 })
    SetReverb({ sizeMeters = 20, decayMs = 4000, dryDb = 0, mixDb = -12 })

    SetHarmony(true)
    SetHarmonyHome(4)          -- E
    SetPedalChannels({ 10 })   -- channel 10 becomes a sustained E pedal, triggered now

    -- A slow arc: settle in for a while, admit some open/ambiguous colour, then seek a
    -- resting place - each gesture's own life cycle (see the impulse table above) does the
    -- rest, this just decides when to nudge.
    Timer.After(60000, function() Open() end)
    Timer.After(150000, function() Arrive() end)
    Timer.Every(240000, function() Stay() end)
end
