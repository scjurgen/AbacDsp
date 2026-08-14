-- Demonstrates every Excite() technique, one per beat, on string 0. Load directly as a
-- patch script (Settings > Scripts) rather than importing it - set Division/BPM to taste
-- for a slower or faster walkthrough.
--
-- Pluck and Strike retrigger the string with fresh energy; Bow/Sympathetic/Wind/Rub then
-- keep it ringing continuously; PalmMute and Mute finally dampen/silence whatever is left
-- ringing from the earlier steps - the order is chosen to make each technique's effect on
-- an already-sounding string audible, not just its own isolated attack.

local techniques = {
    { type = "pluck", strength = 0.8 },
    { type = "strike", strength = 0.9 },
    { type = "bow", strength = 0.4, ["end"] = 1500 },
    { type = "sympathetic", strength = 0.3, harmonic = 2, ["end"] = 1500 },
    { type = "wind", strength = 0.35, ["end"] = 1500 },
    { type = "rub", strength = 0.35, ["end"] = 1500 },
    { type = "palmmute", strength = 0.6, ["end"] = 800 },
    { type = "mute", ["end"] = 30 },
}
local index = 1

function OnTiming(bpm, division)
    BPM = bpm
    DIVISION = division
end

function OnStart()
    index = 1
end

function NextNotes()
    local params = techniques[index]
    index = index + 1
    if index > #techniques then
        index = 1
    end
    Excite(0, params)
    return {}
end
