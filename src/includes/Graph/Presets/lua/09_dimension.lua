-- Family 7: dimension-style widening.
-- Two nearly static delays. Tap A is folded to mono into the left channel, tap B is folded
-- to mono and inverted into the right, so a mono input comes out decorrelated. Movement is a
-- slow wander with no periodic component, so there is no audible wobble.
return {
  version = 1,
  name = "Dimension Widening",

  io = { inputs = { "inL", "inR" }, outputs = { "outL", "outR" } },

  nodes = {
    { id = "tapA", type = "TapeDelay", config = { baseDelayMs = 9, seed = 41 },
      params = { wowDepth = 0.60, wowRate = 0.10, wowVariance = 0.20, flutterDepth = 0.0 } },
    { id = "tapB", type = "TapeDelay", config = { baseDelayMs = 14, seed = 42 },
      params = { wowDepth = 0.60, wowRate = 0.13, wowVariance = 0.20, flutterDepth = 0.0 } },

    { id = "toLeft", type = "Matrix", params = { gainLL = 0.5, gainLR = 0.5, gainRL = 0.0, gainRR = 0.0 } },
    { id = "toRight", type = "Matrix", params = { gainLL = 0.0, gainLR = 0.0, gainRL = -0.5, gainRR = -0.5 } },
    { id = "taps", type = "Mixer" },

    { id = "highpassL", type = "OnePoleHP", params = { cutoffHz = 200 } },
    { id = "highpassR", type = "OnePoleHP", params = { cutoffHz = 200 } },
    { id = "toneL", type = "TiltEQ", params = { tiltDb = -1.5, pivotHz = 1000 } },
    { id = "toneR", type = "TiltEQ", params = { tiltDb = -1.5, pivotHz = 1000 } },
    { id = "wet", type = "Gain", params = { gainDb = -1.0 } },
    { id = "dry", type = "Gain", params = { gainDb = 0.0 } },
    { id = "mix", type = "Mixer" },
  },

  edges = {
    { from = "inL", to = "tapA.inL" },
    { from = "inR", to = "tapA.inR" },
    { from = "inL", to = "tapB.inL" },
    { from = "inR", to = "tapB.inR" },
    { from = "tapA.outL", to = "toLeft.inL" },
    { from = "tapA.outR", to = "toLeft.inR" },
    { from = "tapB.outL", to = "toRight.inL" },
    { from = "tapB.outR", to = "toRight.inR" },
    { from = "toLeft.outL", to = "taps.in1L" },
    { from = "toLeft.outR", to = "taps.in1R" },
    { from = "toRight.outL", to = "taps.in2L" },
    { from = "toRight.outR", to = "taps.in2R" },
    { from = "taps.outL", to = "highpassL.in" },
    { from = "taps.outR", to = "highpassR.in" },
    { from = "highpassL.out", to = "toneL.in" },
    { from = "highpassR.out", to = "toneR.in" },
    { from = "toneL.out", to = "wet.inL" },
    { from = "toneR.out", to = "wet.inR" },
    { from = "wet.outL", to = "mix.in1L" },
    { from = "wet.outR", to = "mix.in1R" },
    { from = "inL", to = "dry.inL" },
    { from = "inR", to = "dry.inR" },
    { from = "dry.outL", to = "mix.in2L" },
    { from = "dry.outR", to = "mix.in2R" },
    { from = "mix.outL", to = "outL" },
    { from = "mix.outR", to = "outR" },
  },
}
