-- mpe-expressive-lead: routes all three MPE dimensions somewhere expressive (X to pitch
-- bend, Y to filter brightness, Z to amplitude-envelope-modulated filter drive), then
-- shapes the amp/filter envelopes and oscillator mix per note from velocity - the things
-- a fixed Vol/Cutoff/Reso dial can't reach on their own. Play on an MPE controller (or
-- just MIDI channel 2, this patch's default Lower Zone member channel) to hear X/Y/Z move.

function OnStart()
    SetOscillator(0, { waveform = 0, level = 0.7, pitchFactor = 1.0 })  -- Triangle, unison
    SetOscillator(1, { waveform = 2, level = 0.4, detune = 0.08, pitchFactor = 1.0 })  -- Saw, slightly detuned
    SetOscillator(2, { waveform = 2, level = 0.4, detune = -0.08, pitchFactor = 2.0 })  -- Saw, an octave up

    SetFilter({ cutoff = 68, resonance = 0.4, type = "LP4" })
    SetFilterEnvelope({ attackMs = 15, decayMs = 300, sustainLevel = 0.4, releaseMs = 200 })
    SetAmpEnvelope({ attackMs = 8, decayMs = 400, sustainLevel = 0.75, releaseMs = 250 })

    -- X (pitch bend, MPE's own per-note deflection) -> PitchBend, BiPolar, +-2 semitones
    SetCtrlSlot(0, { source = 0, curve = 2, target = 1, valueType = 1, depth = 2 / 12 })
    -- Y (MPE CC74 "slide"/timbre) -> FilterCutoff, Abs, up to +2 octaves of brightness
    SetCtrlSlot(1, { source = 1, curve = 2, target = 3, valueType = 0, depth = 1.0 })
    -- Z (channel pressure) -> FilterResonance, Abs, presses into resonance as you push harder
    SetCtrlSlot(2, { source = 2, curve = 1, target = 4, valueType = 0, depth = 0.8 })
end

function OnNoteOn(channel, note, velocity)
    local vel = velocity / 127
    -- Softer notes: slower, gentler filter attack. Harder notes: fast and bright.
    SetFilterEnvelope({
        attackMs = 40 - 30 * vel,
        decayMs = 300,
        sustainLevel = 0.3 + 0.3 * vel,
        releaseMs = 200,
    })
end
