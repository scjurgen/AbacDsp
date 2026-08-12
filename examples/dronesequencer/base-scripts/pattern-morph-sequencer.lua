-- pattern-morph-sequencer: plays one of three rhythmic chord patterns, crossfaded by a
-- "Morph" knob this library declares for you. A patch script configures it by setting:
--
--   MorphPatternA / MorphPatternB / MorphPatternC
--       Each an array of { note = <midi note>, beats = <ticks to hold> } entries - a
--       rest is just { beats = N } with no note. All three patterns must sum to the same
--       total beats (their combined step count), so a given tick index means the same
--       musical position in every pattern.
--   MorphVelocity (optional, default 0.8)
--
-- Morph knob: 0 = pure A, 0.5 = pure B, 1 = pure C. In between, every tick independently
-- rolls a weighted coin between the two bracketing patterns and plays whichever one wins
-- at that tick (its onset note, or nothing if that pattern is mid-sustain there) - e.g.
-- knob = 0.25 is exactly halfway between A and B, so each tick is a 50/50 draw.

MorphPatternA = MorphPatternA or { { note = 60, beats = 8 } }
MorphPatternB = MorphPatternB or { { note = 64, beats = 8 } }
MorphPatternC = MorphPatternC or { { note = 67, beats = 8 } }
MorphVelocity = MorphVelocity or 0.8
MorphStep = MorphStep or 0
MorphAmount = MorphAmount or 0

UICreateParameterSet({
    { id = "morph", name = "Morph", type = "knob", range = { min = 0, max = 1, step = 0.1, skew = 1 },
      default = 0, description = "Crossfades between MorphPatternA (0), B (0.5) and C (1)" },
})

function OnMorphChanged(value)
    MorphAmount = value
end

-- Total ticks a pattern spans - all three patterns are expected to agree on this.
local function patternLength(pattern)
    local total = 0
    for _, entry in ipairs(pattern) do
        total = total + entry.beats
    end
    return total
end

-- Flattens a { note, beats } run-length pattern into one slot per tick: the note to
-- trigger on an onset tick, or false on a tick where the previous note should keep
-- ringing. Padded with false if the pattern is shorter than totalSteps.
local function expandPattern(pattern, totalSteps)
    local grid = {}
    local index = 1
    for _, entry in ipairs(pattern) do
        grid[index] = entry.note or false
        for _ = 2, entry.beats do
            index = index + 1
            grid[index] = false
        end
        index = index + 1
    end
    while #grid < totalSteps do
        grid[#grid + 1] = false
    end
    return grid
end

function OnStart()
    MorphStep = 0
end

function NextNotes()
    local totalSteps = patternLength(MorphPatternA)
    local gridB = expandPattern(MorphPatternB, totalSteps)
    local lowerGrid, upperGrid, fraction

    if MorphAmount <= 0.5 then
        lowerGrid = expandPattern(MorphPatternA, totalSteps)
        upperGrid = gridB
        fraction = MorphAmount / 0.5
    else
        lowerGrid = gridB
        upperGrid = expandPattern(MorphPatternC, totalSteps)
        fraction = (MorphAmount - 0.5) / 0.5
    end

    local index = (MorphStep % totalSteps) + 1
    MorphStep = MorphStep + 1

    local chosen = (math.random() < fraction) and upperGrid[index] or lowerGrid[index]
    if not chosen then
        return {}
    end
    return { { note = chosen, velocity = MorphVelocity, channel = 0, length = 0, delay = 0 } }
end
