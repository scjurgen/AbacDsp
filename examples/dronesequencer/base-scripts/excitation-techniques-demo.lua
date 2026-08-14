-- Interactive Excite() demo: plays a steady pulse on string 0 every beat, and lets the
-- "Excite" switch fire the selected technique on it live from the Lua Controls panel -
-- flip it to apply "Type" with the given "End (ms)" window and "Harmonic" (Sympathetic
-- only), then flip it again for the next one.

UICreateParameterSet({
    { id = "excite", name = "Excite", type = "switch", default = 0 },
    { id = "type", name = "Type", type = "drop",
      items = { "Pluck", "Strike", "Mute", "PalmMute", "Bow", "Sympathetic", "Wind", "Rub" }, default = 0 },
    { id = "endMs", name = "End (ms)", type = "knob",
      range = { min = 0, max = 3000, step = 1, skew = 1 }, default = 1000, unit = "ms" },
    { id = "harmonic", name = "Harmonic", type = "knob",
      range = { min = 1, max = 10, step = 1, skew = 1 }, default = 1 },
    { id = "strength", name = "Strength", type = "knob",
      range = { min = 0, max = 2, step = 0.1, skew = 1 }, default = 1, unit = "" },
})

local typeNames = { "pluck", "strike", "mute", "palmmute", "bow", "sympathetic", "wind", "rub" }
local selectedType = typeNames[1]
local endMs = 1000
local harmonic = 1
local strength = 0.8

function OnTypeChanged(index)
    selectedType = typeNames[index + 1]
end

function OnEndMsChanged(value)
    endMs = value
end

function OnHarmonicChanged(value)
    harmonic = value
end

function OnStrengthChanged(value)
    strength = value
end

function OnExciteChanged(value)
    if value == 1 then
        Excite(0, { type = selectedType, start = 0, ["end"] = endMs, strength = strength, harmonic = harmonic })
    end
end

function OnTiming(bpm, division)
    BPM = bpm
    DIVISION = division
end

function NextNotes()
    return { { note = 60, velocity = 0.8, channel = 0, length = 0, delay = 0 } }
end
