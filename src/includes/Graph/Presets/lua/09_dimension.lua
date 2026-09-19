-- Family 7: dimension-style widening.
-- Two nearly static delays. Tap A is folded to mono into the left channel, tap B is folded
-- to mono and inverted into the right, so a mono input comes out decorrelated. Movement is a
-- slow wander with no periodic component, so there is no audible wobble. Each macro is one knob;
-- its default is in the knob's own units and reproduces the parameter values below.
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

  macros = {
    { id = "wander", label = "Wander", unit = "%", min = 0, max = 100, default = 60,
      targets = { { to = "tapA.wowDepth", min = 0.0, max = 1.0 },
                  { to = "tapB.wowDepth", min = 0.0, max = 1.0 },
                  { to = "tapA.wowVariance", min = 0.0, max = 0.3333 },
                  { to = "tapB.wowVariance", min = 0.0, max = 0.3333 } } },
    { id = "width", label = "Width", unit = "%", min = 0, max = 100, default = 100,
      targets = { { to = "toRight.gainRL", min = 0.0, max = -0.5 },
                  { to = "toRight.gainRR", min = 0.0, max = -0.5 } } },
    { id = "lowcut", label = "Low cut", unit = "Hz", min = 20, max = 800, default = 200,
      targets = { { to = "highpassL.cutoffHz", min = 20, max = 800 },
                  { to = "highpassR.cutoffHz", min = 20, max = 800 } } },
    { id = "tone", label = "Tone", unit = "dB", min = -6, max = 6, default = -1.5,
      targets = { { to = "toneL.tiltDb", min = -6, max = 6 },
                  { to = "toneR.tiltDb", min = -6, max = 6 } } },
    { id = "wet", label = "Wet", unit = "dB", min = -60, max = 0, default = -1,
      targets = { { to = "wet.gainDb", min = -60, max = 0 } } },
  },
}
