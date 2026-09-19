-- Family 2: classic stereo chorus, dual mono.
-- One TapeDelay per channel with its own seed, so left and right wobble independently.
-- Each TapeDelay is stereo: only its own channel is fed and read.
return {
  version = 1,
  name = "Classic Stereo Chorus",

  io = { inputs = { "inL", "inR" }, outputs = { "outL", "outR" } },

  nodes = {
    { id = "tapeL", type = "TapeDelay", config = { baseDelayMs = 12, seed = 11 },
      params = { wowDepth = 0.30, wowRate = 0.80, wowVariance = 0.05, flutterDepth = 0.05, flutterRate = 3.0 } },
    { id = "tapeR", type = "TapeDelay", config = { baseDelayMs = 12, seed = 23 },
      params = { wowDepth = 0.30, wowRate = 0.80, wowVariance = 0.05, flutterDepth = 0.05, flutterRate = 3.0 } },

    { id = "toneL", type = "TiltEQ", params = { tiltDb = -2.0, pivotHz = 1000 } },
    { id = "toneR", type = "TiltEQ", params = { tiltDb = -2.0, pivotHz = 1000 } },
    { id = "wet", type = "Gain", params = { gainDb = -3.0 } },
    { id = "dry", type = "Gain", params = { gainDb = 0.0 } },
    { id = "mix", type = "Mixer" },
  },

  edges = {
    { from = "inL", to = "tapeL.inL" },
    { from = "inR", to = "tapeR.inR" },
    { from = "tapeL.outL", to = "toneL.in" },
    { from = "tapeR.outR", to = "toneR.in" },
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
