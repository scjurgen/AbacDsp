-- full-api-reference: a working reference that calls every function ambientsynth's own Lua
-- API offers (see the script skeleton / README Scripting section for the same list with more
-- detail). This is a syntax reference, not a serious sound-design starting point - some calls
-- here (NoteOn/NoteOff on channel 3, both PlayHarmony calls) only make musical sense spaced
-- out over a real performance, not fired in one OnStart(). Start from performance.lua instead
-- if you want a patch to build on - it's the one real MIDI actually plays.

function OnStart()
    -- Oscillators: both layers on channel 1
    SetOscillator(1, 0, { waveform = 0, level = 0.7, height = 0, cents = 0 })   -- Saw-Sine-Square
    SetOscillator(1, 1, { waveform = 1, level = 0.5, height = 12, cents = -4 }) -- Triangle-Sine-SharkFin, an octave up

    SetGain(1, -3)

    -- Each channel can carry its own slow LFO on six destinations - independent rates/depths
    -- per channel (Lua-only, no dial)
    SetCutoffLfo(1, 6, 12, 0)      -- 6 cycles/min, 12-semitone filter sweep
    SetMaterialLfo(1, 3, 0.3, 90)  -- 3 cycles/min, morph sweep, offset in phase
    SetResonanceLfo(1, 5, 0.3, 0)  -- 5 cycles/min, always pulls resonance up, never down
    SetPitchLfo(1, 3, 8, 0)        -- 3 cycles/min, +/-8 cent vibrato
    SetBreathLfo(1, 4, 2, 180)     -- 4 cycles/min, output-gain ripple
    SetDriftLfo(1, 5, 15, 0)       -- 5 cycles/min, the two oscillators spread apart and back

    -- The base musical-intent controls
    SetMaterial(0.45)
    SetMaterialRange(0.4)
    SetCutoff(0.4)
    SetResonance(0.2)
    SetFilterType(FilterType.BP2)
    SetBloom(0.6)

    -- Fine-tuning how far each destination's own OU process can wander (all Lua-only, no dial)
    SetCutoffOuRange(10)
    SetResonanceRange(0.4)
    SetBreathOuRange(4)
    SetPitchOuRange(20)

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
    SetMaterialLfo(3, 1.5, 0.3, 270)
    NoteOn(3, 64, 70)
    NoteOff(3, 64)

    -- PlayHarmony realizes one chord by voice-leading from whatever is currently playing;
    -- SetHarmonyHome/SetHarmonyGlideTime/SetPedalNote configure the register, transition
    -- speed, and dedicated bass pedal (always channel 16, excluded from the voice-leading)
    SetHarmonyHome(4) -- E
    SetHarmonyGlideTime(2)
    SetPedalNote(28)
    PlayHarmony({ semitones = { 0, 3, 7 } })
    PlayHarmony({ semitones = { -12, 0, 4, 7, 11 } })
end
