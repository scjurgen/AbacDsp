-- Two bars: a rock groove that starts with a crash, then the same groove with a 16th-note fill
-- around the toms on the last beat of bar 2, leading back into the crash.
SetName("Two-Bar Fill")
SetPatternBars(2)
ClearPattern()

local function groove(barStart, beats)
    for beat = 0, beats - 1 do
        local at = barStart + beat
        if at ~= 0 then
            AddHit(at, HihatClosed, 100)
        end
        AddHit(at + 0.5, HihatClosed, 70)
        AddHit(at, beat % 2 == 0 and Kick or Snare)
    end
end

AddHit(0, Crash, 120)
groove(0, BarBeats)
groove(BarBeats, BarBeats - 1)

local fill = PatternBars * BarBeats - 1
local toms = {Snare, TomHigh, TomLow, TomFloor}
for sixteenth = 0, 3 do
    AddHit(fill + sixteenth / 4, toms[sixteenth + 1], sixteenth == 0 and 110 or 95)
end
AddHit(fill, Kick)
