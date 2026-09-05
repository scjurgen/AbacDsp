-- Named constants for morphexsynth's small-integer enum fields (SetCtrlSlot's
-- source/curve/target/valueType, SetOscillator's waveform, SetLfo's waveform). Pull in
-- with `import "constants"` and write e.g. `source = CtrlSource.EnvelopeFilter` instead
-- of `source = 5`.
--
-- Hand-maintained, not generated: keep this file in sync by hand whenever the
-- corresponding C++ side changes -
--   CtrlSource/CtrlCurve/CtrlTarget/CtrlValueType: MorphexCtrlSlotSettings in
--     MorphexsynthScriptEngine.h (source is AbacDsp::CtrlDimension, curve is
--     AbacDsp::CtrlCurve, target is AbacDsp::CtrlTarget, valueType is
--     AbacDsp::CtrlValueType - see MorphexsynthVoice.h for those enum definitions)
--   OscWaveform: MorphexOscillatorSettings's waveform field in MorphexsynthScriptEngine.h
--   LfoWaveform: AbacDsp::LfoType in Generators/SynthLfo.h

CtrlSource = {
    X = 0,
    Y = 1,
    Z = 2,
    Velocity = 3,
    Note = 4,
    EnvelopeFilter = 5,
    EnvelopeAmplitude = 6,
}

CtrlCurve = {
    CubeRoot = 0,
    SquareRoot = 1,
    Linear = 2,
    Square = 3,
    Cube = 4,
}

CtrlTarget = {
    None = 0,
    PitchBend = 1,
    PitchBendSecondary = 2,
    FilterCutoff = 3,
    FilterResonance = 4,
    Pan = 5,
    OscFreq2 = 6,
    OscFreq3 = 7,
    OscLevel1 = 8,
    OscLevel2 = 9,
    OscLevel3 = 10,
    LfoLevel = 11, -- unused
    SustainVol = 12,
    AttackTime = 13,
    DecayTime = 14,
    ReleaseTime = 15,
    Distortion = 16,
}

CtrlValueType = {
    Abs = 0,
    BiPolar = 1,
}

OscWaveform = {
    Triangle = 0,
    SharkFin = 1,
    Saw = 2,
    Square = 3,
    White = 4,
}

LfoWaveform = {
    Sine = 0,
    Triangle = 1,
    Saw = 2,
    Square = 3,
    Noise = 4,
    SampleHoldNoise = 5,
    SampleHoldFlipFlop = 6,
    BrownNoise = 7,
}
