-- Family 1: tape vibrato. One stereo TapeDelay, 100% wet, shared wow and flutter.
-- Each macro is 0 to 1; a target sweeps min to max, with curve = "exp" for a geometric sweep.
-- The values below are the macros at their defaults.
return {
  version = 1,
  name = "Tape Vibrato",

  io = { inputs = { "inL", "inR" }, outputs = { "outL", "outR" } },

  nodes = {
    { id = "tape", type = "TapeDelay",
      config = { baseDelayMs = 10 },
      params = {
        transportRatio = 1.0,
        wowDepth = 0.65, wowRate = 0.52, wowVariance = 0.10, wowDrift = 0.05,
        flutterDepth = 0.65, flutterRate = 5.0,
      },
    },
  },

  edges = {
    { from = "inL", to = "tape.inL" },
    { from = "inR", to = "tape.inR" },
    { from = "tape.outL", to = "outL" },
    { from = "tape.outR", to = "outR" },
  },

  macros = {
    { id = "depth", label = "Depth", default = 0.65,
      targets = { { to = "tape.wowDepth", min = 0.0, max = 1.0 },
                  { to = "tape.flutterDepth", min = 0.0, max = 1.0 } } },
    { id = "speed", label = "Speed", default = 0.47,
      targets = { { to = "tape.wowRate", min = 0.1, max = 1.0 },
                  { to = "tape.flutterRate", min = 1.0, max = 10.0 } } },
    { id = "aggressivity", label = "OU Aggressivity", default = 0.10,
      targets = { { to = "tape.wowVariance", min = 0.0, max = 1.0 },
                  { to = "tape.wowDrift", min = 0.0, max = 0.5 } } },
    { id = "character", label = "Character", default = 0.50,
      targets = { { to = "tape.transportRatio", min = 0.5, max = 2.0, curve = "exp" } } },
  },
}
