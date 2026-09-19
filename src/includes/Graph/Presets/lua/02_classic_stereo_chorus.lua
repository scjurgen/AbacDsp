-- Family 2: classic stereo chorus, dual mono.
-- One TapeDelay per channel with its own seed, so left and right wobble independently.
-- Each TapeDelay is stereo: only its own channel is fed and read. Each macro is one knob; its
-- default is in the knob's own units and reproduces the parameter values below.
return {
  version = 1,
  name = "Classic Stereo Chorus",

  io = { inputs = { "inL", "inR" }, outputs = { "outL", "outR" } },

  nodes = {
    { id = "tapeL", type = "TapeDelay", config = { baseDelayMs = 12, seed = 11 },
      params = { wowDepth = 0.50, wowRate = 0.40, wowVariance = 0.10, flutterDepth = 0.42, flutterRate = 0.80 } },
    { id = "tapeR", type = "TapeDelay", config = { baseDelayMs = 12, seed = 23 },
      params = { wowDepth = 0.50, wowRate = 0.47, wowVariance = 0.10, flutterDepth = 0.50, flutterRate = 0.95 } },

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

  macros = {
    { id = "depth", label = "Depth", unit = "%", min = 0, max = 100, default = 60,
      targets = { { to = "tapeL.flutterDepth", min = 0.0, max = 0.7 },
                  { to = "tapeR.flutterDepth", min = 0.0, max = 0.8333 } } },
    { id = "rate", label = "Rate", unit = "Hz", min = 0.2, max = 2.0, default = 0.8,
      targets = { { to = "tapeL.flutterRate", min = 0.2, max = 2.0 },
                  { to = "tapeR.flutterRate", min = 0.2375, max = 2.375 } } },
    { id = "tone", label = "Tone", unit = "dB", min = -6, max = 6, default = -2,
      targets = { { to = "toneL.tiltDb", min = -6, max = 6 },
                  { to = "toneR.tiltDb", min = -6, max = 6 } } },
    { id = "wet", label = "Wet", unit = "dB", min = -60, max = 0, default = -3,
      targets = { { to = "wet.gainDb", min = -60, max = 0 } } },
  },
}
