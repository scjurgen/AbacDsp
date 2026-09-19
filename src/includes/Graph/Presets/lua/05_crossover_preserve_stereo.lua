-- Crossover-safe chorus, policy 1: preserve the stereo low band.
-- The low band bypasses the delay and stays dry; the high band is processed.
return {
  version = 1,
  name = "Preserve Stereo Low Band",
  io = { inputs = { "inL", "inR" }, outputs = { "outL", "outR" } },
  nodes = {
    { id = "xoL", type = "CrossoverLR4", params = { frequencyHz = 200 } },
    { id = "xoR", type = "CrossoverLR4", params = { frequencyHz = 200 } },
    { id = "tape", type = "TapeDelay" },
    { id = "mix", type = "Mixer" },
  },
  edges = {
    { from = "inL", to = "xoL.in" },
    { from = "inR", to = "xoR.in" },
    { from = "xoL.highOut", to = "tape.inL" },
    { from = "xoR.highOut", to = "tape.inR" },
    { from = "tape.outL", to = "mix.in1L" },
    { from = "tape.outR", to = "mix.in1R" },
    { from = "xoL.lowOut", to = "mix.in2L" },
    { from = "xoR.lowOut", to = "mix.in2R" },
    { from = "mix.outL", to = "outL" },
    { from = "mix.outR", to = "outR" },
  },
}
