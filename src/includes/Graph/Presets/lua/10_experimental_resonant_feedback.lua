-- Family 8: experimental resonant feedback chorus. A DANGEROUS configuration on purpose.
-- The loop is TapeDelay, resonant band-pass, saturator, gain, cross-coupling, then back into the
-- delay. A band-pass does not count as damping, so the validator warns that the cycle is
-- undamped and resonant. The saturators clamp the loop and the final stage to +-1 and stand in
-- for a safety limiter; there is no feedback meter node. A quiet input decays; a loud burst
-- drives the loop into the saturators and it rings for seconds before it dies away. Each macro is
-- one knob; its default is in the knob's own units and reproduces the parameter values below.
-- The Feedback knob reaches +3 dB: the saturators still keep the loop and the output within +-1.
return {
  version = 1,
  name = "Experimental Resonant Feedback",

  io = { inputs = { "inL", "inR" }, outputs = { "outL", "outR" } },

  nodes = {
    { id = "tape", type = "TapeDelay", config = { baseDelayMs = 12, seed = 51 },
      params = { wowDepth = 0.50, wowRate = 0.30, wowVariance = 0.10, flutterDepth = 0.20, flutterRate = 0.50 } },

    { id = "resonatorL", type = "BandPass", params = { frequencyHz = 880, Q = 8.0 } },
    { id = "resonatorR", type = "BandPass", params = { frequencyHz = 1100, Q = 8.0 } },
    { id = "driveL", type = "Saturator", params = { drive = 1.5 } },
    { id = "driveR", type = "Saturator", params = { drive = 1.5 } },
    { id = "loopGain", type = "Gain", params = { gainDb = -3.0 } },
    { id = "coupling", type = "Matrix", params = { gainLL = 0.6, gainLR = 0.4, gainRL = 0.4, gainRR = 0.6 } },
    { id = "returnL", type = "FeedbackDelay" },
    { id = "returnR", type = "FeedbackDelay" },

    { id = "wet", type = "Gain", params = { gainDb = -6.0 } },
    { id = "dry", type = "Gain", params = { gainDb = 0.0 } },
    { id = "mix", type = "Mixer" },
    { id = "limitL", type = "Saturator", params = { drive = 0.0 } },
    { id = "limitR", type = "Saturator", params = { drive = 0.0 } },
  },

  edges = {
    { from = "inL", to = "tape.inL" },
    { from = "inR", to = "tape.inR" },
    { from = "tape.outL", to = "resonatorL.in" },
    { from = "tape.outR", to = "resonatorR.in" },
    { from = "resonatorL.out", to = "driveL.in" },
    { from = "resonatorR.out", to = "driveR.in" },
    { from = "driveL.out", to = "loopGain.inL" },
    { from = "driveR.out", to = "loopGain.inR" },
    { from = "loopGain.outL", to = "coupling.inL" },
    { from = "loopGain.outR", to = "coupling.inR" },
    { from = "coupling.outL", to = "returnL.in" },
    { from = "coupling.outR", to = "returnR.in" },
    { from = "returnL.out", to = "tape.feedbackL" },
    { from = "returnR.out", to = "tape.feedbackR" },

    { from = "driveL.out", to = "wet.inL" },
    { from = "driveR.out", to = "wet.inR" },
    { from = "wet.outL", to = "mix.in1L" },
    { from = "wet.outR", to = "mix.in1R" },
    { from = "inL", to = "dry.inL" },
    { from = "inR", to = "dry.inR" },
    { from = "dry.outL", to = "mix.in2L" },
    { from = "dry.outR", to = "mix.in2R" },
    { from = "mix.outL", to = "limitL.in" },
    { from = "mix.outR", to = "limitR.in" },
    { from = "limitL.out", to = "outL" },
    { from = "limitR.out", to = "outR" },
  },

  macros = {
    { id = "resonance", label = "Resonance", unit = "Hz", min = 200, max = 4000, default = 880,
      targets = { { to = "resonatorL.frequencyHz", min = 200, max = 4000 },
                  { to = "resonatorR.frequencyHz", min = 250, max = 5000 } } },
    { id = "q", label = "Q", min = 0.5, max = 20, default = 8,
      targets = { { to = "resonatorL.Q", min = 0.5, max = 20 },
                  { to = "resonatorR.Q", min = 0.5, max = 20 } } },
    { id = "feedback", label = "Feedback", unit = "dB", min = -12, max = 3, default = -3,
      targets = { { to = "loopGain.gainDb", min = -12, max = 3 } } },
    { id = "drive", label = "Drive", min = 0, max = 5, default = 1.5,
      targets = { { to = "driveL.drive", min = 0, max = 5 },
                  { to = "driveR.drive", min = 0, max = 5 } } },
    { id = "cross", label = "Cross", unit = "%", min = 0, max = 100, default = 40,
      targets = { { to = "coupling.gainLR", min = 0.0, max = 1.0 },
                  { to = "coupling.gainRL", min = 0.0, max = 1.0 },
                  { to = "coupling.gainLL", min = 1.0, max = 0.0 },
                  { to = "coupling.gainRR", min = 1.0, max = 0.0 } } },
    { id = "wet", label = "Wet", unit = "dB", min = -60, max = 0, default = -6,
      targets = { { to = "wet.gainDb", min = -60, max = 0 } } },
  },
}
