-- Crossover-safe chorus, policy 3: wet-only low cut.
-- No split; the full dry signal stays and only the wet bus is high-passed. Each macro is one
-- knob; its default is in the knob's own units and reproduces the parameter values below.
return {
  version = 1,
  name = "Wet Only Low Cut",
  io = { inputs = { "inL", "inR" }, outputs = { "outL", "outR" } },
  nodes = {
    { id = "tape", type = "TapeDelay", config = { baseDelayMs = 12, seed = 61 },
      params = { wowDepth = 0.50, wowRate = 0.40, wowVariance = 0.10, flutterDepth = 0.42, flutterRate = 0.80 } },
    { id = "wetHpL", type = "OnePoleHP", params = { cutoffHz = 200 } },
    { id = "wetHpR", type = "OnePoleHP", params = { cutoffHz = 200 } },
    { id = "wet", type = "Gain", params = { gainDb = 0.0 } },
    { id = "mix", type = "Mixer" },
  },
  edges = {
    { from = "inL", to = "tape.inL" },
    { from = "inR", to = "tape.inR" },
    { from = "tape.outL", to = "wetHpL.in" },
    { from = "tape.outR", to = "wetHpR.in" },
    { from = "wetHpL.out", to = "wet.inL" },
    { from = "wetHpR.out", to = "wet.inR" },
    { from = "wet.outL", to = "mix.in1L" },
    { from = "wet.outR", to = "mix.in1R" },
    { from = "inL", to = "mix.in2L" },
    { from = "inR", to = "mix.in2R" },
    { from = "mix.outL", to = "outL" },
    { from = "mix.outR", to = "outR" },
  },
  macros = {
    { id = "lowcut", label = "Low cut", unit = "Hz", min = 20, max = 800, default = 200,
      targets = { { to = "wetHpL.cutoffHz", min = 20, max = 800 },
                  { to = "wetHpR.cutoffHz", min = 20, max = 800 } } },
    { id = "depth", label = "Depth", unit = "%", min = 0, max = 100, default = 60,
      targets = { { to = "tape.flutterDepth", min = 0.0, max = 0.7 } } },
    { id = "rate", label = "Rate", unit = "Hz", min = 0.2, max = 2.0, default = 0.8,
      targets = { { to = "tape.flutterRate", min = 0.2, max = 2.0 } } },
    { id = "wet", label = "Wet", unit = "dB", min = -60, max = 0, default = 0,
      targets = { { to = "wet.gainDb", min = -60, max = 0 } } },
  },
}
