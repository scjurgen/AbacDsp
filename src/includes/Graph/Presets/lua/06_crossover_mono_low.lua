-- Crossover-safe chorus, policy 2: mono low band.
-- Same split; the low band is folded to mono before it is recombined.
return {
  version = 1,
  name = "Mono Low Band",
  io = { inputs = { "inL", "inR" }, outputs = { "outL", "outR" } },
  nodes = {
    { id = "xoL", type = "CrossoverLR4", params = { frequencyHz = 200 } },
    { id = "xoR", type = "CrossoverLR4", params = { frequencyHz = 200 } },
    { id = "lowMono", type = "StereoToMono" },
    { id = "lowDup", type = "MonoToStereo" },
    { id = "tape", type = "TapeDelay" },
    { id = "mix", type = "Mixer" },
  },
  edges = {
    { from = "inL", to = "xoL.in" },
    { from = "inR", to = "xoR.in" },
    { from = "xoL.lowOut", to = "lowMono.inL" },
    { from = "xoR.lowOut", to = "lowMono.inR" },
    { from = "lowMono.out", to = "lowDup.in" },
    { from = "xoL.highOut", to = "tape.inL" },
    { from = "xoR.highOut", to = "tape.inR" },
    { from = "tape.outL", to = "mix.in1L" },
    { from = "tape.outR", to = "mix.in1R" },
    { from = "lowDup.outL", to = "mix.in2L" },
    { from = "lowDup.outR", to = "mix.in2R" },
    { from = "mix.outL", to = "outL" },
    { from = "mix.outR", to = "outR" },
  },
}
