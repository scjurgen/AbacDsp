-- Family 6: BBD-inspired chorus.
-- A short modulated delay, a two-pole bandwidth limit on the wet path, and a feedback path
-- through a damping filter and a saturator. No companding or clock noise: the node set has
-- no expander to pair with Compander and no noise source.
return {
  version = 1,
  name = "BBD Inspired Chorus",

  io = { inputs = { "inL", "inR" }, outputs = { "outL", "outR" } },

  nodes = {
    { id = "bbd", type = "TapeDelay", config = { baseDelayMs = 8, seed = 31 },
      params = { wowDepth = 0.50, wowRate = 0.40, wowVariance = 0.05, flutterDepth = 0.42, flutterRate = 0.70 } },

    { id = "bw1L", type = "OnePoleLP", params = { cutoffHz = 6000 } },
    { id = "bw2L", type = "OnePoleLP", params = { cutoffHz = 6000 } },
    { id = "bw1R", type = "OnePoleLP", params = { cutoffHz = 6000 } },
    { id = "bw2R", type = "OnePoleLP", params = { cutoffHz = 6000 } },

    { id = "dampL", type = "OnePoleLP", params = { cutoffHz = 3500 } },
    { id = "dampR", type = "OnePoleLP", params = { cutoffHz = 3500 } },
    { id = "satL", type = "Saturator", params = { drive = 1.0 } },
    { id = "satR", type = "Saturator", params = { drive = 1.0 } },
    { id = "feedback", type = "Gain", params = { gainDb = -13.0 } },
    { id = "returnL", type = "FeedbackDelay" },
    { id = "returnR", type = "FeedbackDelay" },

    { id = "wet", type = "Gain", params = { gainDb = -3.0 } },
    { id = "dry", type = "Gain", params = { gainDb = 0.0 } },
    { id = "mix", type = "Mixer" },
  },

  edges = {
    { from = "inL", to = "bbd.inL" },
    { from = "inR", to = "bbd.inR" },
    { from = "bbd.outL", to = "bw1L.in" },
    { from = "bbd.outR", to = "bw1R.in" },
    { from = "bw1L.out", to = "bw2L.in" },
    { from = "bw1R.out", to = "bw2R.in" },

    { from = "bw2L.out", to = "wet.inL" },
    { from = "bw2R.out", to = "wet.inR" },
    { from = "wet.outL", to = "mix.in1L" },
    { from = "wet.outR", to = "mix.in1R" },
    { from = "inL", to = "dry.inL" },
    { from = "inR", to = "dry.inR" },
    { from = "dry.outL", to = "mix.in2L" },
    { from = "dry.outR", to = "mix.in2R" },
    { from = "mix.outL", to = "outL" },
    { from = "mix.outR", to = "outR" },

    { from = "bw2L.out", to = "dampL.in" },
    { from = "bw2R.out", to = "dampR.in" },
    { from = "dampL.out", to = "satL.in" },
    { from = "dampR.out", to = "satR.in" },
    { from = "satL.out", to = "feedback.inL" },
    { from = "satR.out", to = "feedback.inR" },
    { from = "feedback.outL", to = "returnL.in" },
    { from = "feedback.outR", to = "returnR.in" },
    { from = "returnL.out", to = "bbd.feedbackL" },
    { from = "returnR.out", to = "bbd.feedbackR" },
  },
}
