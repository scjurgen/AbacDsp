-- full-api-reference: a working reference that calls every function ambientpad's own Lua API
-- offers (see the script skeleton / README Scripting section for the same list with more
-- detail). This is a syntax reference, not a serious sound-design starting point - some calls
-- here (SetHold's two states, all nine impulse gestures back to back) only make musical sense
-- spaced out over a real performance, not fired in one OnStart(). Start from breathing-drone.lua
-- or one of the harmony-region scripts instead if you want a patch to build on.

function OnStart()
    -- Oscillators: both layers on the hand-played voice (channel 1)
    SetOscillator(1, 0, { waveform = 0, level = 0.7, height = 0, cents = 0 })   -- Saw-Sine-Square
    SetOscillator(1, 1, { waveform = 1, level = 0.5, height = 12, cents = -4 }) -- Triangle-Sine-SharkFin, an octave up

    SetGain(1, -3)

    -- The six musical-intent controls, plus Material's own Range dial and Hold
    SetMaterial(0.45)
    SetMaterialRange(0.4)
    SetLight(0.4)
    SetMotion(0.6)
    SetBreath(0.6)
    SetStability(0.3)
    SetBloom(0.6)
    SetHold(true)  -- freeze the four OU processes at their current value...
    SetHold(false) -- ...and release them again - a real script would space these out in time

    -- Fine-tuning how far Lens/Drift/Breath can wander (all Lua-only, no dial)
    SetCutoffRange(10)
    SetResonanceRange(0.4)
    SetPitchDriftRange(30)
    SetBreathVcaRange(4)

    SetDistortion(0) -- 0 = bypass; try 1.. for a WaveShaperTables.h preset

    -- Master-bus effects
    SetPhaser({ rateHz = 0.15, depth = 0.6, feedback = 0.3, mix = 0.25 })
    SetChorus({ rateHz = 0.3, depth = 0.5, mix = 0.4 })
    SetReverb({ sizeMeters = 25, decayMs = 5000, dryDb = 0, mixDb = -14 })

    -- Play channel 1 directly, then glide it - SetPitch never retriggers the envelope
    NoteOn(1, 57, 90)
    SetPitch(1, 60, 0, 6)

    -- A second, independently-configured channel, released again to show NoteOff
    SetOscillator(3, 0, { waveform = 2, level = 0.6, height = 0, cents = 0 }) -- Square-White-Saw
    NoteOn(3, 64, 70)
    NoteOff(3, 64)

    -- The harmonic organism: home, a soft region preference, and a dedicated bass pedal on
    -- channel 16 (always excluded from the organism's own voice-leading)
    SetHarmonyHome(4)          -- E
    SetHarmonyCharacter(3)     -- Modal Warmth
    SetPedalChannels({ 16 })
    SetPedalNote(28)

    -- Optional: replace the 20 built-in states with a script-authored palette instead -
    -- semitones are literal, home-relative, already spread as wanted. Only shown here for
    -- syntax; see base-scripts/custom-harmony.lua for a patch actually built around this.
    ClearHarmonicPalette()
    AddHarmonicState({ semitones = { 0, 3, 7 } })
    AddHarmonicState({ semitones = { 0, 4, 7 }, region = 2 })
    AddHarmonicState({ semitones = { -12, 0, 5, 7 } })

    -- Optional: override the organism's usual tens-of-seconds pace - see
    -- base-scripts/custom-harmony.lua for why you'd actually want this
    SetHarmonyTiming({ dwellSeconds = 30, cooldownSeconds = 20, glideSeconds = 10 }) -- today's defaults

    SetHarmony(true)

    -- Impulse gestures - performance nudges with their own life cycle, not instant switches.
    -- In real use you'd call one at a time, in response to something, not all nine at once.
    Lean()
    Open()
    Gather()
    Darken()
    Brighten()
    Disturb()
    Arrive()
    Stay()
    Release() -- fades out every impulse above, including itself
end
