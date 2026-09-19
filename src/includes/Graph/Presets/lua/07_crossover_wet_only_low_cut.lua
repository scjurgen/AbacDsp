-- Crossover-safe chorus, policy 3: wet-only low cut.
-- No split; the full dry signal stays and only the wet bus is high-passed.
return {
  version = 1,
  name = "Wet Only Low Cut",
  io = { inputs = { "inL", "inR" }, outputs = { "outL", "outR" } },
  nodes = {
    { id = "tape", type = "TapeDelay" },
    { id = "wetHpL", type = "OnePoleHP", params = { cutoffHz = 200 } },
    { id = "wetHpR", type = "OnePoleHP", params = { cutoffHz = 200 } },
    { id = "mix", type = "Mixer" },
  },
  edges = {
    { from = "inL", to = "tape.inL" },
    { from = "inR", to = "tape.inR" },
    { from = "tape.outL", to = "wetHpL.in" },
    { from = "tape.outR", to = "wetHpR.in" },
    { from = "wetHpL.out", to = "mix.in1L" },
    { from = "wetHpR.out", to = "mix.in1R" },
    { from = "inL", to = "mix.in2L" },
    { from = "inR", to = "mix.in2R" },
    { from = "mix.outL", to = "outL" },
    { from = "mix.outR", to = "outR" },
  },
}
