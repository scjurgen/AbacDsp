-- Family 1: tape vibrato. One stereo TapeDelay, 100% wet, shared wow and flutter.
-- A macro shows as one knob: unit, min and max are what the knob displays, default is in those
-- units. Its targets sweep their own min to max as the knob travels; curve = "exp" sweeps
-- geometrically. The parameter values below are the macros at their defaults.
return {
  version = 1,
  name = "Tape Vibrato",

  io = { inputs = { "inL", "inR" }, outputs = { "outL", "outR" } },

  nodes = {
    { id = "tape", type = "TapeDelay",
      config = { baseDelayMs = 10 },
      params = {
        transportRatio = 1.0,
        wowDepth = 0.65, wowRate = 0.50, wowVariance = 0.10, wowDrift = 0.05,
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
    { id = "depth", label = "Depth", unit = "%", min = 0, max = 100, default = 65,
      targets = { { to = "tape.wowDepth", min = 0.0, max = 1.0 },
                  { to = "tape.flutterDepth", min = 0.0, max = 1.0 } } },
    { id = "speed", label = "Speed", unit = "Hz", min = 1, max = 10, default = 5,
      targets = { { to = "tape.wowRate", min = 0.1, max = 1.0 },
                  { to = "tape.flutterRate", min = 1.0, max = 10.0 } } },
    { id = "aggressivity", label = "OU Aggressivity", unit = "%", min = 0, max = 100, default = 10,
      targets = { { to = "tape.wowVariance", min = 0.0, max = 1.0 },
                  { to = "tape.wowDrift", min = 0.0, max = 0.5 } } },
    { id = "character", label = "Character", unit = "%", min = 0, max = 100, default = 50,
      targets = { { to = "tape.transportRatio", min = 0.5, max = 2.0, curve = "exp" } } },
  },
}
