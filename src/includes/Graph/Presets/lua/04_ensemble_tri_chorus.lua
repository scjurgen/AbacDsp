-- Family 4: multi-voice ensemble (tri-chorus).
-- Three TapeDelay voices with their own seed, rate, base delay and drift, so their
-- modulation is independent. Each voice runs at 1/sqrt(3), -4.77 dB, so three voices sum to
-- the level of one. Matrices lean the voices left, centre and right.
return {
  version = 1,
  name = "Ensemble Tri-Chorus",

  io = { inputs = { "inL", "inR" }, outputs = { "outL", "outR" } },

  nodes = {
    { id = "voice1", type = "TapeDelay", config = { baseDelayMs = 11, seed = 101 },
      params = { wowDepth = 0.30, wowRate = 0.50, wowVariance = 0.20, wowDrift = 0.20, flutterDepth = 0.05, flutterRate = 3.0 } },
    { id = "voice2", type = "TapeDelay", config = { baseDelayMs = 15, seed = 202 },
      params = { wowDepth = 0.30, wowRate = 0.70, wowVariance = 0.20, wowDrift = 0.20, flutterDepth = 0.05, flutterRate = 3.5 } },
    { id = "voice3", type = "TapeDelay", config = { baseDelayMs = 20, seed = 303 },
      params = { wowDepth = 0.30, wowRate = 0.90, wowVariance = 0.20, wowDrift = 0.20, flutterDepth = 0.05, flutterRate = 4.0 } },

    { id = "gain1", type = "Gain", params = { gainDb = -4.77 } },
    { id = "gain2", type = "Gain", params = { gainDb = -4.77 } },
    { id = "gain3", type = "Gain", params = { gainDb = -4.77 } },
    { id = "lean1", type = "Matrix", params = { gainLL = 1.0, gainLR = 0.4, gainRL = 0.0, gainRR = 0.6 } },
    { id = "lean2", type = "Matrix", params = { gainLL = 0.8, gainLR = 0.2, gainRL = 0.2, gainRR = 0.8 } },
    { id = "lean3", type = "Matrix", params = { gainLL = 0.6, gainLR = 0.0, gainRL = 0.4, gainRR = 1.0 } },
    { id = "sum12", type = "Mixer" },
    { id = "sum123", type = "Mixer" },

    { id = "toneL", type = "TiltEQ", params = { tiltDb = -2.0, pivotHz = 1000 } },
    { id = "toneR", type = "TiltEQ", params = { tiltDb = -2.0, pivotHz = 1000 } },
    { id = "wet", type = "Gain", params = { gainDb = -2.0 } },
    { id = "dry", type = "Gain", params = { gainDb = 0.0 } },
    { id = "mix", type = "Mixer" },
  },

  edges = {
    { from = "inL", to = "voice1.inL" },
    { from = "inR", to = "voice1.inR" },
    { from = "inL", to = "voice2.inL" },
    { from = "inR", to = "voice2.inR" },
    { from = "inL", to = "voice3.inL" },
    { from = "inR", to = "voice3.inR" },

    { from = "voice1.outL", to = "gain1.inL" },
    { from = "voice1.outR", to = "gain1.inR" },
    { from = "voice2.outL", to = "gain2.inL" },
    { from = "voice2.outR", to = "gain2.inR" },
    { from = "voice3.outL", to = "gain3.inL" },
    { from = "voice3.outR", to = "gain3.inR" },

    { from = "gain1.outL", to = "lean1.inL" },
    { from = "gain1.outR", to = "lean1.inR" },
    { from = "gain2.outL", to = "lean2.inL" },
    { from = "gain2.outR", to = "lean2.inR" },
    { from = "gain3.outL", to = "lean3.inL" },
    { from = "gain3.outR", to = "lean3.inR" },

    { from = "lean1.outL", to = "sum12.in1L" },
    { from = "lean1.outR", to = "sum12.in1R" },
    { from = "lean2.outL", to = "sum12.in2L" },
    { from = "lean2.outR", to = "sum12.in2R" },
    { from = "sum12.outL", to = "sum123.in1L" },
    { from = "sum12.outR", to = "sum123.in1R" },
    { from = "lean3.outL", to = "sum123.in2L" },
    { from = "lean3.outR", to = "sum123.in2R" },

    { from = "sum123.outL", to = "toneL.in" },
    { from = "sum123.outR", to = "toneR.in" },
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
