-- Family 1: tape vibrato. One stereo TapeDelay, 100% wet, shared wow and flutter.
-- Ports mirror pathfinder. Macros are declarative: a host applies them by hand.
return {
  version = 1,
  name = "Tape Vibrato",

  io = { inputs = { "inL", "inR" }, outputs = { "outL", "outR" } },

  nodes = {
    { id = "tape", type = "TapeDelay",
      config = { baseDelayMs = 8 },
      params = {
        transportRatio = 1.0,
        wowDepth = 0.25, wowRate = 0.30, wowVariance = 0.10, wowDrift = 0.00,
        flutterDepth = 0.02, flutterRate = 4.0,
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
    { id = "depth", label = "Depth", default = 0.35,
      targets = { { to = "tape.wowDepth", map = "vibratoWowDepth" },
                  { to = "tape.flutterDepth", map = "vibratoFlutterDepth" } } },
    { id = "speed", label = "Speed", default = 0.35,
      targets = { { to = "tape.wowRate", map = "vibratoWowRate" },
                  { to = "tape.flutterRate", map = "vibratoFlutterRate" } } },
    { id = "aggressivity", label = "OU Aggressivity", default = 0.15,
      targets = { { to = "tape.wowVariance", map = "vibratoWowVariance" },
                  { to = "tape.wowDrift", map = "vibratoWowDrift" } } },
    { id = "character", label = "Character", default = 0.50,
      targets = { { to = "tape.transportRatio", map = "tapeToCleanTransportRatio" } } },
  },
}
