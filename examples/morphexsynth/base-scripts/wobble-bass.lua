-- wobble-bass: a static patch, no per-note scripting - everything happens once in
-- OnStart(). A slow-to-medium square LFO wobbles the filter cutoff, and every note gets
-- mildly distorted. Change SetLfo's speedHz live from the script editor to hear the
-- wobble rate track a different feel (0.1-0.3 for a slow pulse, 2-6 for a rhythmic wobble).

function OnStart()
    SetOscillator(0, { waveform = 2, level = 0.8, pitchFactor = 0.5 })   -- Saw, one octave down
    SetOscillator(1, { waveform = 3, level = 0.5, pitchFactor = 0.5, detune = 0.1 })  -- Square, one octave down
    SetOscillator(2, { waveform = 2, level = 0.3, pitchFactor = 1.0 })   -- Saw, unison

    SetFilter({ cutoff = 55, resonance = 0.7, type = "LP4" })
    SetAmpEnvelope({ attackMs = 3, decayMs = 100, sustainLevel = 0.9, releaseMs = 150 })
    SetFilterEnvelope({ attackMs = 5, decayMs = 200, sustainLevel = 0.6, releaseMs = 150 })

    SetLfo({ waveform = 3, speedHz = 3.5, filterDepth = 1.5, keyFollow = 0 })  -- Square wobble

    SetDistortion(3)  -- a mild WaveShaperTables.h preset

    -- Z (channel pressure) pushes the wobble depth further for an expressive squeeze.
    SetCtrlSlot(0, { source = 2, curve = 1, target = 4, valueType = 0, depth = 0.6 })
end
